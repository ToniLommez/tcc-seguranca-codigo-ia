package httpapi

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/go-chi/chi/v5"

	authmiddleware "leadmanager/internal/middleware"
	"leadmanager/internal/models"
	"leadmanager/internal/repository"
	"leadmanager/internal/service"
)

type Handler struct {
	authService *service.AuthService
	leadService *service.LeadService
}

func NewHandler(authService *service.AuthService, leadService *service.LeadService) *Handler {
	return &Handler{
		authService: authService,
		leadService: leadService,
	}
}

func (h *Handler) RegisterPublicAuthRoutes(router chi.Router) {
	router.Post("/register", h.register)
	router.Post("/login", h.login)
}

func (h *Handler) RegisterProtectedAuthRoutes(router chi.Router) {
	router.Get("/me", h.me)
}

func (h *Handler) RegisterLeadRoutes(router chi.Router) {
	router.Get("/dashboard", h.dashboard)
	router.Get("/leads", h.listLeads)
	router.Post("/leads", h.createLead)
	router.Get("/leads/export", h.exportLeads)
	router.Post("/leads/import", h.importLeads)
	router.Get("/leads/{leadID}", h.getLead)
	router.Put("/leads/{leadID}", h.updateLead)
	router.Delete("/leads/{leadID}", h.deleteLead)
	router.Post("/leads/{leadID}/interactions", h.addInteraction)
}

func (h *Handler) register(w http.ResponseWriter, r *http.Request) {
	var payload struct {
		Name     string `json:"name"`
		Email    string `json:"email"`
		Password string `json:"password"`
	}

	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		writeError(w, http.StatusBadRequest, "invalid request body")
		return
	}

	user, token, err := h.authService.Register(r.Context(), payload.Name, payload.Email, payload.Password)
	if err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	writeJSON(w, http.StatusCreated, map[string]any{
		"user":  user,
		"token": token,
	})
}

func (h *Handler) login(w http.ResponseWriter, r *http.Request) {
	var payload struct {
		Email    string `json:"email"`
		Password string `json:"password"`
	}

	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		writeError(w, http.StatusBadRequest, "invalid request body")
		return
	}

	user, token, err := h.authService.Login(r.Context(), payload.Email, payload.Password)
	if err != nil {
		if errors.Is(err, service.ErrInvalidCredentials) {
			writeError(w, http.StatusUnauthorized, err.Error())
			return
		}
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	writeJSON(w, http.StatusOK, map[string]any{
		"user":  user,
		"token": token,
	})
}

func (h *Handler) me(w http.ResponseWriter, r *http.Request) {
	userID := authmiddleware.UserIDFromContext(r.Context())
	user, err := h.authService.GetUser(r.Context(), userID)
	if err != nil {
		writeError(w, http.StatusNotFound, "user not found")
		return
	}
	writeJSON(w, http.StatusOK, user)
}

func (h *Handler) dashboard(w http.ResponseWriter, r *http.Request) {
	userID := authmiddleware.UserIDFromContext(r.Context())
	summary, err := h.leadService.GetDashboardSummary(r.Context(), userID)
	if err != nil {
		writeError(w, http.StatusInternalServerError, err.Error())
		return
	}
	writeJSON(w, http.StatusOK, summary)
}

func (h *Handler) listLeads(w http.ResponseWriter, r *http.Request) {
	userID := authmiddleware.UserIDFromContext(r.Context())
	filters, err := parseFilters(r)
	if err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	response, err := h.leadService.ListLeads(r.Context(), userID, filters)
	if err != nil {
		writeError(w, http.StatusInternalServerError, err.Error())
		return
	}
	writeJSON(w, http.StatusOK, response)
}

func (h *Handler) createLead(w http.ResponseWriter, r *http.Request) {
	userID := authmiddleware.UserIDFromContext(r.Context())
	lead, err := decodeLeadPayload(r)
	if err != nil {
		writeError(w, http.StatusBadRequest, "invalid request body")
		return
	}

	created, err := h.leadService.CreateLead(r.Context(), userID, lead)
	if err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}
	writeJSON(w, http.StatusCreated, created)
}

func (h *Handler) getLead(w http.ResponseWriter, r *http.Request) {
	userID := authmiddleware.UserIDFromContext(r.Context())
	leadID := chi.URLParam(r, "leadID")

	detail, err := h.leadService.GetLeadDetail(r.Context(), userID, leadID)
	if err != nil {
		if errors.Is(err, repository.ErrNotFound) {
			writeError(w, http.StatusNotFound, "lead not found")
			return
		}
		writeError(w, http.StatusInternalServerError, err.Error())
		return
	}
	writeJSON(w, http.StatusOK, detail)
}

func (h *Handler) updateLead(w http.ResponseWriter, r *http.Request) {
	userID := authmiddleware.UserIDFromContext(r.Context())
	leadID := chi.URLParam(r, "leadID")

	lead, err := decodeLeadPayload(r)
	if err != nil {
		writeError(w, http.StatusBadRequest, "invalid request body")
		return
	}

	updated, err := h.leadService.UpdateLead(r.Context(), userID, leadID, lead)
	if err != nil {
		if errors.Is(err, repository.ErrNotFound) {
			writeError(w, http.StatusNotFound, "lead not found")
			return
		}
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}
	writeJSON(w, http.StatusOK, updated)
}

