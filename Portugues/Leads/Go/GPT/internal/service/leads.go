package service

import (
	"bytes"
	"context"
	"encoding/csv"
	"errors"
	"fmt"
	"io"
	"net/mail"
	"path/filepath"
	"sort"
	"strings"
	"time"

	"github.com/xuri/excelize/v2"

	"leadmanagergpt/internal/models"
	"leadmanagergpt/internal/store"
)

type LeadService struct {
	store *store.FirestoreStore
}

func NewLeadService(store *store.FirestoreStore) *LeadService {
	return &LeadService{store: store}
}

func (s *LeadService) CreateLead(ctx context.Context, ownerID, createdBy string, input models.LeadInput) (models.Lead, error) {
	lead, err := inputToLead(ownerID, input)
	if err != nil {
		return models.Lead{}, err
	}

	created, err := s.store.CreateLead(ctx, lead)
	if err != nil {
		return models.Lead{}, err
	}

	_, _ = s.store.CreateInteraction(ctx, models.Interaction{
		LeadID:    created.ID,
		OwnerID:   ownerID,
		Type:      models.InteractionTypeSystem,
		Notes:     "Lead criado no sistema.",
		CreatedBy: createdBy,
	})

	return created, nil
}

func (s *LeadService) UpdateLead(ctx context.Context, ownerID, leadID, updatedBy string, input models.LeadInput) (models.Lead, error) {
	existing, err := s.store.GetLeadByID(ctx, leadID)
	if err != nil {
		return models.Lead{}, err
	}
	if existing.OwnerID != ownerID {
		return models.Lead{}, store.ErrNotFound
	}

	updated, err := inputToLead(ownerID, input)
	if err != nil {
		return models.Lead{}, err
	}

	updated.ID = existing.ID
	updated.CreatedAt = existing.CreatedAt

	lead, err := s.store.UpdateLead(ctx, updated)
	if err != nil {
		return models.Lead{}, err
	}

	_, _ = s.store.CreateInteraction(ctx, models.Interaction{
		LeadID:    lead.ID,
		OwnerID:   ownerID,
		Type:      models.InteractionTypeSystem,
		Notes:     "Lead atualizado.",
		CreatedBy: updatedBy,
	})

	return lead, nil
}

func (s *LeadService) DeleteLead(ctx context.Context, ownerID, leadID string) error {
	lead, err := s.store.GetLeadByID(ctx, leadID)
	if err != nil {
		return err
	}
	if lead.OwnerID != ownerID {
		return store.ErrNotFound
	}

	interactions, err := s.store.ListInteractionsByOwner(ctx, ownerID)
	if err == nil {
		for _, interaction := range interactions {
			if interaction.LeadID == leadID {
				_ = s.store.DeleteInteraction(ctx, interaction.ID)
			}
		}
	}

	return s.store.DeleteLead(ctx, leadID)
}

func (s *LeadService) GetLead(ctx context.Context, ownerID, leadID string) (models.Lead, error) {
	lead, err := s.store.GetLeadByID(ctx, leadID)
	if err != nil {
		return models.Lead{}, err
	}
	if lead.OwnerID != ownerID {
		return models.Lead{}, store.ErrNotFound
	}
	return lead, nil
}

func (s *LeadService) ListLeads(ctx context.Context, ownerID string, params models.LeadFilterParams) (models.PaginatedLeads, error) {
	allLeads, err := s.store.ListLeadsByOwner(ctx, ownerID)
	if err != nil {
		return models.PaginatedLeads{}, err
	}

	filtered := filterAndSortLeads(allLeads, params)
	page := sanitizePage(params.Page)
	pageSize := sanitizePageSize(params.PageSize)
	total := len(filtered)
	totalPages := total / pageSize
	if total%pageSize != 0 {
		totalPages++
	}

	start := (page - 1) * pageSize
	if start > total {
		start = total
	}
	end := start + pageSize
	if end > total {
		end = total
	}

	return models.PaginatedLeads{
		Items:      filtered[start:end],
		Page:       page,
		PageSize:   pageSize,
		TotalItems: total,
		TotalPages: totalPages,
	}, nil
}

