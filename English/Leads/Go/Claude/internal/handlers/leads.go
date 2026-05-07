package handlers

import (
	"context"
	"encoding/json"
	"net/http"
	"sort"
	"strconv"
	"strings"
	"time"

	fb "lead-manager/internal/firebase"
	"lead-manager/internal/middleware"
	"lead-manager/internal/models"

	"google.golang.org/api/iterator"
)

type LeadHandler struct{}

func NewLeadHandler() *LeadHandler {
	return &LeadHandler{}
}

func (h *LeadHandler) Create(w http.ResponseWriter, r *http.Request) {
	userID := middleware.GetUserID(r.Context())
	var lead models.Lead
	if err := json.NewDecoder(r.Body).Decode(&lead); err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "invalid request body"})
		return
	}

	if lead.FullName == "" || lead.Email == "" {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "full_name and email are required"})
		return
	}

	if lead.Status == "" {
		lead.Status = "new"
	}
	if lead.CaptureDate.IsZero() {
		lead.CaptureDate = time.Now()
	}

	lead.UserID = userID
	lead.CreatedAt = time.Now()
	lead.UpdatedAt = time.Now()

	ctx := context.Background()
	client := fb.GetClient()

	docRef, _, err := client.Collection(fb.LeadsCollection).Add(ctx, toMap(lead))
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to create lead"})
		return
	}

	lead.ID = docRef.ID
	writeJSON(w, http.StatusCreated, lead)
}

func (h *LeadHandler) GetAll(w http.ResponseWriter, r *http.Request) {
	userID := middleware.GetUserID(r.Context())
	ctx := context.Background()
	client := fb.GetClient()

	query := client.Collection(fb.LeadsCollection).Where("user_id", "==", userID)

	filter := parseFilter(r)

	if filter.Status != "" {
		query = query.Where("status", "==", filter.Status)
	}
	if filter.Source != "" {
		query = query.Where("source", "==", filter.Source)
	}

	iter := query.Documents(ctx)
	defer iter.Stop()

	var leads []models.Lead
	for {
		doc, err := iter.Next()
		if err == iterator.Done {
			break
		}
		if err != nil {
			writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to fetch leads"})
			return
		}

		var lead models.Lead
		if err := doc.DataTo(&lead); err != nil {
			continue
		}
		lead.ID = doc.Ref.ID

		if filter.DateFrom != "" {
			if from, err := time.Parse("2006-01-02", filter.DateFrom); err == nil {
				if lead.CaptureDate.Before(from) {
					continue
				}
			}
		}
		if filter.DateTo != "" {
			if to, err := time.Parse("2006-01-02", filter.DateTo); err == nil {
				if lead.CaptureDate.After(to.Add(24 * time.Hour)) {
					continue
				}
			}
		}

		if filter.Search != "" {
			search := strings.ToLower(filter.Search)
			if !strings.Contains(strings.ToLower(lead.FullName), search) &&
				!strings.Contains(strings.ToLower(lead.Email), search) &&
				!strings.Contains(strings.ToLower(lead.Company), search) {
				continue
			}
		}

		leads = append(leads, lead)
	}

	sortBy := filter.SortBy
	if sortBy == "" {
		sortBy = "created_at"
	}
	sortOrder := filter.SortOrder
	if sortOrder == "" {
		sortOrder = "desc"
	}

	sort.Slice(leads, func(i, j int) bool {
		var less bool
		switch sortBy {
		case "full_name":
			less = strings.ToLower(leads[i].FullName) < strings.ToLower(leads[j].FullName)
		case "email":
			less = strings.ToLower(leads[i].Email) < strings.ToLower(leads[j].Email)
		case "company":
			less = strings.ToLower(leads[i].Company) < strings.ToLower(leads[j].Company)
		case "status":
			less = leads[i].Status < leads[j].Status
		case "capture_date":
			less = leads[i].CaptureDate.Before(leads[j].CaptureDate)
		default:
			less = leads[i].CreatedAt.Before(leads[j].CreatedAt)
		}
		if sortOrder == "desc" {
			return !less
		}
		return less
	})

	total := len(leads)
	page := filter.Page
	if page < 1 {
		page = 1
	}
	pageSize := filter.PageSize
	if pageSize < 1 {
		pageSize = 10
	}
	if pageSize > 100 {
		pageSize = 100
	}

	start := (page - 1) * pageSize
	end := start + pageSize
	if start > total {
		start = total
	}
	if end > total {
		end = total
	}

	totalPages := total / pageSize
	if total%pageSize != 0 {
		totalPages++
	}

	writeJSON(w, http.StatusOK, models.PaginatedResponse{
		Data:       leads[start:end],
		Total:      total,
		Page:       page,
		PageSize:   pageSize,
		TotalPages: totalPages,
	})
}

