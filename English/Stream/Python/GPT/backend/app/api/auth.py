from fastapi import APIRouter, Depends, HTTPException, status

from app.api.dependencies import get_current_user
from app.core.security import create_access_token
from app.schemas.auth import LoginRequest, RegisterRequest, TokenResponse, UserResponse
from app.services.user_service import authenticate_user, create_user


router = APIRouter(prefix="/auth", tags=["Authentication"])


def _build_token_response(user: dict) -> TokenResponse:
    access_token = create_access_token(user["id"], user["email"], user["type"])
    response_user = UserResponse(
        id=user["id"],
        name=user["name"],
        email=user["email"],
        type=user["type"],
        created_at=user["created_at"],
    )
    return TokenResponse(access_token=access_token, user=response_user)


@router.post("/register", response_model=TokenResponse, status_code=status.HTTP_201_CREATED)
def register(payload: RegisterRequest):
    try:
        user = create_user(
            name=payload.name,
            email=payload.email,
            password=payload.password,
            user_type=payload.type.value,
        )
    except ValueError as exc:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=str(exc),
        ) from exc

    return _build_token_response(user)


@router.post("/login", response_model=TokenResponse)
def login(payload: LoginRequest):
    user = authenticate_user(payload.email, payload.password)
    if not user:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid email or password",
        )

    return _build_token_response(user)


@router.get("/me", response_model=UserResponse)
def me(current_user=Depends(get_current_user)):
    return UserResponse(
        id=current_user["id"],
        name=current_user["name"],
        email=current_user["email"],
        type=current_user["type"],
        created_at=current_user["created_at"],
    )
