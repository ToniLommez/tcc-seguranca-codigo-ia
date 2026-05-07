from fastapi import APIRouter, Depends, File, Form, UploadFile, status

from app.core.security import require_user_type
from app.schemas.song import SongResponse
from app.schemas.user import UserResponse, UserType
from app.services.song_service import create_song, list_songs_by_artist

router = APIRouter()


@router.post("/songs", response_model=SongResponse, status_code=status.HTTP_201_CREATED)
def upload_song(
    title: str = Form(...),
    genre: str = Form(...),
    description: str | None = Form(None),
    file: UploadFile = File(...),
    current_user: UserResponse = Depends(require_user_type(UserType.ARTISTA)),
) -> SongResponse:
    return create_song(
        title=title,
        genre=genre,
        description=description,
        upload=file,
        artist=current_user,
    )


@router.get("/songs/mine", response_model=list[SongResponse])
def my_songs(
    current_user: UserResponse = Depends(require_user_type(UserType.ARTISTA)),
) -> list[SongResponse]:
    return list_songs_by_artist(current_user.id)
