from fastapi import APIRouter, Depends, HTTPException, status

from app.core.security import create_access_token, get_current_user
from app.schemas.auth import LoginRequest, TokenResponse
from app.schemas.user import UserCreate, UserResponse
from app.services.user_service import authenticate_user, create_user

router = APIRouter()


@router.post("/register", response_model=UserResponse, status_code=status.HTTP_201_CREATED)
def register(payload: UserCreate) -> UserResponse:
    return create_user(payload)


@router.post("/login", response_model=TokenResponse)
def login(payload: LoginRequest) -> TokenResponse:
    user = authenticate_user(payload.email, payload.password)
    if not user:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="E-mail ou senha invalidos.")

    token = create_access_token(
        {
            "sub": user.id,
            "email": user.email,
            "name": user.name,
            "user_type": user.user_type,
        }
    )
    return TokenResponse(access_token=token, user=user)


@router.get("/me", response_model=UserResponse)
def me(current_user: UserResponse = Depends(get_current_user)) -> UserResponse:
    return current_user
