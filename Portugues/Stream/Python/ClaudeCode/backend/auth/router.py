from typing import Literal

from fastapi import APIRouter, Depends, HTTPException
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from pydantic import BaseModel, EmailStr

from auth.service import (
    create_access_token,
    create_user,
    decode_token,
    get_user_by_email,
    get_user_by_id,
    verify_password,
)

router = APIRouter(prefix="/api/auth", tags=["auth"])
_security = HTTPBearer()


class RegisterRequest(BaseModel):
    name: str
    email: EmailStr
    password: str
    user_type: Literal["ARTISTA", "USUARIO"]


class LoginRequest(BaseModel):
    email: EmailStr
    password: str


def _build_token_response(user: dict) -> dict:
    token = create_access_token(
        {
            "sub": user["id"],
            "email": user["email"],
            "user_type": user["user_type"],
            "name": user["name"],
        }
    )
    return {
        "access_token": token,
        "token_type": "bearer",
        "user": {
            "id": user["id"],
            "name": user["name"],
            "email": user["email"],
            "user_type": user["user_type"],
        },
    }


@router.post("/register")
async def register(req: RegisterRequest):
    if len(req.password) < 6:
        raise HTTPException(status_code=400, detail="A senha deve ter ao menos 6 caracteres.")
    if await get_user_by_email(req.email):
        raise HTTPException(status_code=400, detail="E-mail já cadastrado.")
    user = await create_user(req.name, req.email, req.password, req.user_type)
    return _build_token_response(user)


@router.post("/login")
async def login(req: LoginRequest):
    user = await get_user_by_email(req.email)
    if not user or not verify_password(req.password, user["password_hash"]):
        raise HTTPException(status_code=401, detail="E-mail ou senha inválidos.")
    return _build_token_response(user)


# ── Dependências de autenticação reutilizáveis ────────────────────────────────

async def get_current_user(
    credentials: HTTPAuthorizationCredentials = Depends(_security),
) -> dict:
    payload = decode_token(credentials.credentials)
    if not payload:
        raise HTTPException(status_code=401, detail="Token inválido ou expirado.")
    user = await get_user_by_id(payload["sub"])
    if not user:
        raise HTTPException(status_code=401, detail="Usuário não encontrado.")
    return user


async def require_artist(user: dict = Depends(get_current_user)) -> dict:
    if user["user_type"] != "ARTISTA":
        raise HTTPException(status_code=403, detail="Acesso restrito a artistas.")
    return user


async def require_user(user: dict = Depends(get_current_user)) -> dict:
    if user["user_type"] != "USUARIO":
        raise HTTPException(status_code=403, detail="Acesso restrito a usuários comuns.")
    return user