func (h *Handler) deleteLead(w http.ResponseWriter, r *http.Request) {
	userID := authmiddleware.UserIDFromContext(r.Context())
	leadID := chi.URLParam(r, "leadID")

	if err := h.leadService.DeleteLead(r.Context(), userID, leadID); err != nil {
		if errors.Is(err, repository.ErrNotFound) {
			writeError(w, http.StatusNotFound, "lead not found")
			return
		}
		writeError(w, http.StatusInternalServerError, err.Error())
		return
	}

	w.WriteHeader(http.StatusNoContent)
}

func (h *Handler) addInteraction(w http.ResponseWriter, r *http.Request) {
	userID := authmiddleware.UserIDFromContext(r.Context())
	leadID := chi.URLParam(r, "leadID")

	var payload struct {
		Type string `json:"type"`
		Note string `json:"note"`
	}

	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		writeError(w, http.StatusBadRequest, "invalid request body")
		return
	}

	item, err := h.leadService.AddInteraction(r.Context(), userID, leadID, userID, models.LeadInteraction{
		Type: payload.Type,
		Note: payload.Note,
	})
	if err != nil {
		if errors.Is(err, repository.ErrNotFound) {
			writeError(w, http.StatusNotFound, "lead not found")
			return
		}
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	writeJSON(w, http.StatusCreated, item)
}

func (h *Handler) importLeads(w http.ResponseWriter, r *http.Request) {
	userID := authmiddleware.UserIDFromContext(r.Context())

	if err := r.ParseMultipartForm(16 << 20); err != nil {
		writeError(w, http.StatusBadRequest, "failed to parse upload")
		return
	}

	file, fileHeader, err := r.FormFile("file")
	if err != nil {
		writeError(w, http.StatusBadRequest, "file is required")
		return
	}
	defer file.Close()

	result, err := h.leadService.ImportLeads(r.Context(), userID, fileHeader, file)
	if err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	writeJSON(w, http.StatusOK, result)
}

func (h *Handler) exportLeads(w http.ResponseWriter, r *http.Request) {
	userID := authmiddleware.UserIDFromContext(r.Context())
	filters, err := parseFilters(r)
	if err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	format := strings.TrimSpace(r.URL.Query().Get("format"))
	data, contentType, filename, err := h.leadService.ExportLeads(r.Context(), userID, filters, format)
	if err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	w.Header().Set("Content-Type", contentType)
	w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=%q", filename))
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write(data)
}

func parseFilters(r *http.Request) (models.LeadFilters, error) {
	query := r.URL.Query()
	page, _ := strconv.Atoi(query.Get("page"))
	pageSize, _ := strconv.Atoi(query.Get("pageSize"))

	filters := models.LeadFilters{
		Page:     page,
		PageSize: pageSize,
		SortBy:   strings.TrimSpace(query.Get("sortBy")),
		SortDir:  strings.TrimSpace(query.Get("sortDir")),
		Status:   strings.TrimSpace(query.Get("status")),
		Source:   strings.TrimSpace(query.Get("source")),
		Query:    strings.TrimSpace(query.Get("q")),
	}

	if from := strings.TrimSpace(query.Get("dateFrom")); from != "" {
		parsed, err := parseDate(from)
		if err != nil {
			return models.LeadFilters{}, fmt.Errorf("invalid dateFrom: %w", err)
		}
		filters.DateFrom = &parsed
	}
	if to := strings.TrimSpace(query.Get("dateTo")); to != "" {
		parsed, err := parseDate(to)
		if err != nil {
			return models.LeadFilters{}, fmt.Errorf("invalid dateTo: %w", err)
		}
		filters.DateTo = &parsed
	}

	return filters, nil
}

func parseDate(value string) (time.Time, error) {
	layouts := []string{time.RFC3339, "2006-01-02"}
	for _, layout := range layouts {
		if parsed, err := time.Parse(layout, value); err == nil {
			return parsed.UTC(), nil
		}
	}
	return time.Time{}, fmt.Errorf("unsupported date format %q", value)
}

func decodeLeadPayload(r *http.Request) (models.Lead, error) {
	var payload struct {
		FullName    string `json:"fullName"`
		Email       string `json:"email"`
		Phone       string `json:"phone"`
		Company     string `json:"company"`
		Position    string `json:"position"`
		Source      string `json:"source"`
		Status      string `json:"status"`
		CaptureDate string `json:"captureDate"`
	}

	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		return models.Lead{}, err
	}

	lead := models.Lead{
		FullName: payload.FullName,
		Email:    payload.Email,
		Phone:    payload.Phone,
		Company:  payload.Company,
		Position: payload.Position,
		Source:   payload.Source,
		Status:   payload.Status,
	}

	if strings.TrimSpace(payload.CaptureDate) != "" {
		parsed, err := parseDate(payload.CaptureDate)
		if err != nil {
			return models.Lead{}, err
		}
		lead.CaptureDate = parsed
	}

	return lead, nil
}

func writeJSON(w http.ResponseWriter, status int, payload any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(payload)
}

func writeError(w http.ResponseWriter, status int, message string) {
	writeJSON(w, status, map[string]string{"error": message})
}