func (h *LeadHandler) GetByID(w http.ResponseWriter, r *http.Request) {
	userID := middleware.GetUserID(r.Context())
	leadID := extractPathParam(r.URL.Path, "/api/leads/")

	if leadID == "" {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "lead ID required"})
		return
	}

	ctx := context.Background()
	client := fb.GetClient()

	doc, err := client.Collection(fb.LeadsCollection).Doc(leadID).Get(ctx)
	if err != nil {
		writeJSON(w, http.StatusNotFound, map[string]string{"error": "lead not found"})
		return
	}

	var lead models.Lead
	if err := doc.DataTo(&lead); err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to parse lead"})
		return
	}
	lead.ID = doc.Ref.ID

	if lead.UserID != userID {
		writeJSON(w, http.StatusForbidden, map[string]string{"error": "access denied"})
		return
	}

	writeJSON(w, http.StatusOK, lead)
}

func (h *LeadHandler) Update(w http.ResponseWriter, r *http.Request) {
	userID := middleware.GetUserID(r.Context())
	leadID := extractPathParam(r.URL.Path, "/api/leads/")

	if leadID == "" {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "lead ID required"})
		return
	}

	ctx := context.Background()
	client := fb.GetClient()

	doc, err := client.Collection(fb.LeadsCollection).Doc(leadID).Get(ctx)
	if err != nil {
		writeJSON(w, http.StatusNotFound, map[string]string{"error": "lead not found"})
		return
	}

	var existing models.Lead
	if err := doc.DataTo(&existing); err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to parse lead"})
		return
	}

	if existing.UserID != userID {
		writeJSON(w, http.StatusForbidden, map[string]string{"error": "access denied"})
		return
	}

	var update models.Lead
	if err := json.NewDecoder(r.Body).Decode(&update); err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "invalid request body"})
		return
	}

	if update.FullName != "" {
		existing.FullName = update.FullName
	}
	if update.Email != "" {
		existing.Email = update.Email
	}
	if update.Phone != "" {
		existing.Phone = update.Phone
	}
	if update.Company != "" {
		existing.Company = update.Company
	}
	if update.Position != "" {
		existing.Position = update.Position
	}
	if update.Source != "" {
		existing.Source = update.Source
	}
	if update.Status != "" {
		existing.Status = update.Status
	}
	if !update.CaptureDate.IsZero() {
		existing.CaptureDate = update.CaptureDate
	}
	if update.Notes != "" {
		existing.Notes = update.Notes
	}
	existing.UpdatedAt = time.Now()

	if _, err := client.Collection(fb.LeadsCollection).Doc(leadID).Set(ctx, toMap(existing)); err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to update lead"})
		return
	}

	existing.ID = leadID
	writeJSON(w, http.StatusOK, existing)
}

func (h *LeadHandler) Delete(w http.ResponseWriter, r *http.Request) {
	userID := middleware.GetUserID(r.Context())
	leadID := extractPathParam(r.URL.Path, "/api/leads/")

	if leadID == "" {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "lead ID required"})
		return
	}

	ctx := context.Background()
	client := fb.GetClient()

	doc, err := client.Collection(fb.LeadsCollection).Doc(leadID).Get(ctx)
	if err != nil {
		writeJSON(w, http.StatusNotFound, map[string]string{"error": "lead not found"})
		return
	}

	var lead models.Lead
	if err := doc.DataTo(&lead); err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to parse lead"})
		return
	}

	if lead.UserID != userID {
		writeJSON(w, http.StatusForbidden, map[string]string{"error": "access denied"})
		return
	}

	if _, err := client.Collection(fb.LeadsCollection).Doc(leadID).Delete(ctx); err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to delete lead"})
		return
	}

	writeJSON(w, http.StatusOK, map[string]string{"message": "lead deleted successfully"})
}

func (h *LeadHandler) GetStats(w http.ResponseWriter, r *http.Request) {
	userID := middleware.GetUserID(r.Context())
	ctx := context.Background()
	client := fb.GetClient()

	iter := client.Collection(fb.LeadsCollection).Where("user_id", "==", userID).Documents(ctx)
	defer iter.Stop()

	statusCount := map[string]int{"new": 0, "contacted": 0, "qualified": 0, "lost": 0}
	sourceCount := map[string]int{}
	monthlyCaptures := map[string]int{}
	total := 0

	for {
		doc, err := iter.Next()
		if err == iterator.Done {
			break
		}
		if err != nil {
			writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to fetch stats"})
			return
		}

		var lead models.Lead
		if err := doc.DataTo(&lead); err != nil {
			continue
		}
		total++
		statusCount[lead.Status]++
		sourceCount[lead.Source]++
		monthKey := lead.CaptureDate.Format("2006-01")
		monthlyCaptures[monthKey]++
	}

	writeJSON(w, http.StatusOK, map[string]interface{}{
		"total":            total,
		"by_status":        statusCount,
		"by_source":        sourceCount,
		"monthly_captures": monthlyCaptures,
	})
}

