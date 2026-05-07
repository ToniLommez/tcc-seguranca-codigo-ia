import functools
import jwt
from flask import request, jsonify
from config import JWT_SECRET_KEY


def token_required(f):
    @functools.wraps(f)
    def decorated(*args, **kwargs):
        token = None
        auth_header = request.headers.get("Authorization")
        if auth_header and auth_header.startswith("Bearer "):
            token = auth_header.split(" ", 1)[1]
        if not token:
            return jsonify({"error": "Token is missing"}), 401
        try:
            payload = jwt.decode(token, JWT_SECRET_KEY, algorithms=["HS256"])
            request.user = payload
        except jwt.ExpiredSignatureError:
            return jsonify({"error": "Token has expired"}), 401
        except jwt.InvalidTokenError:
            return jsonify({"error": "Invalid token"}), 401
        return f(*args, **kwargs)
    return decorated


def artist_required(f):
    @functools.wraps(f)
    @token_required
    def decorated(*args, **kwargs):
        if request.user.get("user_type") != "ARTIST":
            return jsonify({"error": "Artist access required"}), 403
        return f(*args, **kwargs)
    return decorated


def user_required(f):
    @functools.wraps(f)
    @token_required
    def decorated(*args, **kwargs):
        if request.user.get("user_type") != "USER":
            return jsonify({"error": "User access required"}), 403
        return f(*args, **kwargs)
    return decorated
