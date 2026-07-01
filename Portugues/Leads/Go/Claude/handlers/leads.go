package handlers

import (
	"bytes"
	"encoding/csv"
	"lead-manager/config"
	"lead-manager/models"
	"lead-manager/utils"
	"net/http"
	"sort"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/xuri/excelize/v2"
)

func ListLeads(c *gin.Context) {
	userID := c.GetString("userID")
	var params models.LeadListParams
	if err := c.ShouldBindQuery(&params); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Parâmetros inválidos"})
		return
	}
	if params.Page < 1 {
		params.Page = 1
	}
	if params.PageSize < 1 || params.PageSize > 100 {
		params.PageSize = 20
	}
	if params.SortBy == "" {
		params.SortBy = "createdAt"
	}
	if params.SortOrder == "" {
		params.SortOrder = "desc"
	}

	allLeads := config.DB.LeadsByUser(userID)

	filtered := make([]models.Lead, 0)
	for _, lead := range allLeads {
		if params.Status != "" && lead.Status != params.Status {
			continue
		}
		if params.Source != "" && lead.Source != params.Source {
			continue
		}
		if params.DateFrom != "" {
			if t, err := time.Parse("2006-01-02", params.DateFrom); err == nil && lead.CapturedAt.Before(t) {
				continue
			}
		}
		if params.DateTo != "" {
			if t, err := time.Parse("2006-01-02", params.DateTo); err == nil {
				if lead.CapturedAt.After(t.Add(24*time.Hour - time.Second)) {
					continue
				}
			}
		}
		if params.Search != "" {
			q := strings.ToLower(params.Search)
			if !strings.Contains(strings.ToLower(lead.FullName), q) &&
				!strings.Contains(strings.ToLower(lead.Email), q) &&
				!strings.Contains(strings.ToLower(lead.Company), q) &&
				!strings.Contains(strings.ToLower(lead.Phone), q) {
				continue
			}
		}
		filtered = append(filtered, lead)
	}

	sort.Slice(filtered, func(i, j int) bool {
		var less bool
		switch params.SortBy {
		case "fullName":
			less = strings.ToLower(filtered[i].FullName) < strings.ToLower(filtered[j].FullName)
		case "company":
			less = strings.ToLower(filtered[i].Company) < strings.ToLower(filtered[j].Company)
		case "capturedAt":
			less = filtered[i].CapturedAt.Before(filtered[j].CapturedAt)
		default:
			less = filtered[i].CreatedAt.Before(filtered[j].CreatedAt)
		}
		if params.SortOrder == "asc" {
			return less
		}
		return !less
	})

	total := len(filtered)
	totalPages := (total + params.PageSize - 1) / params.PageSize
	if totalPages == 0 {
		totalPages = 1
	}
	start := (params.Page - 1) * params.PageSize
	end := start + params.PageSize
	if start > total {
		start = total
	}
	if end > total {
		end = total
	}
	paginated := filtered[start:end]
	if paginated == nil {
		paginated = []models.Lead{}
	}

	c.JSON(http.StatusOK, models.LeadListResponse{
		Leads: paginated, Total: total,
		Page: params.Page, PageSize: params.PageSize, TotalPages: totalPages,
	})
}

func CreateLead(c *gin.Context) {
	userID := c.GetString("userID")
	var req models.CreateLeadRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Dados inválidos: " + err.Error()})
		return
	}
	if req.Status == "" {
		req.Status = models.StatusNovo
	}
	if req.Source == "" {
		req.Source = models.SourceOutro
	}
	capturedAt := time.Now()
	if req.CapturedAt != nil {
		capturedAt = *req.CapturedAt
	}

	lead := models.Lead{
		ID: uuid.New().String(), UserID: userID,
		FullName: req.FullName, Email: strings.ToLower(strings.TrimSpace(req.Email)),
		Phone: req.Phone, Company: req.Company, Role: req.Role,
		Source: req.Source, Status: req.Status, Notes: req.Notes,
		CapturedAt: capturedAt, CreatedAt: time.Now(), UpdatedAt: time.Now(),
		Interactions: []models.Interaction{},
	}
	config.DB.AddLead(lead)
	c.JSON(http.StatusCreated, lead)
}

func GetLead(c *gin.Context) {
	userID := c.GetString("userID")
	leadID := c.Param("id")
	lead, ok := config.DB.LeadByID(leadID)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{"error": "Lead não encontrado"})
		return
	}
	if lead.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Acesso negado"})
		return
	}
	if lead.Interactions == nil {
		lead.Interactions = []models.Interaction{}
	}
	c.JSON(http.StatusOK, lead)
}

