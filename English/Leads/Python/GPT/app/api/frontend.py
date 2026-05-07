from fastapi import APIRouter, Request
from fastapi.responses import HTMLResponse, RedirectResponse
from fastapi.templating import Jinja2Templates


templates = Jinja2Templates(directory="app/templates")
router = APIRouter(tags=["Frontend"])


def render(request: Request, template_name: str, page_title: str, page_data: dict | None = None):
    return templates.TemplateResponse(
        request=request,
        name=template_name,
        context={
            "page_title": page_title,
            "page_data": page_data or {},
        },
    )


@router.get("/", include_in_schema=False)
def home():
    return RedirectResponse("/dashboard")


@router.get("/login", response_class=HTMLResponse, include_in_schema=False)
def login_page(request: Request):
    return render(request, "login.html", "Login")


@router.get("/register", response_class=HTMLResponse, include_in_schema=False)
def register_page(request: Request):
    return render(request, "register.html", "Register")


@router.get("/dashboard", response_class=HTMLResponse, include_in_schema=False)
def dashboard_page(request: Request):
    return render(request, "dashboard.html", "Dashboard")


@router.get("/leads", response_class=HTMLResponse, include_in_schema=False)
def leads_page(request: Request):
    return render(request, "leads.html", "Lead Pipeline")


@router.get("/leads/new", response_class=HTMLResponse, include_in_schema=False)
def new_lead_page(request: Request):
    return render(request, "lead_form.html", "New Lead", {"mode": "create"})


@router.get("/leads/{lead_id}", response_class=HTMLResponse, include_in_schema=False)
def lead_detail_page(request: Request, lead_id: str):
    return render(request, "lead_detail.html", "Lead Details", {"lead_id": lead_id})


@router.get("/leads/{lead_id}/edit", response_class=HTMLResponse, include_in_schema=False)
def edit_lead_page(request: Request, lead_id: str):
    return render(request, "lead_form.html", "Edit Lead", {"mode": "edit", "lead_id": lead_id})
