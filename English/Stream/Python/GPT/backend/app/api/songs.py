from pathlib import Path
import re

from fastapi import APIRouter, Depends, HTTPException, Request, status
from fastapi.responses import StreamingResponse

from app.api.dependencies import DbSession, require_role
from app.core.settings import settings
from app.schemas.auth import UserType
from app.schemas.song import SongResponse
from app.services.song_service import get_song_by_id, search_songs


router = APIRouter(prefix="/songs", tags=["Songs"])
CHUNK_SIZE = 1024 * 1024


def _song_to_response(song) -> SongResponse:
    return SongResponse.model_validate(
        {
            "id": song.id,
            "title": song.title,
            "genre": song.genre,
            "description": song.description,
            "artist_id": song.artist_id,
            "artist_name": song.artist_name,
            "original_file_name": song.original_file_name,
            "created_at": song.created_at,
            "stream_url": f"{settings.api_prefix}/songs/{song.id}/stream",
        }
    )


def _parse_range_header(range_header: str | None, file_size: int) -> tuple[int, int, bool]:
    if not range_header:
        return 0, file_size - 1, False

    match = re.match(r"bytes=(\d*)-(\d*)", range_header)
    if not match:
        raise HTTPException(
            status_code=status.HTTP_416_REQUESTED_RANGE_NOT_SATISFIABLE,
            detail="Invalid range header",
        )

    start_raw, end_raw = match.groups()
    if start_raw == "" and end_raw == "":
        raise HTTPException(
            status_code=status.HTTP_416_REQUESTED_RANGE_NOT_SATISFIABLE,
            detail="Invalid range header",
        )

    if start_raw == "":
        suffix_length = int(end_raw)
        start = max(file_size - suffix_length, 0)
        end = file_size - 1
    else:
        start = int(start_raw)
        end = int(end_raw) if end_raw else file_size - 1

    if start < 0 or end >= file_size or start > end:
        raise HTTPException(
            status_code=status.HTTP_416_REQUESTED_RANGE_NOT_SATISFIABLE,
            detail="Requested range not satisfiable",
        )

    return start, end, True


def _file_iterator(file_path: Path, start: int, end: int):
    with file_path.open("rb") as stream:
        stream.seek(start)
        remaining = end - start + 1
        while remaining > 0:
            chunk = stream.read(min(CHUNK_SIZE, remaining))
            if not chunk:
                break
            remaining -= len(chunk)
            yield chunk


@router.get("", response_model=list[SongResponse])
def list_songs(
    db: DbSession,
    title: str | None = None,
    artist: str | None = None,
    genre: str | None = None,
    current_user=Depends(require_role(UserType.USER)),
):
    del current_user
    songs = search_songs(db, title=title, artist=artist, genre=genre)
    return [_song_to_response(song) for song in songs]


@router.get("/{song_id}/stream")
def stream_song(
    song_id: int,
    request: Request,
    db: DbSession,
    current_user=Depends(require_role(UserType.USER)),
):
    del current_user
    song = get_song_by_id(db, song_id)
    if not song:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Song not found")

    file_path = Path(song.file_path)
    if not file_path.exists():
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Audio file not found on server",
        )

    file_size = file_path.stat().st_size
    start, end, is_partial = _parse_range_header(request.headers.get("range"), file_size)
    headers = {
        "Accept-Ranges": "bytes",
        "Content-Length": str(end - start + 1),
        "Content-Type": "audio/mpeg",
        "Content-Disposition": f'inline; filename="{song.original_file_name}"',
    }

    status_code = status.HTTP_200_OK
    if is_partial:
        headers["Content-Range"] = f"bytes {start}-{end}/{file_size}"
        status_code = status.HTTP_206_PARTIAL_CONTENT

    return StreamingResponse(
        _file_iterator(file_path, start, end),
        status_code=status_code,
        headers=headers,
        media_type="audio/mpeg",
    )

