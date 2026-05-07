package handlers

import (
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"github.com/google/uuid"

	"musicstream/db"
	"musicstream/middleware"
	"musicstream/models"
)

type ArtistHandler struct {
	db *db.Database
}

func NewArtistHandler(database *db.Database) *ArtistHandler {
	return &ArtistHandler{db: database}
}

func (h *ArtistHandler) UploadMusic(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r)

	// Limit upload to 50 MB.
	if err := r.ParseMultipartForm(50 << 20); err != nil {
		writeJSON(w, http.StatusBadRequest, models.ErrorResponse{Error: "falha ao processar formulário (máx 50MB)"})
		return
	}

	title := strings.TrimSpace(r.FormValue("title"))
	genre := strings.TrimSpace(r.FormValue("genre"))
	description := strings.TrimSpace(r.FormValue("description"))

	if title == "" || genre == "" {
		writeJSON(w, http.StatusBadRequest, models.ErrorResponse{Error: "título e gênero são obrigatórios"})
		return
	}

	file, header, err := r.FormFile("file")
	if err != nil {
		writeJSON(w, http.StatusBadRequest, models.ErrorResponse{Error: "arquivo de música é obrigatório"})
		return
	}
	defer file.Close()

	if !strings.HasSuffix(strings.ToLower(header.Filename), ".mp3") {
		writeJSON(w, http.StatusBadRequest, models.ErrorResponse{Error: "apenas arquivos MP3 são permitidos"})
		return
	}

	fileID := uuid.New().String()
	filePath := filepath.Join("uploads", fileID+".mp3")

	dst, err := os.Create(filePath)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao salvar arquivo"})
		return
	}
	defer dst.Close()

	if _, err := io.Copy(dst, file); err != nil {
		os.Remove(filePath)
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao gravar arquivo"})
		return
	}

	music := &models.MusicDB{
		ID:          fileID,
		Title:       title,
		Genre:       genre,
		Description: description,
		ArtistID:    claims.UserID,
		ArtistName:  claims.Name,
		FilePath:    filePath,
	}

	if err := h.db.CreateMusic(music); err != nil {
		os.Remove(filePath)
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao salvar música no banco"})
		return
	}

	writeJSON(w, http.StatusCreated, music.ToPublic())
}

func (h *ArtistHandler) ListMyMusic(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r)

	musics, err := h.db.ListMusicByArtist(claims.UserID)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao listar músicas"})
		return
	}

	result := make([]models.Music, len(musics))
	for i, m := range musics {
		result[i] = m.ToPublic()
	}

	writeJSON(w, http.StatusOK, result)
}
