import shutil
from pathlib import Path
from uuid import uuid4

from fastapi import APIRouter, Depends, File, Form, HTTPException, UploadFile, status

from app.api.dependencies import DbSession, get_current_user, require_role
from app.core.settings import settings
from app.schemas.auth import UserResponse, UserType
from app.schemas.song import SongResponse
from app.services.song_service import create_song, list_artist_songs


router = APIRouter(prefix="/artist", tags=["Artist"])


def _song_to_response(song, include_stream_url: bool = False) -> SongResponse:
    stream_url = f"{settings.api_prefix}/songs/{song.id}/stream" if include_stream_url else None
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
            "stream_url": stream_url,
        }
    )


@router.get("/me", response_model=UserResponse)
def get_artist_profile(current_user=Depends(require_role(UserType.ARTIST))):
    return UserResponse(
        id=current_user["id"],
        name=current_user["name"],
        email=current_user["email"],
        type=current_user["type"],
        created_at=current_user["created_at"],
    )


@router.get("/songs", response_model=list[SongResponse])
def get_my_songs(db: DbSession, current_user=Depends(require_role(UserType.ARTIST))):
    songs = list_artist_songs(db, current_user["id"])
    return [_song_to_response(song) for song in songs]


@router.post("/songs", response_model=SongResponse, status_code=status.HTTP_201_CREATED)
def upload_song(
    db: DbSession,
    current_user=Depends(require_role(UserType.ARTIST)),
    title: str = Form(...),
    genre: str = Form(...),
    description: str | None = Form(default=None),
    file: UploadFile = File(...),
):
    extension = Path(file.filename or "").suffix.lower()
    if extension != ".mp3":
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Only MP3 files are allowed",
        )

    safe_name = f"{uuid4()}.mp3"
    target_path = settings.uploads_dir / safe_name
    settings.uploads_dir.mkdir(parents=True, exist_ok=True)

    with target_path.open("wb") as buffer:
        shutil.copyfileobj(file.file, buffer)

    song = create_song(
        db,
        title=title,
        genre=genre,
        description=description,
        artist_id=current_user["id"],
        artist_name=current_user["name"],
        file_path=str(target_path),
        original_file_name=file.filename or safe_name,
    )
    return _song_to_response(song)

