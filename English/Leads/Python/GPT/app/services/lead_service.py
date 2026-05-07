from datetime import date, datetime, timedelta, timezone
from io import BytesIO
from math import ceil

import pandas as pd
from fastapi import HTTPException, UploadFile, status

from app.core.config import get_settings
from app.core.firebase import get_firestore
from app.models.lead import InteractionCreate, LeadCreate, LeadStatus, LeadUpdate


SORTABLE_FIELDS = {
    "full_name",
    "email",
    "company",
    "position",
    "source",
    "status",
    "capture_date",
    "created_at",
    "updated_at",
}

FRIENDLY_COLUMN_MAP = {
    "full name": "full_name",
    "fullname": "full_name",
    "name": "full_name",
    "job title": "position",
    "captured at": "capture_date",
    "captured_on": "capture_date",
}


def _now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def _search_blob(data: dict) -> str:
    parts = [data.get("full_name", ""), data.get("email", ""), data.get("company", "")]
    return " ".join(part for part in parts if part).strip().lower()


def _lead_payload(owner_id: str, payload: LeadCreate | dict, existing: dict | None = None) -> dict:
    if isinstance(payload, LeadCreate):
        source = payload.model_dump()
    else:
        source = payload.copy()

    base = existing.copy() if existing else {}
    base.update(source)
    base["owner_id"] = owner_id
    base["full_name"] = base["full_name"].strip()
    base["email"] = str(base["email"]).lower().strip()
    base["phone"] = (base.get("phone") or "").strip() or None
    base["company"] = (base.get("company") or "").strip() or None
    base["position"] = (base.get("position") or "").strip() or None
    base["source"] = base["source"].strip()
    if isinstance(base["status"], LeadStatus):
        base["status"] = base["status"].value
    else:
        base["status"] = str(base["status"]).strip().lower()
    if isinstance(base["capture_date"], date):
        base["capture_date"] = base["capture_date"].isoformat()
    base["search_blob"] = _search_blob(base)
    return base


def _serialize_lead(document) -> dict:
    data = document.to_dict()
    return {
        "id": document.id,
        "full_name": data["full_name"],
        "email": data["email"],
        "phone": data.get("phone"),
        "company": data.get("company"),
        "position": data.get("position"),
        "source": data["source"],
        "status": data["status"],
        "capture_date": date.fromisoformat(data["capture_date"]),
        "created_at": datetime.fromisoformat(data["created_at"]),
        "updated_at": datetime.fromisoformat(data["updated_at"]),
        "interaction_count": data.get("interaction_count", 0),
    }


def _serialize_interaction(document) -> dict:
    data = document.to_dict()
    return {
        "id": document.id,
        "interaction_type": data["interaction_type"],
        "summary": data["summary"],
        "notes": data.get("notes"),
        "happened_at": datetime.fromisoformat(data["happened_at"]),
        "created_at": datetime.fromisoformat(data["created_at"]),
    }


def _get_lead_document(owner_id: str, lead_id: str):
    db = get_firestore()
    settings = get_settings()
    document = db.collection(settings.leads_collection).document(lead_id).get()
    if not document.exists:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Lead not found.")
    data = document.to_dict()
    if data.get("owner_id") != owner_id:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Lead not found.")
    return document


def create_lead(owner_id: str, payload: LeadCreate) -> dict:
    db = get_firestore()
    settings = get_settings()
    now = _now_iso()
    document = db.collection(settings.leads_collection).document()
    lead_data = _lead_payload(owner_id, payload)
    lead_data.update({"created_at": now, "updated_at": now, "interaction_count": 0})
    document.set(lead_data)
    return _serialize_lead(document.get())


def update_lead(owner_id: str, lead_id: str, payload: LeadUpdate) -> dict:
    settings = get_settings()
    db = get_firestore()
    document = _get_lead_document(owner_id, lead_id)
    current = document.to_dict()
    updates = {key: value for key, value in payload.model_dump(exclude_none=True).items()}
    merged = _lead_payload(owner_id, updates, existing=current)
    merged["updated_at"] = _now_iso()
    db.collection(settings.leads_collection).document(lead_id).set(merged)
    return _serialize_lead(db.collection(settings.leads_collection).document(lead_id).get())


def delete_lead(owner_id: str, lead_id: str) -> None:
    settings = get_settings()
    db = get_firestore()
    _get_lead_document(owner_id, lead_id)
    db.collection(settings.leads_collection).document(lead_id).delete()

    related = (
        db.collection(settings.interactions_collection)
        .where("lead_id", "==", lead_id)
        .where("owner_id", "==", owner_id)
        .stream()
    )
    for document in related:
        document.reference.delete()


