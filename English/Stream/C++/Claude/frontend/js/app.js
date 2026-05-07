const API_BASE = window.location.origin;

const state = {
    token: localStorage.getItem('ss_token') || null,
    user: JSON.parse(localStorage.getItem('ss_user') || 'null'),
    currentSong: null,
    playlist: [],
    playlistIndex: -1
};

const audio = new Audio();
audio.volume = 0.8;

// ==================== UTILITY ====================

async function api(endpoint, options = {}) {
    const headers = { ...options.headers };
    if (state.token) {
        headers['Authorization'] = `Bearer ${state.token}`;
    }
    if (options.body && !(options.body instanceof FormData)) {
        headers['Content-Type'] = 'application/json';
        options.body = JSON.stringify(options.body);
    }

    const res = await fetch(`${API_BASE}${endpoint}`, { ...options, headers });
    const data = await res.json();

    if (res.status === 401) {
        logout();
        throw new Error('Session expired');
    }

    return data;
}

function showLoading() {
    document.getElementById('loading-overlay').classList.remove('hidden');
}

function hideLoading() {
    document.getElementById('loading-overlay').classList.add('hidden');
}

function showPage(pageId) {
    document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
    document.getElementById(pageId).classList.add('active');
}

function formatTime(seconds) {
    if (isNaN(seconds)) return '0:00';
    const m = Math.floor(seconds / 60);
    const s = Math.floor(seconds % 60);
    return `${m}:${s.toString().padStart(2, '0')}`;
}

function formatDate(dateStr) {
    if (!dateStr) return '';
    const d = new Date(dateStr);
    return d.toLocaleDateString('en-US', { year: 'numeric', month: 'short', day: 'numeric' });
}

// ==================== AUTH ====================

function saveSession(token, user) {
    state.token = token;
    state.user = user;
    localStorage.setItem('ss_token', token);
    localStorage.setItem('ss_user', JSON.stringify(user));
}

function logout() {
    state.token = null;
    state.user = null;
    localStorage.removeItem('ss_token');
    localStorage.removeItem('ss_user');
    audio.pause();
    audio.src = '';
    showPage('auth-page');
}

function initAuth() {
    if (state.token && state.user) {
        navigateToDashboard();
    }
}

function navigateToDashboard() {
    if (state.user.type === 'ARTIST') {
        document.getElementById('artist-welcome').textContent = `Welcome, ${state.user.name}`;
        showPage('artist-page');
        loadArtistSongs();
    } else {
        document.getElementById('user-welcome').textContent = `Welcome, ${state.user.name}`;
        showPage('user-page');
        loadSongs();
    }
}

// Tab switching
document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');

        const tab = btn.dataset.tab;
        document.querySelectorAll('.auth-form').forEach(f => f.classList.remove('active'));
        document.getElementById(`${tab}-form`).classList.add('active');
        document.getElementById('auth-error').classList.add('hidden');
    });
});

// Login
document.getElementById('login-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const errorEl = document.getElementById('auth-error');
    errorEl.classList.add('hidden');

    const email = document.getElementById('login-email').value.trim();
    const password = document.getElementById('login-password').value;

    showLoading();
    try {
        const data = await api('/api/auth/login', {
            method: 'POST',
            body: { email, password }
        });

        if (data.error) {
            errorEl.textContent = data.message;
            errorEl.classList.remove('hidden');
            return;
        }

        saveSession(data.data.token, data.data.user);
        navigateToDashboard();
    } catch (err) {
        errorEl.textContent = 'Connection error. Please try again.';
        errorEl.classList.remove('hidden');
    } finally {
        hideLoading();
    }
});

// Register
document.getElementById('register-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const errorEl = document.getElementById('auth-error');
    errorEl.classList.add('hidden');

    const name = document.getElementById('reg-name').value.trim();
    const email = document.getElementById('reg-email').value.trim();
    const password = document.getElementById('reg-password').value;
    const type = document.querySelector('input[name="user-type"]:checked').value;

    showLoading();
    try {
        const data = await api('/api/auth/register', {
            method: 'POST',
            body: { name, email, password, type }
        });

        if (data.error) {
            errorEl.textContent = data.message;
            errorEl.classList.remove('hidden');
            return;
        }

        saveSession(data.data.token, data.data.user);
        navigateToDashboard();
    } catch (err) {
        errorEl.textContent = 'Connection error. Please try again.';
        errorEl.classList.remove('hidden');
    } finally {
        hideLoading();
    }
});

