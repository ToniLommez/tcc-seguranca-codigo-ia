import os
import firebase_admin
from firebase_admin import credentials, firestore

FIREBASE_CREDENTIALS_PATH = os.getenv(
    "FIREBASE_CREDENTIALS_PATH",
    r"C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json"
)

JWT_SECRET_KEY = os.getenv("JWT_SECRET_KEY", "lead-manager-secret-key-change-in-production")
JWT_ALGORITHM = "HS256"
JWT_EXPIRATION_HOURS = 24

COLLECTION_PREFIX = "ClaudeOpus_Python"

cred = credentials.Certificate(FIREBASE_CREDENTIALS_PATH)
firebase_admin.initialize_app(cred)
db = firestore.client()
