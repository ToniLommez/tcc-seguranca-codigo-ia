package api

import (
	"encoding/json"
	"fmt"
	"io"
	"mime/multipart"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/go-chi/chi/v5"
	chimiddleware "github.com/go-chi/chi/v5/middleware"
	"golang.org/x/crypto/bcrypt"

	"streamapp/internal/auth"
	"streamapp/internal/config"
	"streamapp/internal/domain"
	"streamapp/internal/firebase"
	"streamapp/internal/music"
)

type Server struct {
	cfg         config.Config
	jwt         *auth.JWTService
	users       *firebase.UserRepository
	music       *music.Repository
	fileHandler http.Handler
}

func NewServer(cfg config.Config, jwt *auth.JWTService, users *firebase.UserRepository, musicRepo *music.Repository) *Server {
	return &Server{
		cfg:         cfg,
		jwt:         jwt,
		users:       users,
		music:       musicRepo,
		fileHandler: http.FileServer(http.Dir(cfg.WebDir)),
	}
}

func (s *Server) Router() http.Handler {
	r := chi.NewRouter()
	r.Use(chimiddleware.RealIP)
	r.Use(chimiddleware.Recoverer)
	r.Use(chimiddleware.Logger)

	r.Get("/api/health", s.handleHealth)
	r.Post("/api/auth/register", s.handleRegister)
	r.Post("/api/auth/login", s.handleLogin)

	r.Group(func(protected chi.Router) {
		protected.Use(auth.Middleware(s.jwt))

		protected.Get("/api/me", s.handleMe)

		protected.Group(func(artist chi.Router) {
			artist.Use(auth.RequireRoles(domain.UserTypeArtist))
			artist.Post("/api/artist/songs", s.handleArtistUploadSong)
			artist.Get("/api/artist/songs", s.handleArtistSongs)
		})

		protected.Group(func(user chi.Router) {
			user.Use(auth.RequireRoles(domain.UserTypeUser))
			user.Get("/api/songs", s.handleSongsSearch)
			user.Get("/api/songs/{id}/stream", s.handleSongStream)
		})
	})

	r.Get("/*", s.handleWeb)
	return r
}

func (s *Server) handleHealth(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, map[string]string{
		"status": "ok",
	})
}

func (s *Server) handleRegister(w http.ResponseWriter, r *http.Request) {
	var request struct {
		Name     string `json:"name"`
		Email    string `json:"email"`
		Password string `json:"password"`
		Type     string `json:"type"`
	}

	if err := json.NewDecoder(r.Body).Decode(&request); err != nil {
		writeError(w, http.StatusBadRequest, "payload invalido")
		return
	}

	if strings.TrimSpace(request.Name) == "" || strings.TrimSpace(request.Email) == "" || len(strings.TrimSpace(request.Password)) < 6 {
		writeError(w, http.StatusBadRequest, "nome, email e senha valida sao obrigatorios")
		return
	}

	userType, err := domain.NormalizeUserType(request.Type)
	if err != nil {
		writeError(w, http.StatusBadRequest, "tipo de usuario invalido")
		return
	}

	passwordHash, err := bcrypt.GenerateFromPassword([]byte(request.Password), bcrypt.DefaultCost)
	if err != nil {
		writeError(w, http.StatusInternalServerError, "erro ao proteger a senha")
		return
	}

	user, err := s.users.Create(r.Context(), domain.User{
		Name:         strings.TrimSpace(request.Name),
		Email:        strings.TrimSpace(request.Email),
		PasswordHash: string(passwordHash),
		Type:         userType,
	})
	if err != nil {
		writeError(w, http.StatusConflict, err.Error())
		return
	}

	token, err := s.jwt.GenerateToken(user)
	if err != nil {
		writeError(w, http.StatusInternalServerError, "erro ao gerar token")
		return
	}

	writeJSON(w, http.StatusCreated, map[string]interface{}{
		"token": token,
		"user":  userResponse(user),
	})
}

