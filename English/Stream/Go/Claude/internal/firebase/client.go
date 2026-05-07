package firebase

import (
	"context"
	"fmt"
	"strings"
	"time"

	"cloud.google.com/go/firestore"
	fb "firebase.google.com/go/v4"
	"google.golang.org/api/iterator"
	"google.golang.org/api/option"

	"music-stream/internal/models"
)

type Client struct {
	firestore *firestore.Client
}

func NewClient(credentialsFile string) (*Client, error) {
	ctx := context.Background()
	opt := option.WithCredentialsFile(credentialsFile)

	app, err := fb.NewApp(ctx, nil, opt)
	if err != nil {
		return nil, fmt.Errorf("failed to initialize firebase app: %w", err)
	}

	fs, err := app.Firestore(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to initialize firestore: %w", err)
	}

	return &Client{firestore: fs}, nil
}

func (c *Client) Close() error {
	return c.firestore.Close()
}

type userDoc struct {
	Name      string `firestore:"name"`
	Email     string `firestore:"email"`
	Password  string `firestore:"password"`
	Type      string `firestore:"type"`
	CreatedAt string `firestore:"created_at"`
}

type songDoc struct {
	Title       string `firestore:"title"`
	Genre       string `firestore:"genre"`
	Description string `firestore:"description"`
	ArtistID    string `firestore:"artist_id"`
	ArtistName  string `firestore:"artist_name"`
	FilePath    string `firestore:"file_path"`
	FileName    string `firestore:"file_name"`
	CreatedAt   string `firestore:"created_at"`
}

func (c *Client) CreateUser(ctx context.Context, user *models.User) (string, error) {
	doc := userDoc{
		Name:      user.Name,
		Email:     user.Email,
		Password:  user.Password,
		Type:      string(user.Type),
		CreatedAt: time.Now().Format(time.RFC3339),
	}
	ref, _, err := c.firestore.Collection("users").Add(ctx, doc)
	if err != nil {
		return "", fmt.Errorf("failed to create user: %w", err)
	}
	return ref.ID, nil
}

func (c *Client) GetUserByEmail(ctx context.Context, email string) (*models.User, error) {
	iter := c.firestore.Collection("users").Where("email", "==", email).Limit(1).Documents(ctx)
	doc, err := iter.Next()
	if err == iterator.Done {
		return nil, fmt.Errorf("user not found")
	}
	if err != nil {
		return nil, fmt.Errorf("failed to query user: %w", err)
	}

	return parseUserDoc(doc)
}

func (c *Client) GetUserByID(ctx context.Context, id string) (*models.User, error) {
	doc, err := c.firestore.Collection("users").Doc(id).Get(ctx)
	if err != nil {
		return nil, fmt.Errorf("user not found: %w", err)
	}

	return parseUserDoc(doc)
}

func parseUserDoc(doc *firestore.DocumentSnapshot) (*models.User, error) {
	var d userDoc
	if err := doc.DataTo(&d); err != nil {
		return nil, fmt.Errorf("failed to parse user: %w", err)
	}
	createdAt, _ := time.Parse(time.RFC3339, d.CreatedAt)
	return &models.User{
		ID:        doc.Ref.ID,
		Name:      d.Name,
		Email:     d.Email,
		Password:  d.Password,
		Type:      models.UserType(d.Type),
		CreatedAt: createdAt,
	}, nil
}

func (c *Client) CreateSong(ctx context.Context, song *models.Song) (string, error) {
	doc := songDoc{
		Title:       song.Title,
		Genre:       song.Genre,
		Description: song.Description,
		ArtistID:    song.ArtistID,
		ArtistName:  song.ArtistName,
		FilePath:    song.FilePath,
		FileName:    song.FileName,
		CreatedAt:   time.Now().Format(time.RFC3339),
	}
	ref, _, err := c.firestore.Collection("songs").Add(ctx, doc)
	if err != nil {
		return "", fmt.Errorf("failed to create song: %w", err)
	}
	return ref.ID, nil
}

func (c *Client) ListSongs(ctx context.Context) ([]models.Song, error) {
	iter := c.firestore.Collection("songs").OrderBy("created_at", firestore.Desc).Documents(ctx)
	return c.parseSongs(iter)
}

func (c *Client) SearchSongs(ctx context.Context, query, field string) ([]models.Song, error) {
	allSongs, err := c.ListSongs(ctx)
	if err != nil {
		return nil, err
	}

	q := strings.ToLower(query)
	var results []models.Song
	for _, s := range allSongs {
		match := false
		switch field {
		case "title":
			match = strings.Contains(strings.ToLower(s.Title), q)
		case "artist":
			match = strings.Contains(strings.ToLower(s.ArtistName), q)
		case "genre":
			match = strings.Contains(strings.ToLower(s.Genre), q)
		default:
			match = strings.Contains(strings.ToLower(s.Title), q) ||
				strings.Contains(strings.ToLower(s.ArtistName), q) ||
				strings.Contains(strings.ToLower(s.Genre), q)
		}
		if match {
			results = append(results, s)
		}
	}
	return results, nil
}

func (c *Client) GetSong(ctx context.Context, id string) (*models.Song, error) {
	doc, err := c.firestore.Collection("songs").Doc(id).Get(ctx)
	if err != nil {
		return nil, fmt.Errorf("song not found: %w", err)
	}

	return parseSongDoc(doc)
}

func (c *Client) GetSongsByArtist(ctx context.Context, artistID string) ([]models.Song, error) {
	iter := c.firestore.Collection("songs").Where("artist_id", "==", artistID).OrderBy("created_at", firestore.Desc).Documents(ctx)
	return c.parseSongs(iter)
}

func (c *Client) DeleteSong(ctx context.Context, id string) error {
	_, err := c.firestore.Collection("songs").Doc(id).Delete(ctx)
	return err
}

func parseSongDoc(doc *firestore.DocumentSnapshot) (*models.Song, error) {
	var d songDoc
	if err := doc.DataTo(&d); err != nil {
		return nil, fmt.Errorf("failed to parse song: %w", err)
	}
	createdAt, _ := time.Parse(time.RFC3339, d.CreatedAt)
	return &models.Song{
		ID:          doc.Ref.ID,
		Title:       d.Title,
		Genre:       d.Genre,
		Description: d.Description,
		ArtistID:    d.ArtistID,
		ArtistName:  d.ArtistName,
		FilePath:    d.FilePath,
		FileName:    d.FileName,
		CreatedAt:   createdAt,
	}, nil
}

func (c *Client) parseSongs(iter *firestore.DocumentIterator) ([]models.Song, error) {
	var songs []models.Song
	for {
		doc, err := iter.Next()
		if err == iterator.Done {
			break
		}
		if err != nil {
			return nil, fmt.Errorf("failed to iterate songs: %w", err)
		}
		song, err := parseSongDoc(doc)
		if err != nil {
			continue
		}
		songs = append(songs, *song)
	}
	return songs, nil
}