func (s *LeadService) ListLeadsNoPagination(ctx context.Context, ownerID string, params models.LeadFilterParams) ([]models.Lead, error) {
	allLeads, err := s.store.ListLeadsByOwner(ctx, ownerID)
	if err != nil {
		return nil, err
	}
	return filterAndSortLeads(allLeads, params), nil
}

func (s *LeadService) Dashboard(ctx context.Context, ownerID string) (models.DashboardStats, error) {
	leads, err := s.store.ListLeadsByOwner(ctx, ownerID)
	if err != nil {
		return models.DashboardStats{}, err
	}

	stats := models.DashboardStats{
		ByStatus:   map[string]int{},
		TopSources: map[string]int{},
	}

	now := time.Now()
	startDate := now.AddDate(0, 0, -13)
	dayMap := map[string]int{}
	for i := 0; i < 14; i++ {
		day := startDate.AddDate(0, 0, i).Format("2006-01-02")
		dayMap[day] = 0
	}

	for _, lead := range leads {
		stats.TotalLeads++
		stats.ByStatus[lead.Status]++
		stats.TopSources[lead.Source]++

		switch lead.Status {
		case models.LeadStatusNew:
			stats.NewLeads++
		case models.LeadStatusQualified:
			stats.QualifiedLeads++
		case models.LeadStatusLost:
			stats.LostLeads++
		}

		captureDay := lead.CaptureDate.In(now.Location()).Format("2006-01-02")
		if _, ok := dayMap[captureDay]; ok {
			dayMap[captureDay]++
			stats.RecentLeadCount++
		}
	}

	for i := 0; i < 14; i++ {
		day := startDate.AddDate(0, 0, i).Format("2006-01-02")
		stats.CapturesByDay = append(stats.CapturesByDay, models.DailyCapture{
			Date:  day,
			Count: dayMap[day],
		})
	}

	return stats, nil
}

func (s *LeadService) ListInteractions(ctx context.Context, ownerID, leadID string) ([]models.Interaction, error) {
	_, err := s.GetLead(ctx, ownerID, leadID)
	if err != nil {
		return nil, err
	}

	allInteractions, err := s.store.ListInteractionsByOwner(ctx, ownerID)
	if err != nil {
		return nil, err
	}

	var filtered []models.Interaction
	for _, interaction := range allInteractions {
		if interaction.LeadID == leadID {
			filtered = append(filtered, interaction)
		}
	}

	sort.Slice(filtered, func(i, j int) bool {
		return filtered[i].CreatedAt.After(filtered[j].CreatedAt)
	})

	return filtered, nil
}

func (s *LeadService) AddInteraction(ctx context.Context, ownerID, leadID, createdBy string, input models.InteractionInput) (models.Interaction, error) {
	if strings.TrimSpace(input.Notes) == "" {
		return models.Interaction{}, errors.New("descricao da interacao e obrigatoria")
	}

	_, err := s.GetLead(ctx, ownerID, leadID)
	if err != nil {
		return models.Interaction{}, err
	}

	interactionType := strings.TrimSpace(strings.ToLower(input.Type))
	if interactionType == "" {
		interactionType = models.InteractionTypeNote
	}

	return s.store.CreateInteraction(ctx, models.Interaction{
		LeadID:    leadID,
		OwnerID:   ownerID,
		Type:      interactionType,
		Notes:     strings.TrimSpace(input.Notes),
		CreatedBy: createdBy,
	})
}

func (s *LeadService) DeleteInteraction(ctx context.Context, ownerID, leadID, interactionID string) error {
	interactions, err := s.ListInteractions(ctx, ownerID, leadID)
	if err != nil {
		return err
	}

	for _, interaction := range interactions {
		if interaction.ID == interactionID {
			return s.store.DeleteInteraction(ctx, interactionID)
		}
	}
	return store.ErrNotFound
}

