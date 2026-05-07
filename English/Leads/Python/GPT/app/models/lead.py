from datetime import date, datetime
from enum import Enum
from typing import Optional

from pydantic import BaseModel, EmailStr, Field


class LeadStatus(str, Enum):
    new = "new"
    contacted = "contacted"
    qualified = "qualified"
    lost = "lost"


class LeadBase(BaseModel):
    full_name: str = Field(min_length=2, max_length=160)
    email: EmailStr
    phone: Optional[str] = Field(default=None, max_length=40)
    company: Optional[str] = Field(default=None, max_length=160)
    position: Optional[str] = Field(default=None, max_length=160)
    source: str = Field(min_length=2, max_length=80)
    status: LeadStatus = LeadStatus.new
    capture_date: date


class LeadCreate(LeadBase):
    pass


class LeadUpdate(BaseModel):
    full_name: Optional[str] = Field(default=None, min_length=2, max_length=160)
    email: Optional[EmailStr] = None
    phone: Optional[str] = Field(default=None, max_length=40)
    company: Optional[str] = Field(default=None, max_length=160)
    position: Optional[str] = Field(default=None, max_length=160)
    source: Optional[str] = Field(default=None, min_length=2, max_length=80)
    status: Optional[LeadStatus] = None
    capture_date: Optional[date] = None


class LeadRead(LeadBase):
    id: str
    created_at: datetime
    updated_at: datetime
    interaction_count: int = 0


class LeadListResponse(BaseModel):
    items: list[LeadRead]
    total: int
    page: int
    page_size: int
    total_pages: int


class InteractionCreate(BaseModel):
    interaction_type: str = Field(min_length=2, max_length=60)
    summary: str = Field(min_length=2, max_length=160)
    notes: Optional[str] = Field(default=None, max_length=4000)
    happened_at: datetime


class InteractionRead(InteractionCreate):
    id: str
    created_at: datetime


class LeadDetailResponse(BaseModel):
    lead: LeadRead
    interactions: list[InteractionRead]
