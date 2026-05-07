package service

import (
	"bytes"
	"context"
	"encoding/csv"
	"errors"
	"fmt"
	"mime/multipart"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/google/uuid"
	"github.com/xuri/excelize/v2"

	"leadmanager/internal/models"
	"leadmanager/internal/repository"
)

type LeadService struct {
	repo *repository.FirestoreRepository
}

func NewLeadService(repo *repository.FirestoreRepository) *LeadService {
	return &LeadService{repo: repo}
}

func (s *LeadService) CreateLead(ctx context.Context, userID string, lead models.Lead) (models.Lead, error) {
	prepared, err := prepareLead(lead, true)
	if err != nil {
		return models.Lead{}, err
	}

	prepared.ID = uuid.NewString()
	now := time.Now().UTC()
	prepared.CreatedAt = now
	prepared.UpdatedAt = now

	if err := s.repo.CreateLead(ctx, userID, prepared); err != nil {
		return models.Lead{}, err
	}
	return prepared, nil
}

func (s *LeadService) UpdateLead(ctx context.Context, userID, leadID string, lead models.Lead) (models.Lead, error) {
	current, err := s.repo.GetLeadByID(ctx, userID, leadID)
	if err != nil {
		return models.Lead{}, err
	}

	prepared, err := prepareLead(lead, false)
	if err != nil {
		return models.Lead{}, err
	}

	prepared.ID = current.ID
	prepared.CreatedAt = current.CreatedAt
	prepared.UpdatedAt = time.Now().UTC()

	if err := s.repo.UpdateLead(ctx, userID, prepared); err != nil {
		return models.Lead{}, err
	}
	return prepared, nil
}

func (s *LeadService) DeleteLead(ctx context.Context, userID, leadID string) error {
	return s.repo.DeleteLead(ctx, userID, leadID)
}

func (s *LeadService) GetLeadDetail(ctx context.Context, userID, leadID string) (models.LeadDetail, error) {
	lead, err := s.repo.GetLeadByID(ctx, userID, leadID)
	if err != nil {
		return models.LeadDetail{}, err
	}

	interactions, err := s.repo.ListInteractions(ctx, userID, leadID)
	if err != nil {
		return models.LeadDetail{}, err
	}

	sort.Slice(interactions, func(i, j int) bool {
		return interactions[i].CreatedAt.After(interactions[j].CreatedAt)
	})

	return models.LeadDetail{Lead: lead, Interactions: interactions}, nil
}

func (s *LeadService) ListLeads(ctx context.Context, userID string, filters models.LeadFilters) (models.PaginatedLeads, error) {
	leads, err := s.repo.ListLeads(ctx, userID)
	if err != nil {
		return models.PaginatedLeads{}, err
	}

	filtered := filterLeads(leads, filters)
	sortLeads(filtered, filters.SortBy, filters.SortDir)

	page := normalizePage(filters.Page)
	pageSize := normalizePageSize(filters.PageSize)
	totalItems := len(filtered)
	totalPages := 0
	if totalItems > 0 {
		totalPages = (totalItems + pageSize - 1) / pageSize
	}

	start := (page - 1) * pageSize
	if start > totalItems {
		start = totalItems
	}
	end := start + pageSize
	if end > totalItems {
		end = totalItems
	}

	items := filtered[start:end]
	return models.PaginatedLeads{
		Items:      items,
		Page:       page,
		PageSize:   pageSize,
		TotalItems: totalItems,
		TotalPages: totalPages,
	}, nil
}

