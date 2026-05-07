from pydantic import BaseModel, EmailStr
from typing import Optional
from enum import Enum


class LeadStatus(str, Enum):
    NEW = "new"
    CONTACTED = "contacted"
    QUALIFIED = "qualified"
    LOST = "lost"


class LeadSource(str, Enum):
    REFERRAL = "referral"
    WEBSITE = "website"
    EVENT = "event"
    SOCIAL_MEDIA = "social_media"
    COLD_CALL = "cold_call"
    OTHER = "other"


class UserCreate(BaseModel):
    name: str
    email: EmailStr
    password: str


class UserLogin(BaseModel):
    email: EmailStr
    password: str


class UserResponse(BaseModel):
    id: str
    name: str
    email: str


class LeadCreate(BaseModel):
    full_name: str
    email: EmailStr
    phone: Optional[str] = None
    company: Optional[str] = None
    position: Optional[str] = None
    source: LeadSource = LeadSource.OTHER
    status: LeadStatus = LeadStatus.NEW


class LeadUpdate(BaseModel):
    full_name: Optional[str] = None
    email: Optional[EmailStr] = None
    phone: Optional[str] = None
    company: Optional[str] = None
    position: Optional[str] = None
    source: Optional[LeadSource] = None
    status: Optional[LeadStatus] = None


class InteractionCreate(BaseModel):
    note: str
    interaction_type: str = "note"
