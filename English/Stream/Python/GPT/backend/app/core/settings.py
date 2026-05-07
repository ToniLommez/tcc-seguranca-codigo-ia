from pathlib import Path

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    app_name: str = "Stream Music Platform"
    api_prefix: str = "/api"
    jwt_secret_key: str = "change-this-secret-key"
    jwt_algorithm: str = "HS256"
    jwt_expire_minutes: int = 120
    firebase_project_id: str = "lead-manager-54595"
    firebase_credentials_path: Path = Path(
        r"C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json"
    )

    model_config = SettingsConfigDict(
        env_file=Path(__file__).resolve().parents[2] / ".env",
        env_file_encoding="utf-8",
        extra="ignore",
    )

    @property
    def backend_dir(self) -> Path:
        return Path(__file__).resolve().parents[2]

    @property
    def project_dir(self) -> Path:
        return Path(__file__).resolve().parents[3]

    @property
    def frontend_dir(self) -> Path:
        return self.project_dir / "frontend"

    @property
    def frontend_assets_dir(self) -> Path:
        return self.frontend_dir / "assets"

    @property
    def uploads_dir(self) -> Path:
        return self.backend_dir / "uploads"

    @property
    def sqlite_path(self) -> Path:
        return self.backend_dir / "stream_catalog.db"


settings = Settings()

