from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

from app.api.auth import router as auth_router
from app.api.dashboard import router as dashboard_router
from app.api.frontend import router as frontend_router
from app.api.leads import router as leads_router
from app.core.config import get_settings
from app.core.firebase import initialize_firebase


settings = get_settings()
app = FastAPI(title=settings.app_name)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.mount("/static", StaticFiles(directory="app/static"), name="static")

app.include_router(frontend_router)
app.include_router(auth_router)
app.include_router(dashboard_router)
app.include_router(leads_router)


@app.on_event("startup")
def startup() -> None:
    initialize_firebase()


@app.get("/health", tags=["System"])
def health() -> dict:
    return {"status": "ok", "app": settings.app_name}
