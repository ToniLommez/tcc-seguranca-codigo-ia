from __future__ import annotations

import os
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path


@dataclass(slots=True)
class Settings:
    app_name: str = "Lead Manager GPT"
    api_prefix: str = "/api"
    secret_key: str = os.getenv("SECRET_KEY", "troque-esta-chave-em-producao")
    jwt_algorithm: str = "HS256"
    access_token_expire_minutes: int = int(os.getenv("ACCESS_TOKEN_EXPIRE_MINUTES", "480"))
    firebase_credentials_path: str = os.getenv(
        "FIREBASE_CREDENTIALS_PATH",
        r"C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json",
    )
    ai_model_name: str = os.getenv("AI_MODEL_NAME", "gpt")
    language_name: str = os.getenv("LANGUAGE_NAME", "python")

    @property
    def base_dir(self) -> Path:
        return Path(__file__).resolve().parent

    @property
    def templates_dir(self) -> Path:
        return self.base_dir / "templates"

    @property
    def static_dir(self) -> Path:
        return self.base_dir / "static"

    def collection_name(self, suffix: str) -> str:
        normalized_model = self.ai_model_name.lower().replace(" ", "_").replace("-", "_")
        normalized_language = self.language_name.lower().replace(" ", "_").replace("-", "_")
        return f"{normalized_model}_{normalized_language}_{suffix}"

    @property
    def users_collection(self) -> str:
        return self.collection_name("users")

    @property
    def leads_collection(self) -> str:
        return self.collection_name("leads")


@lru_cache(maxsize=1)
def get_settings() -> Settings:
    return Settings()
