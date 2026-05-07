from __future__ import annotations

from datetime import date
from typing import Literal

from pydantic import BaseModel, EmailStr, Field, field_validator


LeadStatus = Literal["novo", "em_contato", "qualificado", "perdido"]


class UserCreateRequest(BaseModel):
    name: str = Field(min_length=2, max_length=120)
    email: EmailStr
    password: str = Field(min_length=6, max_length=128)


class UserLoginRequest(BaseModel):
    email: EmailStr
    password: str = Field(min_length=6, max_length=128)


class UserResponse(BaseModel):
    id: str
    name: str
    email: EmailStr
    created_at: str


class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    user: UserResponse


class InteractionCreateRequest(BaseModel):
    interaction_type: str = Field(default="observacao", min_length=2, max_length=50)
    note: str = Field(min_length=2, max_length=3000)


class InteractionResponse(BaseModel):
    id: str
    interaction_type: str
    note: str
    created_at: str
    created_by_name: str
    created_by_id: str


class LeadBase(BaseModel):
    full_name: str = Field(min_length=2, max_length=150)
    email: EmailStr
    phone: str = Field(min_length=6, max_length=40)
    company: str = Field(min_length=2, max_length=150)
    role: str = Field(min_length=2, max_length=120)
    source: str = Field(min_length=2, max_length=80)
    status: LeadStatus = "novo"
    captured_at: date

    @field_validator("full_name", "phone", "company", "role", "source")
    @classmethod
    def strip_text(cls, value: str) -> str:
        return value.strip()


class LeadCreateRequest(LeadBase):
    pass


class LeadUpdateRequest(LeadBase):
    pass


class LeadResponse(LeadBase):
    id: str
    owner_id: str
    created_at: str
    updated_at: str
    interactions: list[InteractionResponse] = []


class LeadListResponse(BaseModel):
    items: list[LeadResponse]
    total: int
    page: int
    page_size: int
    total_pages: int


class ImportResultResponse(BaseModel):
    imported: int
    skipped: int
    errors: list[str]


class DailyCaptureResponse(BaseModel):
    date: str
    total: int


class SourceSummaryResponse(BaseModel):
    source: str
    total: int


class DashboardSummaryResponse(BaseModel):
    total_leads: int
    status_summary: dict[str, int]
    captures_last_14_days: list[DailyCaptureResponse]
    top_sources: list[SourceSummaryResponse]
