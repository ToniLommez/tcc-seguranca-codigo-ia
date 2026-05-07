package models

import "time"

// Constantes de status do lead
const (
	StatusNovo        = "novo"
	StatusEmContato   = "em_contato"
	StatusQualificado = "qualificado"
	StatusPerdido     = "perdido"
)

// Constantes de fonte do lead
const (
	SourceIndicacao = "indicacao"
	SourceSite      = "site"
	SourceEvento    = "evento"
	SourceLinkedIn  = "linkedin"
	SourceEmail     = "email"
	SourceOutro     = "outro"
)

// Lead representa um lead captado
type Lead struct {
	ID           string        `json:"id" firestore:"id"`
	UserID       string        `json:"userId" firestore:"userId"`
	FullName     string        `json:"fullName" firestore:"fullName"`
	Email        string        `json:"email" firestore:"email"`
	Phone        string        `json:"phone" firestore:"phone"`
	Company      string        `json:"company" firestore:"company"`
	Role         string        `json:"role" firestore:"role"`
	Source       string        `json:"source" firestore:"source"`
	Status       string        `json:"status" firestore:"status"`
	Notes        string        `json:"notes" firestore:"notes"`
	CapturedAt   time.Time     `json:"capturedAt" firestore:"capturedAt"`
	CreatedAt    time.Time     `json:"createdAt" firestore:"createdAt"`
	UpdatedAt    time.Time     `json:"updatedAt" firestore:"updatedAt"`
	Interactions []Interaction `json:"interactions" firestore:"interactions"`
}

// Interaction representa uma interação registrada com o lead
type Interaction struct {
	ID        string    `json:"id" firestore:"id"`
	Type      string    `json:"type" firestore:"type"` // ligacao, email, reuniao, nota
	Note      string    `json:"note" firestore:"note"`
	CreatedAt time.Time `json:"createdAt" firestore:"createdAt"`
	CreatedBy string    `json:"createdBy" firestore:"createdBy"`
}

// CreateLeadRequest representa o corpo da requisição de criação de lead
type CreateLeadRequest struct {
	FullName   string     `json:"fullName" binding:"required,min=2,max=200"`
	Email      string     `json:"email" binding:"omitempty,email"`
	Phone      string     `json:"phone"`
	Company    string     `json:"company"`
	Role       string     `json:"role"`
	Source     string     `json:"source"`
	Status     string     `json:"status"`
	Notes      string     `json:"notes"`
	CapturedAt *time.Time `json:"capturedAt"`
}

// UpdateLeadRequest representa o corpo da requisição de atualização de lead
type UpdateLeadRequest struct {
	FullName   string     `json:"fullName"`
	Email      string     `json:"email"`
	Phone      string     `json:"phone"`
	Company    string     `json:"company"`
	Role       string     `json:"role"`
	Source     string     `json:"source"`
	Status     string     `json:"status"`
	Notes      string     `json:"notes"`
	CapturedAt *time.Time `json:"capturedAt"`
}

// AddInteractionRequest representa o corpo para adicionar uma interação
type AddInteractionRequest struct {
	Type string `json:"type" binding:"required"`
	Note string `json:"note" binding:"required,min=1"`
}

// LeadListParams representa os parâmetros de filtro e paginação
type LeadListParams struct {
	Page       int    `form:"page"`
	PageSize   int    `form:"pageSize"`
	Status     string `form:"status"`
	Source     string `form:"source"`
	Search     string `form:"search"`
	DateFrom   string `form:"dateFrom"`
	DateTo     string `form:"dateTo"`
	SortBy     string `form:"sortBy"`
	SortOrder  string `form:"sortOrder"`
}

// LeadListResponse é a resposta paginada de leads
type LeadListResponse struct {
	Leads      []Lead `json:"leads"`
	Total      int    `json:"total"`
	Page       int    `json:"page"`
	PageSize   int    `json:"pageSize"`
	TotalPages int    `json:"totalPages"`
}

// DashboardStats representa as estatísticas do dashboard
type DashboardStats struct {
	TotalLeads        int            `json:"totalLeads"`
	LeadsByStatus     map[string]int `json:"leadsByStatus"`
	LeadsBySource     map[string]int `json:"leadsBySource"`
	CapturesByDay     []DayCapture   `json:"capturesByDay"`
	RecentLeads       []Lead         `json:"recentLeads"`
	ConversionRate    float64        `json:"conversionRate"`
}

// DayCapture representa capturas em um dia específico
type DayCapture struct {
	Date  string `json:"date"`
	Count int    `json:"count"`
}
