import os
import jwt
from flask import Blueprint, request, jsonify, send_file, Response
from auth import user_required
from firebase_setup import db
from config import UPLOAD_FOLDER, JWT_SECRET_KEY

songs_bp = Blueprint("songs", __name__)


@songs_bp.route("/api/songs", methods=["GET"])
@user_required
def list_songs():
    search = request.args.get("search", "").strip().lower()
    genre = request.args.get("genre", "").strip().lower()

    songs_ref = db.collection("songs")
    songs = []

    for doc in songs_ref.stream():
        song = doc.to_dict()
        song["id"] = doc.id

        if search:
            title_match = search in song.get("title", "").lower()
            artist_match = search in song.get("artist_name", "").lower()
            if not title_match and not artist_match:
                continue

        if genre and genre != song.get("genre", "").lower():
            continue

        songs.append({
            "id": song["id"],
            "title": song["title"],
            "genre": song["genre"],
            "description": song.get("description", ""),
            "artist_name": song["artist_name"],
            "created_at": song["created_at"]
        })

    return jsonify(songs), 200


@songs_bp.route("/api/songs/genres", methods=["GET"])
@user_required
def list_genres():
    songs_ref = db.collection("songs")
    genres = set()
    for doc in songs_ref.stream():
        genre = doc.to_dict().get("genre", "")
        if genre:
            genres.add(genre)
    return jsonify(sorted(genres)), 200


@songs_bp.route("/api/stream/<song_id>", methods=["GET"])
def stream_song(song_id):
    token = request.args.get("token")
    if not token:
        auth_header = request.headers.get("Authorization")
        if auth_header and auth_header.startswith("Bearer "):
            token = auth_header.split(" ", 1)[1]
    if not token:
        return jsonify({"error": "Token is missing"}), 401
    try:
        payload = jwt.decode(token, JWT_SECRET_KEY, algorithms=["HS256"])
        if payload.get("user_type") != "USER":
            return jsonify({"error": "User access required"}), 403
    except jwt.InvalidTokenError:
        return jsonify({"error": "Invalid token"}), 401
    doc = db.collection("songs").document(song_id).get()
    if not doc.exists:
        return jsonify({"error": "Song not found"}), 404

    song = doc.to_dict()
    file_path = os.path.join(UPLOAD_FOLDER, song["file_path"])

    if not os.path.exists(file_path):
        return jsonify({"error": "Audio file not found on server"}), 404

    file_size = os.path.getsize(file_path)
    range_header = request.headers.get("Range")

    if range_header:
        byte_start = 0
        byte_end = file_size - 1

        range_match = range_header.replace("bytes=", "").split("-")
        byte_start = int(range_match[0])
        if range_match[1]:
            byte_end = int(range_match[1])

        content_length = byte_end - byte_start + 1

        def generate():
            with open(file_path, "rb") as f:
                f.seek(byte_start)
                remaining = content_length
                while remaining > 0:
                    chunk_size = min(8192, remaining)
                    data = f.read(chunk_size)
                    if not data:
                        break
                    remaining -= len(data)
                    yield data

        response = Response(
            generate(),
            status=206,
            mimetype="audio/mpeg",
            direct_passthrough=True
        )
        response.headers["Content-Range"] = f"bytes {byte_start}-{byte_end}/{file_size}"
        response.headers["Accept-Ranges"] = "bytes"
        response.headers["Content-Length"] = content_length
        return response

    return send_file(
        file_path,
        mimetype="audio/mpeg",
        as_attachment=False
    )
