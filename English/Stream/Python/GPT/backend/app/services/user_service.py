from datetime import datetime, timezone
from uuid import uuid4

from google.cloud.firestore_v1.base_query import FieldFilter

from app.core.firebase import get_firestore_client
from app.core.security import hash_password, verify_password


USERS_COLLECTION = "users"


def _serialize_user(document) -> dict | None:
    if not document or not document.exists:
        return None

    data = document.to_dict()
    data["id"] = document.id
    return data


def get_user_by_email(email: str) -> dict | None:
    client = get_firestore_client()
    users_ref = client.collection(USERS_COLLECTION)
    query = users_ref.where(filter=FieldFilter("email", "==", email.lower())).limit(1)
    matches = list(query.stream())
    if not matches:
        return None
    return _serialize_user(matches[0])


def get_user_by_id(user_id: str) -> dict | None:
    client = get_firestore_client()
    document = client.collection(USERS_COLLECTION).document(user_id).get()
    return _serialize_user(document)


def create_user(name: str, email: str, password: str, user_type: str) -> dict:
    existing_user = get_user_by_email(email)
    if existing_user:
        raise ValueError("Email already registered")

    client = get_firestore_client()
    user_id = str(uuid4())
    created_at = datetime.now(timezone.utc)
    user_data = {
        "name": name.strip(),
        "email": email.lower(),
        "password_hash": hash_password(password),
        "type": user_type,
        "created_at": created_at,
    }
    client.collection(USERS_COLLECTION).document(user_id).set(user_data)

    user_data["id"] = user_id
    return user_data


def authenticate_user(email: str, password: str) -> dict | None:
    user = get_user_by_email(email)
    if not user:
        return None

    if not verify_password(password, user["password_hash"]):
        return None

    return user

