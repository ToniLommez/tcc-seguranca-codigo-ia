import sqlite3
from contextlib import contextmanager

from app.core.config import settings


def init_db() -> None:
    with sqlite3.connect(settings.sqlite_db_path) as connection:
        connection.execute(
            """
            CREATE TABLE IF NOT EXISTS songs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL,
                genre TEXT NOT NULL,
                description TEXT,
                file_path TEXT NOT NULL,
                original_filename TEXT NOT NULL,
                artist_id TEXT NOT NULL,
                artist_name TEXT NOT NULL,
                created_at TEXT NOT NULL
            )
            """
        )
        connection.commit()


@contextmanager
def get_connection():
    connection = sqlite3.connect(settings.sqlite_db_path)
    connection.row_factory = sqlite3.Row
    try:
        yield connection
    finally:
        connection.close()