func (s *LeadService) ImportLeads(ctx context.Context, ownerID, createdBy, fileName string, contents io.Reader) (models.ImportResult, error) {
	ext := strings.ToLower(filepath.Ext(fileName))
	rows, err := parseLeadRows(ext, contents)
	if err != nil {
		return models.ImportResult{}, err
	}

	existingLeads, err := s.store.ListLeadsByOwner(ctx, ownerID)
	if err != nil {
		return models.ImportResult{}, err
	}

	existingByEmail := map[string]models.Lead{}
	for _, lead := range existingLeads {
		key := strings.ToLower(strings.TrimSpace(lead.Email))
		if key != "" {
			existingByEmail[key] = lead
		}
	}

	result := models.ImportResult{}
	for idx, row := range rows {
		input, rowErr := rowToLeadInput(row)
		if rowErr != nil {
			result.Skipped++
			result.Errors = append(result.Errors, fmt.Sprintf("Linha %d: %v", idx+2, rowErr))
			continue
		}

		emailKey := strings.ToLower(strings.TrimSpace(input.Email))
		if existing, ok := existingByEmail[emailKey]; ok && emailKey != "" {
			if _, err := s.UpdateLead(ctx, ownerID, existing.ID, createdBy, input); err != nil {
				result.Skipped++
				result.Errors = append(result.Errors, fmt.Sprintf("Linha %d: %v", idx+2, err))
				continue
			}
			result.Updated++
			continue
		}

		lead, err := s.CreateLead(ctx, ownerID, createdBy, input)
		if err != nil {
			result.Skipped++
			result.Errors = append(result.Errors, fmt.Sprintf("Linha %d: %v", idx+2, err))
			continue
		}

		if emailKey != "" {
			existingByEmail[emailKey] = lead
		}
		result.Imported++
	}

	return result, nil
}

func (s *LeadService) ExportLeads(ctx context.Context, ownerID string, params models.LeadFilterParams, format string) ([]byte, string, string, error) {
	leads, err := s.ListLeadsNoPagination(ctx, ownerID, params)
	if err != nil {
		return nil, "", "", err
	}

	switch strings.ToLower(format) {
	case "xlsx", "excel":
		return exportXLSX(leads)
	default:
		return exportCSV(leads)
	}
}

func inputToLead(ownerID string, input models.LeadInput) (models.Lead, error) {
	fullName := strings.TrimSpace(input.FullName)
	email := strings.TrimSpace(input.Email)
	status := strings.TrimSpace(strings.ToLower(input.Status))

	if fullName == "" {
		return models.Lead{}, errors.New("nome completo e obrigatorio")
	}
	if email != "" {
		if _, err := mail.ParseAddress(email); err != nil {
			return models.Lead{}, errors.New("email do lead invalido")
		}
	}
	if status == "" {
		status = models.LeadStatusNew
	}
	if _, ok := models.ValidLeadStatuses[status]; !ok {
		return models.Lead{}, errors.New("status invalido")
	}

	captureDate, err := parseCaptureDate(input.CaptureDate)
	if err != nil {
		return models.Lead{}, err
	}

	return models.Lead{
		OwnerID:     ownerID,
		FullName:    fullName,
		Email:       email,
		Phone:       strings.TrimSpace(input.Phone),
		Company:     strings.TrimSpace(input.Company),
		Role:        strings.TrimSpace(input.Role),
		Source:      strings.TrimSpace(input.Source),
		Status:      status,
		CaptureDate: captureDate,
	}, nil
}

func parseCaptureDate(value string) (time.Time, error) {
	value = strings.TrimSpace(value)
	if value == "" {
		now := time.Now()
		return time.Date(now.Year(), now.Month(), now.Day(), 0, 0, 0, 0, now.Location()).UTC(), nil
	}

	layouts := []string{
		"2006-01-02",
		time.RFC3339,
		"02/01/2006",
		"02-01-2006",
	}
	for _, layout := range layouts {
		if parsed, err := time.Parse(layout, value); err == nil {
			return parsed.UTC(), nil
		}
	}

	return time.Time{}, errors.New("data de captura invalida")
}

func sanitizePage(page int) int {
	if page < 1 {
		return 1
	}
	return page
}

func sanitizePageSize(size int) int {
	switch {
	case size <= 0:
		return 10
	case size > 100:
		return 100
	default:
		return size
	}
}

