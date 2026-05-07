package firebase

import (
	"context"
	"fmt"
	"strings"
	"time"

	"cloud.google.com/go/firestore"
	firebase "firebase.google.com/go/v4"
	"google.golang.org/api/option"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"streamapp/internal/domain"
)

type UserRepository struct {
	client *firestore.Client
}

func NewUserRepository(ctx context.Context, credentialsPath, projectID string) (*UserRepository, error) {
	var appConfig *firebase.Config
	if strings.TrimSpace(projectID) != "" {
		appConfig = &firebase.Config{ProjectID: projectID}
	}

	app, err := firebase.NewApp(ctx, appConfig, option.WithCredentialsFile(credentialsPath))
	if err != nil {
		return nil, fmt.Errorf("create firebase app: %w", err)
	}

	client, err := app.Firestore(ctx)
	if err != nil {
		return nil, fmt.Errorf("create firestore client: %w", err)
	}

	return &UserRepository{client: client}, nil
}

func (r *UserRepository) Close() error {
	return r.client.Close()
}

func (r *UserRepository) Create(ctx context.Context, user domain.User) (domain.User, error) {
	doc := r.client.Collection("users").Doc(normalizeEmail(user.Email))
	_, err := doc.Get(ctx)
	if err == nil {
		return domain.User{}, fmt.Errorf("user already exists")
	}
	if status.Code(err) != codes.NotFound {
		return domain.User{}, fmt.Errorf("check existing user: %w", err)
	}

	user.ID = normalizeEmail(user.Email)
	user.Email = normalizeEmail(user.Email)
	user.CreatedAt = time.Now().UTC()

	if _, err := doc.Set(ctx, user); err != nil {
		return domain.User{}, fmt.Errorf("save user: %w", err)
	}

	return user, nil
}

func (r *UserRepository) GetByEmail(ctx context.Context, email string) (domain.User, error) {
	doc, err := r.client.Collection("users").Doc(normalizeEmail(email)).Get(ctx)
	if err != nil {
		if status.Code(err) == codes.NotFound {
			return domain.User{}, fmt.Errorf("user not found")
		}
		return domain.User{}, fmt.Errorf("fetch user: %w", err)
	}

	var user domain.User
	if err := doc.DataTo(&user); err != nil {
		return domain.User{}, fmt.Errorf("decode user: %w", err)
	}

	return user, nil
}

func normalizeEmail(email string) string {
	return strings.ToLower(strings.TrimSpace(email))
}
