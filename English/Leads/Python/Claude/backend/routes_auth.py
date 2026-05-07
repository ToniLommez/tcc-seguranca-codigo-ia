from fastapi import APIRouter, HTTPException, status, Depends
from models import UserCreate, UserLogin, UserResponse
from auth import hash_password, verify_password, create_access_token, get_current_user
from config import db, COLLECTION_PREFIX

router = APIRouter(prefix="/api/auth", tags=["Authentication"])


@router.post("/register", response_model=dict)
def register(user: UserCreate):
    users_ref = db.collection(f"{COLLECTION_PREFIX}_users")
    existing = users_ref.where("email", "==", user.email).limit(1).get()
    if list(existing):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Email already registered")

    user_data = {
        "name": user.name,
        "email": user.email,
        "password_hash": hash_password(user.password),
    }
    doc_ref = users_ref.add(user_data)
    user_id = doc_ref[1].id

    token = create_access_token({"sub": user_id, "email": user.email})
    return {
        "access_token": token,
        "token_type": "bearer",
        "user": {"id": user_id, "name": user.name, "email": user.email},
    }


@router.post("/login", response_model=dict)
def login(user: UserLogin):
    users_ref = db.collection(f"{COLLECTION_PREFIX}_users")
    results = list(users_ref.where("email", "==", user.email).limit(1).get())
    if not results:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid credentials")

    user_doc = results[0]
    user_data = user_doc.to_dict()

    if not verify_password(user.password, user_data["password_hash"]):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid credentials")

    token = create_access_token({"sub": user_doc.id, "email": user_data["email"]})
    return {
        "access_token": token,
        "token_type": "bearer",
        "user": {"id": user_doc.id, "name": user_data["name"], "email": user_data["email"]},
    }


@router.get("/me", response_model=UserResponse)
def get_me(current_user: dict = Depends(get_current_user)):
    return UserResponse(id=current_user["id"], name=current_user["name"], email=current_user["email"])
