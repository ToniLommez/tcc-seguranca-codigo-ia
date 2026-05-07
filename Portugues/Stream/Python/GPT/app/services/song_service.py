import shutil
import uuid
from datetime import datetime, timezone
from pathlib import Path

from fastapi import HTTPException, UploadFile, status

from app.core.config import settings
from app.db.sqlite import get_connection
from app.schemas.song import SongResponse
from app.schemas.user import UserResponse


def create_song(
    *,
    title: str,
    genre: str,
    description: str | None,
    upload: UploadFile,
    artist: UserResponse,
) -> SongResponse:
    if artist.user_type != "ARTISTA":
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Apenas artistas podem cadastrar musicas.")

    extension = Path(upload.filename or "").suffix.lower()
    if extension != ".mp3" or upload.content_type not in {"audio/mpeg", "audio/mp3", "application/octet-stream"}:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Apenas arquivos MP3 sao permitidos.")

    safe_name = f"{uuid.uuid4().hex}{extension}"
    stored_path = settings.upload_dir / safe_name
    settings.upload_dir.mkdir(parents=True, exist_ok=True)

    with stored_path.open("wb") as buffer:
        shutil.copyfileobj(upload.file, buffer)

    created_at = datetime.now(timezone.utc).isoformat()
    with get_connection() as connection:
        cursor = connection.execute(
            """
            INSERT INTO songs (title, genre, description, file_path, original_filename, artist_id, artist_name, created_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                title.strip(),
                genre.strip(),
                description.strip() if description else None,
                str(stored_path.resolve()),
                upload.filename or safe_name,
                artist.id,
                artist.name,
                created_at,
            ),
        )
        connection.commit()
        song_id = cursor.lastrowid

    return get_song_by_id(song_id)


def _row_to_song(row) -> SongResponse:
    return SongResponse(
        id=row["id"],
        title=row["title"],
        genre=row["genre"],
        description=row["description"],
        file_path=row["file_path"],
        original_filename=row["original_filename"],
        artist_id=row["artist_id"],
        artist_name=row["artist_name"],
        created_at=row["created_at"],
    )


def get_song_by_id(song_id: int) -> SongResponse:
    with get_connection() as connection:
        row = connection.execute("SELECT * FROM songs WHERE id = ?", (song_id,)).fetchone()

    if not row:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Musica nao encontrada.")
    return _row_to_song(row)


def list_songs_by_artist(artist_id: str) -> list[SongResponse]:
    with get_connection() as connection:
        rows = connection.execute(
            "SELECT * FROM songs WHERE artist_id = ? ORDER BY created_at DESC",
            (artist_id,),
        ).fetchall()
    return [_row_to_song(row) for row in rows]


def search_songs(
    *,
    name: str | None = None,
    artist: str | None = None,
    genre: str | None = None,
) -> list[SongResponse]:
    clauses = []
    values: list[str] = []

    if name:
        clauses.append("LOWER(title) LIKE ?")
        values.append(f"%{name.lower().strip()}%")
    if artist:
        clauses.append("LOWER(artist_name) LIKE ?")
        values.append(f"%{artist.lower().strip()}%")
    if genre:
        clauses.append("LOWER(genre) LIKE ?")
        values.append(f"%{genre.lower().strip()}%")

    where_clause = f"WHERE {' AND '.join(clauses)}" if clauses else ""
    query = f"SELECT * FROM songs {where_clause} ORDER BY created_at DESC"

    with get_connection() as connection:
        rows = connection.execute(query, values).fetchall()
    return [_row_to_song(row) for row in rows]
