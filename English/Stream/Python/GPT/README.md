# Stream Music Platform

Complete music streaming system in Python with:

- FastAPI backend
- JWT authentication
- Firebase for user registration and storage
- Local MP3 storage on the server
- SQLite for song catalog metadata
- Desktop-first frontend in HTML, CSS, and JavaScript

## Architecture

- `Firebase (Firestore)`: stores users, encrypted passwords, and user type (`ARTIST` or `USER`)
- `SQLite`: stores song metadata locally
- `Filesystem`: stores `.mp3` files inside `backend/uploads`
- `FastAPI`: exposes REST endpoints, role-based access control, and audio streaming
- `Frontend`: single-page desktop-first interface served by the backend

## Business Rules Implemented

- Users register with `name`, `email`, `password`, and `type`
- Authentication returns a JWT token
- `ARTIST` users can upload songs but cannot use the listening module
- `USER` users can list, search, and listen to songs
- Only `.mp3` files are accepted for uploads
- Audio is served directly from local storage by the backend

## Project Structure

```text
backend/
  app/
    api/
    core/
    models/
    schemas/
    services/
  uploads/
  requirements.txt
frontend/
  assets/
  index.html
```

## Setup on Windows

1. Create and activate a virtual environment:

```powershell
cd C:\Users\tonil\Desktop\tcc\English\Stream\Python\GPT\backend
python -m venv .venv
.\.venv\Scripts\Activate.ps1
```

2. Install dependencies:

```powershell
pip install -r requirements.txt
```

3. Create the environment file:

```powershell
Copy-Item .env.example .env
```

4. Edit `backend/.env` and set a strong `JWT_SECRET_KEY`.

5. Start the API and frontend together:

```powershell
uvicorn app.main:app --reload --host 127.0.0.1 --port 8000
```

6. Open the system in the browser:

```text
http://127.0.0.1:8000
```

## Main Endpoints

### Authentication

- `POST /api/auth/register`
- `POST /api/auth/login`
- `GET /api/auth/me`

### Artist

- `GET /api/artist/me`
- `GET /api/artist/songs`
- `POST /api/artist/songs`

### User

- `GET /api/songs`
- `GET /api/songs/{song_id}/stream`

## Search Parameters

`GET /api/songs` supports:

- `title`
- `artist`
- `genre`

Example:

```text
/api/songs?title=love&artist=anna&genre=pop
```

## Notes

- The backend already points to your Firebase credentials path:
  `C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json`
- Song metadata stays local in SQLite so Firebase is used only for user data.
- Uploaded audio files are not exposed as public static files; playback always goes through the protected backend endpoint.
