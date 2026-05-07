import os
from flask import Flask, send_from_directory
from flask_cors import CORS
from config import UPLOAD_FOLDER, MAX_CONTENT_LENGTH
from routes_auth import auth_bp
from routes_artist import artist_bp
from routes_songs import songs_bp

app = Flask(__name__, static_folder="static", template_folder="templates")
app.config["MAX_CONTENT_LENGTH"] = MAX_CONTENT_LENGTH
CORS(app)

os.makedirs(UPLOAD_FOLDER, exist_ok=True)

app.register_blueprint(auth_bp)
app.register_blueprint(artist_bp)
app.register_blueprint(songs_bp)


@app.route("/")
def index():
    return send_from_directory("templates", "index.html")


if __name__ == "__main__":
    app.run(debug=True, port=5000)