func filterAndSortLeads(leads []models.Lead, params models.LeadFilterParams) []models.Lead {
	query := strings.ToLower(strings.TrimSpace(params.Query))
	status := strings.ToLower(strings.TrimSpace(params.Status))
	source := strings.ToLower(strings.TrimSpace(params.Source))

	var dateFrom time.Time
	var dateTo time.Time
	var hasDateFrom bool
	var hasDateTo bool

	if params.CaptureDateFrom != "" {
		if parsed, err := time.Parse("2006-01-02", params.CaptureDateFrom); err == nil {
			dateFrom = parsed
			hasDateFrom = true
		}
	}
	if params.CaptureDateTo != "" {
		if parsed, err := time.Parse("2006-01-02", params.CaptureDateTo); err == nil {
			dateTo = parsed.Add(23*time.Hour + 59*time.Minute + 59*time.Second)
			hasDateTo = true
		}
	}

	var filtered []models.Lead
	for _, lead := range leads {
		if status != "" && strings.ToLower(lead.Status) != status {
			continue
		}
		if source != "" && strings.ToLower(lead.Source) != source {
			continue
		}
		if hasDateFrom && lead.CaptureDate.Before(dateFrom) {
			continue
		}
		if hasDateTo && lead.CaptureDate.After(dateTo) {
			continue
		}
		if query != "" {
			search := strings.ToLower(strings.Join([]string{lead.FullName, lead.Email, lead.Company}, " "))
			if !strings.Contains(search, query) {
				continue
			}
		}
		filtered = append(filtered, lead)
	}

	sortBy := strings.TrimSpace(strings.ToLower(params.SortBy))
	if sortBy == "" {
		sortBy = "capture_date"
	}
	desc := strings.TrimSpace(strings.ToLower(params.SortDirection)) != "asc"

	sort.Slice(filtered, func(i, j int) bool {
		left := filtered[i]
		right := filtered[j]

		var less bool
		switch sortBy {
		case "full_name", "nome":
			less = strings.ToLower(left.FullName) < strings.ToLower(right.FullName)
		case "company", "empresa":
			less = strings.ToLower(left.Company) < strings.ToLower(right.Company)
		case "status":
			less = strings.ToLower(left.Status) < strings.ToLower(right.Status)
		case "source", "fonte":
			less = strings.ToLower(left.Source) < strings.ToLower(right.Source)
		case "created_at":
			less = left.CreatedAt.Before(right.CreatedAt)
		default:
			less = left.CaptureDate.Before(right.CaptureDate)
		}

		if desc {
			return !less
		}
		return less
	})

	return filtered
}

func parseLeadRows(ext string, contents io.Reader) ([]map[string]string, error) {
	switch ext {
	case ".xlsx", ".xlsm", ".xls":
		file, err := excelize.OpenReader(contents)
		if err != nil {
			return nil, fmt.Errorf("nao foi possivel ler o Excel: %w", err)
		}
		defer func() { _ = file.Close() }()

		sheets := file.GetSheetList()
		if len(sheets) == 0 {
			return nil, errors.New("o arquivo Excel nao possui abas")
		}

		rows, err := file.GetRows(sheets[0])
		if err != nil {
			return nil, fmt.Errorf("nao foi possivel ler as linhas do Excel: %w", err)
		}
		return rowsToMaps(rows)
	default:
		reader := csv.NewReader(contents)
		reader.FieldsPerRecord = -1
		rows, err := reader.ReadAll()
		if err != nil {
			return nil, fmt.Errorf("nao foi possivel ler o CSV: %w", err)
		}
		return rowsToMaps(rows)
	}
}

func rowsToMaps(rows [][]string) ([]map[string]string, error) {
	if len(rows) < 2 {
		return nil, errors.New("o arquivo precisa ter cabecalho e pelo menos uma linha")
	}

	headers := make([]string, len(rows[0]))
	for i, value := range rows[0] {
		headers[i] = canonicalHeader(value)
	}

	var result []map[string]string
	for _, row := range rows[1:] {
		if len(strings.TrimSpace(strings.Join(row, ""))) == 0 {
			continue
		}
		entry := make(map[string]string, len(headers))
		for i, header := range headers {
			if i < len(row) {
				entry[header] = strings.TrimSpace(row[i])
			}
		}
		result = append(result, entry)
	}

	return result, nil
}

