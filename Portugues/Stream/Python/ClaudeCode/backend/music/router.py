import os
from typing import Optional

from fastapi import APIRouter, Depends, File, Form, HTTPException, Query, Request, UploadFile
from fastapi.responses import StreamingResponse
from sqlalchemy.orm import Session

from auth.router import get_current_user, require_artist, require_user
from auth.service import decode_token
from database import get_db
from music.service import (
    create_song,
    get_artist_songs,
    get_song_by_id,
    get_songs,
    save_music_file,
    search_songs,
    validate_music_file,
)

router = APIRouter(prefix="/api/music", tags=["music"])


# ── Artista ───────────────────────────────────────────────────────────────────

@router.post("/upload")
async def upload_music(
    title: str = Form(...),
    genre: str = Form(...),
    description: Optional[str] = Form(None),
    file: UploadFile = File(...),
    db: Session = Depends(get_db),
    artist: dict = Depends(require_artist),
):
    validate_music_file(file)
    file_path, file_name = await save_music_file(file, artist["id"])
    song = create_song(
        db=db,
        title=title,
        genre=genre,
        description=description,
        artist_id=artist["id"],
        artist_name=artist["name"],
        file_path=file_path,
        file_name=file_name,
    )
    return {"message": "Música cadastrada com sucesso!", "song": song.to_dict()}


@router.get("/my")
async def my_songs(
    db: Session = Depends(get_db),
    artist: dict = Depends(require_artist),
):
    songs = get_artist_songs(db, artist["id"])
    return [s.to_dict() for s in songs]


# ── Usuário comum ─────────────────────────────────────────────────────────────

@router.get("/list")
async def list_songs(
    skip: int = 0,
    limit: int = 50,
    db: Session = Depends(get_db),
    user: dict = Depends(require_user),
):
    return [s.to_dict() for s in get_songs(db, skip=skip, limit=limit)]


@router.get("/search")
async def search(
    title: Optional[str] = None,
    artist: Optional[str] = None,
    genre: Optional[str] = None,
    db: Session = Depends(get_db),
    user: dict = Depends(require_user),
):
    songs = search_songs(db, title=title, artist=artist, genre=genre)
    return [s.to_dict() for s in songs]


# ── Streaming ─────────────────────────────────────────────────────────────────

@router.get("/stream/{song_id}")
async def stream_song(
    song_id: int,
    request: Request,
    token: Optional[str] = Query(default=None),
    db: Session = Depends(get_db),
):
    # Aceita token via query param (necessário para o elemento <audio> do navegador)
    # ou via header Authorization: Bearer <token>
    auth_token = token
    if not auth_token:
        auth_header = request.headers.get("Authorization", "")
        if auth_header.startswith("Bearer "):
            auth_token = auth_header[7:]

    if not auth_token:
        raise HTTPException(status_code=401, detail="Token não fornecido.")

    payload = decode_token(auth_token)
    if not payload:
        raise HTTPException(status_code=401, detail="Token inválido ou expirado.")

    if payload.get("user_type") != "USUARIO":
        raise HTTPException(status_code=403, detail="Apenas usuários podem ouvir músicas.")

    song = get_song_by_id(db, song_id)
    file_path = song.file_path

    if not os.path.exists(file_path):
        raise HTTPException(status_code=404, detail="Arquivo de áudio não encontrado no servidor.")

    file_size = os.path.getsize(file_path)
    range_header = request.headers.get("range")

    def iter_file(start: int, end: int):
        with open(file_path, "rb") as f:
            f.seek(start)
            remaining = end - start + 1
            while remaining > 0:
                chunk = f.read(min(65536, remaining))
                if not chunk:
                    break
                remaining -= len(chunk)
                yield chunk

    if range_header:
        try:
            range_str = range_header.replace("bytes=", "")
            parts = range_str.split("-")
            start = int(parts[0]) if parts[0] else 0
            end = int(parts[1]) if len(parts) > 1 and parts[1] else file_size - 1
            end = min(end, file_size - 1)
        except (ValueError, IndexError):
            start, end = 0, file_size - 1

        return StreamingResponse(
            iter_file(start, end),
            status_code=206,
            headers={
                "Content-Range": f"bytes {start}-{end}/{file_size}",
                "Accept-Ranges": "bytes",
                "Content-Length": str(end - start + 1),
                "Content-Type": "audio/mpeg",
                "Cache-Control": "no-cache",
            },
        )

    return StreamingResponse(
        iter_file(0, file_size - 1),
        headers={
            "Content-Length": str(file_size),
            "Accept-Ranges": "bytes",
            "Content-Type": "audio/mpeg",
            "Cache-Control": "no-cache",
        },
    )


# ── Detalhes de uma música (qualquer usuário autenticado) ─────────────────────

@router.get("/{song_id}")
async def get_song(
    song_id: int,
    db: Session = Depends(get_db),
    user: dict = Depends(get_current_user),
):
    return get_song_by_id(db, song_id).to_dict()
