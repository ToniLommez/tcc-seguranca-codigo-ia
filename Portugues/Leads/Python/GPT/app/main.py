from __future__ import annotations

from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles

from app.config import get_settings
from app.routers import api_auth, api_dashboard, api_leads, web

settings = get_settings()

app = FastAPI(
    title=settings.app_name,
    version="1.0.0",
    description="Sistema completo de gestão de leads com autenticação JWT, frontend e Firebase.",
)

app.mount("/static", StaticFiles(directory=str(settings.static_dir)), name="static")

app.include_router(api_auth.router)
app.include_router(api_dashboard.router)
app.include_router(api_leads.router)
app.include_router(web.router)


@app.get("/health", tags=["Infra"])
def healthcheck():
    return {
        "status": "ok",
        "app": settings.app_name,
        "collections": {
            "users": settings.users_collection,
            "leads": settings.leads_collection,
        },
    }