func rowToLeadInput(row map[string]string) (models.LeadInput, error) {
	input := models.LeadInput{
		FullName:    firstNonEmpty(row["full_name"], row["nome"], row["nome_completo"]),
		Email:       row["email"],
		Phone:       firstNonEmpty(row["phone"], row["telefone"]),
		Company:     firstNonEmpty(row["company"], row["empresa"]),
		Role:        firstNonEmpty(row["role"], row["cargo"]),
		Source:      firstNonEmpty(row["source"], row["fonte"]),
		Status:      row["status"],
		CaptureDate: firstNonEmpty(row["capture_date"], row["data_captura"], row["data"]),
	}

	if strings.TrimSpace(input.FullName) == "" {
		return models.LeadInput{}, errors.New("nome completo ausente")
	}
	return input, nil
}

func canonicalHeader(value string) string {
	value = strings.TrimSpace(strings.ToLower(value))
	replacer := strings.NewReplacer(" ", "_", "-", "_", "ã", "a", "á", "a", "é", "e", "í", "i", "ó", "o", "ú", "u", "ç", "c")
	value = replacer.Replace(value)

	mapping := map[string]string{
		"nome_completo":   "full_name",
		"nome":            "nome",
		"telefone":        "phone",
		"empresa":         "company",
		"cargo":           "role",
		"fonte":           "source",
		"data":            "capture_date",
		"data_de_captura": "capture_date",
		"data_captura":    "capture_date",
	}
	if mapped, ok := mapping[value]; ok {
		return mapped
	}
	return value
}

func firstNonEmpty(values ...string) string {
	for _, value := range values {
		if strings.TrimSpace(value) != "" {
			return strings.TrimSpace(value)
		}
	}
	return ""
}

func exportCSV(leads []models.Lead) ([]byte, string, string, error) {
	buffer := &bytes.Buffer{}
	writer := csv.NewWriter(buffer)

	header := []string{"nome_completo", "email", "telefone", "empresa", "cargo", "fonte", "status", "data_captura"}
	if err := writer.Write(header); err != nil {
		return nil, "", "", err
	}

	for _, lead := range leads {
		row := []string{
			lead.FullName,
			lead.Email,
			lead.Phone,
			lead.Company,
			lead.Role,
			lead.Source,
			lead.Status,
			lead.CaptureDate.Format("2006-01-02"),
		}
		if err := writer.Write(row); err != nil {
			return nil, "", "", err
		}
	}

	writer.Flush()
	if err := writer.Error(); err != nil {
		return nil, "", "", err
	}

	return buffer.Bytes(), "text/csv; charset=utf-8", fmt.Sprintf("leads_%s.csv", time.Now().Format("20060102_150405")), nil
}

func exportXLSX(leads []models.Lead) ([]byte, string, string, error) {
	file := excelize.NewFile()
	sheet := file.GetSheetName(0)

	header := []string{"Nome completo", "E-mail", "Telefone", "Empresa", "Cargo", "Fonte", "Status", "Data de captura"}
	for i, value := range header {
		cell, _ := excelize.CoordinatesToCellName(i+1, 1)
		if err := file.SetCellValue(sheet, cell, value); err != nil {
			return nil, "", "", err
		}
	}

	for idx, lead := range leads {
		row := idx + 2
		values := []string{
			lead.FullName,
			lead.Email,
			lead.Phone,
			lead.Company,
			lead.Role,
			lead.Source,
			lead.Status,
			lead.CaptureDate.Format("2006-01-02"),
		}
		for col, value := range values {
			cell, _ := excelize.CoordinatesToCellName(col+1, row)
			if err := file.SetCellValue(sheet, cell, value); err != nil {
				return nil, "", "", err
			}
		}
	}

	buffer, err := file.WriteToBuffer()
	if err != nil {
		return nil, "", "", err
	}

	return buffer.Bytes(), "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", fmt.Sprintf("leads_%s.xlsx", time.Now().Format("20060102_150405")), nil
}
