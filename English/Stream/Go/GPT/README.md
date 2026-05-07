# Streamline Music

Desktop-first music streaming system built in Go with:

- JWT authentication
- Firestore for user data
- Local server storage for MP3 files
- SQLite for song metadata
- A browser frontend served by the Go backend

## Features

- User registration with `name`, `email`, `password`, and `type` (`ARTIST` or `USER`)
- Login with email and password
- JWT token generation on authentication
- Role-based access control
- Artist-only MP3 upload flow
- User-only catalog listing, search, and playback
- Backend streaming endpoint with browser playback support

## Tech Stack

- Backend: Go, Gin
- Users: Google Firestore
- Songs metadata: SQLite
- Audio storage: local filesystem (`uploads/`)
- Frontend: HTML, CSS, JavaScript

## Project Structure

```text
cmd/server
internal/config
internal/database
internal/handlers
internal/middleware
internal/models
internal/repository
internal/router
internal/storage
web/static
uploads
data
```

## Run Locally

1. Copy `.env.example` values into your environment if you want to override defaults.
2. Install dependencies:

```powershell
go mod tidy
```

3. Start the server:

```powershell
go run ./cmd/server
```

4. Open:

```text
http://localhost:8080
```

## Main Endpoints

### Authentication

- `POST /api/auth/register`
- `POST /api/auth/login`
- `POST /api/auth/logout`
- `GET /api/auth/me`

### Artist

- `POST /api/artist/songs`
- `GET /api/artist/songs`

### User

- `GET /api/songs`
- `GET /api/songs/:id/stream`

## Important Notes

- Only `USER` accounts can list and stream music.
- `ARTIST` accounts can upload songs and list only their own uploads.
- The application accepts MP3 files only.
- The Firebase service account file is read from the path you provided by default.
- For production, change `JWT_SECRET` and consider moving static assets and logs behind your preferred Windows service or reverse proxy setup.
