package store

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"time"

	"cloud.google.com/go/firestore"
	"github.com/google/uuid"
	"google.golang.org/api/iterator"
	"google.golang.org/api/option"

	"leadmanagergpt/internal/config"
	"leadmanagergpt/internal/models"
)

var ErrNotFound = errors.New("document not found")

type FirestoreStore struct {
	client      *firestore.Client
	collections config.Collections
}

func NewFirestoreStore(ctx context.Context, cfg config.Config) (*FirestoreStore, error) {
	client, err := firestore.NewClient(ctx, cfg.ProjectID, option.WithCredentialsFile(cfg.FirebaseCredentialsPath))
	if err != nil {
		return nil, fmt.Errorf("create firestore client: %w", err)
	}

	return &FirestoreStore{
		client:      client,
		collections: cfg.Collections,
	}, nil
}

func (s *FirestoreStore) Close() error {
	return s.client.Close()
}

func (s *FirestoreStore) users() *firestore.CollectionRef {
	return s.client.Collection(s.collections.Users)
}

func (s *FirestoreStore) leads() *firestore.CollectionRef {
	return s.client.Collection(s.collections.Leads)
}

func (s *FirestoreStore) interactions() *firestore.CollectionRef {
	return s.client.Collection(s.collections.Interactions)
}

func (s *FirestoreStore) CreateUser(ctx context.Context, user models.User) (models.User, error) {
	if user.ID == "" {
		user.ID = uuid.NewString()
	}
	user.CreatedAt = time.Now().UTC()
	user.EmailNormalized = normalize(user.Email)

	if _, err := s.users().Doc(user.ID).Set(ctx, user); err != nil {
		return models.User{}, fmt.Errorf("create user: %w", err)
	}

	return user, nil
}

func (s *FirestoreStore) GetUserByEmail(ctx context.Context, email string) (models.User, error) {
	iter := s.users().Where("email_normalized", "==", normalize(email)).Limit(1).Documents(ctx)
	defer iter.Stop()

	doc, err := iter.Next()
	if errors.Is(err, iterator.Done) {
		return models.User{}, ErrNotFound
	}
	if err != nil {
		return models.User{}, fmt.Errorf("get user by email: %w", err)
	}

	var user models.User
	if err := doc.DataTo(&user); err != nil {
		return models.User{}, fmt.Errorf("decode user: %w", err)
	}
	return user, nil
}

func (s *FirestoreStore) GetUserByID(ctx context.Context, userID string) (models.User, error) {
	doc, err := s.users().Doc(userID).Get(ctx)
	if err != nil {
		if strings.Contains(strings.ToLower(err.Error()), "not found") {
			return models.User{}, ErrNotFound
		}
		return models.User{}, fmt.Errorf("get user by id: %w", err)
	}

	var user models.User
	if err := doc.DataTo(&user); err != nil {
		return models.User{}, fmt.Errorf("decode user: %w", err)
	}
	return user, nil
}

func (s *FirestoreStore) CreateLead(ctx context.Context, lead models.Lead) (models.Lead, error) {
	if lead.ID == "" {
		lead.ID = uuid.NewString()
	}
	now := time.Now().UTC()
	lead.CreatedAt = now
	lead.UpdatedAt = now
	lead.SearchText = normalizeSearch(lead)

	if _, err := s.leads().Doc(lead.ID).Set(ctx, lead); err != nil {
		return models.Lead{}, fmt.Errorf("create lead: %w", err)
	}
	return lead, nil
}

func (s *FirestoreStore) UpdateLead(ctx context.Context, lead models.Lead) (models.Lead, error) {
	lead.UpdatedAt = time.Now().UTC()
	lead.SearchText = normalizeSearch(lead)

	if _, err := s.leads().Doc(lead.ID).Set(ctx, lead); err != nil {
		return models.Lead{}, fmt.Errorf("update lead: %w", err)
	}
	return lead, nil
}

func (s *FirestoreStore) DeleteLead(ctx context.Context, leadID string) error {
	if _, err := s.leads().Doc(leadID).Delete(ctx); err != nil {
		return fmt.Errorf("delete lead: %w", err)
	}
	return nil
}

func (s *FirestoreStore) GetLeadByID(ctx context.Context, leadID string) (models.Lead, error) {
	doc, err := s.leads().Doc(leadID).Get(ctx)
	if err != nil {
		if strings.Contains(strings.ToLower(err.Error()), "not found") {
			return models.Lead{}, ErrNotFound
		}
		return models.Lead{}, fmt.Errorf("get lead by id: %w", err)
	}

	var lead models.Lead
	if err := doc.DataTo(&lead); err != nil {
		return models.Lead{}, fmt.Errorf("decode lead: %w", err)
	}
	return lead, nil
}

func (s *FirestoreStore) ListLeadsByOwner(ctx context.Context, ownerID string) ([]models.Lead, error) {
	iter := s.leads().Where("owner_id", "==", ownerID).Documents(ctx)
	defer iter.Stop()

	var leads []models.Lead
	for {
		doc, err := iter.Next()
		if errors.Is(err, iterator.Done) {
			break
		}
		if err != nil {
			return nil, fmt.Errorf("list leads: %w", err)
		}

		var lead models.Lead
		if err := doc.DataTo(&lead); err != nil {
			return nil, fmt.Errorf("decode lead: %w", err)
		}
		leads = append(leads, lead)
	}

	return leads, nil
}

func (s *FirestoreStore) CreateInteraction(ctx context.Context, interaction models.Interaction) (models.Interaction, error) {
	if interaction.ID == "" {
		interaction.ID = uuid.NewString()
	}
	interaction.CreatedAt = time.Now().UTC()

	if _, err := s.interactions().Doc(interaction.ID).Set(ctx, interaction); err != nil {
		return models.Interaction{}, fmt.Errorf("create interaction: %w", err)
	}
	return interaction, nil
}

func (s *FirestoreStore) ListInteractionsByOwner(ctx context.Context, ownerID string) ([]models.Interaction, error) {
	iter := s.interactions().Where("owner_id", "==", ownerID).Documents(ctx)
	defer iter.Stop()

	var interactions []models.Interaction
	for {
		doc, err := iter.Next()
		if errors.Is(err, iterator.Done) {
			break
		}
		if err != nil {
			return nil, fmt.Errorf("list interactions: %w", err)
		}

		var interaction models.Interaction
		if err := doc.DataTo(&interaction); err != nil {
			return nil, fmt.Errorf("decode interaction: %w", err)
		}
		interactions = append(interactions, interaction)
	}

	return interactions, nil
}

func (s *FirestoreStore) DeleteInteraction(ctx context.Context, interactionID string) error {
	if _, err := s.interactions().Doc(interactionID).Delete(ctx); err != nil {
		return fmt.Errorf("delete interaction: %w", err)
	}
	return nil
}

func normalize(email string) string {
	return strings.ToLower(strings.TrimSpace(email))
}

func normalizeSearch(lead models.Lead) string {
	return strings.ToLower(strings.Join([]string{
		lead.FullName,
		lead.Email,
		lead.Company,
		lead.Phone,
		lead.Role,
		lead.Source,
	}, " "))
}