func (s *LeadService) GetDashboardSummary(ctx context.Context, userID string) (models.DashboardSummary, error) {
	leads, err := s.repo.ListLeads(ctx, userID)
	if err != nil {
		return models.DashboardSummary{}, err
	}

	statusCounts := map[string]int{"new": 0, "contacted": 0, "qualified": 0, "lost": 0}
	sourceCounts := map[string]int{}
	trendMap := map[string]int{}
	now := time.Now().UTC()

	for _, lead := range leads {
		statusCounts[lead.Status]++
		sourceCounts[lead.Source]++

		if lead.CaptureDate.After(now.AddDate(0, 0, -13)) {
			label := lead.CaptureDate.UTC().Format("2006-01-02")
			trendMap[label]++
		}
	}

	points := make([]models.TrendPoint, 0, 14)
	for i := 13; i >= 0; i-- {
		day := now.AddDate(0, 0, -i).UTC()
		label := day.Format("2006-01-02")
		points = append(points, models.TrendPoint{
			Label: label,
			Count: trendMap[label],
		})
	}

	return models.DashboardSummary{
		TotalLeads:       len(leads),
		StatusCounts:     statusCounts,
		SourceCounts:     sourceCounts,
		CapturesOverTime: points,
	}, nil
}

func (s *LeadService) AddInteraction(ctx context.Context, userID, leadID, author string, interaction models.LeadInteraction) (models.LeadInteraction, error) {
	interaction.Type = strings.TrimSpace(interaction.Type)
	interaction.Note = strings.TrimSpace(interaction.Note)
	if interaction.Type == "" || interaction.Note == "" {
		return models.LeadInteraction{}, errors.New("interaction type and note are required")
	}

	item := models.LeadInteraction{
		ID:        uuid.NewString(),
		LeadID:    leadID,
		Type:      interaction.Type,
		Note:      interaction.Note,
		CreatedAt: time.Now().UTC(),
		CreatedBy: author,
	}

	if err := s.repo.AddInteraction(ctx, userID, leadID, item); err != nil {
		return models.LeadInteraction{}, err
	}
	return item, nil
}

func (s *LeadService) ImportLeads(ctx context.Context, userID string, fileHeader *multipart.FileHeader, file multipart.File) (models.ImportResult, error) {
	ext := strings.ToLower(filepath.Ext(fileHeader.Filename))
	var rows [][]string
	var err error

	switch ext {
	case ".csv":
		rows, err = readCSVRows(file)
	case ".xlsx":
		rows, err = readExcelRows(file)
	default:
		return models.ImportResult{}, errors.New("unsupported file type; use CSV or XLSX")
	}
	if err != nil {
		return models.ImportResult{}, err
	}
	if len(rows) < 2 {
		return models.ImportResult{}, errors.New("the file must include a header row and at least one data row")
	}

	indexes := headerIndexes(rows[0])
	result := models.ImportResult{Errors: []string{}}
	for rowIndex := 1; rowIndex < len(rows); rowIndex++ {
		row := rows[rowIndex]
		lead := models.Lead{
			FullName:    valueAt(row, indexes, "full name", "name"),
			Email:       valueAt(row, indexes, "email"),
			Phone:       valueAt(row, indexes, "phone"),
			Company:     valueAt(row, indexes, "company"),
			Position:    valueAt(row, indexes, "position"),
			Source:      valueAt(row, indexes, "source"),
			Status:      valueAt(row, indexes, "status"),
			CaptureDate: mustParseDate(valueAt(row, indexes, "capture date", "capture_date", "captured at")),
		}

		if _, err := s.CreateLead(ctx, userID, lead); err != nil {
			result.Failed++
			result.Errors = append(result.Errors, fmt.Sprintf("row %d: %v", rowIndex+1, err))
			continue
		}

		result.Imported++
	}

	return result, nil
}

func (s *LeadService) ExportLeads(ctx context.Context, userID string, filters models.LeadFilters, format string) ([]byte, string, string, error) {
	leads, err := s.repo.ListLeads(ctx, userID)
	if err != nil {
		return nil, "", "", err
	}

	filtered := filterLeads(leads, filters)
	sortLeads(filtered, filters.SortBy, filters.SortDir)

	switch strings.ToLower(format) {
	case "", "csv":
		data, err := exportCSV(filtered)
		return data, "text/csv", "leads.csv", err
	case "xlsx":
		data, err := exportXLSX(filtered)
		return data, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "leads.xlsx", err
	default:
		return nil, "", "", errors.New("unsupported export format")
	}
}

