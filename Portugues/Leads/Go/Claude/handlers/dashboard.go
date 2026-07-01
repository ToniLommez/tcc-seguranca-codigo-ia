package handlers

import (
	"lead-manager/config"
	"lead-manager/models"
	"net/http"
	"sort"
	"time"

	"github.com/gin-gonic/gin"
)

// GetDashboard retorna as estatísticas do dashboard
func GetDashboard(c *gin.Context) {
	userID := c.GetString("userID")

	allLeads := config.DB.LeadsByUser(userID)

	// ── Leads por status ─────────────────────────────────────────
	byStatus := map[string]int{
		models.StatusNovo:        0,
		models.StatusEmContato:   0,
		models.StatusQualificado: 0,
		models.StatusPerdido:     0,
	}
	for _, lead := range allLeads {
		byStatus[lead.Status]++
	}

	// ── Leads por fonte ──────────────────────────────────────────
	bySource := map[string]int{}
	for _, lead := range allLeads {
		if lead.Source != "" {
			bySource[lead.Source]++
		}
	}

	// ── Capturas por dia (últimos 30 dias) ───────────────────────
	now := time.Now()
	dayMap := map[string]int{}
	for i := 29; i >= 0; i-- {
		day := now.AddDate(0, 0, -i).Format("2006-01-02")
		dayMap[day] = 0
	}
	for _, lead := range allLeads {
		day := lead.CapturedAt.Format("2006-01-02")
		if _, ok := dayMap[day]; ok {
			dayMap[day]++
		}
	}

	// Ordena os dias
	days := make([]string, 0, len(dayMap))
	for day := range dayMap {
		days = append(days, day)
	}
	sort.Strings(days)

	capturesByDay := make([]models.DayCapture, 0, len(days))
	for _, day := range days {
		capturesByDay = append(capturesByDay, models.DayCapture{
			Date:  day,
			Count: dayMap[day],
		})
	}

	// ── Leads recentes (últimos 5) ───────────────────────────────
	sorted := make([]models.Lead, len(allLeads))
	copy(sorted, allLeads)
	sort.Slice(sorted, func(i, j int) bool {
		return sorted[i].CreatedAt.After(sorted[j].CreatedAt)
	})
	recentCount := 5
	if len(sorted) < recentCount {
		recentCount = len(sorted)
	}
	recentLeads := sorted[:recentCount]
	if recentLeads == nil {
		recentLeads = []models.Lead{}
	}
	for i := range recentLeads {
		recentLeads[i].Interactions = nil // Não enviar interações no dashboard
	}

	// ── Taxa de conversão ────────────────────────────────────────
	var conversionRate float64
	if len(allLeads) > 0 {
		conversionRate = float64(byStatus[models.StatusQualificado]) / float64(len(allLeads)) * 100
	}

	c.JSON(http.StatusOK, models.DashboardStats{
		TotalLeads:     len(allLeads),
		LeadsByStatus:  byStatus,
		LeadsBySource:  bySource,
		CapturesByDay:  capturesByDay,
		RecentLeads:    recentLeads,
		ConversionRate: conversionRate,
	})
}
