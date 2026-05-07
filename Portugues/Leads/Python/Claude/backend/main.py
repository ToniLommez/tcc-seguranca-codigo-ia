from pathlib import Path

import aiofiles
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, HTMLResponse
from fastapi.staticfiles import StaticFiles

from auth.routes import router as auth_router
from leads.routes import router as leads_router

app = FastAPI(title="Lead Manager API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(auth_router)
app.include_router(leads_router)

FRONTEND_DIR = Path(__file__).parent.parent / "frontend"

app.mount("/static", StaticFiles(directory=str(FRONTEND_DIR / "static")), name="static")


def _html(name: str):
    path = FRONTEND_DIR / name
    return FileResponse(str(path), media_type="text/html")


@app.get("/", include_in_schema=False)
def index():
    return _html("index.html")


@app.get("/dashboard", include_in_schema=False)
def dashboard():
    return _html("dashboard.html")


@app.get("/leads", include_in_schema=False)
def leads_page():
    return _html("leads.html")


@app.get("/lead-detail", include_in_schema=False)
def lead_detail():
    return _html("lead-detail.html")


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)
