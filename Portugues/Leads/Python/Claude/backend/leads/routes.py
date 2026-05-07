import io
from datetime import datetime
from typing import Optional

import pandas as pd
from fastapi import APIRouter, Depends, File, HTTPException, Query, UploadFile
from fastapi.responses import StreamingResponse
from google.cloud.firestore_v1.base_query import FieldFilter

from auth.routes import get_current_user
from config import (
    DEFAULT_PAGE_SIZE,
    INTERACTIONS_COLLECTION,
    LEADS_COLLECTION,
    MAX_PAGE_SIZE,
)
from firebase_config import get_db
from leads.models import InteracaoCreate, LeadCreate, LeadUpdate

router = APIRouter(prefix="/api/leads", tags=["leads"])

LEAD_CSV_COLUMNS = [
    "nome_completo", "email", "telefone", "empresa", "cargo",
    "fonte", "status", "data_captura", "notas",
]

FONTE_LABELS = {
    "indicacao": "Indicação",
    "site": "Site",
    "evento": "Evento",
    "redes_sociais": "Redes Sociais",
    "email_marketing": "E-mail Marketing",
    "outros": "Outros",
}

STATUS_LABELS = {
    "novo": "Novo",
    "em_contato": "Em Contato",
    "qualificado": "Qualificado",
    "perdido": "Perdido",
}


def _lead_to_dict(doc) -> dict:
    data = doc.to_dict()
    data["id"] = doc.id
    return data


def _fetch_user_leads(db, user_id: str, status: Optional[str] = None, fonte: Optional[str] = None,
                      data_inicio: Optional[str] = None, data_fim: Optional[str] = None) -> list[dict]:
    ref = db.collection(LEADS_COLLECTION)
    query = ref.where(filter=FieldFilter("user_id", "==", user_id))

    if status:
        query = query.where(filter=FieldFilter("status", "==", status))
    if fonte:
        query = query.where(filter=FieldFilter("fonte", "==", fonte))

    docs = query.stream()
    leads = [_lead_to_dict(d) for d in docs]

    if data_inicio:
        leads = [l for l in leads if l.get("data_captura", "") >= data_inicio]
    if data_fim:
        leads = [l for l in leads if l.get("data_captura", "") <= data_fim]

    return leads


# ─── STATS (deve vir antes de /{lead_id}) ────────────────────────────────────

@router.get("/stats")
def get_stats(current_user: dict = Depends(get_current_user)):
    db = get_db()
    leads = _fetch_user_leads(db, current_user["sub"])

    by_status: dict[str, int] = {}
    by_fonte: dict[str, int] = {}
    by_day: dict[str, int] = {}

    for lead in leads:
        s = lead.get("status", "novo")
        by_status[s] = by_status.get(s, 0) + 1

        f = lead.get("fonte", "outros")
        by_fonte[f] = by_fonte.get(f, 0) + 1

        dc = lead.get("data_captura", "")
        if dc:
            day = dc[:10]
            by_day[day] = by_day.get(day, 0) + 1

    recent = sorted(leads, key=lambda l: l.get("created_at", ""), reverse=True)[:10]

    return {
        "total": len(leads),
        "by_status": by_status,
        "by_fonte": by_fonte,
        "by_day": dict(sorted(by_day.items())),
        "recent": recent,
    }


# ─── EXPORT ───────────────────────────────────────────────────────────────────

@router.get("/export")
def export_leads(
    fmt: str = Query("csv", pattern="^(csv|excel)$"),
    status: Optional[str] = None,
    fonte: Optional[str] = None,
    current_user: dict = Depends(get_current_user),
):
    db = get_db()
    leads = _fetch_user_leads(db, current_user["sub"], status=status, fonte=fonte)

    rows = [
        {col: lead.get(col, "") for col in LEAD_CSV_COLUMNS}
        for lead in leads
    ]
    df = pd.DataFrame(rows, columns=LEAD_CSV_COLUMNS)

    if fmt == "excel":
        buf = io.BytesIO()
        with pd.ExcelWriter(buf, engine="openpyxl") as writer:
            df.to_excel(writer, index=False, sheet_name="Leads")
        buf.seek(0)
        return StreamingResponse(
            buf,
            media_type="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
            headers={"Content-Disposition": "attachment; filename=leads.xlsx"},
        )

    buf = io.StringIO()
    df.to_csv(buf, index=False, encoding="utf-8-sig")
    buf.seek(0)
    return StreamingResponse(
        io.BytesIO(buf.getvalue().encode("utf-8-sig")),
        media_type="text/csv",
        headers={"Content-Disposition": "attachment; filename=leads.csv"},
    )


