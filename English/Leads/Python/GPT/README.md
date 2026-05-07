# LeadFlow Command Center

Lead management platform for tech-focused lead generation teams, built with Python, FastAPI, Firestore, JWT authentication, and a desktop-first frontend.

## Features

- Company/manager registration and login with JWT authentication
- Full lead CRUD
- Lead filtering by status, source, capture date, and free text
- Pagination and sortable lead table
- Dashboard metrics for status totals, capture trends, and top sources
- Lead detail page with interaction history
- CSV and Excel (`.xlsx`) import/export
- Firebase Firestore persistence using the provided service account

## Stack

- Backend: FastAPI
- Frontend: Jinja2 templates, vanilla JavaScript, Chart.js
- Database: Firebase Firestore
- Auth: Custom JWT authentication

## Collection Naming

This project uses the shared Firestore naming rule `AI model name + programming language + collection name` with the prefix:

`codex_python_lead_manager`

That produces these collections:

- `codex_python_lead_manager_users`
- `codex_python_lead_manager_leads`
- `codex_python_lead_manager_interactions`

## Setup

1. Create a virtual environment:

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
```

2. Install dependencies:

```powershell
pip install -r requirements.txt
```

3. Copy `.env.example` to `.env` and adjust values if needed.

4. Start the app:

```powershell
uvicorn app.main:app --reload
```

5. Open:

`http://127.0.0.1:8000`

## Import Format

Accepted import file types:

- `.csv`
- `.xlsx`
- `.xls`

Supported columns:

- `full_name`
- `email`
- `phone`
- `company`
- `position`
- `source`
- `status`
- `capture_date`

Friendly variants such as `full name`, `job title`, and `captured at` are also normalized automatically.

## API Highlights

- `POST /api/auth/register`
- `POST /api/auth/login`
- `GET /api/auth/me`
- `GET /api/dashboard/summary`
- `GET /api/leads`
- `POST /api/leads`
- `GET /api/leads/{lead_id}`
- `PUT /api/leads/{lead_id}`
- `DELETE /api/leads/{lead_id}`
- `POST /api/leads/import`
- `GET /api/leads/export?format=csv|xlsx`
- `GET /api/leads/{lead_id}/interactions`
- `POST /api/leads/{lead_id}/interactions`

## Notes

- The app stores users in Firestore and issues its own JWTs.
- For production use, set a strong `JWT_SECRET_KEY`.
- The filtering logic is implemented server-side in Python after loading the authenticated user's leads, which keeps the API flexible without requiring Firestore composite indexes for every filter combination.
