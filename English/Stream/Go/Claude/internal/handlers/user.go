package handlers

import (
	"net/http"

	"music-stream/internal/firebase"
	"music-stream/internal/models"
)

type UserHandler struct {
	fb *firebase.Client
}

func NewUserHandler(fb *firebase.Client) *UserHandler {
	return &UserHandler{fb: fb}
}

func (h *UserHandler) ListSongs(w http.ResponseWriter, r *http.Request) {
	songs, err := h.fb.ListSongs(r.Context())
	if err != nil {
		respondError(w, http.StatusInternalServerError, "failed to fetch songs")
		return
	}

	if songs == nil {
		songs = []models.Song{}
	}

	respondJSON(w, http.StatusOK, songs)
}

func (h *UserHandler) SearchSongs(w http.ResponseWriter, r *http.Request) {
	query := r.URL.Query().Get("q")
	field := r.URL.Query().Get("field")

	if query == "" {
		respondError(w, http.StatusBadRequest, "query parameter 'q' is required")
		return
	}

	songs, err := h.fb.SearchSongs(r.Context(), query, field)
	if err != nil {
		respondError(w, http.StatusInternalServerError, "failed to search songs")
		return
	}

	if songs == nil {
		songs = []models.Song{}
	}

	respondJSON(w, http.StatusOK, songs)
}
