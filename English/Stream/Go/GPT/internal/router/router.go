package router

import (
	"net/http"
	"strings"

	"streammusic/internal/config"
	"streammusic/internal/handlers"
	"streammusic/internal/middleware"
	"streammusic/internal/models"

	"github.com/gin-gonic/gin"
)

func Setup(
	cfg *config.Config,
	authHandler *handlers.AuthHandler,
	artistHandler *handlers.ArtistHandler,
	userHandler *handlers.UserHandler,
) *gin.Engine {
	engine := gin.Default()
	engine.MaxMultipartMemory = 128 << 20

	engine.GET("/api/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"status": "ok"})
	})

	auth := engine.Group("/api/auth")
	{
		auth.POST("/register", authHandler.Register)
		auth.POST("/login", authHandler.Login)
		auth.POST("/logout", authHandler.Logout)
	}

	protected := engine.Group("/api")
	protected.Use(middleware.AuthMiddleware(cfg.JWTSecret))
	{
		protected.GET("/auth/me", authHandler.Me)

		artist := protected.Group("/artist")
		artist.Use(middleware.RequireUserType(models.UserTypeArtist))
		{
			artist.POST("/songs", artistHandler.UploadSong)
			artist.GET("/songs", artistHandler.ListOwnSongs)
		}

		users := protected.Group("/")
		users.Use(middleware.RequireUserType(models.UserTypeUser))
		{
			users.GET("/songs", userHandler.ListSongs)
			users.GET("/songs/:id/stream", userHandler.StreamSong)
		}
	}

	engine.Static("/static", "./web/static")
	engine.GET("/", func(c *gin.Context) {
		c.File("./web/static/index.html")
	})

	engine.NoRoute(func(c *gin.Context) {
		if strings.HasPrefix(c.Request.URL.Path, "/api/") {
			c.JSON(http.StatusNotFound, gin.H{"error": "route not found"})
			return
		}
		c.File("./web/static/index.html")
	})

	return engine
}