def get_lead(owner_id: str, lead_id: str) -> dict:
    document = _get_lead_document(owner_id, lead_id)
    return _serialize_lead(document)


def list_interactions(owner_id: str, lead_id: str) -> list[dict]:
    _get_lead_document(owner_id, lead_id)
    db = get_firestore()
    settings = get_settings()
    documents = (
        db.collection(settings.interactions_collection)
        .where("lead_id", "==", lead_id)
        .where("owner_id", "==", owner_id)
        .stream()
    )
    interactions = [_serialize_interaction(document) for document in documents]
    return sorted(interactions, key=lambda item: item["happened_at"], reverse=True)


def add_interaction(owner_id: str, lead_id: str, payload: InteractionCreate) -> dict:
    _get_lead_document(owner_id, lead_id)
    db = get_firestore()
    settings = get_settings()
    now = _now_iso()
    document = db.collection(settings.interactions_collection).document()
    document.set(
        {
            "owner_id": owner_id,
            "lead_id": lead_id,
            "interaction_type": payload.interaction_type.strip(),
            "summary": payload.summary.strip(),
            "notes": (payload.notes or "").strip() or None,
            "happened_at": payload.happened_at.astimezone(timezone.utc).isoformat(),
            "created_at": now,
        }
    )

    lead_ref = db.collection(settings.leads_collection).document(lead_id)
    lead = lead_ref.get().to_dict()
    lead_ref.update(
        {
            "interaction_count": int(lead.get("interaction_count", 0)) + 1,
            "updated_at": now,
        }
    )
    return _serialize_interaction(document.get())


def _load_owner_leads(owner_id: str) -> list[dict]:
    db = get_firestore()
    settings = get_settings()
    documents = db.collection(settings.leads_collection).where("owner_id", "==", owner_id).stream()
    return [_serialize_lead(document) for document in documents]


def _apply_filters(
    leads: list[dict],
    status_value: str | None,
    source: str | None,
    capture_date_from: date | None,
    capture_date_to: date | None,
    search: str | None,
) -> list[dict]:
    filtered = leads
    if status_value:
        filtered = [lead for lead in filtered if lead["status"] == status_value]
    if source:
        filtered = [lead for lead in filtered if lead["source"].lower() == source.lower()]
    if capture_date_from:
        filtered = [lead for lead in filtered if lead["capture_date"] >= capture_date_from]
    if capture_date_to:
        filtered = [lead for lead in filtered if lead["capture_date"] <= capture_date_to]
    if search:
        term = search.strip().lower()
        filtered = [
            lead
            for lead in filtered
            if term in " ".join([lead["full_name"], lead["email"], lead.get("company") or ""]).lower()
        ]
    return filtered


def list_leads(
    owner_id: str,
    page: int = 1,
    page_size: int = 10,
    sort_by: str = "capture_date",
    sort_order: str = "desc",
    status_value: str | None = None,
    source: str | None = None,
    capture_date_from: date | None = None,
    capture_date_to: date | None = None,
    search: str | None = None,
) -> dict:
    if sort_by not in SORTABLE_FIELDS:
        sort_by = "capture_date"
    if sort_order not in {"asc", "desc"}:
        sort_order = "desc"

    leads = _load_owner_leads(owner_id)
    filtered = _apply_filters(leads, status_value, source, capture_date_from, capture_date_to, search)
    reverse = sort_order == "desc"
    filtered.sort(key=lambda item: item.get(sort_by) or "", reverse=reverse)

    total = len(filtered)
    total_pages = max(1, ceil(total / page_size)) if page_size else 1
    start = (page - 1) * page_size
    end = start + page_size
    items = filtered[start:end]
    return {
        "items": items,
        "total": total,
        "page": page,
        "page_size": page_size,
        "total_pages": total_pages,
    }


def get_lead_detail(owner_id: str, lead_id: str) -> dict:
    return {"lead": get_lead(owner_id, lead_id), "interactions": list_interactions(owner_id, lead_id)}


