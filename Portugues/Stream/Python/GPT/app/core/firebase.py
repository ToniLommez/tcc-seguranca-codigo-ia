import firebase_admin
from firebase_admin import credentials, firestore

from app.core.config import settings


def initialize_firebase():
    if firebase_admin._apps:
        return firebase_admin.get_app()

    if not settings.firebase_credentials_path.exists():
        raise FileNotFoundError(
            f"Arquivo de credenciais do Firebase nao encontrado: {settings.firebase_credentials_path}"
        )

    cred = credentials.Certificate(str(settings.firebase_credentials_path))
    return firebase_admin.initialize_app(cred)


def get_firestore_client():
    initialize_firebase()
    return firestore.client()
