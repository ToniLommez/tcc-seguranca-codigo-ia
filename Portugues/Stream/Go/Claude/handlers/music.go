package handlers

import (
	"net/http"
	"os"

	"github.com/gorilla/mux"

	"musicstream/db"
	"musicstream/models"
)

type MusicHandler struct {
	db *db.Database
}

func NewMusicHandler(database *db.Database) *MusicHandler {
	return &MusicHandler{db: database}
}

func (h *MusicHandler) ListMusic(w http.ResponseWriter, r *http.Request) {
	musics, err := h.db.ListAllMusic()
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

func (h *MusicHandler) SearchMusic(w http.ResponseWriter, r *http.Request) {
	q := r.URL.Query()
	musics, err := h.db.SearchMusic(q.Get("title"), q.Get("artist"), q.Get("genre"))
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao buscar músicas"})
		return
	}

	result := make([]models.Music, len(musics))
	for i, m := range musics {
		result[i] = m.ToPublic()
	}

	writeJSON(w, http.StatusOK, result)
}

func (h *MusicHandler) StreamMusic(w http.ResponseWriter, r *http.Request) {
	id := mux.Vars(r)["id"]

	music, err := h.db.GetMusicByID(id)
	if err != nil {
		writeJSON(w, http.StatusNotFound, models.ErrorResponse{Error: "música não encontrada"})
		return
	}

	file, err := os.Open(music.FilePath)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "arquivo não encontrado no servidor"})
		return
	}
	defer file.Close()

	stat, err := file.Stat()
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, models.ErrorResponse{Error: "erro ao ler arquivo"})
		return
	}

	w.Header().Set("Content-Type", "audio/mpeg")
	w.Header().Set("Accept-Ranges", "bytes")
	// http.ServeContent handles Range requests automatically (seek support).
	http.ServeContent(w, r, music.Title+".mp3", stat.ModTime(), file)
}
