from fastapi import APIRouter, Depends, Query, Request

from app.core.security import require_user_type
from app.schemas.song import SongResponse
from app.schemas.user import UserResponse, UserType
from app.services.song_service import get_song_by_id, search_songs
from app.services.stream_service import stream_song_file

router = APIRouter()


@router.get("", response_model=list[SongResponse])
def list_songs(
    name: str | None = Query(None),
    artist: str | None = Query(None),
    genre: str | None = Query(None),
    _: UserResponse = Depends(require_user_type(UserType.USUARIO)),
) -> list[SongResponse]:
    return search_songs(name=name, artist=artist, genre=genre)


@router.get("/{song_id}", response_model=SongResponse)
def song_details(
    song_id: int,
    _: UserResponse = Depends(require_user_type(UserType.USUARIO)),
) -> SongResponse:
    return get_song_by_id(song_id)


@router.get("/{song_id}/stream")
def stream_song(
    song_id: int,
    request: Request,
    _: UserResponse = Depends(require_user_type(UserType.USUARIO)),
):
    song = get_song_by_id(song_id)
    return stream_song_file(song.file_path, request)
