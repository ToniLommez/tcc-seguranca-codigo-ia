package handlers

import (
	"net/http"
	"strings"
	"time"

	"streammusic/internal/middleware"
	"streammusic/internal/models"
	"streammusic/internal/repository"
	"streammusic/internal/storage"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

type ArtistHandler struct {
	songRepo    *repository.SongRepository
	fileStorage *storage.LocalMusicStorage
}

func NewArtistHandler(songRepo *repository.SongRepository, fileStorage *storage.LocalMusicStorage) *ArtistHandler {
	return &ArtistHandler{songRepo: songRepo, fileStorage: fileStorage}
}

func (h *ArtistHandler) UploadSong(c *gin.Context) {
	authUser, ok := middleware.GetAuthUser(c)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "authentication required"})
		return
	}

	title := strings.TrimSpace(c.PostForm("title"))
	genre := strings.TrimSpace(c.PostForm("genre"))
	description := strings.TrimSpace(c.PostForm("description"))

	if title == "" || genre == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "title and genre are required"})
		return
	}

	fileHeader, err := c.FormFile("file")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "MP3 file is required"})
		return
	}

	filePath, err := h.fileStorage.SaveMP3(fileHeader)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	song := &models.Song{
		ID:          uuid.NewString(),
		Title:       title,
		Genre:       genre,
		Description: description,
		FilePath:    filePath,
		ArtistID:    authUser.ID,
		ArtistName:  authUser.Name,
		CreatedAt:   time.Now().UTC(),
	}

	if err := h.songRepo.Create(c.Request.Context(), song); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to save song"})
		return
	}

	c.JSON(http.StatusCreated, gin.H{
		"message": "song uploaded successfully",
		"song":    song,
	})
}

func (h *ArtistHandler) ListOwnSongs(c *gin.Context) {
	authUser, ok := middleware.GetAuthUser(c)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "authentication required"})
		return
	}

	songs, err := h.songRepo.ListByArtist(c.Request.Context(), authUser.ID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to list songs"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"songs": songs})
}
