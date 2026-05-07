from sqlalchemy import Select, select
from sqlalchemy.orm import Session

from app.models.song import Song


def create_song(
    db: Session,
    *,
    title: str,
    genre: str,
    description: str | None,
    artist_id: str,
    artist_name: str,
    file_path: str,
    original_file_name: str,
) -> Song:
    song = Song(
        title=title.strip(),
        genre=genre.strip(),
        description=description.strip() if description else None,
        artist_id=artist_id,
        artist_name=artist_name,
        file_path=file_path,
        original_file_name=original_file_name,
    )
    db.add(song)
    db.commit()
    db.refresh(song)
    return song


def list_artist_songs(db: Session, artist_id: str) -> list[Song]:
    statement = (
        select(Song)
        .where(Song.artist_id == artist_id)
        .order_by(Song.created_at.desc())
    )
    return list(db.scalars(statement))


def search_songs(
    db: Session,
    *,
    title: str | None = None,
    artist: str | None = None,
    genre: str | None = None,
) -> list[Song]:
    statement: Select[tuple[Song]] = select(Song)

    if title:
        statement = statement.where(Song.title.ilike(f"%{title.strip()}%"))
    if artist:
        statement = statement.where(Song.artist_name.ilike(f"%{artist.strip()}%"))
    if genre:
        statement = statement.where(Song.genre.ilike(f"%{genre.strip()}%"))

    statement = statement.order_by(Song.created_at.desc())
    return list(db.scalars(statement))


def get_song_by_id(db: Session, song_id: int) -> Song | None:
    statement = select(Song).where(Song.id == song_id)
    return db.scalar(statement)