func (s *Server) handleLogin(w http.ResponseWriter, r *http.Request) {
	var request struct {
		Email    string `json:"email"`
		Password string `json:"password"`
	}

	if err := json.NewDecoder(r.Body).Decode(&request); err != nil {
		writeError(w, http.StatusBadRequest, "payload invalido")
		return
	}

	user, err := s.users.GetByEmail(r.Context(), request.Email)
	if err != nil {
		writeError(w, http.StatusUnauthorized, "email ou senha invalidos")
		return
	}

	if err := bcrypt.CompareHashAndPassword([]byte(user.PasswordHash), []byte(request.Password)); err != nil {
		writeError(w, http.StatusUnauthorized, "email ou senha invalidos")
		return
	}

	token, err := s.jwt.GenerateToken(user)
	if err != nil {
		writeError(w, http.StatusInternalServerError, "erro ao gerar token")
		return
	}

	writeJSON(w, http.StatusOK, map[string]interface{}{
		"token": token,
		"user":  userResponse(user),
	})
}

func (s *Server) handleMe(w http.ResponseWriter, r *http.Request) {
	claims, ok := auth.ClaimsFromContext(r.Context())
	if !ok {
		writeError(w, http.StatusUnauthorized, "token ausente")
		return
	}

	writeJSON(w, http.StatusOK, map[string]interface{}{
		"user": map[string]string{
			"id":    claims.Subject,
			"name":  claims.Name,
			"email": claims.Email,
			"type":  string(claims.Type),
		},
	})
}

func (s *Server) handleArtistUploadSong(w http.ResponseWriter, r *http.Request) {
	claims, ok := auth.ClaimsFromContext(r.Context())
	if !ok {
		writeError(w, http.StatusUnauthorized, "token ausente")
		return
	}

	if err := r.ParseMultipartForm(30 << 20); err != nil {
		writeError(w, http.StatusBadRequest, "falha ao processar upload")
		return
	}

	title := strings.TrimSpace(r.FormValue("title"))
	genre := strings.TrimSpace(r.FormValue("genre"))
	description := strings.TrimSpace(r.FormValue("description"))
	if title == "" || genre == "" {
		writeError(w, http.StatusBadRequest, "titulo e genero sao obrigatorios")
		return
	}

	file, header, err := r.FormFile("file")
	if err != nil {
		writeError(w, http.StatusBadRequest, "arquivo mp3 obrigatorio")
		return
	}
	defer file.Close()

	if strings.ToLower(filepath.Ext(header.Filename)) != ".mp3" {
		writeError(w, http.StatusBadRequest, "apenas arquivos mp3 sao aceitos")
		return
	}

	okMP3, err := music.LooksLikeMP3(file)
	if err != nil {
		writeError(w, http.StatusBadRequest, "nao foi possivel validar o mp3")
		return
	}
	if !okMP3 {
		writeError(w, http.StatusBadRequest, "o arquivo enviado nao parece ser um mp3 valido")
		return
	}

	targetPath, err := s.persistSongFile(file, header, claims.Email)
	if err != nil {
		writeError(w, http.StatusInternalServerError, "falha ao salvar arquivo localmente")
		return
	}

	song, err := s.music.Create(domain.Song{
		Title:        title,
		Genre:        genre,
		Description:  description,
		ArtistEmail:  claims.Email,
		ArtistName:   claims.Name,
		FilePath:     targetPath,
		OriginalName: header.Filename,
	})
	if err != nil {
		_ = os.Remove(targetPath)
		writeError(w, http.StatusInternalServerError, "falha ao registrar musica")
		return
	}

	writeJSON(w, http.StatusCreated, songResponse(song))
}

