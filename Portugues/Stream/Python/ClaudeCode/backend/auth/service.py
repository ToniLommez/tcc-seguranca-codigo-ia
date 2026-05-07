import uuid
from datetime import datetime, timedelta
from typing import Optional

import firebase_admin
from firebase_admin import credentials, firestore
from jose import JWTError, jwt
from passlib.context import CryptContext

from config import (
    ACCESS_TOKEN_EXPIRE_MINUTES,
    ALGORITHM,
    FIREBASE_CREDENTIALS_PATH,
    SECRET_KEY,
)

pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")

if not firebase_admin._apps:
    cred = credentials.Certificate(FIREBASE_CREDENTIALS_PATH)
    firebase_admin.initialize_app(cred)

_firestore = firestore.client()
USERS_COLLECTION = "users"


def hash_password(password: str) -> str:
    return pwd_context.hash(password)


def verify_password(plain: str, hashed: str) -> bool:
    return pwd_context.verify(plain, hashed)


def create_access_token(data: dict, expires_delta: Optional[timedelta] = None) -> str:
    payload = data.copy()
    expire = datetime.utcnow() + (expires_delta or timedelta(minutes=ACCESS_TOKEN_EXPIRE_MINUTES))
    payload["exp"] = expire
    return jwt.encode(payload, SECRET_KEY, algorithm=ALGORITHM)


def decode_token(token: str) -> Optional[dict]:
    try:
        return jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
    except JWTError:
        return None


async def get_user_by_email(email: str) -> Optional[dict]:
    docs = (
        _firestore.collection(USERS_COLLECTION)
        .where("email", "==", email)
        .limit(1)
        .get()
    )
    for doc in docs:
        data = doc.to_dict()
        data["id"] = doc.id
        return data
    return None


async def get_user_by_id(user_id: str) -> Optional[dict]:
    doc = _firestore.collection(USERS_COLLECTION).document(user_id).get()
    if doc.exists:
        data = doc.to_dict()
        data["id"] = doc.id
        return data
    return None


async def create_user(name: str, email: str, password: str, user_type: str) -> dict:
    user_id = str(uuid.uuid4())
    data = {
        "name": name,
        "email": email,
        "password_hash": hash_password(password),
        "user_type": user_type,
        "created_at": datetime.utcnow().isoformat(),
    }
    _firestore.collection(USERS_COLLECTION).document(user_id).set(data)
    return {"id": user_id, **data}