def dashboard_summary(owner_id: str) -> dict:
    leads = _load_owner_leads(owner_id)
    status_summary = {status.value: 0 for status in LeadStatus}
    source_summary: dict[str, int] = {}
    daily_counts: dict[str, int] = {}
    today = date.today()
    rolling_start = today - timedelta(days=13)

    for lead in leads:
        status_summary[lead["status"]] = status_summary.get(lead["status"], 0) + 1
        source_summary[lead["source"]] = source_summary.get(lead["source"], 0) + 1
        if lead["capture_date"] >= rolling_start:
            key = lead["capture_date"].isoformat()
            daily_counts[key] = daily_counts.get(key, 0) + 1

    capture_series = []
    for offset in range(14):
        current = rolling_start + timedelta(days=offset)
        key = current.isoformat()
        capture_series.append({"date": key, "count": daily_counts.get(key, 0)})

    recent_leads = [lead for lead in leads if lead["created_at"].date() >= today - timedelta(days=6)]
    qualified = status_summary.get(LeadStatus.qualified.value, 0)
    conversion_rate = round((qualified / len(leads)) * 100, 1) if leads else 0.0

    return {
        "totals": {
            "all_leads": len(leads),
            "recent_leads": len(recent_leads),
            "qualified_rate": conversion_rate,
        },
        "by_status": status_summary,
        "captures_over_time": capture_series,
        "top_sources": sorted(
            [{"source": source, "count": count} for source, count in source_summary.items()],
            key=lambda item: item["count"],
            reverse=True,
        )[:6],
    }


def _normalize_import_columns(frame: pd.DataFrame) -> pd.DataFrame:
    normalized = []
    for column in frame.columns:
        key = str(column).strip().lower().replace("-", "_").replace(" ", "_")
        key = FRIENDLY_COLUMN_MAP.get(key.replace("_", " "), key)
        normalized.append(key)
    frame.columns = normalized
    return frame


def import_leads(owner_id: str, upload: UploadFile) -> dict:
    filename = (upload.filename or "").lower()
    if filename.endswith(".csv"):
        frame = pd.read_csv(upload.file)
    elif filename.endswith(".xlsx") or filename.endswith(".xls"):
        frame = pd.read_excel(upload.file)
    else:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Only CSV, XLSX, and XLS files are supported.",
        )

    frame = _normalize_import_columns(frame).fillna("")
    required = {"full_name", "email"}
    missing = required - set(frame.columns)
    if missing:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Missing required columns: {', '.join(sorted(missing))}",
        )

    created = 0
    errors: list[str] = []
    db = get_firestore()
    settings = get_settings()

    for index, row in frame.iterrows():
        try:
            capture_raw = row.get("capture_date") or date.today().isoformat()
            capture_date = pd.to_datetime(capture_raw).date()
            status_value = str(row.get("status") or LeadStatus.new.value).strip().lower()
            payload = LeadCreate(
                full_name=str(row.get("full_name") or "").strip(),
                email=str(row.get("email") or "").strip(),
                phone=str(row.get("phone") or "").strip() or None,
                company=str(row.get("company") or "").strip() or None,
                position=str(row.get("position") or "").strip() or None,
                source=str(row.get("source") or "import").strip(),
                status=LeadStatus(status_value),
                capture_date=capture_date,
            )
            now = _now_iso()
            document = db.collection(settings.leads_collection).document()
            lead_data = _lead_payload(owner_id, payload)
            lead_data.update({"created_at": now, "updated_at": now, "interaction_count": 0})
            document.set(lead_data)
            created += 1
        except Exception as exc:
            errors.append(f"Row {index + 2}: {exc}")

    return {"created": created, "failed": len(errors), "errors": errors[:20]}


def export_leads(
    owner_id: str,
    format_name: str,
    status_value: str | None = None,
    source: str | None = None,
    capture_date_from: date | None = None,
    capture_date_to: date | None = None,
    search: str | None = None,
) -> tuple[BytesIO, str, str]:
    items = list_leads(
        owner_id=owner_id,
        page=1,
        page_size=100000,
        sort_by="capture_date",
        sort_order="desc",
        status_value=status_value,
        source=source,
        capture_date_from=capture_date_from,
        capture_date_to=capture_date_to,
        search=search,
    )["items"]

    frame = pd.DataFrame(
        [
            {
                "full_name": lead["full_name"],
                "email": lead["email"],
                "phone": lead["phone"],
                "company": lead["company"],
                "position": lead["position"],
                "source": lead["source"],
                "status": lead["status"],
                "capture_date": lead["capture_date"].isoformat(),
                "created_at": lead["created_at"].isoformat(),
                "updated_at": lead["updated_at"].isoformat(),
                "interaction_count": lead["interaction_count"],
            }
            for lead in items
        ]
    )

    buffer = BytesIO()
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    if format_name == "csv":
        buffer.write(frame.to_csv(index=False).encode("utf-8"))
        media_type = "text/csv"
        filename = f"leads_export_{timestamp}.csv"
    elif format_name == "xlsx":
        with pd.ExcelWriter(buffer, engine="openpyxl") as writer:
            frame.to_excel(writer, index=False, sheet_name="Leads")
        media_type = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"
        filename = f"leads_export_{timestamp}.xlsx"
    else:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Invalid export format.")

    buffer.seek(0)
    return buffer, media_type, filename
