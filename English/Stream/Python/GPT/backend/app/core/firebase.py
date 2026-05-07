from pathlib import Path

import firebase_admin
from firebase_admin import credentials, firestore

from app.core.settings import settings


def get_firestore_client():
    credentials_path = Path(settings.firebase_credentials_path)
    if not credentials_path.exists():
        raise FileNotFoundError(
            f"Firebase credentials file not found: {credentials_path}"
        )

    if not firebase_admin._apps:
        credential = credentials.Certificate(str(credentials_path))
        firebase_admin.initialize_app(
            credential,
            {"projectId": settings.firebase_project_id},
        )

    return firestore.client()

