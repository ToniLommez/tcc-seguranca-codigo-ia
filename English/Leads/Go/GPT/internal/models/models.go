package models

import "time"

var AllowedLeadStatuses = map[string]struct{}{
	"new":       {},
	"contacted": {},
	"qualified": {},
	"lost":      {},
}

type User struct {
	ID           string    `firestore:"id" json:"id"`
	Name         string    `firestore:"name" json:"name"`
	Email        string    `firestore:"email" json:"email"`
	PasswordHash string    `firestore:"passwordHash" json:"-"`
	CreatedAt    time.Time `firestore:"createdAt" json:"createdAt"`
}

type Lead struct {
	ID          string    `firestore:"id" json:"id"`
	FullName    string    `firestore:"fullName" json:"fullName"`
	Email       string    `firestore:"email" json:"email"`
	Phone       string    `firestore:"phone" json:"phone"`
	Company     string    `firestore:"company" json:"company"`
	Position    string    `firestore:"position" json:"position"`
	Source      string    `firestore:"source" json:"source"`
	Status      string    `firestore:"status" json:"status"`
	CaptureDate time.Time `firestore:"captureDate" json:"captureDate"`
	CreatedAt   time.Time `firestore:"createdAt" json:"createdAt"`
	UpdatedAt   time.Time `firestore:"updatedAt" json:"updatedAt"`
}

type LeadInteraction struct {
	ID        string    `firestore:"id" json:"id"`
	LeadID    string    `firestore:"leadId" json:"leadId"`
	Type      string    `firestore:"type" json:"type"`
	Note      string    `firestore:"note" json:"note"`
	CreatedAt time.Time `firestore:"createdAt" json:"createdAt"`
	CreatedBy string    `firestore:"createdBy" json:"createdBy"`
}

type LeadDetail struct {
	Lead         Lead              `json:"lead"`
	Interactions []LeadInteraction `json:"interactions"`
}

type LeadFilters struct {
	Page      int
	PageSize  int
	SortBy    string
	SortDir   string
	Status    string
	Source    string
	Query     string
	DateFrom  *time.Time
	DateTo    *time.Time
	ExportAll bool
}

type PaginatedLeads struct {
	Items      []Lead `json:"items"`
	Page       int    `json:"page"`
	PageSize   int    `json:"pageSize"`
	TotalItems int    `json:"totalItems"`
	TotalPages int    `json:"totalPages"`
}

type DashboardSummary struct {
	TotalLeads       int            `json:"totalLeads"`
	StatusCounts     map[string]int `json:"statusCounts"`
	SourceCounts     map[string]int `json:"sourceCounts"`
	CapturesOverTime []TrendPoint   `json:"capturesOverTime"`
}

type TrendPoint struct {
	Label string `json:"label"`
	Count int    `json:"count"`
}

type ImportResult struct {
	Imported int      `json:"imported"`
	Failed   int      `json:"failed"`
	Errors   []string `json:"errors"`
}
