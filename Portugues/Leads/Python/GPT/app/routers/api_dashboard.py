from __future__ import annotations

from fastapi import APIRouter, Depends

from app.dependencies import get_current_user, get_firestore_service
from app.schemas import DashboardSummaryResponse
from app.services.firestore_service import FirestoreService

router = APIRouter(prefix="/api/dashboard", tags=["Dashboard"])


@router.get("/summary", response_model=DashboardSummaryResponse)
def get_dashboard_summary(
    user: dict = Depends(get_current_user),
    service: FirestoreService = Depends(get_firestore_service),
):
    return service.get_dashboard_summary(user["id"])
