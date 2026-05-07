package repository

import (
	"context"
	"errors"
	"streammusic/internal/models"

	"cloud.google.com/go/firestore"
	"google.golang.org/api/iterator"
	"google.golang.org/api/option"
)

var ErrUserNotFound = errors.New("user not found")

type FirestoreUserRepository struct {
	client     *firestore.Client
	collection *firestore.CollectionRef
}

func NewFirestoreUserRepository(ctx context.Context, projectID, credentialsPath string) (*FirestoreUserRepository, error) {
	client, err := firestore.NewClient(ctx, projectID, option.WithCredentialsFile(credentialsPath))
	if err != nil {
		return nil, err
	}

	return &FirestoreUserRepository{
		client:     client,
		collection: client.Collection("users"),
	}, nil
}

func (r *FirestoreUserRepository) Close() error {
	return r.client.Close()
}

func (r *FirestoreUserRepository) Create(ctx context.Context, user *models.User) error {
	_, err := r.collection.Doc(user.ID).Set(ctx, user)
	return err
}

func (r *FirestoreUserRepository) GetByID(ctx context.Context, id string) (*models.User, error) {
	doc, err := r.collection.Doc(id).Get(ctx)
	if err != nil {
		return nil, ErrUserNotFound
	}

	var user models.User
	if err := doc.DataTo(&user); err != nil {
		return nil, err
	}

	return &user, nil
}

func (r *FirestoreUserRepository) GetByEmail(ctx context.Context, email string) (*models.User, error) {
	iter := r.collection.Where("email", "==", email).Limit(1).Documents(ctx)
	defer iter.Stop()

	doc, err := iter.Next()
	if err != nil {
		if errors.Is(err, iterator.Done) {
			return nil, ErrUserNotFound
		}
		return nil, err
	}

	var user models.User
	if err := doc.DataTo(&user); err != nil {
		return nil, err
	}

	return &user, nil
}