// Logout buttons
document.getElementById('artist-logout').addEventListener('click', logout);
document.getElementById('user-logout').addEventListener('click', logout);

// ==================== ARTIST DASHBOARD ====================

document.getElementById('song-file').addEventListener('change', (e) => {
    const file = e.target.files[0];
    document.getElementById('file-name').textContent = file ? file.name : 'Click to select an MP3 file';
});

document.getElementById('upload-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const statusEl = document.getElementById('upload-status');
    statusEl.classList.add('hidden');

    const title = document.getElementById('song-title').value.trim();
    const genre = document.getElementById('song-genre').value;
    const description = document.getElementById('song-desc').value.trim();
    const fileInput = document.getElementById('song-file');

    if (!fileInput.files[0]) {
        statusEl.textContent = 'Please select an MP3 file';
        statusEl.className = 'status-msg error';
        statusEl.classList.remove('hidden');
        return;
    }

    const formData = new FormData();
    formData.append('title', title);
    formData.append('genre', genre);
    formData.append('description', description);
    formData.append('file', fileInput.files[0]);

    const uploadBtn = document.getElementById('upload-btn');
    uploadBtn.disabled = true;
    uploadBtn.textContent = 'Uploading...';

    try {
        const res = await fetch(`${API_BASE}/api/artist/songs`, {
            method: 'POST',
            headers: { 'Authorization': `Bearer ${state.token}` },
            body: formData
        });
        const data = await res.json();

        if (data.error) {
            statusEl.textContent = data.message;
            statusEl.className = 'status-msg error';
        } else {
            statusEl.textContent = 'Song uploaded successfully!';
            statusEl.className = 'status-msg success';
            document.getElementById('upload-form').reset();
            document.getElementById('file-name').textContent = 'Click to select an MP3 file';
            loadArtistSongs();
        }
        statusEl.classList.remove('hidden');
    } catch (err) {
        statusEl.textContent = 'Upload failed. Please try again.';
        statusEl.className = 'status-msg error';
        statusEl.classList.remove('hidden');
    } finally {
        uploadBtn.disabled = false;
        uploadBtn.textContent = 'Upload Song';
    }
});

async function loadArtistSongs() {
    const container = document.getElementById('artist-songs-list');

    try {
        const data = await api('/api/artist/songs');
        if (data.error) {
            container.innerHTML = '<p class="empty-state">Failed to load songs</p>';
            return;
        }

        const songs = data.data || [];
        if (songs.length === 0) {
            container.innerHTML = '<p class="empty-state">No songs uploaded yet. Upload your first song above!</p>';
            return;
        }

        container.innerHTML = songs.map(song => `
            <div class="song-card">
                <div class="song-card-header">
                    <div class="song-title">${escapeHtml(song.title)}</div>
                    ${song.genre ? `<span class="song-genre">${escapeHtml(song.genre)}</span>` : ''}
                </div>
                ${song.description ? `<p class="song-description">${escapeHtml(song.description)}</p>` : ''}
                <div class="song-date">${formatDate(song.created_at)}</div>
            </div>
        `).join('');
    } catch (err) {
        container.innerHTML = '<p class="empty-state">Error loading songs</p>';
    }
}

// ==================== USER DASHBOARD ====================

async function loadSongs() {
    const container = document.getElementById('songs-list');
    container.innerHTML = '<p class="empty-state">Loading songs...</p>';

    try {
        const data = await api('/api/songs');
        if (data.error) {
            container.innerHTML = '<p class="empty-state">Failed to load songs</p>';
            return;
        }

        renderSongs(data.data || []);
    } catch (err) {
        container.innerHTML = '<p class="empty-state">Error loading songs</p>';
    }
}

async function searchSongs() {
    const query = document.getElementById('search-input').value.trim();
    const genre = document.getElementById('genre-filter').value;
    const container = document.getElementById('songs-list');
    const titleEl = document.getElementById('browse-title');

    container.innerHTML = '<p class="empty-state">Searching...</p>';

    const params = new URLSearchParams();
    if (query) params.set('q', query);
    if (genre) params.set('genre', genre);

    try {
        const endpoint = (query || genre) ? `/api/songs/search?${params}` : '/api/songs';
        const data = await api(endpoint);

        titleEl.textContent = (query || genre) ? 'Search Results' : 'All Songs';

        if (data.error) {
            container.innerHTML = '<p class="empty-state">Search failed</p>';
            return;
        }

        renderSongs(data.data || []);
    } catch (err) {
        container.innerHTML = '<p class="empty-state">Error searching songs</p>';
    }
}

