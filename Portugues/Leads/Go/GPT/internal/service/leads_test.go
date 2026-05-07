package service

import (
	"strings"
	"testing"
	"time"

	"leadmanagergpt/internal/models"
)

func TestFilterAndSortLeads(t *testing.T) {
	leads := []models.Lead{
		{ID: "1", FullName: "Carlos Lima", Email: "carlos@example.com", Company: "Alpha", Status: models.LeadStatusNew, Source: "site", CaptureDate: mustDate("2026-03-15")},
		{ID: "2", FullName: "Ana Souza", Email: "ana@example.com", Company: "Beta", Status: models.LeadStatusQualified, Source: "evento", CaptureDate: mustDate("2026-03-10")},
		{ID: "3", FullName: "Bruno Dias", Email: "bruno@example.com", Company: "Gamma", Status: models.LeadStatusNew, Source: "site", CaptureDate: mustDate("2026-03-12")},
	}

	filtered := filterAndSortLeads(leads, models.LeadFilterParams{
		Query:         "beta",
		Status:        models.LeadStatusQualified,
		SortBy:        "full_name",
		SortDirection: "asc",
	})

	if len(filtered) != 1 {
		t.Fatalf("expected 1 lead, got %d", len(filtered))
	}
	if filtered[0].ID != "2" {
		t.Fatalf("expected lead 2, got %s", filtered[0].ID)
	}
}

func TestRowToLeadInput(t *testing.T) {
	input, err := rowToLeadInput(map[string]string{
		"nome_completo": "Maria Oliveira",
		"email":         "maria@example.com",
		"empresa":       "Acme",
		"telefone":      "11999990000",
		"fonte":         "indicacao",
		"status":        "novo",
		"data_captura":  "2026-03-20",
	})
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}

	if input.FullName != "Maria Oliveira" {
		t.Fatalf("unexpected full name: %s", input.FullName)
	}
}

func TestParseLeadRowsCSV(t *testing.T) {
	raw := "nome_completo,email,empresa\nMaria,maria@example.com,Acme"
	rows, err := parseLeadRows(".csv", strings.NewReader(raw))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(rows) != 1 {
		t.Fatalf("expected 1 row, got %d", len(rows))
	}
	if rows[0]["email"] != "maria@example.com" {
		t.Fatalf("unexpected email: %s", rows[0]["email"])
	}
}

func mustDate(value string) time.Time {
	parsed, err := time.Parse("2006-01-02", value)
	if err != nil {
		panic(err)
	}
	return parsed
}