# ─── IMPORT ───────────────────────────────────────────────────────────────────

@router.post("/import", status_code=201)
async def import_leads(
    file: UploadFile = File(...),
    current_user: dict = Depends(get_current_user),
):
    content = await file.read()
    filename = file.filename or ""

    try:
        if filename.endswith(".xlsx") or filename.endswith(".xls"):
            df = pd.read_excel(io.BytesIO(content))
        else:
            df = pd.read_csv(io.BytesIO(content), encoding="utf-8-sig")
    except Exception as e:
        raise HTTPException(status_code=400, detail=f"Erro ao ler arquivo: {e}")

    df.columns = [c.strip().lower().replace(" ", "_") for c in df.columns]

    db = get_db()
    leads_ref = db.collection(LEADS_COLLECTION)
    imported = 0
    errors = []

    for i, row in df.iterrows():
        try:
            now = datetime.utcnow().isoformat()
            data = {
                "nome_completo": str(row.get("nome_completo", "")).strip(),
                "email": str(row.get("email", "")).strip(),
                "telefone": str(row.get("telefone", "")).strip(),
                "empresa": str(row.get("empresa", "")).strip(),
                "cargo": str(row.get("cargo", "")).strip(),
                "fonte": str(row.get("fonte", "outros")).strip().lower(),
                "status": str(row.get("status", "novo")).strip().lower(),
                "data_captura": str(row.get("data_captura", now[:10])).strip(),
                "notas": str(row.get("notas", "")).strip(),
                "user_id": current_user["sub"],
                "created_at": now,
                "updated_at": now,
            }
            if not data["nome_completo"] or not data["email"]:
                errors.append(f"Linha {i + 2}: nome e e-mail são obrigatórios")
                continue
            leads_ref.add(data)
            imported += 1
        except Exception as e:
            errors.append(f"Linha {i + 2}: {e}")

    return {"imported": imported, "errors": errors}


# ─── CRUD ─────────────────────────────────────────────────────────────────────

