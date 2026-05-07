import os
from pathlib import Path

SECRET_KEY = os.getenv("SECRET_KEY", "claude-python-leads-secret-2024-change-in-production")
ALGORITHM = "HS256"
ACCESS_TOKEN_EXPIRE_MINUTES = 60 * 24  # 24 horas

FIREBASE_CREDENTIALS_PATH = os.getenv(
    "FIREBASE_CREDENTIALS_PATH",
    str(
        Path(__file__).parent.parent.parent.parent.parent
        / "lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json"
    ),
)

# Nomenclatura: modelo IA + linguagem + coleção
COLLECTION_PREFIX = "claude-python"
USERS_COLLECTION = f"{COLLECTION_PREFIX}-users"
LEADS_COLLECTION = f"{COLLECTION_PREFIX}-leads"
INTERACTIONS_COLLECTION = f"{COLLECTION_PREFIX}-interactions"

DEFAULT_PAGE_SIZE = 20
MAX_PAGE_SIZE = 100
