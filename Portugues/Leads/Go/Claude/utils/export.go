package utils

import (
	"encoding/csv"
	"fmt"
	"io"
	"lead-manager/models"
	"strconv"
	"strings"
	"time"

	"github.com/xuri/excelize/v2"
)

// LeadsToCSV converte uma slice de leads para formato CSV
func LeadsToCSV(leads []models.Lead) (string, error) {
	var sb strings.Builder
	writer := csv.NewWriter(&sb)

	// Cabeçalho
	header := []string{
		"Nome Completo", "E-mail", "Telefone", "Empresa", "Cargo",
		"Fonte", "Status", "Notas", "Data de Captura", "Criado em",
	}
	if err := writer.Write(header); err != nil {
		return "", fmt.Errorf("erro ao escrever cabeçalho: %v", err)
	}

	// Dados
	for _, lead := range leads {
		row := []string{
			lead.FullName,
			lead.Email,
			lead.Phone,
			lead.Company,
			lead.Role,
			lead.Source,
			lead.Status,
			lead.Notes,
			lead.CapturedAt.Format("2006-01-02"),
			lead.CreatedAt.Format("2006-01-02 15:04"),
		}
		if err := writer.Write(row); err != nil {
			return "", fmt.Errorf("erro ao escrever linha: %v", err)
		}
	}

	writer.Flush()
	if err := writer.Error(); err != nil {
		return "", err
	}

	return sb.String(), nil
}

// LeadsToExcel converte uma slice de leads para arquivo Excel (.xlsx)
func LeadsToExcel(leads []models.Lead) (*excelize.File, error) {
	f := excelize.NewFile()
	sheet := "Leads"
	f.SetSheetName("Sheet1", sheet)

	// Estilo do cabeçalho
	headerStyle, err := f.NewStyle(&excelize.Style{
		Font: &excelize.Font{Bold: true, Color: "FFFFFF", Size: 11},
		Fill: excelize.Fill{Type: "pattern", Color: []string{"1E3A5F"}, Pattern: 1},
		Alignment: &excelize.Alignment{Horizontal: "center", Vertical: "center"},
		Border: []excelize.Border{
			{Type: "bottom", Color: "2563EB", Style: 2},
		},
	})
	if err != nil {
		return nil, err
	}

	// Cabeçalhos
	headers := []string{
		"Nome Completo", "E-mail", "Telefone", "Empresa", "Cargo",
		"Fonte", "Status", "Notas", "Data de Captura", "Criado em",
	}
	cols := []string{"A", "B", "C", "D", "E", "F", "G", "H", "I", "J"}

	for i, h := range headers {
		cell := cols[i] + "1"
		f.SetCellValue(sheet, cell, h)
		f.SetCellStyle(sheet, cell, cell, headerStyle)
	}

	// Largura das colunas
	widths := map[string]float64{
		"A": 30, "B": 28, "C": 18, "D": 25, "E": 20,
		"F": 15, "G": 15, "H": 35, "I": 18, "J": 18,
	}
	for col, width := range widths {
		f.SetColWidth(sheet, col, col, width)
	}

	// Dados
	for i, lead := range leads {
		row := strconv.Itoa(i + 2)
		values := []interface{}{
			lead.FullName,
			lead.Email,
			lead.Phone,
			lead.Company,
			lead.Role,
			StatusLabel(lead.Source),
			StatusLabel(lead.Status),
			lead.Notes,
			lead.CapturedAt.Format("02/01/2006"),
			lead.CreatedAt.Format("02/01/2006 15:04"),
		}
		for j, v := range values {
			f.SetCellValue(sheet, cols[j]+row, v)
		}
	}

	return f, nil
}

// ParseCSVToLeads analisa um arquivo CSV e retorna leads
func ParseCSVToLeads(reader io.Reader, userID string) ([]models.Lead, []string, error) {
	csvReader := csv.NewReader(reader)
	csvReader.TrimLeadingSpace = true

	records, err := csvReader.ReadAll()
	if err != nil {
		return nil, nil, fmt.Errorf("erro ao ler CSV: %v", err)
	}

	if len(records) < 2 {
		return nil, nil, fmt.Errorf("CSV vazio ou sem dados (somente cabeçalho)")
	}

	var leads []models.Lead
	var errors []string

	for i, record := range records[1:] { // Pula cabeçalho
		lineNum := i + 2
		if len(record) < 7 {
			errors = append(errors, fmt.Sprintf("Linha %d: colunas insuficientes (mínimo 7)", lineNum))
			continue
		}

		fullName := strings.TrimSpace(record[0])
		if fullName == "" {
			errors = append(errors, fmt.Sprintf("Linha %d: nome completo é obrigatório", lineNum))
			continue
		}

		capturedAt := time.Now()
		if len(record) >= 9 && record[8] != "" {
			for _, layout := range []string{"2006-01-02", "02/01/2006", "01/02/2006"} {
				if t, err := time.Parse(layout, strings.TrimSpace(record[8])); err == nil {
					capturedAt = t
					break
				}
			}
		}

		status := NormalizeStatus(strings.TrimSpace(record[6]))
		source := NormalizeSource(strings.TrimSpace(record[5]))

		lead := models.Lead{
			UserID:     userID,
			FullName:   fullName,
			Email:      strings.TrimSpace(record[1]),
			Phone:      strings.TrimSpace(record[2]),
			Company:    strings.TrimSpace(record[3]),
			Role:       strings.TrimSpace(record[4]),
			Source:     source,
			Status:     status,
			Notes:      func() string { if len(record) >= 8 { return strings.TrimSpace(record[7]) }; return "" }(),
			CapturedAt: capturedAt,
			CreatedAt:  time.Now(),
			UpdatedAt:  time.Now(),
		}

		leads = append(leads, lead)
	}

	return leads, errors, nil
}

// NormalizeStatus normaliza o valor do status
func NormalizeStatus(s string) string {
	s = strings.ToLower(strings.TrimSpace(s))
	switch s {
	case "novo", "new":
		return models.StatusNovo
	case "em contato", "em_contato", "em-contato", "contato":
		return models.StatusEmContato
	case "qualificado", "qualified":
		return models.StatusQualificado
	case "perdido", "lost":
		return models.StatusPerdido
	}
	return models.StatusNovo
}

// NormalizeSource normaliza o valor da fonte
func NormalizeSource(s string) string {
	s = strings.ToLower(strings.TrimSpace(s))
	switch s {
	case "indicacao", "indicação", "referral":
		return models.SourceIndicacao
	case "site", "website":
		return models.SourceSite
	case "evento", "event":
		return models.SourceEvento
	case "linkedin":
		return models.SourceLinkedIn
	case "email", "e-mail":
		return models.SourceEmail
	}
	return models.SourceOutro
}

// StatusLabel retorna o label legível de um status/fonte
func StatusLabel(key string) string {
	labels := map[string]string{
		"novo":        "Novo",
		"em_contato":  "Em Contato",
		"qualificado": "Qualificado",
		"perdido":     "Perdido",
		"indicacao":   "Indicação",
		"site":        "Site",
		"evento":      "Evento",
		"linkedin":    "LinkedIn",
		"email":       "E-mail",
		"outro":       "Outro",
	}
	if label, ok := labels[key]; ok {
		return label
	}
	return key
}
