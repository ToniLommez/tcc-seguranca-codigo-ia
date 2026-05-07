package music

import (
	"bytes"
	"io"
)

func LooksLikeMP3(file io.ReadSeeker) (bool, error) {
	defer file.Seek(0, io.SeekStart)

	header := make([]byte, 10)
	n, err := file.Read(header)
	if err != nil && err != io.EOF {
		return false, err
	}
	header = header[:n]

	if len(header) >= 3 && bytes.Equal(header[:3], []byte("ID3")) {
		return true, nil
	}

	if len(header) >= 2 && header[0] == 0xFF && (header[1]&0xE0) == 0xE0 {
		return true, nil
	}

	return false, nil
}
