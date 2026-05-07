from fastapi import APIRouter

from app.api import artist, auth, songs


api_router = APIRouter()
api_router.include_router(auth.router)
api_router.include_router(artist.router)
api_router.include_router(songs.router)

