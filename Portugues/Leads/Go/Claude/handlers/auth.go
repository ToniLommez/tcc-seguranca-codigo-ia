package handlers

import (
	"context"
	"lead-manager/config"
	"lead-manager/models"
	"lead-manager/utils"
	"net/http"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"golang.org/x/crypto/bcrypt"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// Register cadastra um novo usuário no sistema
func Register(c *gin.Context) {
	var req models.RegisterRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Dados inválidos: " + err.Error()})
		return
	}

	req.Email = strings.ToLower(strings.TrimSpace(req.Email))
	req.Name = strings.TrimSpace(req.Name)

	ctx := context.Background()

	// Verifica se e-mail já está cadastrado
	docs, err := config.FirestoreClient.Collection(config.CollectionUsers).
		Where("email", "==", req.Email).
		Limit(1).
		Documents(ctx).
		GetAll()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Erro ao verificar e-mail"})
		return
	}
	if len(docs) > 0 {
		c.JSON(http.StatusConflict, gin.H{"error": "E-mail já cadastrado"})
		return
	}

	// Hash da senha
	hash, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Erro ao processar senha"})
		return
	}

	user := models.User{
		ID:           uuid.New().String(),
		Name:         req.Name,
		Email:        req.Email,
		PasswordHash: string(hash),
		CreatedAt:    time.Now(),
	}

	// Salva no Firestore
	_, err = config.FirestoreClient.Collection(config.CollectionUsers).
		Doc(user.ID).
		Set(ctx, user)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Erro ao salvar usuário"})
		return
	}

	// Gera JWT
	token, err := utils.GenerateJWT(user.ID, user.Email, user.Name)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Erro ao gerar token"})
		return
	}

	resp := models.AuthResponse{Token: token}
	resp.User.ID = user.ID
	resp.User.Name = user.Name
	resp.User.Email = user.Email

	c.JSON(http.StatusCreated, resp)
}

// Login autentica um usuário e retorna um token JWT
func Login(c *gin.Context) {
	var req models.LoginRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "Dados inválidos: " + err.Error()})
		return
	}

	req.Email = strings.ToLower(strings.TrimSpace(req.Email))

	ctx := context.Background()

	// Busca usuário por e-mail
	docs, err := config.FirestoreClient.Collection(config.CollectionUsers).
		Where("email", "==", req.Email).
		Limit(1).
		Documents(ctx).
		GetAll()
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Erro ao buscar usuário"})
		return
	}
	if len(docs) == 0 {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "E-mail ou senha incorretos"})
		return
	}

	var user models.User
	if err := docs[0].DataTo(&user); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Erro ao ler dados do usuário"})
		return
	}

	// Verifica senha
	if err := bcrypt.CompareHashAndPassword([]byte(user.PasswordHash), []byte(req.Password)); err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "E-mail ou senha incorretos"})
		return
	}

	// Gera JWT
	token, err := utils.GenerateJWT(user.ID, user.Email, user.Name)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Erro ao gerar token"})
		return
	}

	resp := models.AuthResponse{Token: token}
	resp.User.ID = user.ID
	resp.User.Name = user.Name
	resp.User.Email = user.Email

	c.JSON(http.StatusOK, resp)
}

// isNotFound verifica se é um erro "not found" do Firestore
func isNotFound(err error) bool {
	return status.Code(err) == codes.NotFound
}
