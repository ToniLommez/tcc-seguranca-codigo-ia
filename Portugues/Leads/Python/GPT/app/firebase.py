from __future__ import annotations

from pathlib import Path

import firebase_admin
from firebase_admin import credentials, firestore

from .config import get_settings


def get_firestore_client():
    settings = get_settings()
    credentials_path = Path(settings.firebase_credentials_path)

    if not credentials_path.exists():
        raise FileNotFoundError(
            f"Arquivo de credenciais do Firebase não encontrado em {credentials_path}"
        )

    if not firebase_admin._apps:
        firebase_admin.initialize_app(credentials.Certificate(str(credentials_path)))

    return firestore.client()
