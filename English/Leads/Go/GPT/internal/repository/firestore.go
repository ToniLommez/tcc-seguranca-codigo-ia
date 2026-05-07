package repository

import (
	"context"
	"errors"
	"fmt"
	"strings"

	"cloud.google.com/go/firestore"
	"firebase.google.com/go/v4"
	"google.golang.org/api/option"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"leadmanager/internal/config"
	"leadmanager/internal/models"
)

var ErrNotFound = errors.New("resource not found")

type FirestoreRepository struct {
	client          *firestore.Client
	usersCollection string
}

func NewFirestoreRepository(ctx context.Context, cfg config.Config) (*FirestoreRepository, error) {
	app, err := firebase.NewApp(ctx, &firebase.Config{ProjectID: cfg.FirebaseProjectID}, option.WithCredentialsFile(cfg.FirebaseCredentialsPath))
	if err != nil {
		return nil, fmt.Errorf("create firebase app: %w", err)
	}

	client, err := app.Firestore(ctx)
	if err != nil {
		return nil, fmt.Errorf("create firestore client: %w", err)
	}

	return &FirestoreRepository{
		client:          client,
		usersCollection: cfg.FirestoreUsersCollection,
	}, nil
}

func (r *FirestoreRepository) Close() error {
	return r.client.Close()
}

func (r *FirestoreRepository) CreateUser(ctx context.Context, user models.User) error {
	_, err := r.client.Collection(r.usersCollection).Doc(user.ID).Set(ctx, user)
	return err
}

func (r *FirestoreRepository) GetUserByEmail(ctx context.Context, email string) (models.User, error) {
	iter := r.client.Collection(r.usersCollection).
		Where("email", "==", strings.ToLower(strings.TrimSpace(email))).
		Limit(1).
		Documents(ctx)

	docs, err := iter.GetAll()
	if err != nil {
		return models.User{}, err
	}
	if len(docs) == 0 {
		return models.User{}, ErrNotFound
	}

	var user models.User
	if err := docs[0].DataTo(&user); err != nil {
		return models.User{}, err
	}
	return user, nil
}

func (r *FirestoreRepository) GetUserByID(ctx context.Context, userID string) (models.User, error) {
	doc, err := r.client.Collection(r.usersCollection).Doc(userID).Get(ctx)
	if err != nil {
		if isNotFound(err) {
			return models.User{}, ErrNotFound
		}
		return models.User{}, err
	}

	var user models.User
	if err := doc.DataTo(&user); err != nil {
		return models.User{}, err
	}
	return user, nil
}

func (r *FirestoreRepository) CreateLead(ctx context.Context, userID string, lead models.Lead) error {
	_, err := r.leadsCollection(userID).Doc(lead.ID).Set(ctx, lead)
	return err
}

func (r *FirestoreRepository) UpdateLead(ctx context.Context, userID string, lead models.Lead) error {
	_, err := r.leadsCollection(userID).Doc(lead.ID).Set(ctx, lead)
	return err
}

func (r *FirestoreRepository) DeleteLead(ctx context.Context, userID, leadID string) error {
	leadRef := r.leadsCollection(userID).Doc(leadID)
	if _, err := leadRef.Get(ctx); err != nil {
		if isNotFound(err) {
			return ErrNotFound
		}
		return err
	}

	interactionDocs, err := leadRef.Collection("interactions").Documents(ctx).GetAll()
	if err != nil {
		return err
	}

	for _, doc := range interactionDocs {
		if _, err := doc.Ref.Delete(ctx); err != nil {
			return err
		}
	}

	_, err = leadRef.Delete(ctx)
	return err
}

func (r *FirestoreRepository) GetLeadByID(ctx context.Context, userID, leadID string) (models.Lead, error) {
	doc, err := r.leadsCollection(userID).Doc(leadID).Get(ctx)
	if err != nil {
		if isNotFound(err) {
			return models.Lead{}, ErrNotFound
		}
		return models.Lead{}, err
	}

	var lead models.Lead
	if err := doc.DataTo(&lead); err != nil {
		return models.Lead{}, err
	}
	return lead, nil
}

func (r *FirestoreRepository) ListLeads(ctx context.Context, userID string) ([]models.Lead, error) {
	docs, err := r.leadsCollection(userID).Documents(ctx).GetAll()
	if err != nil {
		return nil, err
	}

	leads := make([]models.Lead, 0, len(docs))
	for _, doc := range docs {
		var lead models.Lead
		if err := doc.DataTo(&lead); err != nil {
			return nil, err
		}
		leads = append(leads, lead)
	}

	return leads, nil
}

func (r *FirestoreRepository) AddInteraction(ctx context.Context, userID, leadID string, interaction models.LeadInteraction) error {
	leadRef := r.leadsCollection(userID).Doc(leadID)
	if _, err := leadRef.Get(ctx); err != nil {
		if isNotFound(err) {
			return ErrNotFound
		}
		return err
	}

	_, err := leadRef.Collection("interactions").Doc(interaction.ID).Set(ctx, interaction)
	return err
}

func (r *FirestoreRepository) ListInteractions(ctx context.Context, userID, leadID string) ([]models.LeadInteraction, error) {
	docs, err := r.leadsCollection(userID).Doc(leadID).Collection("interactions").Documents(ctx).GetAll()
	if err != nil {
		return nil, err
	}

	interactions := make([]models.LeadInteraction, 0, len(docs))
	for _, doc := range docs {
		var interaction models.LeadInteraction
		if err := doc.DataTo(&interaction); err != nil {
			return nil, err
		}
		interactions = append(interactions, interaction)
	}

	return interactions, nil
}

func (r *FirestoreRepository) leadsCollection(userID string) *firestore.CollectionRef {
	return r.client.Collection(r.usersCollection).Doc(userID).Collection("leads")
}

func isNotFound(err error) bool {
	return status.Code(err) == codes.NotFound
}
