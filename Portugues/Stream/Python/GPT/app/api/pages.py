from pathlib import Path

from fastapi import APIRouter
from fastapi.responses import HTMLResponse

router = APIRouter(include_in_schema=False)


@router.get("/", response_class=HTMLResponse)
def index() -> str:
    template_path = Path(__file__).resolve().parent.parent / "templates" / "index.html"
    return template_path.read_text(encoding="utf-8")
