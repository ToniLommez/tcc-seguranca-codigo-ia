import io
import csv
from datetime import datetime, timezone
from fastapi import APIRouter, HTTPException, Depends, Query, UploadFile, File
from fastapi.responses import StreamingResponse
from typing import Optional
from models import LeadCreate, LeadUpdate, LeadStatus, LeadSource, InteractionCreate
from auth import get_current_user
from config import db, COLLECTION_PREFIX
import pandas as pd

router = APIRouter(prefix="/api/leads", tags=["Leads"])


def get_leads_collection(user_id: str):
    return db.collection(f"{COLLECTION_PREFIX}_leads_{user_id}")


@router.post("")
def create_lead(lead: LeadCreate, current_user: dict = Depends(get_current_user)):
    leads_ref = get_leads_collection(current_user["id"])
    lead_data = lead.model_dump()
    lead_data["capture_date"] = datetime.now(timezone.utc).isoformat()
    lead_data["created_at"] = datetime.now(timezone.utc).isoformat()
    lead_data["updated_at"] = datetime.now(timezone.utc).isoformat()
    lead_data["interactions"] = []

    doc_ref = leads_ref.add(lead_data)
    lead_data["id"] = doc_ref[1].id
    return lead_data


@router.get("")
def list_leads(
    page: int = Query(1, ge=1),
    per_page: int = Query(20, ge=1, le=100),
    sort_by: str = Query("capture_date", regex="^(capture_date|full_name|company|status|created_at)$"),
    sort_order: str = Query("desc", regex="^(asc|desc)$"),
    status: Optional[LeadStatus] = None,
    source: Optional[LeadSource] = None,
    date_from: Optional[str] = None,
    date_to: Optional[str] = None,
    search: Optional[str] = None,
    current_user: dict = Depends(get_current_user),
):
    leads_ref = get_leads_collection(current_user["id"])
    query = leads_ref

    if status:
        query = query.where("status", "==", status.value)
    if source:
        query = query.where("source", "==", source.value)

    direction = "DESCENDING" if sort_order == "desc" else "ASCENDING"
    from google.cloud.firestore_v1.base_query import FieldFilter
    query = query.order_by(sort_by, direction=direction)

    all_docs = list(query.stream())

    results = []
    for doc in all_docs:
        data = doc.to_dict()
        data["id"] = doc.id

        if date_from and data.get("capture_date", "") < date_from:
            continue
        if date_to and data.get("capture_date", "") > date_to:
            continue
        if search:
            search_lower = search.lower()
            searchable = f"{data.get('full_name', '')} {data.get('email', '')} {data.get('company', '')}".lower()
            if search_lower not in searchable:
                continue
        results.append(data)

    total = len(results)
    start = (page - 1) * per_page
    end = start + per_page
    paginated = results[start:end]

    return {
        "leads": paginated,
        "total": total,
        "page": page,
        "per_page": per_page,
        "total_pages": (total + per_page - 1) // per_page,
    }


@router.get("/stats")
def get_stats(current_user: dict = Depends(get_current_user)):
    leads_ref = get_leads_collection(current_user["id"])
    all_docs = list(leads_ref.stream())

    status_counts = {"new": 0, "contacted": 0, "qualified": 0, "lost": 0}
    source_counts = {}
    monthly_captures = {}

    for doc in all_docs:
        data = doc.to_dict()
        s = data.get("status", "new")
        status_counts[s] = status_counts.get(s, 0) + 1

        src = data.get("source", "other")
        source_counts[src] = source_counts.get(src, 0) + 1

        capture_date = data.get("capture_date", "")
        if capture_date:
            month_key = capture_date[:7]
            monthly_captures[month_key] = monthly_captures.get(month_key, 0) + 1

    return {
        "total_leads": len(all_docs),
        "status_counts": status_counts,
        "source_counts": source_counts,
        "monthly_captures": dict(sorted(monthly_captures.items())),
    }


@router.get("/{lead_id}")
def get_lead(lead_id: str, current_user: dict = Depends(get_current_user)):
    leads_ref = get_leads_collection(current_user["id"])
    doc = leads_ref.document(lead_id).get()
    if not doc.exists:
        raise HTTPException(status_code=404, detail="Lead not found")
    data = doc.to_dict()
    data["id"] = doc.id
    return data


@router.put("/{lead_id}")
def update_lead(lead_id: str, lead: LeadUpdate, current_user: dict = Depends(get_current_user)):
    leads_ref = get_leads_collection(current_user["id"])
    doc_ref = leads_ref.document(lead_id)
    doc = doc_ref.get()
    if not doc.exists:
        raise HTTPException(status_code=404, detail="Lead not found")

    update_data = {k: v for k, v in lead.model_dump().items() if v is not None}
    update_data["updated_at"] = datetime.now(timezone.utc).isoformat()
    doc_ref.update(update_data)

    updated_doc = doc_ref.get()
    data = updated_doc.to_dict()
    data["id"] = updated_doc.id
    return data


