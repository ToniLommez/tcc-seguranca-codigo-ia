package main

import (
	"lead-manager/config"
	"lead-manager/handlers"
	"lead-manager/middleware"
	"log"
	"net/http"
	"os"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
)

func main() {
	// Carrega variáveis de ambiente
	if err := godotenv.Load(); err != nil {
		log.Println("Arquivo .env não encontrado, usando variáveis de ambiente do sistema")
	}

	// Inicializa Firebase
	if err := config.InitFirebase(); err != nil {
		log.Fatalf("Falha ao inicializar Firebase: %v", err)
	}
	defer config.CloseFirebase()

	// Configura modo do Gin
	if os.Getenv("GIN_MODE") == "release" {
		gin.SetMode(gin.ReleaseMode)
	}

	r := gin.Default()

	// Configuração de CORS
	r.Use(cors.New(cors.Config{
		AllowOrigins:     []string{"*"},
		AllowMethods:     []string{"GET", "POST", "PUT", "DELETE", "OPTIONS"},
		AllowHeaders:     []string{"Origin", "Content-Type", "Authorization"},
		ExposeHeaders:    []string{"Content-Disposition", "Content-Type"},
		AllowCredentials: false,
	}))

	// Serve arquivos estáticos do frontend
	r.Static("/app", "./frontend")

	// Redireciona raiz para o app
	r.GET("/", func(c *gin.Context) {
		c.Redirect(http.StatusFound, "/app/index.html")
	})

	// ─── Rotas da API ─────────────────────────────────────────────
	api := r.Group("/api")
	{
		// Autenticação (pública)
		auth := api.Group("/auth")
		{
			auth.POST("/register", handlers.Register)
			auth.POST("/login", handlers.Login)
		}

		// Rotas protegidas por JWT
		protected := api.Group("/")
		protected.Use(middleware.AuthRequired())
		{
			// Dashboard
			protected.GET("/dashboard", handlers.GetDashboard)

			// Leads
			leads := protected.Group("/leads")
			{
				leads.GET("", handlers.ListLeads)
				leads.POST("", handlers.CreateLead)
				leads.GET("/export", handlers.ExportLeads)
				leads.POST("/import", handlers.ImportLeads)
				leads.GET("/:id", handlers.GetLead)
				leads.PUT("/:id", handlers.UpdateLead)
				leads.DELETE("/:id", handlers.DeleteLead)

				// Interações de um lead
				leads.POST("/:id/interactions", handlers.AddInteraction)
			}
		}
	}

	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	log.Printf("╔══════════════════════════════════════════════╗")
	log.Printf("║   Lead Manager - Servidor iniciado           ║")
	log.Printf("║   http://localhost:%s                     ║", port)
	log.Printf("║   Frontend: http://localhost:%s/app/        ║", port)
	log.Printf("╚══════════════════════════════════════════════╝")

	if err := r.Run(":" + port); err != nil {
		log.Fatalf("Falha ao iniciar servidor: %v", err)
	}
}