func prepareLead(lead models.Lead, applyDefaults bool) (models.Lead, error) {
	lead.FullName = strings.TrimSpace(lead.FullName)
	lead.Email = strings.ToLower(strings.TrimSpace(lead.Email))
	lead.Phone = strings.TrimSpace(lead.Phone)
	lead.Company = strings.TrimSpace(lead.Company)
	lead.Position = strings.TrimSpace(lead.Position)
	lead.Source = strings.TrimSpace(lead.Source)
	lead.Status = strings.ToLower(strings.TrimSpace(lead.Status))

	if lead.FullName == "" || lead.Email == "" || lead.Company == "" || lead.Source == "" {
		return models.Lead{}, errors.New("full name, email, company and source are required")
	}
	if _, ok := models.AllowedLeadStatuses[lead.Status]; !ok {
		if applyDefaults && lead.Status == "" {
			lead.Status = "new"
		} else if lead.Status == "" {
			return models.Lead{}, errors.New("status is required")
		} else {
			return models.Lead{}, errors.New("status must be one of new, contacted, qualified or lost")
		}
	}
	if lead.CaptureDate.IsZero() {
		lead.CaptureDate = time.Now().UTC()
	}

	return lead, nil
}

func filterLeads(leads []models.Lead, filters models.LeadFilters) []models.Lead {
	status := strings.ToLower(strings.TrimSpace(filters.Status))
	source := strings.ToLower(strings.TrimSpace(filters.Source))
	query := strings.ToLower(strings.TrimSpace(filters.Query))

	filtered := make([]models.Lead, 0, len(leads))
	for _, lead := range leads {
		if status != "" && strings.ToLower(lead.Status) != status {
			continue
		}
		if source != "" && strings.ToLower(lead.Source) != source {
			continue
		}
		if filters.DateFrom != nil && lead.CaptureDate.Before(startOfDay(*filters.DateFrom)) {
			continue
		}
		if filters.DateTo != nil && lead.CaptureDate.After(endOfDay(*filters.DateTo)) {
			continue
		}
		if query != "" {
			haystack := strings.ToLower(strings.Join([]string{lead.FullName, lead.Email, lead.Company}, " "))
			if !strings.Contains(haystack, query) {
				continue
			}
		}
		filtered = append(filtered, lead)
	}
	return filtered
}

func sortLeads(leads []models.Lead, sortBy, sortDir string) {
	sortBy = strings.TrimSpace(sortBy)
	if sortBy == "" {
		sortBy = "captureDate"
	}

	desc := !strings.EqualFold(sortDir, "asc")
	sort.SliceStable(leads, func(i, j int) bool {
		left := leads[i]
		right := leads[j]

		var comparison int
		switch sortBy {
		case "fullName":
			comparison = strings.Compare(strings.ToLower(left.FullName), strings.ToLower(right.FullName))
		case "company":
			comparison = strings.Compare(strings.ToLower(left.Company), strings.ToLower(right.Company))
		case "status":
			comparison = strings.Compare(strings.ToLower(left.Status), strings.ToLower(right.Status))
		case "source":
			comparison = strings.Compare(strings.ToLower(left.Source), strings.ToLower(right.Source))
		case "createdAt":
			comparison = compareTimes(left.CreatedAt, right.CreatedAt)
		case "updatedAt":
			comparison = compareTimes(left.UpdatedAt, right.UpdatedAt)
		default:
			comparison = compareTimes(left.CaptureDate, right.CaptureDate)
		}

		if desc {
			return comparison > 0
		}
		return comparison < 0
	})
}

func normalizePage(page int) int {
	if page < 1 {
		return 1
	}
	return page
}

func normalizePageSize(pageSize int) int {
	if pageSize < 1 {
		return 10
	}
	if pageSize > 100 {
		return 100
	}
	return pageSize
}

func readCSVRows(file multipart.File) ([][]string, error) {
	reader := csv.NewReader(file)
	reader.TrimLeadingSpace = true
	return reader.ReadAll()
}

