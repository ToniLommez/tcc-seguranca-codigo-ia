from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException, Response, status

from app.dependencies import get_current_user, get_firestore_service
from app.schemas import TokenResponse, UserCreateRequest, UserLoginRequest, UserResponse
from app.security import create_access_token, verify_password
from app.services.firestore_service import FirestoreService

router = APIRouter(prefix="/api/auth", tags=["Autenticação"])


def _build_token_response(user: dict) -> dict:
    token = create_access_token({"sub": user["id"], "email": user["email"], "name": user["name"]})
    return {
        "access_token": token,
        "token_type": "bearer",
        "user": {
            "id": user["id"],
            "name": user["name"],
            "email": user["email"],
            "created_at": user["created_at"],
        },
    }


@router.post("/register", response_model=TokenResponse, status_code=status.HTTP_201_CREATED)
def register_user(
    payload: UserCreateRequest,
    response: Response,
    service: FirestoreService = Depends(get_firestore_service),
):
    try:
        user = service.create_user(payload.name, payload.email, payload.password)
    except ValueError as exc:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail=str(exc)) from exc

    auth_payload = _build_token_response(user)
    response.set_cookie(
        key="access_token",
        value=auth_payload["access_token"],
        httponly=True,
        secure=False,
        samesite="lax",
        max_age=60 * 60 * 8,
        path="/",
    )
    return auth_payload


@router.post("/login", response_model=TokenResponse)
def login_user(
    payload: UserLoginRequest,
    response: Response,
    service: FirestoreService = Depends(get_firestore_service),
):
    user = service.get_user_by_email(payload.email)
    if not user or not verify_password(payload.password, user["password_hash"]):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="E-mail ou senha inválidos.",
        )

    auth_payload = _build_token_response(user)
    response.set_cookie(
        key="access_token",
        value=auth_payload["access_token"],
        httponly=True,
        secure=False,
        samesite="lax",
        max_age=60 * 60 * 8,
        path="/",
    )
    return auth_payload


@router.post("/logout", status_code=status.HTTP_204_NO_CONTENT)
def logout_user():
    response = Response(status_code=status.HTTP_204_NO_CONTENT)
    response.delete_cookie("access_token", path="/")
    return response


@router.get("/me", response_model=UserResponse)
def get_me(user: dict = Depends(get_current_user)):
    return {
        "id": user["id"],
        "name": user["name"],
        "email": user["email"],
        "created_at": user["created_at"],
    }
