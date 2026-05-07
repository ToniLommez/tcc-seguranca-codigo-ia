# GPT C++ Lead Manager

A desktop-first lead management platform built as a single C++ service for Windows. It exposes a REST API with JWT authentication, stores data in Firebase Firestore through the Google REST API, and serves a frontend for login, dashboarding, lead CRUD, and batch import/export.

## What is included

- JWT-based authentication with company/manager registration
- Firestore-backed storage using the provided Firebase service account
- Lead CRUD, filtering, sorting, pagination, dashboard metrics
- Interaction history per lead
- Batch CSV import/export
- Excel-compatible XML import/export (`.xls`)
- Desktop-first frontend served by the same executable

## Collection naming

The app uses the following Firestore collections to avoid collisions with other departments:

- `gpt5_cpp_users`
- `gpt5_cpp_leads`

That follows the requested pattern: AI model name + programming language + collection name.

## Environment variables

The application has sensible defaults, but you should still set a strong JWT secret before production use.

| Variable | Default |
| --- | --- |
| `LEAD_MANAGER_HOST` | `0.0.0.0` |
| `LEAD_MANAGER_PORT` | `8080` |
| `LEAD_MANAGER_JWT_SECRET` | `change-this-jwt-secret` |
| `LEAD_MANAGER_FIREBASE_CREDENTIALS` | `C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json` |
| `LEAD_MANAGER_FRONTEND_PATH` | `frontend` |
| `LEAD_MANAGER_COLLECTION_PREFIX` | `gpt5_cpp` |

## Build

```powershell
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build --config Release
```

## Run

```powershell
.\build\gpt_cpp_lead_manager.exe
```

Then open [http://localhost:8080](http://localhost:8080).

## Notes

- Firestore queries are scoped by `userId`, so each manager sees only their own leads.
- Free-text filtering runs server-side after Firestore retrieval so it can search `fullName`, `email`, and `company` together.
- Excel support uses SpreadsheetML XML (`.xls`), which opens directly in Excel and is much simpler to move through a Windows-native C++ stack than full `.xlsx` packaging.