func readExcelRows(file multipart.File) ([][]string, error) {
	xlsx, err := excelize.OpenReader(file)
	if err != nil {
		return nil, err
	}

	sheetName := xlsx.GetSheetName(0)
	if sheetName == "" {
		return nil, errors.New("xlsx file does not contain any sheets")
	}
	return xlsx.GetRows(sheetName)
}

func exportCSV(leads []models.Lead) ([]byte, error) {
	buffer := &bytes.Buffer{}
	writer := csv.NewWriter(buffer)

	headers := []string{"Full Name", "Email", "Phone", "Company", "Position", "Source", "Status", "Capture Date"}
	if err := writer.Write(headers); err != nil {
		return nil, err
	}

	for _, lead := range leads {
		record := []string{
			lead.FullName,
			lead.Email,
			lead.Phone,
			lead.Company,
			lead.Position,
			lead.Source,
			lead.Status,
			lead.CaptureDate.Format(time.RFC3339),
		}
		if err := writer.Write(record); err != nil {
			return nil, err
		}
	}

	writer.Flush()
	return buffer.Bytes(), writer.Error()
}

func exportXLSX(leads []models.Lead) ([]byte, error) {
	file := excelize.NewFile()
	sheet := "Leads"
	file.SetSheetName("Sheet1", sheet)

	headers := []string{"Full Name", "Email", "Phone", "Company", "Position", "Source", "Status", "Capture Date"}
	for col, header := range headers {
		cell, _ := excelize.CoordinatesToCellName(col+1, 1)
		if err := file.SetCellValue(sheet, cell, header); err != nil {
			return nil, err
		}
	}

	for rowIndex, lead := range leads {
		row := rowIndex + 2
		values := []interface{}{
			lead.FullName,
			lead.Email,
			lead.Phone,
			lead.Company,
			lead.Position,
			lead.Source,
			lead.Status,
			lead.CaptureDate.Format(time.RFC3339),
		}
		for col, value := range values {
			cell, _ := excelize.CoordinatesToCellName(col+1, row)
			if err := file.SetCellValue(sheet, cell, value); err != nil {
				return nil, err
			}
		}
	}

	buffer, err := file.WriteToBuffer()
	if err != nil {
		return nil, err
	}
	return buffer.Bytes(), nil
}

func headerIndexes(headers []string) map[string]int {
	indexes := make(map[string]int, len(headers))
	for idx, header := range headers {
		key := strings.ToLower(strings.TrimSpace(header))
		indexes[key] = idx
	}
	return indexes
}

func valueAt(row []string, indexes map[string]int, keys ...string) string {
	for _, key := range keys {
		if idx, ok := indexes[strings.ToLower(key)]; ok && idx < len(row) {
			return strings.TrimSpace(row[idx])
		}
	}
	return ""
}

func mustParseDate(value string) time.Time {
	if strings.TrimSpace(value) == "" {
		return time.Time{}
	}

	layouts := []string{
		time.RFC3339,
		"2006-01-02",
		"02/01/2006",
		"01/02/2006",
	}
	for _, layout := range layouts {
		if parsed, err := time.Parse(layout, strings.TrimSpace(value)); err == nil {
			return parsed.UTC()
		}
	}

	if excelFloat, err := strconv.ParseFloat(strings.TrimSpace(value), 64); err == nil {
		if parsed, err := excelize.ExcelDateToTime(excelFloat, false); err == nil {
			return parsed.UTC()
		}
	}

	return time.Time{}
}

func startOfDay(value time.Time) time.Time {
	return time.Date(value.Year(), value.Month(), value.Day(), 0, 0, 0, 0, time.UTC)
}

func endOfDay(value time.Time) time.Time {
	return time.Date(value.Year(), value.Month(), value.Day(), 23, 59, 59, 999999999, time.UTC)
}

func compareTimes(left, right time.Time) int {
	if left.Equal(right) {
		return 0
	}
	if left.Before(right) {
		return -1
	}
	return 1
}
