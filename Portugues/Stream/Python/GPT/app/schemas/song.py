from pydantic import BaseModel, Field


class SongCreate(BaseModel):
    title: str = Field(min_length=1, max_length=120)
    genre: str = Field(min_length=1, max_length=80)
    description: str | None = Field(default=None, max_length=500)


class SongResponse(BaseModel):
    id: int
    title: str
    genre: str
    description: str | None
    artist_id: str
    artist_name: str
    file_path: str
    original_filename: str
    created_at: str
