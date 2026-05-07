from datetime import datetime, timezone

from fastapi import HTTPException, status

from app.core.config import get_settings
from app.core.firebase import get_firestore
from app.core.security import hash_password, verify_password
from app.models.auth import LoginRequest, RegisterRequest


def _serialize_user(document) -> dict:
    data = document.to_dict()
    return {
        "id": document.id,
        "name": data["name"],
        "email": data["email"],
        "created_at": datetime.fromisoformat(data["created_at"]),
        "hashed_password": data["hashed_password"],
    }


def get_user_by_email(email: str) -> dict | None:
    db = get_firestore()
    settings = get_settings()
    query = (
        db.collection(settings.users_collection)
        .where("email", "==", email.lower().strip())
        .limit(1)
        .stream()
    )
    for document in query:
        return _serialize_user(document)
    return None


def get_user_by_id(user_id: str) -> dict | None:
    db = get_firestore()
    settings = get_settings()
    document = db.collection(settings.users_collection).document(user_id).get()
    if not document.exists:
        return None
    return _serialize_user(document)


def create_user(payload: RegisterRequest) -> dict:
    existing = get_user_by_email(payload.email)
    if existing:
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail="Email is already registered.")

    db = get_firestore()
    settings = get_settings()
    now = datetime.now(timezone.utc).isoformat()
    document = db.collection(settings.users_collection).document()
    document.set(
        {
            "name": payload.name.strip(),
            "email": payload.email.lower().strip(),
            "hashed_password": hash_password(payload.password),
            "created_at": now,
        }
    )
    created = document.get()
    return _serialize_user(created)


def authenticate_user(payload: LoginRequest) -> dict:
    user = get_user_by_email(payload.email)
    if not user or not verify_password(payload.password, user["hashed_password"]):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Invalid email or password.",
        )
    return user