@router.delete("/{lead_id}")
def delete_lead(lead_id: str, current_user: dict = Depends(get_current_user)):
    leads_ref = get_leads_collection(current_user["id"])
    doc_ref = leads_ref.document(lead_id)
    doc = doc_ref.get()
    if not doc.exists:
        raise HTTPException(status_code=404, detail="Lead not found")
    doc_ref.delete()
    return {"message": "Lead deleted successfully"}


@router.post("/{lead_id}/interactions")
def add_interaction(lead_id: str, interaction: InteractionCreate, current_user: dict = Depends(get_current_user)):
    leads_ref = get_leads_collection(current_user["id"])
    doc_ref = leads_ref.document(lead_id)
    doc = doc_ref.get()
    if not doc.exists:
        raise HTTPException(status_code=404, detail="Lead not found")

    data = doc.to_dict()
    interactions = data.get("interactions", [])
    new_interaction = {
        "note": interaction.note,
        "interaction_type": interaction.interaction_type,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "created_by": current_user["name"],
    }
    interactions.append(new_interaction)
    doc_ref.update({"interactions": interactions, "updated_at": datetime.now(timezone.utc).isoformat()})

    return new_interaction


@router.post("/import/csv")
def import_csv(file: UploadFile = File(...), current_user: dict = Depends(get_current_user)):
    if not file.filename.endswith((".csv", ".xlsx", ".xls")):
        raise HTTPException(status_code=400, detail="File must be CSV or Excel format")

    content = file.file.read()
    if file.filename.endswith(".csv"):
        df = pd.read_csv(io.BytesIO(content))
    else:
        df = pd.read_excel(io.BytesIO(content))

    required_columns = ["full_name", "email"]
    for col in required_columns:
        if col not in df.columns:
            raise HTTPException(status_code=400, detail=f"Missing required column: {col}")

    leads_ref = get_leads_collection(current_user["id"])
    imported_count = 0
    errors = []

    valid_statuses = [s.value for s in LeadStatus]
    valid_sources = [s.value for s in LeadSource]

    for idx, row in df.iterrows():
        try:
            status_val = str(row.get("status", "new")).lower().strip()
            if status_val not in valid_statuses:
                status_val = "new"

            source_val = str(row.get("source", "other")).lower().strip()
            if source_val not in valid_sources:
                source_val = "other"

            lead_data = {
                "full_name": str(row["full_name"]).strip(),
                "email": str(row["email"]).strip(),
                "phone": str(row.get("phone", "")).strip() if pd.notna(row.get("phone")) else "",
                "company": str(row.get("company", "")).strip() if pd.notna(row.get("company")) else "",
                "position": str(row.get("position", "")).strip() if pd.notna(row.get("position")) else "",
                "source": source_val,
                "status": status_val,
                "capture_date": datetime.now(timezone.utc).isoformat(),
                "created_at": datetime.now(timezone.utc).isoformat(),
                "updated_at": datetime.now(timezone.utc).isoformat(),
                "interactions": [],
            }
            leads_ref.add(lead_data)
            imported_count += 1
        except Exception as e:
            errors.append(f"Row {idx + 1}: {str(e)}")

    return {"imported": imported_count, "errors": errors, "total_rows": len(df)}


@router.get("/export/csv")
def export_csv(current_user: dict = Depends(get_current_user)):
    leads_ref = get_leads_collection(current_user["id"])
    all_docs = list(leads_ref.stream())

    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(["full_name", "email", "phone", "company", "position", "source", "status", "capture_date"])

    for doc in all_docs:
        data = doc.to_dict()
        writer.writerow([
            data.get("full_name", ""),
            data.get("email", ""),
            data.get("phone", ""),
            data.get("company", ""),
            data.get("position", ""),
            data.get("source", ""),
            data.get("status", ""),
            data.get("capture_date", ""),
        ])

    output.seek(0)
    return StreamingResponse(
        io.BytesIO(output.getvalue().encode()),
        media_type="text/csv",
        headers={"Content-Disposition": "attachment; filename=leads_export.csv"},
    )


@router.get("/export/excel")
def export_excel(current_user: dict = Depends(get_current_user)):
    leads_ref = get_leads_collection(current_user["id"])
    all_docs = list(leads_ref.stream())

    rows = []
    for doc in all_docs:
        data = doc.to_dict()
        rows.append({
            "full_name": data.get("full_name", ""),
            "email": data.get("email", ""),
            "phone": data.get("phone", ""),
            "company": data.get("company", ""),
            "position": data.get("position", ""),
            "source": data.get("source", ""),
            "status": data.get("status", ""),
            "capture_date": data.get("capture_date", ""),
        })

    df = pd.DataFrame(rows)
    output = io.BytesIO()
    df.to_excel(output, index=False, engine="openpyxl")
    output.seek(0)

    return StreamingResponse(
        output,
        media_type="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        headers={"Content-Disposition": "attachment; filename=leads_export.xlsx"},
    )
