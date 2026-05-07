from __future__ import annotations

import io
from datetime import date

import pandas as pd
from fastapi import APIRouter, Depends, File, HTTPException, Query, UploadFile, status
from fastapi.responses import Response

from app.dependencies import get_current_user, get_firestore_service
from app.schemas import (
    ImportResultResponse,
    InteractionCreateRequest,
    InteractionResponse,
    LeadCreateRequest,
    LeadListResponse,
    LeadResponse,
    LeadUpdateRequest,
)
from app.services.firestore_service import FirestoreService

router = APIRouter(prefix="/api/leads", tags=["Leads"])


@router.get("", response_model=LeadListResponse)
def list_leads(
    page: int = Query(default=1, ge=1),
    page_size: int = Query(default=10, ge=1, le=100),
    sort_by: str = Query(default="captured_at"),
    sort_direction: str = Query(default="desc"),
    status_filter: str | None = Query(default=None, alias="status"),
    source: str | None = Query(default=None),
    captured_from: date | None = Query(default=None),
    captured_to: date | None = Query(default=None),
    search: str | None = Query(default=None),
    user: dict = Depends(get_current_user),
    service: FirestoreService = Depends(get_firestore_service),
):
    try:
        return service.list_leads(
            owner_id=user["id"],
            page=page,
            page_size=page_size,
            sort_by=sort_by,
            sort_direction=sort_direction,
            status=status_filter,
            source=source,
            captured_from=captured_from,
            captured_to=captured_to,
            search=search,
        )
    except ValueError as exc:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=str(exc)) from exc


@router.post("", response_model=LeadResponse, status_code=status.HTTP_201_CREATED)
def create_lead(
    payload: LeadCreateRequest,
    user: dict = Depends(get_current_user),
    service: FirestoreService = Depends(get_firestore_service),
):
    return service.create_lead(owner_id=user["id"], payload=payload.model_dump())


@router.get("/export")
def export_leads(
    file_format: str = Query(default="csv", pattern="^(csv|xlsx)$"),
    sort_by: str = Query(default="captured_at"),
    sort_direction: str = Query(default="desc"),
    status_filter: str | None = Query(default=None, alias="status"),
    source: str | None = Query(default=None),
    captured_from: date | None = Query(default=None),
    captured_to: date | None = Query(default=None),
    search: str | None = Query(default=None),
    user: dict = Depends(get_current_user),
    service: FirestoreService = Depends(get_firestore_service),
):
    result = service.list_leads(
        owner_id=user["id"],
        page=1,
        page_size=50000,
        sort_by=sort_by,
        sort_direction=sort_direction,
        status=status_filter,
        source=source,
        captured_from=captured_from,
        captured_to=captured_to,
        search=search,
    )
    content, media_type, filename = service.export_leads(result["items"], file_format=file_format)
    headers = {"Content-Disposition": f'attachment; filename="{filename}"'}
    return Response(content=content, media_type=media_type, headers=headers)


@router.post("/import", response_model=ImportResultResponse)
async def import_leads(
    file: UploadFile = File(...),
    user: dict = Depends(get_current_user),
    service: FirestoreService = Depends(get_firestore_service),
):
    filename = (file.filename or "").lower()
    content = await file.read()

    if filename.endswith(".csv"):
        dataframe = pd.read_csv(io.BytesIO(content))
    elif filename.endswith(".xlsx") or filename.endswith(".xls"):
        dataframe = pd.read_excel(io.BytesIO(content))
    else:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Envie um arquivo CSV ou Excel (.xlsx/.xls).",
        )

    try:
        return service.import_leads_from_dataframe(user["id"], dataframe)
    except ValueError as exc:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=str(exc)) from exc


@router.get("/{lead_id}", response_model=LeadResponse)
def get_lead(
    lead_id: str,
    user: dict = Depends(get_current_user),
    service: FirestoreService = Depends(get_firestore_service),
):
    lead = service.get_lead(owner_id=user["id"], lead_id=lead_id)
    if not lead:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Lead não encontrado.")
    return lead


@router.put("/{lead_id}", response_model=LeadResponse)
def update_lead(
    lead_id: str,
    payload: LeadUpdateRequest,
    user: dict = Depends(get_current_user),
    service: FirestoreService = Depends(get_firestore_service),
):
    lead = service.update_lead(owner_id=user["id"], lead_id=lead_id, payload=payload.model_dump())
    if not lead:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Lead não encontrado.")
    return lead


@router.delete("/{lead_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_lead(
    lead_id: str,
    user: dict = Depends(get_current_user),
    service: FirestoreService = Depends(get_firestore_service),
):
    deleted = service.delete_lead(owner_id=user["id"], lead_id=lead_id)
    if not deleted:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Lead não encontrado.")
    return Response(status_code=status.HTTP_204_NO_CONTENT)


@router.get("/{lead_id}/interactions", response_model=list[InteractionResponse])
def list_interactions(
    lead_id: str,
    user: dict = Depends(get_current_user),
    service: FirestoreService = Depends(get_firestore_service),
):
    lead = service.get_lead(owner_id=user["id"], lead_id=lead_id)
    if not lead:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Lead não encontrado.")
    return lead["interactions"]


@router.post("/{lead_id}/interactions", response_model=InteractionResponse, status_code=status.HTTP_201_CREATED)
def create_interaction(
    lead_id: str,
    payload: InteractionCreateRequest,
    user: dict = Depends(get_current_user),
    service: FirestoreService = Depends(get_firestore_service),
):
    interaction = service.add_interaction(
        owner_id=user["id"],
        lead_id=lead_id,
        interaction_type=payload.interaction_type,
        note=payload.note,
        created_by_id=user["id"],
        created_by_name=user["name"],
    )
    if not interaction:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Lead não encontrado.")
    return interaction
