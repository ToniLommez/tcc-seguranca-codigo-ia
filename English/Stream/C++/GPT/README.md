# Stream Music Service

REST music streaming service in C++ for Windows with:

- JWT authentication
- User types: `ARTIST` and `USER`
- Firebase storage for users only
- Local MP3 storage on the server
- Desktop-first frontend served by the backend
- Browser playback through a backend streaming endpoint

## Architecture

- Backend: C++20 with `cpp-httplib`
- Frontend: static HTML, CSS, and JavaScript
- User persistence: Firebase Firestore using the provided service account
- Music metadata: local JSON file at `storage/data/songs.json`
- Music binaries: local filesystem at `storage/uploads`

## Requirements

- Windows
- CMake 3.20+
- A C++ compiler such as MinGW `g++` or LLVM `clang++`
- Network access to Firebase / Google OAuth APIs

## Setup

1. Copy `appsettings.example.json` to `appsettings.json`.
2. Adjust `jwt_secret` before production use.
3. Confirm `firebase_service_account_path` points to your JSON file.

## Build

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

## Run

```powershell
.\build\stream_music_service.exe
```

Then open [http://localhost:8080](http://localhost:8080).

## Main Endpoints

- `POST /api/auth/register`
- `POST /api/auth/login`
- `GET /api/me`
- `POST /api/artist/songs`
- `GET /api/artist/songs`
- `GET /api/songs?title=&artist=&genre=`
- `GET /api/songs/{id}/stream`

## Business Rules Implemented

- Only `ARTIST` users can upload songs.
- Only `USER` users can list and stream songs.
- Artists cannot consume music through the streaming endpoints.
- Only `.mp3` files are accepted.
- Uploaded files stay on the local server.

