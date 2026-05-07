package csvutil

import (
	"encoding/csv"
	"fmt"
	"io"
	"strings"
	"time"

	"lead-manager/internal/models"

	"github.com/xuri/excelize/v2"
)

var csvHeaders = []string{"Full Name", "Email", "Phone", "Company", "Position", "Source", "Status", "Capture Date", "Notes"}

func ExportCSV(w io.Writer, leads []models.Lead) error {
	writer := csv.NewWriter(w)
	defer writer.Flush()

	if err := writer.Write(csvHeaders); err != nil {
		return err
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
			lead.CaptureDate.Format("2006-01-02"),
			lead.Notes,
		}
		if err := writer.Write(record); err != nil {
			return err
		}
	}
	return nil
}

func ImportCSV(r io.Reader) ([]models.Lead, error) {
	reader := csv.NewReader(r)
	reader.TrimLeadingSpace = true

	header, err := reader.Read()
	if err != nil {
		return nil, fmt.Errorf("failed to read header: %w", err)
	}

	colMap := map[string]int{}
	for i, h := range header {
		colMap[strings.ToLower(strings.TrimSpace(h))] = i
	}

	var leads []models.Lead
	lineNum := 1
	for {
		record, err := reader.Read()
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, fmt.Errorf("error reading line %d: %w", lineNum+1, err)
		}
		lineNum++

		lead := models.Lead{
			Status:      "new",
			CaptureDate: time.Now(),
			CreatedAt:   time.Now(),
			UpdatedAt:   time.Now(),
		}

		if idx, ok := colMap["full name"]; ok && idx < len(record) {
			lead.FullName = strings.TrimSpace(record[idx])
		}
		if idx, ok := colMap["email"]; ok && idx < len(record) {
			lead.Email = strings.TrimSpace(record[idx])
		}
		if idx, ok := colMap["phone"]; ok && idx < len(record) {
			lead.Phone = strings.TrimSpace(record[idx])
		}
		if idx, ok := colMap["company"]; ok && idx < len(record) {
			lead.Company = strings.TrimSpace(record[idx])
		}
		if idx, ok := colMap["position"]; ok && idx < len(record) {
			lead.Position = strings.TrimSpace(record[idx])
		}
		if idx, ok := colMap["source"]; ok && idx < len(record) {
			lead.Source = strings.TrimSpace(record[idx])
		}
		if idx, ok := colMap["status"]; ok && idx < len(record) {
			s := strings.TrimSpace(strings.ToLower(record[idx]))
			if s != "" {
				lead.Status = s
			}
		}
		if idx, ok := colMap["capture date"]; ok && idx < len(record) {
			if t, err := time.Parse("2006-01-02", strings.TrimSpace(record[idx])); err == nil {
				lead.CaptureDate = t
			}
		}
		if idx, ok := colMap["notes"]; ok && idx < len(record) {
			lead.Notes = strings.TrimSpace(record[idx])
		}

		if lead.FullName == "" || lead.Email == "" {
			continue
		}

		leads = append(leads, lead)
	}

	return leads, nil
}

func ExportExcel(leads []models.Lead) (*excelize.File, error) {
	f := excelize.NewFile()
	sheet := "Leads"
	idx, err := f.NewSheet(sheet)
	if err != nil {
		return nil, err
	}
	f.SetActiveSheet(idx)
	f.DeleteSheet("Sheet1")

	headerStyle, _ := f.NewStyle(&excelize.Style{
		Font:      &excelize.Font{Bold: true, Color: "FFFFFF", Size: 11},
		Fill:      excelize.Fill{Type: "pattern", Color: []string{"4472C4"}, Pattern: 1},
		Alignment: &excelize.Alignment{Horizontal: "center"},
	})

	for i, h := range csvHeaders {
		cell, _ := excelize.CoordinatesToCellName(i+1, 1)
		f.SetCellValue(sheet, cell, h)
		f.SetCellStyle(sheet, cell, cell, headerStyle)
	}

	for i, lead := range leads {
		row := i + 2
		f.SetCellValue(sheet, cellName(1, row), lead.FullName)
		f.SetCellValue(sheet, cellName(2, row), lead.Email)
		f.SetCellValue(sheet, cellName(3, row), lead.Phone)
		f.SetCellValue(sheet, cellName(4, row), lead.Company)
		f.SetCellValue(sheet, cellName(5, row), lead.Position)
		f.SetCellValue(sheet, cellName(6, row), lead.Source)
		f.SetCellValue(sheet, cellName(7, row), lead.Status)
		f.SetCellValue(sheet, cellName(8, row), lead.CaptureDate.Format("2006-01-02"))
		f.SetCellValue(sheet, cellName(9, row), lead.Notes)
	}

	for i := range csvHeaders {
		col, _ := excelize.ColumnNumberToName(i + 1)
		f.SetColWidth(sheet, col, col, 18)
	}

	return f, nil
}

func ImportExcel(r io.Reader) ([]models.Lead, error) {
	f, err := excelize.OpenReader(r)
	if err != nil {
		return nil, fmt.Errorf("failed to open Excel file: %w", err)
	}
	defer f.Close()

	sheet := f.GetSheetName(0)
	rows, err := f.GetRows(sheet)
	if err != nil {
		return nil, fmt.Errorf("failed to read rows: %w", err)
	}

	if len(rows) < 2 {
		return nil, fmt.Errorf("file must have a header row and at least one data row")
	}

	colMap := map[string]int{}
	for i, h := range rows[0] {
		colMap[strings.ToLower(strings.TrimSpace(h))] = i
	}

	var leads []models.Lead
	for _, row := range rows[1:] {
		lead := models.Lead{
			Status:      "new",
			CaptureDate: time.Now(),
			CreatedAt:   time.Now(),
			UpdatedAt:   time.Now(),
		}

		getVal := func(key string) string {
			if idx, ok := colMap[key]; ok && idx < len(row) {
				return strings.TrimSpace(row[idx])
			}
			return ""
		}

		lead.FullName = getVal("full name")
		lead.Email = getVal("email")
		lead.Phone = getVal("phone")
		lead.Company = getVal("company")
		lead.Position = getVal("position")
		lead.Source = getVal("source")
		if s := getVal("status"); s != "" {
			lead.Status = strings.ToLower(s)
		}
		if d := getVal("capture date"); d != "" {
			if t, err := time.Parse("2006-01-02", d); err == nil {
				lead.CaptureDate = t
			}
		}
		lead.Notes = getVal("notes")

		if lead.FullName == "" || lead.Email == "" {
			continue
		}

		leads = append(leads, lead)
	}

	return leads, nil
}

func cellName(col, row int) string {
	name, _ := excelize.CoordinatesToCellName(col, row)
	return name
}
