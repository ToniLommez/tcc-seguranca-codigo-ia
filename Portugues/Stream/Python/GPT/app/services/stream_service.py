from pathlib import Path

from fastapi import HTTPException, Request, status
from fastapi.responses import StreamingResponse


CHUNK_SIZE = 1024 * 256


def _read_file_range(file_path: Path, start: int, end: int):
    with file_path.open("rb") as file:
        file.seek(start)
        remaining = end - start + 1
        while remaining > 0:
            chunk = file.read(min(CHUNK_SIZE, remaining))
            if not chunk:
                break
            remaining -= len(chunk)
            yield chunk


def stream_song_file(file_path: str, request: Request):
    path = Path(file_path)
    if not path.exists():
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Arquivo de audio nao encontrado.")

    file_size = path.stat().st_size
    headers = {
        "Accept-Ranges": "bytes",
        "Content-Type": "audio/mpeg",
        "Content-Disposition": f'inline; filename="{path.name}"',
    }

    range_header = request.headers.get("range")
    if not range_header:
        headers["Content-Length"] = str(file_size)
        return StreamingResponse(
            _read_file_range(path, 0, file_size - 1),
            status_code=status.HTTP_200_OK,
            headers=headers,
            media_type="audio/mpeg",
        )

    units, _, range_value = range_header.partition("=")
    if units != "bytes" or "-" not in range_value:
        raise HTTPException(status_code=status.HTTP_416_REQUESTED_RANGE_NOT_SATISFIABLE, detail="Range invalido.")

    start_text, end_text = range_value.split("-", maxsplit=1)
    start = int(start_text) if start_text else 0
    end = int(end_text) if end_text else file_size - 1

    if start >= file_size or end >= file_size or start > end:
        raise HTTPException(status_code=status.HTTP_416_REQUESTED_RANGE_NOT_SATISFIABLE, detail="Range invalido.")

    content_length = end - start + 1
    headers["Content-Length"] = str(content_length)
    headers["Content-Range"] = f"bytes {start}-{end}/{file_size}"

    return StreamingResponse(
        _read_file_range(path, start, end),
        status_code=status.HTTP_206_PARTIAL_CONTENT,
        headers=headers,
        media_type="audio/mpeg",
    )