func UpdateLead(c *gin.Context) {
	userID := c.GetString("userID")
	leadID := c.Param("id")

	existing, ok := config.DB.LeadByID(leadID)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{"error": "Lead não encontrado"})
		return
	}
	if existing.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Acesso negado"})
		return
	}

	var req models.UpdateLeadRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Dados inválidos"})
		return
	}

	existing.Notes = req.Notes
	if req.FullName != "" {
		existing.FullName = req.FullName
	}
	if req.Email != "" {
		existing.Email = strings.ToLower(req.Email)
	}
	if req.Phone != "" {
		existing.Phone = req.Phone
	}
	if req.Company != "" {
		existing.Company = req.Company
	}
	if req.Role != "" {
		existing.Role = req.Role
	}
	if req.Source != "" {
		existing.Source = req.Source
	}
	if req.Status != "" {
		existing.Status = req.Status
	}
	if req.CapturedAt != nil {
		existing.CapturedAt = *req.CapturedAt
	}
	existing.UpdatedAt = time.Now()
	if existing.Interactions == nil {
		existing.Interactions = []models.Interaction{}
	}

	config.DB.UpdateLead(existing)
	c.JSON(http.StatusOK, existing)
}

func DeleteLead(c *gin.Context) {
	userID := c.GetString("userID")
	leadID := c.Param("id")
	lead, ok := config.DB.LeadByID(leadID)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{"error": "Lead não encontrado"})
		return
	}
	if lead.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Acesso negado"})
		return
	}
	config.DB.DeleteLead(leadID)
	c.JSON(http.StatusOK, gin.H{"message": "Lead removido com sucesso"})
}

func AddInteraction(c *gin.Context) {
	userID := c.GetString("userID")
	userName := c.GetString("userName")
	leadID := c.Param("id")

	lead, ok := config.DB.LeadByID(leadID)
	if !ok {
		c.JSON(http.StatusNotFound, gin.H{"error": "Lead não encontrado"})
		return
	}
	if lead.UserID != userID {
		c.JSON(http.StatusForbidden, gin.H{"error": "Acesso negado"})
		return
	}

	var req models.AddInteractionRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Dados inválidos: " + err.Error()})
		return
	}

	interaction := models.Interaction{
		ID: uuid.New().String(), Type: req.Type, Note: req.Note,
		CreatedAt: time.Now(), CreatedBy: userName,
	}
	if lead.Interactions == nil {
		lead.Interactions = []models.Interaction{}
	}
	lead.Interactions = append([]models.Interaction{interaction}, lead.Interactions...)
	lead.UpdatedAt = time.Now()

	config.DB.UpdateLead(lead)
	c.JSON(http.StatusCreated, interaction)
}

func ExportLeads(c *gin.Context) {
	userID := c.GetString("userID")
	format := c.DefaultQuery("format", "csv")
	leads := config.DB.LeadsByUser(userID)

	timestamp := time.Now().Format("20060102_150405")
	if format == "xlsx" {
		f, err := utils.LeadsToExcel(leads)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Erro ao gerar Excel"})
			return
		}
		var buf bytes.Buffer
		if err := f.Write(&buf); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "Erro ao serializar Excel"})
			return
		}
		c.Header("Content-Disposition", "attachment; filename=leads_"+timestamp+".xlsx")
		c.Data(http.StatusOK, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", buf.Bytes())
		return
	}
	csvData, err := utils.LeadsToCSV(leads)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Erro ao gerar CSV"})
		return
	}
	c.Header("Content-Disposition", "attachment; filename=leads_"+timestamp+".csv")
	c.Data(http.StatusOK, "text/csv; charset=utf-8", []byte("\xef\xbb\xbf"+csvData))
}

func ImportLeads(c *gin.Context) {
	userID := c.GetString("userID")
	file, header, err := c.Request.FormFile("file")
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Arquivo não encontrado no request"})
		return
	}
	defer file.Close()

	filename := strings.ToLower(header.Filename)
	var leads []models.Lead
	var parseErrors []string

	if strings.HasSuffix(filename, ".xlsx") || strings.HasSuffix(filename, ".xls") {
		f, err := excelize.OpenReader(file)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "Erro ao abrir Excel"})
			return
		}
		rows, err := f.GetRows(f.GetSheetName(0))
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": "Erro ao ler planilha"})
			return
		}
		var sb strings.Builder
		w := csv.NewWriter(&sb)
		for _, row := range rows {
			_ = w.Write(row)
		}
		w.Flush()
		leads, parseErrors, err = utils.ParseCSVToLeads(strings.NewReader(sb.String()), userID)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
			return
		}
	} else {
		leads, parseErrors, err = utils.ParseCSVToLeads(file, userID)
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
			return
		}
	}

	for i := range leads {
		leads[i].ID = uuid.New().String()
		leads[i].Interactions = []models.Interaction{}
		config.DB.AddLead(leads[i])
	}
	c.JSON(http.StatusOK, gin.H{"imported": len(leads), "errors": parseErrors, "message": "Importação concluída"})
}
