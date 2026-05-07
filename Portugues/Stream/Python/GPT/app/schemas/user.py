from enum import Enum

from pydantic import BaseModel, EmailStr, Field, field_validator


class UserType(str, Enum):
    ARTISTA = "ARTISTA"
    USUARIO = "USUARIO"


class UserCreate(BaseModel):
    name: str = Field(min_length=2, max_length=120)
    email: EmailStr
    password: str = Field(min_length=6, max_length=128)
    user_type: UserType

    @field_validator("name")
    @classmethod
    def normalize_name(cls, value: str) -> str:
        return " ".join(value.split())


class UserInDB(BaseModel):
    id: str
    name: str
    email: EmailStr
    password_hash: str
    user_type: UserType
    created_at: str


class UserResponse(BaseModel):
    id: str
    name: str
    email: EmailStr
    user_type: UserType
    created_at: str
