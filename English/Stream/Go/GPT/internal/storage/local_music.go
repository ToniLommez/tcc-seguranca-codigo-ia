package storage

import (
	"errors"
	"io"
	"mime/multipart"
	"os"
	"path/filepath"
	"strings"

	"github.com/google/uuid"
)

type LocalMusicStorage struct {
	uploadDir string
}

func NewLocalMusicStorage(uploadDir string) *LocalMusicStorage {
	return &LocalMusicStorage{uploadDir: uploadDir}
}

func (s *LocalMusicStorage) SaveMP3(fileHeader *multipart.FileHeader) (string, error) {
	if err := os.MkdirAll(s.uploadDir, 0o755); err != nil {
		return "", err
	}

	src, err := fileHeader.Open()
	if err != nil {
		return "", err
	}
	defer src.Close()

	if err := validateMP3(src, fileHeader.Filename); err != nil {
		return "", err
	}

	fileName := uuid.NewString() + ".mp3"
	filePath := filepath.Join(s.uploadDir, fileName)

	dst, err := os.Create(filePath)
	if err != nil {
		return "", err
	}
	defer dst.Close()

	if _, err := src.Seek(0, io.SeekStart); err != nil {
		return "", err
	}

	if _, err := io.Copy(dst, src); err != nil {
		return "", err
	}

	return filePath, nil
}

func validateMP3(file multipart.File, originalName string) error {
	if strings.ToLower(filepath.Ext(originalName)) != ".mp3" {
		return errors.New("only MP3 files are allowed")
	}

	header := make([]byte, 10)
	n, err := file.Read(header)
	if err != nil && err != io.EOF {
		return err
	}

	header = header[:n]
	if len(header) >= 3 && string(header[:3]) == "ID3" {
		return nil
	}

	if len(header) >= 2 && header[0] == 0xFF && (header[1]&0xE0) == 0xE0 {
		return nil
	}

	return errors.New("invalid MP3 file")
}
