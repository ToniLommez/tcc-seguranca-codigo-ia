import uuid
from pathlib import Path
from typing import Optional

from fastapi import HTTPException, UploadFile
from sqlalchemy.orm import Session

from config import ALLOWED_EXTENSIONS, MAX_FILE_SIZE, UPLOAD_DIR
from models import Song


def validate_music_file(file: UploadFile) -> None:
    ext = Path(file.filename).suffix.lower()
    if ext not in ALLOWED_EXTENSIONS:
        raise HTTPException(status_code=400, detail="Apenas arquivos MP3 são permitidos.")


async def save_music_file(file: UploadFile, artist_id: str) -> tuple[str, str]:
    artist_dir = UPLOAD_DIR / artist_id
    artist_dir.mkdir(exist_ok=True)

    ext = Path(file.filename).suffix.lower()
    unique_name = f"{uuid.uuid4()}{ext}"
    dest = artist_dir / unique_name

    content = await file.read()
    if len(content) > MAX_FILE_SIZE:
        raise HTTPException(
            status_code=413,
            detail=f"Arquivo muito grande. Máximo permitido: {MAX_FILE_SIZE // (1024*1024)} MB.",
        )

    dest.write_bytes(content)
    return str(dest), unique_name


def create_song(
    db: Session,
    title: str,
    genre: str,
    description: Optional[str],
    artist_id: str,
    artist_name: str,
    file_path: str,
    file_name: str,
) -> Song:
    song = Song(
        title=title,
        genre=genre,
        description=description,
        artist_id=artist_id,
        artist_name=artist_name,
        file_path=file_path,
        file_name=file_name,
    )
    db.add(song)
    db.commit()
    db.refresh(song)
    return song


def get_songs(db: Session, skip: int = 0, limit: int = 50) -> list[Song]:
    return db.query(Song).order_by(Song.created_at.desc()).offset(skip).limit(limit).all()


def search_songs(
    db: Session,
    title: Optional[str] = None,
    artist: Optional[str] = None,
    genre: Optional[str] = None,
) -> list[Song]:
    q = db.query(Song)
    if title:
        q = q.filter(Song.title.ilike(f"%{title}%"))
    if artist:
        q = q.filter(Song.artist_name.ilike(f"%{artist}%"))
    if genre:
        q = q.filter(Song.genre.ilike(f"%{genre}%"))
    return q.order_by(Song.created_at.desc()).all()


def get_song_by_id(db: Session, song_id: int) -> Song:
    song = db.query(Song).filter(Song.id == song_id).first()
    if not song:
        raise HTTPException(status_code=404, detail="Música não encontrada.")
    return song


def get_artist_songs(db: Session, artist_id: str) -> list[Song]:
    return (
        db.query(Song)
        .filter(Song.artist_id == artist_id)
        .order_by(Song.created_at.desc())
        .all()
    )
