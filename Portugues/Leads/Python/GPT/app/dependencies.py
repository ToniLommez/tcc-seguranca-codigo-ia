from __future__ import annotations

from fastapi import Cookie, Depends, Header, HTTPException, status

from .firebase import get_firestore_client
from .security import decode_access_token
from .services.firestore_service import FirestoreService


def get_firestore_service() -> FirestoreService:
    return FirestoreService(get_firestore_client())


def get_current_user(
    authorization: str | None = Header(default=None),
    access_token: str | None = Cookie(default=None),
    service: FirestoreService = Depends(get_firestore_service),
) -> dict:
    token = None
    if authorization and authorization.lower().startswith("bearer "):
        token = authorization.split(" ", 1)[1].strip()
    elif access_token:
        token = access_token

    if not token:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Autenticação obrigatória.",
        )

    try:
        payload = decode_access_token(token)
        user_id = payload["sub"]
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token inválido ou expirado.",
        ) from exc

    user = service.get_user_by_id(user_id)
    if not user:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Usuário não encontrado.",
        )
    return user
