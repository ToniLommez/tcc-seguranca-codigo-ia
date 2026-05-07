package music

import (
	"bytes"
	"testing"
)

func TestLooksLikeMP3WithID3Header(t *testing.T) {
	ok, err := LooksLikeMP3(bytes.NewReader([]byte("ID3abcdef")))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !ok {
		t.Fatal("expected ID3 header to be accepted")
	}
}

func TestLooksLikeMP3WithFrameHeader(t *testing.T) {
	ok, err := LooksLikeMP3(bytes.NewReader([]byte{0xFF, 0xFB, 0x90, 0x64}))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !ok {
		t.Fatal("expected MPEG frame header to be accepted")
	}
}

func TestLooksLikeMP3RejectsRandomData(t *testing.T) {
	ok, err := LooksLikeMP3(bytes.NewReader([]byte("not-mp3")))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if ok {
		t.Fatal("expected random content to be rejected")
	}
}
