from pathlib import Path

import firebase_admin
from firebase_admin import credentials, firestore

from app.core.config import get_settings


def initialize_firebase() -> None:
    settings = get_settings()
    if firebase_admin._apps:
        return

    credential_path = Path(settings.firebase_credential_path)
    if not credential_path.exists():
        raise FileNotFoundError(f"Firebase credential file not found: {credential_path}")

    credential = credentials.Certificate(str(credential_path))
    firebase_admin.initialize_app(credential)


def get_firestore():
    initialize_firebase()
    return firestore.client()
