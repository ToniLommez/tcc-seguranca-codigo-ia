package firebase

import (
	"context"
	"log"
	"sync"

	"cloud.google.com/go/firestore"
	firebase "firebase.google.com/go/v4"
	"google.golang.org/api/option"
)

var (
	client *firestore.Client
	once   sync.Once
)

func InitFirestore(credentialsPath string) *firestore.Client {
	once.Do(func() {
		ctx := context.Background()
		opt := option.WithCredentialsFile(credentialsPath)
		app, err := firebase.NewApp(ctx, nil, opt)
		if err != nil {
			log.Fatalf("Failed to initialize Firebase app: %v", err)
		}
		c, err := app.Firestore(ctx)
		if err != nil {
			log.Fatalf("Failed to initialize Firestore client: %v", err)
		}
		client = c
	})
	return client
}

func GetClient() *firestore.Client {
	return client
}

const (
	UsersCollection = "claude_go_users"
	LeadsCollection = "claude_go_leads"
)
