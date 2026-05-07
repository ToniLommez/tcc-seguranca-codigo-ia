from datetime import date

from fastapi import APIRouter, Depends, Query, UploadFile
from fastapi.responses import StreamingResponse

from app.api.dependencies import get_current_user
from app.models.lead import (
    InteractionCreate,
    InteractionRead,
    LeadCreate,
    LeadDetailResponse,
    LeadListResponse,
    LeadRead,
    LeadUpdate,
)
from app.services.lead_service import (
    add_interaction,
    create_lead,
    delete_lead,
    export_leads,
    get_lead_detail,
    import_leads,
    list_interactions,
    list_leads,
    update_lead,
)


router = APIRouter(prefix="/api/leads", tags=["Leads"])


@router.get("", response_model=LeadListResponse)
def get_leads(
    page: int = Query(default=1, ge=1),
    page_size: int = Query(default=10, ge=1, le=100),
    sort_by: str = Query(default="capture_date"),
    sort_order: str = Query(default="desc"),
    status: str | None = Query(default=None),
    source: str | None = Query(default=None),
    capture_date_from: date | None = Query(default=None),
    capture_date_to: date | None = Query(default=None),
    search: str | None = Query(default=None),
    current_user: dict = Depends(get_current_user),
):
    return list_leads(
        owner_id=current_user["id"],
        page=page,
        page_size=page_size,
        sort_by=sort_by,
        sort_order=sort_order,
        status_value=status,
        source=source,
        capture_date_from=capture_date_from,
        capture_date_to=capture_date_to,
        search=search,
    )


@router.post("", response_model=LeadRead, status_code=201)
def post_lead(payload: LeadCreate, current_user: dict = Depends(get_current_user)):
    return create_lead(current_user["id"], payload)


@router.post("/import")
def post_import(upload: UploadFile, current_user: dict = Depends(get_current_user)):
    return import_leads(current_user["id"], upload)


@router.get("/export")
def get_export(
    format: str = Query(default="csv"),
    status: str | None = Query(default=None),
    source: str | None = Query(default=None),
    capture_date_from: date | None = Query(default=None),
    capture_date_to: date | None = Query(default=None),
    search: str | None = Query(default=None),
    current_user: dict = Depends(get_current_user),
):
    buffer, media_type, filename = export_leads(
        owner_id=current_user["id"],
        format_name=format,
        status_value=status,
        source=source,
        capture_date_from=capture_date_from,
        capture_date_to=capture_date_to,
        search=search,
    )
    return StreamingResponse(
        buffer,
        media_type=media_type,
        headers={"Content-Disposition": f'attachment; filename="{filename}"'},
    )


@router.get("/{lead_id}", response_model=LeadDetailResponse)
def get_lead(lead_id: str, current_user: dict = Depends(get_current_user)):
    return get_lead_detail(current_user["id"], lead_id)


@router.put("/{lead_id}", response_model=LeadRead)
def put_lead(lead_id: str, payload: LeadUpdate, current_user: dict = Depends(get_current_user)):
    return update_lead(current_user["id"], lead_id, payload)


@router.delete("/{lead_id}", status_code=204)
def remove_lead(lead_id: str, current_user: dict = Depends(get_current_user)):
    delete_lead(current_user["id"], lead_id)


@router.get("/{lead_id}/interactions", response_model=list[InteractionRead])
def get_interactions(lead_id: str, current_user: dict = Depends(get_current_user)):
    return list_interactions(current_user["id"], lead_id)


@router.post("/{lead_id}/interactions", response_model=InteractionRead, status_code=201)
def post_interaction(
    lead_id: str,
    payload: InteractionCreate,
    current_user: dict = Depends(get_current_user),
):
    return add_interaction(current_user["id"], lead_id, payload)
