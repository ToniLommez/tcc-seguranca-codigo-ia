from functools import lru_cache
from pathlib import Path

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    app_name: str = "LeadFlow Command Center"
    jwt_secret_key: str = "change-me-before-production"
    jwt_algorithm: str = "HS256"
    access_token_expire_minutes: int = 720
    firebase_credential_path: Path = Path(
        r"C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json"
    )
    firebase_collection_prefix: str = "codex_python_lead_manager"

    model_config = SettingsConfigDict(env_file=".env", env_file_encoding="utf-8", case_sensitive=False)

    @property
    def users_collection(self) -> str:
        return f"{self.firebase_collection_prefix}_users"

    @property
    def leads_collection(self) -> str:
        return f"{self.firebase_collection_prefix}_leads"

    @property
    def interactions_collection(self) -> str:
        return f"{self.firebase_collection_prefix}_interactions"


@lru_cache
def get_settings() -> Settings:
    return Settings()
