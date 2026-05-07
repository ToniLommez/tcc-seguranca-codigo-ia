import os
from pathlib import Path

from dotenv import load_dotenv


BASE_DIR = Path(__file__).resolve().parent.parent.parent
load_dotenv(BASE_DIR / ".env")


class Settings:
    def __init__(self) -> None:
        self.app_name = os.getenv("APP_NAME", "Streaming Platform")
        self.app_host = os.getenv("APP_HOST", "127.0.0.1")
        self.app_port = int(os.getenv("APP_PORT", "8000"))
        self.debug = os.getenv("DEBUG", "true").lower() == "true"
        self.secret_key = os.getenv("SECRET_KEY", "change-me-in-production")
        self.jwt_algorithm = os.getenv("JWT_ALGORITHM", "HS256")
        self.jwt_expire_minutes = int(os.getenv("JWT_EXPIRE_MINUTES", "120"))
        self.firebase_credentials_path = Path(
            os.getenv(
                "FIREBASE_CREDENTIALS_PATH",
                r"C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json",
            )
        )
        self.firebase_users_collection = os.getenv("FIREBASE_USERS_COLLECTION", "users")
        self.sqlite_db_path = self._resolve_path(os.getenv("SQLITE_DB_PATH", "storage.db"))
        self.upload_dir = self._resolve_path(os.getenv("UPLOAD_DIR", "uploads"))
        self.static_dir = BASE_DIR / "app" / "static"
        cors_origins = os.getenv("CORS_ORIGINS", "http://127.0.0.1:8000,http://localhost:8000")
        self.cors_origins = [origin.strip() for origin in cors_origins.split(",") if origin.strip()]

    @staticmethod
    def _resolve_path(value: str) -> Path:
        path = Path(value)
        return path if path.is_absolute() else BASE_DIR / path


settings = Settings()
