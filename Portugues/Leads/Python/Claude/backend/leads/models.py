from datetime import datetime
from enum import Enum
from typing import Optional

from pydantic import BaseModel, EmailStr


class FonteLead(str, Enum):
    indicacao = "indicacao"
    site = "site"
    evento = "evento"
    redes_sociais = "redes_sociais"
    email_marketing = "email_marketing"
    outros = "outros"


class StatusLead(str, Enum):
    novo = "novo"
    em_contato = "em_contato"
    qualificado = "qualificado"
    perdido = "perdido"


class LeadCreate(BaseModel):
    nome_completo: str
    email: EmailStr
    telefone: Optional[str] = None
    empresa: Optional[str] = None
    cargo: Optional[str] = None
    fonte: FonteLead = FonteLead.outros
    status: StatusLead = StatusLead.novo
    data_captura: Optional[str] = None
    notas: Optional[str] = None


class LeadUpdate(BaseModel):
    nome_completo: Optional[str] = None
    email: Optional[EmailStr] = None
    telefone: Optional[str] = None
    empresa: Optional[str] = None
    cargo: Optional[str] = None
    fonte: Optional[FonteLead] = None
    status: Optional[StatusLead] = None
    data_captura: Optional[str] = None
    notas: Optional[str] = None


class TipoInteracao(str, Enum):
    ligacao = "ligacao"
    email = "email"
    reuniao = "reuniao"
    mensagem = "mensagem"
    outro = "outro"


class InteracaoCreate(BaseModel):
    tipo: TipoInteracao
    descricao: str
    data: Optional[str] = None
