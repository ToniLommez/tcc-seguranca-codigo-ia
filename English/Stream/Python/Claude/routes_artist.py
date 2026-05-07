import os
import uuid
import datetime
from flask import Blueprint, request, jsonify
from auth import artist_required
from firebase_setup import db
from config import UPLOAD_FOLDER, ALLOWED_EXTENSIONS

artist_bp = Blueprint("artist", __name__)


def allowed_file(filename):
    return "." in filename and filename.rsplit(".", 1)[1].lower() in ALLOWED_EXTENSIONS


@artist_bp.route("/api/artist/songs", methods=["POST"])
@artist_required
def upload_song():
    if "file" not in request.files:
        return jsonify({"error": "Music file is required"}), 400

    file = request.files["file"]
    if file.filename == "":
        return jsonify({"error": "No file selected"}), 400

    if not allowed_file(file.filename):
        return jsonify({"error": "Only MP3 files are allowed"}), 400

    title = request.form.get("title", "").strip()
    genre = request.form.get("genre", "").strip()
    description = request.form.get("description", "").strip()

    if not title or not genre:
        return jsonify({"error": "Title and genre are required"}), 400

    unique_name = f"{uuid.uuid4().hex}.mp3"
    file_path = os.path.join(UPLOAD_FOLDER, unique_name)
    file.save(file_path)

    song_data = {
        "title": title,
        "genre": genre,
        "description": description,
        "file_path": unique_name,
        "artist_id": request.user["user_id"],
        "artist_name": request.user["name"],
        "created_at": datetime.datetime.now(datetime.timezone.utc).isoformat()
    }

    doc_ref = db.collection("songs").add(song_data)

    return jsonify({
        "message": "Song uploaded successfully",
        "song_id": doc_ref[1].id,
        "title": title
    }), 201


@artist_bp.route("/api/artist/songs", methods=["GET"])
@artist_required
def list_my_songs():
    songs_ref = db.collection("songs").where("artist_id", "==", request.user["user_id"])
    songs = []
    for doc in songs_ref.stream():
        song = doc.to_dict()
        song["id"] = doc.id
        song.pop("file_path", None)
        songs.append(song)
    return jsonify(songs), 200


@artist_bp.route("/api/artist/songs/<song_id>", methods=["DELETE"])
@artist_required
def delete_song(song_id):
    doc_ref = db.collection("songs").document(song_id)
    doc = doc_ref.get()

    if not doc.exists:
        return jsonify({"error": "Song not found"}), 404

    song_data = doc.to_dict()
    if song_data["artist_id"] != request.user["user_id"]:
        return jsonify({"error": "You can only delete your own songs"}), 403

    file_path = os.path.join(UPLOAD_FOLDER, song_data["file_path"])
    if os.path.exists(file_path):
        os.remove(file_path)

    doc_ref.delete()
    return jsonify({"message": "Song deleted successfully"}), 200
