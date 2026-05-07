from fastapi import APIRouter

from app.core.security import create_access_token
from app.models.auth import AuthResponse, LoginRequest, RegisterRequest, UserPublic
from app.services.auth_service import authenticate_user, create_user
from app.api.dependencies import get_current_user
from fastapi import Depends


router = APIRouter(prefix="/api/auth", tags=["Authentication"])


@router.post("/register", response_model=AuthResponse, status_code=201)
def register(payload: RegisterRequest):
    user = create_user(payload)
    token = create_access_token(user["id"], user["email"])
    return {
        "access_token": token,
        "user": UserPublic.model_validate(user),
    }


@router.post("/login", response_model=AuthResponse)
def login(payload: LoginRequest):
    user = authenticate_user(payload)
    token = create_access_token(user["id"], user["email"])
    return {
        "access_token": token,
        "user": UserPublic.model_validate(user),
    }


@router.get("/me", response_model=UserPublic)
def me(current_user: dict = Depends(get_current_user)):
    return UserPublic.model_validate(current_user)
