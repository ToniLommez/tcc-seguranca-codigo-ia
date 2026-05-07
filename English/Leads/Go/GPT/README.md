# GPT Go Lead Manager

A full-stack lead management system built in Go with:

- JWT-based authentication
- Firebase Firestore persistence
- RESTful lead CRUD APIs
- Dashboard analytics
- CSV/XLSX import and export
- Desktop-first frontend served by the same Go backend

## Stack

- Backend: Go, Chi router, JWT, bcrypt
- Database: Firebase Firestore
- Frontend: HTML, CSS, vanilla JavaScript
- File support: CSV and XLSX via `excelize`

## Firestore Collection Strategy

Because the Firebase project is shared, the user collection is namespaced as:

- `gpt5_go_lead_manager_users`

Each user document stores a manager/company account and contains a `leads` subcollection. Each lead document contains an `interactions` subcollection for timeline history.

## Requirements

- Go `1.24+`
- Access to the Firebase service account JSON

## Configuration

Copy `.env.example` values into your environment if needed:

```powershell
$env:PORT="8080"
$env:JWT_SECRET="replace-with-a-long-random-secret"
$env:FIREBASE_PROJECT_ID="lead-manager-54595"
$env:FIREBASE_CREDENTIALS_PATH="C:/Users/tonil/Desktop/tcc/lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json"
$env:FIRESTORE_USERS_COLLECTION="gpt5_go_lead_manager_users"
```

## Run

```powershell
go mod tidy
go run ./cmd/server
```

Open [http://localhost:8080](http://localhost:8080).

## REST API Overview

### Public

- `POST /api/auth/register`
- `POST /api/auth/login`

### Protected

- `GET /api/auth/me`
- `GET /api/dashboard`
- `GET /api/leads`
- `POST /api/leads`
- `GET /api/leads/{id}`
- `PUT /api/leads/{id}`
- `DELETE /api/leads/{id}`
- `POST /api/leads/{id}/interactions`
- `POST /api/leads/import`
- `GET /api/leads/export?format=csv|xlsx`

## Filtering and Pagination

`GET /api/leads` supports:

- `page`
- `pageSize`
- `sortBy`
- `sortDir`
- `status`
- `source`
- `dateFrom`
- `dateTo`
- `q`

## Supported Lead Fields

- Full name
- Email
- Phone
- Company
- Position
- Source
- Status
- Capture date

## Notes

- Authentication is handled by the Go API using signed JWTs, not by a client-side Firebase SDK.
- Passwords are hashed with bcrypt before storage.
- The frontend is optimized for desktop workflows while remaining responsive on smaller screens.
