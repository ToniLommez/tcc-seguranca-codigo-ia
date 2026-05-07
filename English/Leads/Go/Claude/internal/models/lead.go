package models

import "time"

type Lead struct {
	ID          string    `json:"id" firestore:"-"`
	FullName    string    `json:"full_name" firestore:"full_name"`
	Email       string    `json:"email" firestore:"email"`
	Phone       string    `json:"phone" firestore:"phone"`
	Company     string    `json:"company" firestore:"company"`
	Position    string    `json:"position" firestore:"position"`
	Source      string    `json:"source" firestore:"source"`
	Status      string    `json:"status" firestore:"status"`
	CaptureDate time.Time `json:"capture_date" firestore:"capture_date"`
	Notes       string    `json:"notes" firestore:"notes"`
	UserID      string    `json:"user_id" firestore:"user_id"`
	CreatedAt   time.Time `json:"created_at" firestore:"created_at"`
	UpdatedAt   time.Time `json:"updated_at" firestore:"updated_at"`
}

type Interaction struct {
	ID        string    `json:"id" firestore:"-"`
	LeadID    string    `json:"lead_id" firestore:"lead_id"`
	UserID    string    `json:"user_id" firestore:"user_id"`
	Type      string    `json:"type" firestore:"type"`
	Notes     string    `json:"notes" firestore:"notes"`
	CreatedAt time.Time `json:"created_at" firestore:"created_at"`
}

type LeadFilter struct {
	Status      string `json:"status"`
	Source      string `json:"source"`
	DateFrom    string `json:"date_from"`
	DateTo      string `json:"date_to"`
	Search      string `json:"search"`
	Page        int    `json:"page"`
	PageSize    int    `json:"page_size"`
	SortBy      string `json:"sort_by"`
	SortOrder   string `json:"sort_order"`
}

type PaginatedResponse struct {
	Data       interface{} `json:"data"`
	Total      int         `json:"total"`
	Page       int         `json:"page"`
	PageSize   int         `json:"page_size"`
	TotalPages int         `json:"total_pages"`
}

var ValidStatuses = []string{"new", "contacted", "qualified", "lost"}
var ValidSources = []string{"referral", "website", "event", "social_media", "cold_call", "other"}
