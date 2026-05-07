package models

import "time"

const (
	LeadStatusNew         = "novo"
	LeadStatusContacted   = "em contato"
	LeadStatusQualified   = "qualificado"
	LeadStatusLost        = "perdido"
	InteractionTypeNote   = "nota"
	InteractionTypeSystem = "sistema"
)

var ValidLeadStatuses = map[string]struct{}{
	LeadStatusNew:       {},
	LeadStatusContacted: {},
	LeadStatusQualified: {},
	LeadStatusLost:      {},
}

type User struct {
	ID              string    `firestore:"id" json:"id"`
	Name            string    `firestore:"name" json:"name"`
	Email           string    `firestore:"email" json:"email"`
	EmailNormalized string    `firestore:"email_normalized" json:"-"`
	PasswordHash    string    `firestore:"password_hash" json:"-"`
	CreatedAt       time.Time `firestore:"created_at" json:"created_at"`
}

type Lead struct {
	ID             string    `firestore:"id" json:"id"`
	OwnerID        string    `firestore:"owner_id" json:"owner_id"`
	FullName       string    `firestore:"full_name" json:"full_name"`
	Email          string    `firestore:"email" json:"email"`
	Phone          string    `firestore:"phone" json:"phone"`
	Company        string    `firestore:"company" json:"company"`
	Role           string    `firestore:"role" json:"role"`
	Source         string    `firestore:"source" json:"source"`
	Status         string    `firestore:"status" json:"status"`
	CaptureDate    time.Time `firestore:"capture_date" json:"capture_date"`
	SearchText     string    `firestore:"search_text" json:"-"`
	CreatedAt      time.Time `firestore:"created_at" json:"created_at"`
	UpdatedAt      time.Time `firestore:"updated_at" json:"updated_at"`
	LastImportedAt time.Time `firestore:"last_imported_at,omitempty" json:"last_imported_at,omitempty"`
}

type Interaction struct {
	ID        string    `firestore:"id" json:"id"`
	LeadID    string    `firestore:"lead_id" json:"lead_id"`
	OwnerID   string    `firestore:"owner_id" json:"owner_id"`
	Type      string    `firestore:"type" json:"type"`
	Notes     string    `firestore:"notes" json:"notes"`
	CreatedBy string    `firestore:"created_by" json:"created_by"`
	CreatedAt time.Time `firestore:"created_at" json:"created_at"`
}

type LeadFilterParams struct {
	Query           string
	Status          string
	Source          string
	CaptureDateFrom string
	CaptureDateTo   string
	SortBy          string
	SortDirection   string
	Page            int
	PageSize        int
}

type PaginatedLeads struct {
	Items      []Lead `json:"items"`
	Page       int    `json:"page"`
	PageSize   int    `json:"page_size"`
	TotalItems int    `json:"total_items"`
	TotalPages int    `json:"total_pages"`
}

type DailyCapture struct {
	Date  string `json:"date"`
	Count int    `json:"count"`
}

type DashboardStats struct {
	TotalLeads      int            `json:"total_leads"`
	NewLeads        int            `json:"new_leads"`
	QualifiedLeads  int            `json:"qualified_leads"`
	LostLeads       int            `json:"lost_leads"`
	ByStatus        map[string]int `json:"by_status"`
	CapturesByDay   []DailyCapture `json:"captures_by_day"`
	TopSources      map[string]int `json:"top_sources"`
	RecentLeadCount int            `json:"recent_lead_count"`
}

type LeadInput struct {
	FullName    string `json:"full_name"`
	Email       string `json:"email"`
	Phone       string `json:"phone"`
	Company     string `json:"company"`
	Role        string `json:"role"`
	Source      string `json:"source"`
	Status      string `json:"status"`
	CaptureDate string `json:"capture_date"`
}

type InteractionInput struct {
	Type  string `json:"type"`
	Notes string `json:"notes"`
}

type ImportResult struct {
	Imported int      `json:"imported"`
	Updated  int      `json:"updated"`
	Skipped  int      `json:"skipped"`
	Errors   []string `json:"errors"`
}
