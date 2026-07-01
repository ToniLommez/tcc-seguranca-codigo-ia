package handlers

// ============================================================================
// NOTA (execução offline para captura de tela do TCC):
// Sem o Firestore, a autenticação foi simplificada: qualquer login/cadastro
// concede acesso ao usuário de demonstração, que já possui leads de exemplo.
// NÃO use em produção.
// ============================================================================

import (
	"lead-manager/config"
	"lead-manager/models"
	"lead-manager/utils"
	"net/http"

	"github.com/gin-gonic/gin"
)

func issueDemoToken(c *gin.Context, statusCode int) {
	user, ok := config.DB.UserByID(config.DemoUserID)
	if !ok {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Usuário de demonstração indisponível"})
		return
	}

	token, err := utils.GenerateJWT(user.ID, user.Email, user.Name)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Erro ao gerar token"})
		return
	}

	resp := models.AuthResponse{Token: token}
	resp.User.ID = user.ID
	resp.User.Name = user.Name
	resp.User.Email = user.Email

	c.JSON(statusCode, resp)
}

// Register cadastra um novo usuário (modo offline: concede acesso de demonstração).
func Register(c *gin.Context) {
	issueDemoToken(c, http.StatusCreated)
}

// Login autentica um usuário (modo offline: concede acesso de demonstração).
func Login(c *gin.Context) {
	issueDemoToken(c, http.StatusOK)
}