@router.get("")
def list_leads(
    page: int = Query(1, ge=1),
    page_size: int = Query(DEFAULT_PAGE_SIZE, ge=1, le=MAX_PAGE_SIZE),
    status: Optional[str] = None,
    fonte: Optional[str] = None,
    data_inicio: Optional[str] = None,
    data_fim: Optional[str] = None,
    q: Optional[str] = None,
    order_by: str = Query("created_at", pattern="^(nome_completo|empresa|data_captura|status|created_at)$"),
    order_dir: str = Query("desc", pattern="^(asc|desc)$"),
    current_user: dict = Depends(get_current_user),
):
    db = get_db()
    leads = _fetch_user_leads(db, current_user["sub"], status=status, fonte=fonte,
                              data_inicio=data_inicio, data_fim=data_fim)

    if q:
        q_lower = q.lower()
        leads = [
            l for l in leads
            if q_lower in l.get("nome_completo", "").lower()
            or q_lower in l.get("email", "").lower()
            or q_lower in l.get("empresa", "").lower()
        ]

    reverse = order_dir == "desc"
    leads.sort(key=lambda l: l.get(order_by, ""), reverse=reverse)

    total = len(leads)
    start = (page - 1) * page_size
    page_leads = leads[start: start + page_size]

    return {
        "total": total,
        "page": page,
        "page_size": page_size,
        "total_pages": max(1, (total + page_size - 1) // page_size),
        "items": page_leads,
    }


@router.post("", status_code=201)
def create_lead(body: LeadCreate, current_user: dict = Depends(get_current_user)):
    db = get_db()
    now = datetime.utcnow().isoformat()
    data = body.model_dump()
    data["data_captura"] = data.get("data_captura") or now[:10]
    data["user_id"] = current_user["sub"]
    data["created_at"] = now
    data["updated_at"] = now

    doc_ref = db.collection(LEADS_COLLECTION).add(data)
    lead_id = doc_ref[1].id
    return {**data, "id": lead_id}


@router.get("/{lead_id}")
def get_lead(lead_id: str, current_user: dict = Depends(get_current_user)):
    db = get_db()
    doc = db.collection(LEADS_COLLECTION).document(lead_id).get()
    if not doc.exists:
        raise HTTPException(status_code=404, detail="Lead não encontrado")
    lead = _lead_to_dict(doc)
    if lead["user_id"] != current_user["sub"]:
        raise HTTPException(status_code=403, detail="Acesso negado")
    return lead


@router.put("/{lead_id}")
def update_lead(lead_id: str, body: LeadUpdate, current_user: dict = Depends(get_current_user)):
    db = get_db()
    ref = db.collection(LEADS_COLLECTION).document(lead_id)
    doc = ref.get()
    if not doc.exists:
        raise HTTPException(status_code=404, detail="Lead não encontrado")
    if doc.to_dict()["user_id"] != current_user["sub"]:
        raise HTTPException(status_code=403, detail="Acesso negado")

    updates = {k: v for k, v in body.model_dump().items() if v is not None}
    updates["updated_at"] = datetime.utcnow().isoformat()
    ref.update(updates)

    updated = ref.get().to_dict()
    updated["id"] = lead_id
    return updated


@router.delete("/{lead_id}", status_code=204)
def delete_lead(lead_id: str, current_user: dict = Depends(get_current_user)):
    db = get_db()
    ref = db.collection(LEADS_COLLECTION).document(lead_id)
    doc = ref.get()
    if not doc.exists:
        raise HTTPException(status_code=404, detail="Lead não encontrado")
    if doc.to_dict()["user_id"] != current_user["sub"]:
        raise HTTPException(status_code=403, detail="Acesso negado")
    # Remove interações associadas
    ints = db.collection(INTERACTIONS_COLLECTION).where(filter=FieldFilter("lead_id", "==", lead_id)).stream()
    for i in ints:
        i.reference.delete()
    ref.delete()


# ─── INTERAÇÕES ───────────────────────────────────────────────────────────────

@router.get("/{lead_id}/interactions")
def list_interactions(lead_id: str, current_user: dict = Depends(get_current_user)):
    db = get_db()
    doc = db.collection(LEADS_COLLECTION).document(lead_id).get()
    if not doc.exists or doc.to_dict()["user_id"] != current_user["sub"]:
        raise HTTPException(status_code=404, detail="Lead não encontrado")

    docs = db.collection(INTERACTIONS_COLLECTION).where(filter=FieldFilter("lead_id", "==", lead_id)).stream()
    interactions = []
    for d in docs:
        item = d.to_dict()
        item["id"] = d.id
        interactions.append(item)
    interactions.sort(key=lambda i: i.get("data", ""), reverse=True)
    return interactions


@router.post("/{lead_id}/interactions", status_code=201)
def create_interaction(
    lead_id: str,
    body: InteracaoCreate,
    current_user: dict = Depends(get_current_user),
):
    db = get_db()
    doc = db.collection(LEADS_COLLECTION).document(lead_id).get()
    if not doc.exists or doc.to_dict()["user_id"] != current_user["sub"]:
        raise HTTPException(status_code=404, detail="Lead não encontrado")

    now = datetime.utcnow().isoformat()
    data = {
        "lead_id": lead_id,
        "user_id": current_user["sub"],
        "tipo": body.tipo,
        "descricao": body.descricao,
        "data": body.data or now[:10],
        "created_at": now,
    }
    ref = db.collection(INTERACTIONS_COLLECTION).add(data)
    return {**data, "id": ref[1].id}


@router.delete("/{lead_id}/interactions/{interaction_id}", status_code=204)
def delete_interaction(
    lead_id: str,
    interaction_id: str,
    current_user: dict = Depends(get_current_user),
):
    db = get_db()
    ref = db.collection(INTERACTIONS_COLLECTION).document(interaction_id)
    doc = ref.get()
    if not doc.exists or doc.to_dict().get("user_id") != current_user["sub"]:
        raise HTTPException(status_code=404, detail="Interação não encontrada")
    ref.delete()
