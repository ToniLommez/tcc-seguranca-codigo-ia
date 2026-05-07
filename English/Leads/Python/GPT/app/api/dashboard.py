from fastapi import APIRouter, Depends

from app.api.dependencies import get_current_user
from app.services.lead_service import dashboard_summary


router = APIRouter(prefix="/api/dashboard", tags=["Dashboard"])


@router.get("/summary")
def get_dashboard_summary(current_user: dict = Depends(get_current_user)):
    return dashboard_summary(current_user["id"])
