package handlers

import (
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/gorilla/mux"

	"music-stream/internal/auth"
	"music-stream/internal/config"
	"music-stream/internal/firebase"
	"music-stream/internal/models"
)

type ArtistHandler struct {
	fb  *firebase.Client
	cfg *config.Config
}

func NewArtistHandler(fb *firebase.Client, cfg *config.Config) *ArtistHandler {
	return &ArtistHandler{fb: fb, cfg: cfg}
}

func (h *ArtistHandler) UploadSong(w http.ResponseWriter, r *http.Request) {
	claims := auth.GetClaims(r.Context())

	r.Body = http.MaxBytesReader(w, r.Body, 50<<20) // 50MB max
	if err := r.ParseMultipartForm(50 << 20); err != nil {
		respondError(w, http.StatusBadRequest, "file too large (max 50MB)")
		return
	}

	title := strings.TrimSpace(r.FormValue("title"))
	genre := strings.TrimSpace(r.FormValue("genre"))
	description := strings.TrimSpace(r.FormValue("description"))

	if title == "" || genre == "" {
		respondError(w, http.StatusBadRequest, "title and genre are required")
		return
	}

	file, header, err := r.FormFile("file")
	if err != nil {
		respondError(w, http.StatusBadRequest, "music file is required")
		return
	}
	defer file.Close()

	if !strings.HasSuffix(strings.ToLower(header.Filename), ".mp3") {
		respondError(w, http.StatusBadRequest, "only MP3 files are allowed")
		return
	}

	buf := make([]byte, 512)
	n, _ := file.Read(buf)
	contentType := http.DetectContentType(buf[:n])
	if contentType != "audio/mpeg" && !strings.HasPrefix(contentType, "application/octet-stream") {
		respondError(w, http.StatusBadRequest, "invalid file type: only MP3 files are allowed")
		return
	}
	file.Seek(0, io.SeekStart)

	fileName := fmt.Sprintf("%d_%s", time.Now().UnixNano(), filepath.Base(header.Filename))
	filePath := filepath.Join(h.cfg.UploadsDir, fileName)

	dst, err := os.Create(filePath)
	if err != nil {
		respondError(w, http.StatusInternalServerError, "failed to save file")
		return
	}
	defer dst.Close()

	if _, err := io.Copy(dst, file); err != nil {
		os.Remove(filePath)
		respondError(w, http.StatusInternalServerError, "failed to save file")
		return
	}

	artist, _ := h.fb.GetUserByID(r.Context(), claims.UserID)
	artistName := claims.Email
	if artist != nil {
		artistName = artist.Name
	}

	song := &models.Song{
		Title:       title,
		Genre:       genre,
		Description: description,
		ArtistID:    claims.UserID,
		ArtistName:  artistName,
		FilePath:    filePath,
		FileName:    fileName,
		CreatedAt:   time.Now(),
	}

	id, err := h.fb.CreateSong(r.Context(), song)
	if err != nil {
		os.Remove(filePath)
		respondError(w, http.StatusInternalServerError, "failed to save song metadata")
		return
	}
	song.ID = id

	respondJSON(w, http.StatusCreated, song)
}

func (h *ArtistHandler) MySongs(w http.ResponseWriter, r *http.Request) {
	claims := auth.GetClaims(r.Context())

	songs, err := h.fb.GetSongsByArtist(r.Context(), claims.UserID)
	if err != nil {
		respondError(w, http.StatusInternalServerError, "failed to fetch songs")
		return
	}

	if songs == nil {
		songs = []models.Song{}
	}

	respondJSON(w, http.StatusOK, songs)
}

func (h *ArtistHandler) DeleteSong(w http.ResponseWriter, r *http.Request) {
	claims := auth.GetClaims(r.Context())
	songID := mux.Vars(r)["id"]

	song, err := h.fb.GetSong(r.Context(), songID)
	if err != nil {
		respondError(w, http.StatusNotFound, "song not found")
		return
	}

	if song.ArtistID != claims.UserID {
		respondError(w, http.StatusForbidden, "you can only delete your own songs")
		return
	}

	os.Remove(song.FilePath)

	if err := h.fb.DeleteSong(r.Context(), songID); err != nil {
		respondError(w, http.StatusInternalServerError, "failed to delete song")
		return
	}

	respondJSON(w, http.StatusOK, map[string]string{"message": "song deleted"})
}