func (s *Server) handleArtistSongs(w http.ResponseWriter, r *http.Request) {
	claims, ok := auth.ClaimsFromContext(r.Context())
	if !ok {
		writeError(w, http.StatusUnauthorized, "token ausente")
		return
	}

	songs, err := s.music.ListByArtist(claims.Email)
	if err != nil {
		writeError(w, http.StatusInternalServerError, "falha ao listar musicas")
		return
	}

	writeJSON(w, http.StatusOK, map[string]interface{}{
		"items": songsToResponse(songs),
	})
}

func (s *Server) handleSongsSearch(w http.ResponseWriter, r *http.Request) {
	songs, err := s.music.Search(
		r.URL.Query().Get("name"),
		r.URL.Query().Get("artist"),
		r.URL.Query().Get("genre"),
	)
	if err != nil {
		writeError(w, http.StatusInternalServerError, "falha ao pesquisar musicas")
		return
	}

	writeJSON(w, http.StatusOK, map[string]interface{}{
		"items": songsToResponse(songs),
	})
}

func (s *Server) handleSongStream(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.ParseInt(chi.URLParam(r, "id"), 10, 64)
	if err != nil {
		writeError(w, http.StatusBadRequest, "id invalido")
		return
	}

	song, err := s.music.GetByID(id)
	if err != nil {
		writeError(w, http.StatusNotFound, "musica nao encontrada")
		return
	}

	file, err := os.Open(song.FilePath)
	if err != nil {
		writeError(w, http.StatusNotFound, "arquivo de audio nao encontrado")
		return
	}
	defer file.Close()

	w.Header().Set("Content-Type", "audio/mpeg")
	w.Header().Set("Accept-Ranges", "bytes")
	http.ServeContent(w, r, song.OriginalName, song.CreatedAt, file)
}

func (s *Server) handleWeb(w http.ResponseWriter, r *http.Request) {
	requestPath := strings.TrimPrefix(filepath.Clean(r.URL.Path), string(os.PathSeparator))
	target := filepath.Join(s.cfg.WebDir, requestPath)
	if info, err := os.Stat(target); err == nil && !info.IsDir() {
		s.fileHandler.ServeHTTP(w, r)
		return
	}

	http.ServeFile(w, r, filepath.Join(s.cfg.WebDir, "index.html"))
}

func (s *Server) persistSongFile(file multipart.File, header *multipart.FileHeader, artistEmail string) (string, error) {
	if _, err := file.Seek(0, io.SeekStart); err != nil {
		return "", fmt.Errorf("rewind upload: %w", err)
	}

	safeArtist := strings.NewReplacer("@", "_", ".", "_").Replace(strings.ToLower(artistEmail))
	filename := fmt.Sprintf("%s_%d.mp3", safeArtist, time.Now().UnixNano())
	targetPath := filepath.Join(s.cfg.UploadDir, filename)

	dst, err := os.Create(targetPath)
	if err != nil {
		return "", fmt.Errorf("create target file: %w", err)
	}
	defer dst.Close()

	if _, err := io.Copy(dst, file); err != nil {
		return "", fmt.Errorf("copy upload: %w", err)
	}

	return targetPath, nil
}

func userResponse(user domain.User) map[string]interface{} {
	return map[string]interface{}{
		"id":        user.ID,
		"name":      user.Name,
		"email":     user.Email,
		"type":      user.Type,
		"createdAt": user.CreatedAt,
	}
}

func songResponse(song domain.Song) map[string]interface{} {
	return map[string]interface{}{
		"id":          song.ID,
		"title":       song.Title,
		"genre":       song.Genre,
		"description": song.Description,
		"artistEmail": song.ArtistEmail,
		"artistName":  song.ArtistName,
		"createdAt":   song.CreatedAt,
	}
}

func songsToResponse(songs []domain.Song) []map[string]interface{} {
	items := make([]map[string]interface{}, 0, len(songs))
	for _, song := range songs {
		items = append(items, songResponse(song))
	}
	return items
}

func writeJSON(w http.ResponseWriter, status int, payload interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(payload)
}

func writeError(w http.ResponseWriter, status int, message string) {
	writeJSON(w, status, map[string]string{"error": message})
}