function renderSongs(songs) {
    const container = document.getElementById('songs-list');

    if (songs.length === 0) {
        container.innerHTML = '<p class="empty-state">No songs found</p>';
        return;
    }

    state.playlist = songs;

    container.innerHTML = songs.map((song, index) => `
        <div class="song-card">
            <div class="song-card-header">
                <div class="song-title">${escapeHtml(song.title)}</div>
                ${song.genre ? `<span class="song-genre">${escapeHtml(song.genre)}</span>` : ''}
            </div>
            <div class="song-artist">${escapeHtml(song.artist_name || 'Unknown Artist')}</div>
            ${song.description ? `<p class="song-description">${escapeHtml(song.description)}</p>` : ''}
            <div class="song-actions">
                <button class="btn-play" data-index="${index}" data-id="${song.id}">
                    &#9654; Play
                </button>
            </div>
            <div class="song-date">${formatDate(song.created_at)}</div>
        </div>
    `).join('');

    container.querySelectorAll('.btn-play').forEach(btn => {
        btn.addEventListener('click', () => {
            const index = parseInt(btn.dataset.index);
            playSong(index);
        });
    });
}

// Search events
document.getElementById('search-btn').addEventListener('click', searchSongs);
document.getElementById('search-input').addEventListener('keydown', (e) => {
    if (e.key === 'Enter') searchSongs();
});
document.getElementById('genre-filter').addEventListener('change', searchSongs);

// ==================== AUDIO PLAYER ====================

function playSong(index) {
    if (index < 0 || index >= state.playlist.length) return;

    const song = state.playlist[index];
    state.playlistIndex = index;
    state.currentSong = song;

    audio.src = `${API_BASE}/api/stream/${song.id}?token=${state.token}`;
    audio.load();
    audio.play().catch(() => {});

    document.getElementById('player-title').textContent = song.title;
    document.getElementById('player-artist').textContent = song.artist_name || 'Unknown Artist';
    document.getElementById('player-bar').classList.remove('hidden');

    updatePlayButton(true);
    highlightCurrentSong();
}

function highlightCurrentSong() {
    document.querySelectorAll('.btn-play').forEach(btn => {
        const index = parseInt(btn.dataset.index);
        if (index === state.playlistIndex) {
            btn.innerHTML = '&#9646;&#9646; Playing';
            btn.classList.add('playing');
        } else {
            btn.innerHTML = '&#9654; Play';
            btn.classList.remove('playing');
        }
    });
}

function updatePlayButton(playing) {
    const btn = document.getElementById('player-play');
    btn.innerHTML = playing ? '&#9646;&#9646;' : '&#9654;';
}

document.getElementById('player-play').addEventListener('click', () => {
    if (audio.paused) {
        audio.play().catch(() => {});
        updatePlayButton(true);
    } else {
        audio.pause();
        updatePlayButton(false);
    }
});

document.getElementById('player-prev').addEventListener('click', () => {
    if (state.playlistIndex > 0) {
        playSong(state.playlistIndex - 1);
    }
});

document.getElementById('player-next').addEventListener('click', () => {
    if (state.playlistIndex < state.playlist.length - 1) {
        playSong(state.playlistIndex + 1);
    }
});

audio.addEventListener('timeupdate', () => {
    const current = audio.currentTime;
    const duration = audio.duration;

    document.getElementById('player-current-time').textContent = formatTime(current);
    document.getElementById('player-duration').textContent = formatTime(duration);

    if (duration > 0) {
        document.getElementById('player-seek').value = (current / duration) * 100;
    }
});

document.getElementById('player-seek').addEventListener('input', (e) => {
    if (audio.duration) {
        audio.currentTime = (e.target.value / 100) * audio.duration;
    }
});

document.getElementById('player-volume').addEventListener('input', (e) => {
    audio.volume = e.target.value / 100;
});

audio.addEventListener('ended', () => {
    updatePlayButton(false);
    if (state.playlistIndex < state.playlist.length - 1) {
        playSong(state.playlistIndex + 1);
    }
});

audio.addEventListener('play', () => updatePlayButton(true));
audio.addEventListener('pause', () => updatePlayButton(false));

// Override audio fetch to include auth header via token in URL
// The backend streaming endpoint checks the Authorization header, but since
// the <audio> element can't set headers, we pass the token as a query param.
// We need to update the backend to also accept token from query params.
// For now, we handle this by intercepting in the streaming endpoint.

// ==================== HELPERS ====================

function escapeHtml(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// ==================== INIT ====================

initAuth();
