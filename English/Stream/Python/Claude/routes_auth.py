import datetime
import hashlib
import os
import jwt
from flask import Blueprint, request, jsonify
from firebase_setup import db
from config import JWT_SECRET_KEY

auth_bp = Blueprint("auth", __name__)


def hash_password(password, salt=None):
    if salt is None:
        salt = os.urandom(32)
    key = hashlib.pbkdf2_hmac("sha256", password.encode(), salt, 100000)
    return salt + key


def verify_password(stored, password):
    salt = stored[:32]
    stored_key = stored[32:]
    key = hashlib.pbkdf2_hmac("sha256", password.encode(), salt, 100000)
    return key == stored_key


@auth_bp.route("/api/auth/register", methods=["POST"])
def register():
    data = request.get_json()
    if not data:
        return jsonify({"error": "Request body is required"}), 400

    name = data.get("name", "").strip()
    email = data.get("email", "").strip().lower()
    password = data.get("password", "")
    user_type = data.get("user_type", "").upper()

    if not all([name, email, password, user_type]):
        return jsonify({"error": "All fields are required: name, email, password, user_type"}), 400

    if user_type not in ("ARTIST", "USER"):
        return jsonify({"error": "user_type must be ARTIST or USER"}), 400

    if len(password) < 6:
        return jsonify({"error": "Password must be at least 6 characters"}), 400

    users_ref = db.collection("users")
    existing = users_ref.where("email", "==", email).limit(1).get()
    if len(existing) > 0:
        return jsonify({"error": "Email already registered"}), 409

    hashed = hash_password(password)

    doc_ref = users_ref.add({
        "name": name,
        "email": email,
        "password": hashed,
        "user_type": user_type,
        "created_at": datetime.datetime.now(datetime.timezone.utc).isoformat()
    })

    return jsonify({"message": "User registered successfully", "user_id": doc_ref[1].id}), 201


@auth_bp.route("/api/auth/login", methods=["POST"])
def login():
    data = request.get_json()
    if not data:
        return jsonify({"error": "Request body is required"}), 400

    email = data.get("email", "").strip().lower()
    password = data.get("password", "")

    if not email or not password:
        return jsonify({"error": "Email and password are required"}), 400

    users_ref = db.collection("users")
    results = users_ref.where("email", "==", email).limit(1).get()

    if len(results) == 0:
        return jsonify({"error": "Invalid email or password"}), 401

    user_doc = results[0]
    user_data = user_doc.to_dict()

    stored_password = user_data.get("password")
    if not stored_password or not verify_password(stored_password, password):
        return jsonify({"error": "Invalid email or password"}), 401

    token = jwt.encode({
        "user_id": user_doc.id,
        "email": user_data["email"],
        "name": user_data["name"],
        "user_type": user_data["user_type"],
        "exp": datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(hours=24)
    }, JWT_SECRET_KEY, algorithm="HS256")

    return jsonify({
        "token": token,
        "user": {
            "id": user_doc.id,
            "name": user_data["name"],
            "email": user_data["email"],
            "user_type": user_data["user_type"]
        }
    }), 200
