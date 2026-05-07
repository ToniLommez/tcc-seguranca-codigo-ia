package handlers

import (
	"bytes"
	"context"
	"fmt"
	"net/http"
	"strings"
	"time"

	"lead-manager/internal/csvutil"
	fb "lead-manager/internal/firebase"
	"lead-manager/internal/middleware"
	"lead-manager/internal/models"

	"google.golang.org/api/iterator"
)

type ImportExportHandler struct{}

func NewImportExportHandler() *ImportExportHandler {
	return &ImportExportHandler{}
}

func (h *ImportExportHandler) ExportCSV(w http.ResponseWriter, r *http.Request) {
	leads, err := fetchAllLeads(r)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": err.Error()})
		return
	}

	w.Header().Set("Content-Type", "text/csv")
	w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=leads_%s.csv", time.Now().Format("20060102_150405")))

	if err := csvutil.ExportCSV(w, leads); err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to export CSV"})
	}
}

func (h *ImportExportHandler) ExportExcel(w http.ResponseWriter, r *http.Request) {
	leads, err := fetchAllLeads(r)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": err.Error()})
		return
	}

	f, err := csvutil.ExportExcel(leads)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to export Excel"})
		return
	}
	defer f.Close()

	w.Header().Set("Content-Type", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet")
	w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=leads_%s.xlsx", time.Now().Format("20060102_150405")))

	if err := f.Write(w); err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "failed to write Excel file"})
	}
}

func (h *ImportExportHandler) Import(w http.ResponseWriter, r *http.Request) {
	userID := middleware.GetUserID(r.Context())

	if err := r.ParseMultipartForm(10 << 20); err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "file too large or invalid form"})
		return
	}

	file, header, err := r.FormFile("file")
	if err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "file is required"})
		return
	}
	defer file.Close()

	var leads []models.Lead
	filename := strings.ToLower(header.Filename)

	if strings.HasSuffix(filename, ".csv") {
		leads, err = csvutil.ImportCSV(file)
	} else if strings.HasSuffix(filename, ".xlsx") || strings.HasSuffix(filename, ".xls") {
		var buf bytes.Buffer
		if _, err2 := buf.ReadFrom(file); err2 != nil {
			writeJSON(w, http.StatusBadRequest, map[string]string{"error": "failed to read file"})
			return
		}
		leads, err = csvutil.ImportExcel(bytes.NewReader(buf.Bytes()))
	} else {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "unsupported file format. Use .csv or .xlsx"})
		return
	}

	if err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": fmt.Sprintf("failed to parse file: %v", err)})
		return
	}

	ctx := context.Background()
	client := fb.GetClient()

	imported := 0
	for _, lead := range leads {
		lead.UserID = userID
		lead.CreatedAt = time.Now()
		lead.UpdatedAt = time.Now()

		_, _, err := client.Collection(fb.LeadsCollection).Add(ctx, map[string]interface{}{
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
		})
		if err == nil {
			imported++
		}
	}

	writeJSON(w, http.StatusOK, map[string]interface{}{
		"message":  fmt.Sprintf("successfully imported %d leads", imported),
		"imported": imported,
		"total":    len(leads),
	})
}

func fetchAllLeads(r *http.Request) ([]models.Lead, error) {
	userID := middleware.GetUserID(r.Context())
	ctx := context.Background()
	client := fb.GetClient()

	iter := client.Collection(fb.LeadsCollection).Where("user_id", "==", userID).Documents(ctx)
	defer iter.Stop()

	var leads []models.Lead
	for {
		doc, err := iter.Next()
		if err == iterator.Done {
			break
		}
		if err != nil {
			return nil, fmt.Errorf("failed to fetch leads")
		}
		var lead models.Lead
		if err := doc.DataTo(&lead); err != nil {
			continue
		}
		lead.ID = doc.Ref.ID
		leads = append(leads, lead)
	}
	return leads, nil
}
