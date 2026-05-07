from dataclasses import dataclass


@dataclass
class Song:
    id: int
    title: str
    genre: str
    description: str | None
    file_path: str
    original_filename: str
    artist_id: str
    artist_name: str
    created_at: str
