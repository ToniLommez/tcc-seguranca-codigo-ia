from datetime import datetime, timezone

from fastapi import HTTPException, status

from app.core.config import settings
from app.core.firebase import get_firestore_client
from app.core.security import hash_password, verify_password
from app.schemas.user import UserCreate, UserInDB, UserResponse


def _users_collection():
    return get_firestore_client().collection(settings.firebase_users_collection)


def _to_response(user: UserInDB) -> UserResponse:
    return UserResponse(
        id=user.id,
        name=user.name,
        email=user.email,
        user_type=user.user_type,
        created_at=user.created_at,
    )


def get_user_by_email(email: str) -> UserInDB | None:
    normalized_email = email.strip().lower()
    documents = (
        _users_collection()
        .where("email", "==", normalized_email)
        .limit(1)
        .stream()
    )
    for document in documents:
        payload = document.to_dict()
        return UserInDB(id=document.id, **payload)
    return None


def get_user_by_id(user_id: str) -> UserResponse | None:
    document = _users_collection().document(user_id).get()
    if not document.exists:
        return None
    payload = document.to_dict()
    user = UserInDB(id=document.id, **payload)
    return _to_response(user)


def create_user(payload: UserCreate) -> UserResponse:
    if get_user_by_email(payload.email):
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Ja existe um usuario cadastrado com este e-mail.",
        )

    created_at = datetime.now(timezone.utc).isoformat()
    document_ref = _users_collection().document()
    user_data = {
        "name": payload.name.strip(),
        "email": payload.email.strip().lower(),
        "password_hash": hash_password(payload.password),
        "user_type": payload.user_type.value,
        "created_at": created_at,
    }
    document_ref.set(user_data)
    return UserResponse(id=document_ref.id, **{k: v for k, v in user_data.items() if k != "password_hash"})


def authenticate_user(email: str, password: str) -> UserResponse | None:
    user = get_user_by_email(email)
    if not user or not verify_password(password, user.password_hash):
        return None
    return _to_response(user)
