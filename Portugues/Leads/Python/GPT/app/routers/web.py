from __future__ import annotations

from fastapi import APIRouter, Request
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates

from app.config import get_settings

templates = Jinja2Templates(directory=str(get_settings().templates_dir))
router = APIRouter(tags=["Frontend"])


def render_template(request: Request, name: str, context: dict | None = None) -> HTMLResponse:
    payload = {"request": request, **(context or {})}
    return templates.TemplateResponse(name, payload)


@router.get("/", response_class=HTMLResponse)
def auth_page(request: Request):
    return render_template(
        request,
        "login.html",
        {"page_name": "auth", "page_title": "Acesse sua operação de leads"},
    )


@router.get("/dashboard", response_class=HTMLResponse)
def dashboard_page(request: Request):
    return render_template(
        request,
        "dashboard.html",
        {"page_name": "dashboard", "page_title": "Dashboard"},
    )


@router.get("/leads", response_class=HTMLResponse)
def leads_page(request: Request):
    return render_template(
        request,
        "leads.html",
        {"page_name": "leads", "page_title": "Leads"},
    )


@router.get("/leads/new", response_class=HTMLResponse)
def lead_create_page(request: Request):
    return render_template(
        request,
        "lead_form.html",
        {"page_name": "lead-form", "page_title": "Novo lead", "lead_id": ""},
    )


@router.get("/leads/{lead_id}", response_class=HTMLResponse)
def lead_detail_page(request: Request, lead_id: str):
    return render_template(
        request,
        "lead_detail.html",
        {"page_name": "lead-detail", "page_title": "Detalhes do lead", "lead_id": lead_id},
    )


@router.get("/leads/{lead_id}/edit", response_class=HTMLResponse)
def lead_edit_page(request: Request, lead_id: str):
    return render_template(
        request,
        "lead_form.html",
        {"page_name": "lead-form", "page_title": "Editar lead", "lead_id": lead_id},
    )