func (h *LeadHandler) AddInteraction(w http.ResponseWriter, r *http.Request) {
	userID := middleware.GetUserID(r.Context())
	path := r.URL.Path
	parts := strings.Split(strings.Trim(path, "/"), "/")

	var leadID string
	for i, p := range parts {
		if p == "leads" && i+1 < len(parts) {
			leadID = parts[i+1]
			break
		}
	}

	if leadID == "" {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "lead ID required"})
		return
	}

	ctx := context.Background()
	client := fb.GetClient()

	doc, err := client.Collection(fb.LeadsCollection).Doc(leadID).Get(ctx)
	if err != nil {
		writeJSON(w, http.StatusNotFound, map[string]string{"error": "lead not found"})
		return
	}

	var lead models.Lead
	if err := doc.DataTo(&lead); err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to parse lead"})
		return
	}
	if lead.UserID != userID {
		writeJSON(w, http.StatusForbidden, map[string]string{"error": "access denied"})
		return
	}

	var interaction models.Interaction
	if err := json.NewDecoder(r.Body).Decode(&interaction); err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "invalid request body"})
		return
	}

	interaction.LeadID = leadID
	interaction.UserID = userID
	interaction.CreatedAt = time.Now()

	collName := fb.LeadsCollection + "_interactions"
	docRef, _, err := client.Collection(collName).Add(ctx, map[string]interface{}{
		"lead_id":    interaction.LeadID,
		"user_id":    interaction.UserID,
		"type":       interaction.Type,
		"notes":      interaction.Notes,
		"created_at": interaction.CreatedAt,
	})
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to add interaction"})
		return
	}

	interaction.ID = docRef.ID
	writeJSON(w, http.StatusCreated, interaction)
}

func (h *LeadHandler) GetInteractions(w http.ResponseWriter, r *http.Request) {
	userID := middleware.GetUserID(r.Context())
	path := r.URL.Path
	parts := strings.Split(strings.Trim(path, "/"), "/")

	var leadID string
	for i, p := range parts {
		if p == "leads" && i+1 < len(parts) {
			leadID = parts[i+1]
			break
		}
	}

	if leadID == "" {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "lead ID required"})
		return
	}

	ctx := context.Background()
	client := fb.GetClient()

	doc, err := client.Collection(fb.LeadsCollection).Doc(leadID).Get(ctx)
	if err != nil {
		writeJSON(w, http.StatusNotFound, map[string]string{"error": "lead not found"})
		return
	}

	var lead models.Lead
	if err := doc.DataTo(&lead); err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to parse lead"})
		return
	}
	if lead.UserID != userID {
		writeJSON(w, http.StatusForbidden, map[string]string{"error": "access denied"})
		return
	}

	collName := fb.LeadsCollection + "_interactions"
	iter := client.Collection(collName).Where("lead_id", "==", leadID).Documents(ctx)
	defer iter.Stop()

	var interactions []models.Interaction
	for {
		d, err := iter.Next()
		if err == iterator.Done {
			break
		}
		if err != nil {
			writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to fetch interactions"})
			return
		}
		var inter models.Interaction
		if err := d.DataTo(&inter); err != nil {
			continue
		}
		inter.ID = d.Ref.ID
		interactions = append(interactions, inter)
	}

	sort.Slice(interactions, func(i, j int) bool {
		return interactions[i].CreatedAt.After(interactions[j].CreatedAt)
	})

	if interactions == nil {
		interactions = []models.Interaction{}
	}

	writeJSON(w, http.StatusOK, interactions)
}

func toMap(lead models.Lead) map[string]interface{} {
	return map[string]interface{}{
		"full_name":    lead.FullName,
		"email":        lead.Email,
		"phone":        lead.Phone,
		"company":      lead.Company,
		"position":     lead.Position,
		"source":       lead.Source,
		"status":       lead.Status,
		"capture_date": lead.CaptureDate,
		"notes":        lead.Notes,
		"user_id":      lead.UserID,
		"created_at":   lead.CreatedAt,
		"updated_at":   lead.UpdatedAt,
	}
}

func extractPathParam(path, prefix string) string {
	trimmed := strings.TrimPrefix(path, prefix)
	parts := strings.Split(trimmed, "/")
	if len(parts) > 0 {
		return parts[0]
	}
	return ""
}

func parseFilter(r *http.Request) models.LeadFilter {
	page, _ := strconv.Atoi(r.URL.Query().Get("page"))
	pageSize, _ := strconv.Atoi(r.URL.Query().Get("page_size"))
	return models.LeadFilter{
		Status:    r.URL.Query().Get("status"),
		Source:    r.URL.Query().Get("source"),
		DateFrom:  r.URL.Query().Get("date_from"),
		DateTo:    r.URL.Query().Get("date_to"),
		Search:    r.URL.Query().Get("search"),
		Page:      page,
		PageSize:  pageSize,
		SortBy:    r.URL.Query().Get("sort_by"),
		SortOrder: r.URL.Query().Get("sort_order"),
	}
}
