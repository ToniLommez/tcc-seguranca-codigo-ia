from pathlib import Path

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from auth.router import router as auth_router
from database import init_db
from music.router import router as music_router

app = FastAPI(title="SoundStream API", version="1.0.0", docs_url="/api/docs")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

FRONTEND_DIR = Path(__file__).parent.parent / "frontend"


@app.on_event("startup")
async def startup():
    init_db()


app.include_router(auth_router)
app.include_router(music_router)

app.mount(
    "/static",
    StaticFiles(directory=str(FRONTEND_DIR / "static")),
    name="static",
)


@app.get("/", include_in_schema=False)
async def index():
    return FileResponse(str(FRONTEND_DIR / "index.html"))


@app.get("/register", include_in_schema=False)
async def register_page():
    return FileResponse(str(FRONTEND_DIR / "register.html"))


@app.get("/artist", include_in_schema=False)
async def artist_page():
    return FileResponse(str(FRONTEND_DIR / "artist.html"))


@app.get("/user", include_in_schema=False)
async def user_page():
    return FileResponse(str(FRONTEND_DIR / "user.html"))
