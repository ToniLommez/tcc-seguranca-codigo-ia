from sqlalchemy import Column, Integer, String, DateTime, Text
from datetime import datetime
from database import Base


class Song(Base):
    __tablename__ = "songs"

    id = Column(Integer, primary_key=True, index=True)
    title = Column(String(255), nullable=False, index=True)
    genre = Column(String(100), nullable=False, index=True)
    description = Column(Text, nullable=True)
    artist_id = Column(String(128), nullable=False, index=True)
    artist_name = Column(String(255), nullable=False, index=True)
    file_path = Column(String(500), nullable=False)
    file_name = Column(String(255), nullable=False)
    created_at = Column(DateTime, default=datetime.utcnow)

    def to_dict(self):
        return {
            "id": self.id,
            "title": self.title,
            "genre": self.genre,
            "description": self.description,
            "artist_id": self.artist_id,
            "artist_name": self.artist_name,
            "created_at": self.created_at.isoformat(),
        }
