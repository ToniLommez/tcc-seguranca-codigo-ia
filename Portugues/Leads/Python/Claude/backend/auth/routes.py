from datetime import datetime

from fastapi import APIRouter, Depends, HTTPException, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from google.cloud.firestore_v1.base_query import FieldFilter
from pydantic import BaseModel, EmailStr

from auth.jwt_handler import (
    create_access_token,
    decode_token,
    hash_password,
    verify_password,
)
from config import USERS_COLLECTION
from firebase_config import get_db

router = APIRouter(prefix="/api/auth", tags=["auth"])
security = HTTPBearer()


class RegisterRequest(BaseModel):
    name: str
    email: EmailStr
    password: str


class LoginRequest(BaseModel):
    email: EmailStr
    password: str


def get_current_user(credentials: HTTPAuthorizationCredentials = Depends(security)):
    token = credentials.credentials
    payload = decode_token(token)
    if not payload:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token inválido ou expirado",
        )
    return payload


@router.post("/register", status_code=201)
def register(body: RegisterRequest):
    db = get_db()
    users_ref = db.collection(USERS_COLLECTION)

    existing = users_ref.where(filter=FieldFilter("email", "==", body.email)).limit(1).get()
    if existing:
        raise HTTPException(status_code=400, detail="E-mail já cadastrado")

    user_data = {
        "name": body.name,
        "email": body.email,
        "password_hash": hash_password(body.password),
        "created_at": datetime.utcnow().isoformat(),
    }
    doc_ref = users_ref.add(user_data)
    user_id = doc_ref[1].id

    token = create_access_token({"sub": user_id, "email": body.email, "name": body.name})
    return {"access_token": token, "token_type": "bearer", "user": {"id": user_id, "name": body.name, "email": body.email}}


@router.post("/login")
def login(body: LoginRequest):
    db = get_db()
    users_ref = db.collection(USERS_COLLECTION)

    docs = users_ref.where(filter=FieldFilter("email", "==", body.email)).limit(1).get()
    if not docs:
        raise HTTPException(status_code=401, detail="Credenciais inválidas")

    doc = docs[0]
    user = doc.to_dict()

    if not verify_password(body.password, user["password_hash"]):
        raise HTTPException(status_code=401, detail="Credenciais inválidas")

    token = create_access_token({"sub": doc.id, "email": user["email"], "name": user["name"]})
    return {"access_token": token, "token_type": "bearer", "user": {"id": doc.id, "name": user["name"], "email": user["email"]}}


@router.get("/me")
def me(current_user: dict = Depends(get_current_user)):
    return {"id": current_user["sub"], "name": current_user["name"], "email": current_user["email"]}
