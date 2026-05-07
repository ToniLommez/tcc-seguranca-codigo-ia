package config

import (
	"context"
	"fmt"
	"log"
	"os"

	"cloud.google.com/go/firestore"
	firebase "firebase.google.com/go/v4"
	"google.golang.org/api/option"
)

var (
	FirestoreClient *firestore.Client
	Ctx             = context.Background()
)

// Nomes das coleções no Firestore — prefixados para isolar do resto do projeto
const (
	CollectionUsers = "leadmanager_users"
	CollectionLeads = "leadmanager_leads"
)

// InitFirebase inicializa o cliente do Firebase/Firestore
func InitFirebase() error {
	credPath := os.Getenv("FIREBASE_CREDENTIALS_PATH")
	if credPath == "" {
		// Caminho padrão baseado nas credenciais fornecidas
		credPath = `C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json`
	}

	// Normaliza o caminho para o sistema operacional atual
	opt := option.WithCredentialsFile(credPath)

	app, err := firebase.NewApp(Ctx, nil, opt)
	if err != nil {
		return fmt.Errorf("erro ao inicializar app Firebase: %v", err)
	}

	FirestoreClient, err = app.Firestore(Ctx)
	if err != nil {
		return fmt.Errorf("erro ao inicializar Firestore: %v", err)
	}

	log.Printf("Firebase inicializado com sucesso (projeto: lead-manager-54595)")
	return nil
}

// CloseFirebase fecha a conexão com o Firestore
func CloseFirebase() {
	if FirestoreClient != nil {
		if err := FirestoreClient.Close(); err != nil {
			log.Printf("Erro ao fechar Firestore: %v", err)
		}
	}
}
