package handlers

import (
	"fmt"
	"net/http"
	"os"
	"strconv"
	"strings"

	"github.com/gorilla/mux"

	"music-stream/internal/firebase"
)

type StreamHandler struct {
	fb *firebase.Client
}

func NewStreamHandler(fb *firebase.Client) *StreamHandler {
	return &StreamHandler{fb: fb}
}

func (h *StreamHandler) StreamSong(w http.ResponseWriter, r *http.Request) {
	songID := mux.Vars(r)["id"]

	song, err := h.fb.GetSong(r.Context(), songID)
	if err != nil {
		respondError(w, http.StatusNotFound, "song not found")
		return
	}

	file, err := os.Open(song.FilePath)
	if err != nil {
		respondError(w, http.StatusInternalServerError, "audio file not available")
		return
	}
	defer file.Close()

	stat, err := file.Stat()
	if err != nil {
		respondError(w, http.StatusInternalServerError, "failed to read file info")
		return
	}

	fileSize := stat.Size()

	rangeHeader := r.Header.Get("Range")
	if rangeHeader == "" {
		w.Header().Set("Content-Type", "audio/mpeg")
		w.Header().Set("Content-Length", strconv.FormatInt(fileSize, 10))
		w.Header().Set("Accept-Ranges", "bytes")
		http.ServeContent(w, r, song.FileName, stat.ModTime(), file)
		return
	}

	rangeParts := strings.Replace(rangeHeader, "bytes=", "", 1)
	rangeBounds := strings.Split(rangeParts, "-")

	start, err := strconv.ParseInt(rangeBounds[0], 10, 64)
	if err != nil {
		respondError(w, http.StatusBadRequest, "invalid range")
		return
	}

	var end int64
	if len(rangeBounds) > 1 && rangeBounds[1] != "" {
		end, err = strconv.ParseInt(rangeBounds[1], 10, 64)
		if err != nil {
			respondError(w, http.StatusBadRequest, "invalid range")
			return
		}
	} else {
		end = fileSize - 1
	}

	if start > end || start >= fileSize {
		w.Header().Set("Content-Range", fmt.Sprintf("bytes */%d", fileSize))
		w.WriteHeader(http.StatusRequestedRangeNotSatisfiable)
		return
	}

	contentLength := end - start + 1

	w.Header().Set("Content-Type", "audio/mpeg")
	w.Header().Set("Accept-Ranges", "bytes")
	w.Header().Set("Content-Length", strconv.FormatInt(contentLength, 10))
	w.Header().Set("Content-Range", fmt.Sprintf("bytes %d-%d/%d", start, end, fileSize))
	w.WriteHeader(http.StatusPartialContent)

	file.Seek(start, 0)
	buf := make([]byte, 8192)
	remaining := contentLength
	for remaining > 0 {
		toRead := int64(len(buf))
		if toRead > remaining {
			toRead = remaining
		}
		n, err := file.Read(buf[:toRead])
		if n > 0 {
			w.Write(buf[:n])
			remaining -= int64(n)
		}
		if err != nil {
			break
		}
	}
}
