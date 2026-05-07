package handlers

import (
	"errors"
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"streammusic/internal/models"
	"streammusic/internal/repository"

	"github.com/gin-gonic/gin"
)

type UserHandler struct {
	songRepo *repository.SongRepository
}

func NewUserHandler(songRepo *repository.SongRepository) *UserHandler {
	return &UserHandler{songRepo: songRepo}
}

func (h *UserHandler) ListSongs(c *gin.Context) {
	filter := models.SongFilter{
		Title:  strings.TrimSpace(c.Query("title")),
		Artist: strings.TrimSpace(c.Query("artist")),
		Genre:  strings.TrimSpace(c.Query("genre")),
	}

	songs, err := h.songRepo.List(c.Request.Context(), filter)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to list songs"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"songs": songs})
}

func (h *UserHandler) StreamSong(c *gin.Context) {
	songID := c.Param("id")
	song, err := h.songRepo.GetByID(c.Request.Context(), songID)
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "song not found"})
		return
	}

	file, err := os.Open(song.FilePath)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			c.JSON(http.StatusNotFound, gin.H{"error": "audio file not found on server"})
			return
		}
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to open audio file"})
		return
	}
	defer file.Close()

	info, err := file.Stat()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to read audio file"})
		return
	}

	c.Header("Content-Type", "audio/mpeg")
	c.Header("Content-Disposition", `inline; filename="`+filepath.Base(song.FilePath)+`"`)
	http.ServeContent(c.Writer, c.Request, song.Title, info.ModTime(), file)
}
