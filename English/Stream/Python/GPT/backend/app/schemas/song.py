from datetime import datetime

from pydantic import BaseModel, ConfigDict


class SongResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    title: str
    genre: str
    description: str | None
    artist_id: str
    artist_name: str
    original_file_name: str
    created_at: datetime
    stream_url: str | None = None

