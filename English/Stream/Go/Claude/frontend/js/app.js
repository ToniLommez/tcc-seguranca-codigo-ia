const API = '';
let token = null;
let currentUser = null;
let audio = new Audio();
let currentSongId = null;
let searchTimeout = null;

// AUTH
let isLoginMode = true;

function toggleAuthMode() {
    isLoginMode = !isLoginMode;
    document.getElementById('auth-title').textContent = isLoginMode ? 'Sign In' : 'Create Account';
    document.getElementById('auth-subtitle').textContent = isLoginMode
        ? 'Welcome back to MusicStream'
        : 'Join MusicStream today';
    document.getElementById('auth-btn').textContent = isLoginMode ? 'Sign In' : 'Create Account';
    document.getElementById('auth-switch-text').textContent = isLoginMode
        ? "Don't have an account? "
        : 'Already have an account? ';
    document.getElementById('auth-switch-link').textContent = isLoginMode ? 'Create one' : 'Sign in';

    document.getElementById('name-group').classList.toggle('hidden', isLoginMode);
    document.getElementById('type-group').classList.toggle('hidden', isLoginMode);
    hideAlert('auth-alert');
}

function showAlert(id, message, type) {
    const el = document.getElementById(id);
    el.className = `alert alert-${type}`;
    el.textContent = message;
    el.classList.remove('hidden');
}

function hideAlert(id) {
    document.getElementById(id).classList.add('hidden');
}

document.getElementById('auth-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    hideAlert('auth-alert');

    const email = document.getElementById('email').value.trim();
    const password = document.getElementById('password').value;
    const btn = document.getElementById('auth-btn');

    btn.disabled = true;

    try {
        if (isLoginMode) {
            const res = await fetch(`${API}/api/auth/login`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ email, password })
            });
            const data = await res.json();
            if (!res.ok) throw new Error(data.error);
            handleAuthSuccess(data);
        } else {
            const name = document.getElementById('name').value.trim();
            const type = document.getElementById('user-type').value;

            if (!name) throw new Error('Name is required');

            const res = await fetch(`${API}/api/auth/register`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name, email, password, type })
            });
            const data = await res.json();
            if (!res.ok) throw new Error(data.error);
            handleAuthSuccess(data);
        }
    } catch (err) {
        showAlert('auth-alert', err.message, 'error');
    } finally {
        btn.disabled = false;
    }
});

function handleAuthSuccess(data) {
    token = data.token;
    currentUser = data.user;
    localStorage.setItem('token', token);
    localStorage.setItem('user', JSON.stringify(currentUser));
    showApp();
}

function logout() {
    token = null;
    currentUser = null;
    localStorage.removeItem('token');
    localStorage.removeItem('user');
    audio.pause();
    document.getElementById('audio-player').classList.remove('visible');
    document.getElementById('auth-page').classList.remove('hidden');
    document.getElementById('app-page').classList.add('hidden');
    document.getElementById('email').value = '';
    document.getElementById('password').value = '';
}

function showApp() {
    document.getElementById('auth-page').classList.add('hidden');
    document.getElementById('app-page').classList.remove('hidden');

    document.getElementById('sidebar-username').textContent = currentUser.name;
    const typeEl = document.getElementById('sidebar-user-type');
    typeEl.textContent = currentUser.type;
    typeEl.className = `user-type ${currentUser.type.toLowerCase()}`;

    buildNav();
}

function buildNav() {
    const nav = document.getElementById('sidebar-nav');
    nav.innerHTML = '';

    if (currentUser.type === 'ARTIST') {
        nav.innerHTML = `
            <button class="nav-item active" onclick="showPage('upload')" data-page="upload">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/></svg>
                Upload Song
            </button>
            <button class="nav-item" onclick="showPage('my-songs')" data-page="my-songs">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 18V5l12-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="18" cy="16" r="3"/></svg>
                My Songs
            </button>
        `;
        showPage('upload');
    } else {
        nav.innerHTML = `
            <button class="nav-item active" onclick="showPage('browse')" data-page="browse">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
                Browse Music
            </button>
        `;
        showPage('browse');
    }
}

function showPage(page) {
    document.querySelectorAll('.page').forEach(p => p.classList.add('hidden'));
    document.getElementById(`page-${page}`).classList.remove('hidden');

    document.querySelectorAll('.nav-item[data-page]').forEach(n => n.classList.remove('active'));
    const activeNav = document.querySelector(`.nav-item[data-page="${page}"]`);
    if (activeNav) activeNav.classList.add('active');

    if (page === 'my-songs') loadMySongs();
    if (page === 'browse') loadBrowseSongs();
}

// UPLOAD
document.getElementById('song-file').addEventListener('change', (e) => {
    const file = e.target.files[0];
    const zone = document.getElementById('file-upload-zone');
    const text = document.getElementById('file-upload-text');

    if (file) {
        zone.classList.add('has-file');
        text.innerHTML = `<span class="file-name">${file.name}</span>`;
    } else {
        zone.classList.remove('has-file');
        text.innerHTML = '<strong>Click to select</strong> or drag an MP3 file here';
    }
});

document.getElementById('upload-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    hideAlert('upload-alert');

    const title = document.getElementById('song-title').value.trim();
    const genre = document.getElementById('song-genre').value.trim();
    const description = document.getElementById('song-description').value.trim();
    const fileInput = document.getElementById('song-file');
    const btn = document.getElementById('upload-btn');

    if (!fileInput.files[0]) {
        showAlert('upload-alert', 'Please select an MP3 file', 'error');
        return;
    }

    btn.disabled = true;
    btn.textContent = 'Uploading...';

    const formData = new FormData();
    formData.append('title', title);
    formData.append('genre', genre);
    formData.append('description', description);
    formData.append('file', fileInput.files[0]);

    try {
        const res = await fetch(`${API}/api/artist/songs`, {
            method: 'POST',
            headers: { 'Authorization': `Bearer ${token}` },
            body: formData
        });
        const data = await res.json();
        if (!res.ok) throw new Error(data.error);

        showAlert('upload-alert', 'Song uploaded successfully!', 'success');
        document.getElementById('upload-form').reset();
        document.getElementById('file-upload-zone').classList.remove('has-file');
        document.getElementById('file-upload-text').innerHTML = '<strong>Click to select</strong> or drag an MP3 file here';
    } catch (err) {
        showAlert('upload-alert', err.message, 'error');
    } finally {
        btn.disabled = false;
        btn.textContent = 'Upload Song';
    }
});

// MY SONGS (ARTIST)
async function loadMySongs() {
    const container = document.getElementById('my-songs-list');
    container.innerHTML = '<div class="loading"><div class="spinner"></div></div>';

    try {
        const res = await fetch(`${API}/api/artist/songs`, {
            headers: { 'Authorization': `Bearer ${token}` }
        });
        const songs = await res.json();
        if (!res.ok) throw new Error(songs.error);

        if (songs.length === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5"><path d="M9 18V5l12-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="18" cy="16" r="3"/></svg>
                    <h3>No songs yet</h3>
                    <p>Upload your first song to get started</p>
                </div>
            `;
            return;
        }

        container.innerHTML = songs.map(song => `
            <div class="song-card">
                <div class="song-card-header">
                    <div class="song-title">${escapeHtml(song.title)}</div>
                    <span class="song-genre">${escapeHtml(song.genre)}</span>
                </div>
                ${song.description ? `<div class="song-description">${escapeHtml(song.description)}</div>` : ''}
                <div class="song-actions">
                    <button class="btn btn-danger" onclick="deleteSong('${song.id}')">Delete</button>
                </div>
            </div>
        `).join('');
    } catch (err) {
        container.innerHTML = `<div class="empty-state"><h3>Error</h3><p>${escapeHtml(err.message)}</p></div>`;
    }
}

async function deleteSong(id) {
    if (!confirm('Are you sure you want to delete this song?')) return;

    try {
        const res = await fetch(`${API}/api/artist/songs/${id}`, {
            method: 'DELETE',
            headers: { 'Authorization': `Bearer ${token}` }
        });
        if (!res.ok) {
            const data = await res.json();
            throw new Error(data.error);
        }
        loadMySongs();
    } catch (err) {
        alert('Failed to delete: ' + err.message);
    }
}

// BROWSE (USER)
async function loadBrowseSongs() {
    const container = document.getElementById('browse-songs-list');
    container.innerHTML = '<div class="loading"><div class="spinner"></div></div>';

    try {
        const res = await fetch(`${API}/api/songs`, {
            headers: { 'Authorization': `Bearer ${token}` }
        });
        const songs = await res.json();
        if (!res.ok) throw new Error(songs.error);
        renderBrowseSongs(songs);
    } catch (err) {
        container.innerHTML = `<div class="empty-state"><h3>Error</h3><p>${escapeHtml(err.message)}</p></div>`;
    }
}

document.getElementById('search-input').addEventListener('input', (e) => {
    clearTimeout(searchTimeout);
    searchTimeout = setTimeout(() => searchSongs(), 300);
});

document.getElementById('search-field').addEventListener('change', () => searchSongs());

async function searchSongs() {
    const query = document.getElementById('search-input').value.trim();
    const field = document.getElementById('search-field').value;

    if (!query) {
        loadBrowseSongs();
        return;
    }

    const container = document.getElementById('browse-songs-list');
    container.innerHTML = '<div class="loading"><div class="spinner"></div></div>';

    try {
        const params = new URLSearchParams({ q: query });
        if (field) params.set('field', field);

        const res = await fetch(`${API}/api/songs/search?${params}`, {
            headers: { 'Authorization': `Bearer ${token}` }
        });
        const songs = await res.json();
        if (!res.ok) throw new Error(songs.error);
        renderBrowseSongs(songs);
    } catch (err) {
        container.innerHTML = `<div class="empty-state"><h3>Error</h3><p>${escapeHtml(err.message)}</p></div>`;
    }
}

function renderBrowseSongs(songs) {
    const container = document.getElementById('browse-songs-list');

    if (!songs || songs.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
                <h3>No songs found</h3>
                <p>Try a different search or check back later</p>
            </div>
        `;
        return;
    }

    container.innerHTML = songs.map(song => `
        <div class="song-card">
            <div class="song-card-header">
                <div class="song-title">${escapeHtml(song.title)}</div>
                <span class="song-genre">${escapeHtml(song.genre)}</span>
            </div>
            <div class="song-artist">by ${escapeHtml(song.artist_name)}</div>
            ${song.description ? `<div class="song-description">${escapeHtml(song.description)}</div>` : ''}
            <div class="song-actions">
                <button class="btn-play ${currentSongId === song.id && !audio.paused ? 'playing' : ''}" onclick="playSong('${song.id}', '${escapeHtml(song.title)}', '${escapeHtml(song.artist_name)}')">
                    ${currentSongId === song.id && !audio.paused ? '&#9646;&#9646; Playing' : '&#9654; Play'}
                </button>
            </div>
        </div>
    `).join('');
}

// AUDIO PLAYER
function playSong(id, title, artist) {
    const player = document.getElementById('audio-player');

    if (currentSongId === id && !audio.paused) {
        audio.pause();
        updatePlayButtons();
        return;
    }

    if (currentSongId === id && audio.paused) {
        audio.play();
        updatePlayButtons();
        return;
    }

    currentSongId = id;
    audio.src = `${API}/api/stream/${id}?token=${token}`;
    audio.volume = document.getElementById('volume-slider').value;
    audio.play();

    document.getElementById('player-title').textContent = title;
    document.getElementById('player-artist').textContent = artist;
    player.classList.add('visible');

    updatePlayButtons();
}

audio.addEventListener('timeupdate', () => {
    if (audio.duration) {
        const pct = (audio.currentTime / audio.duration) * 100;
        document.getElementById('progress-fill').style.width = pct + '%';
        document.getElementById('current-time').textContent = formatTime(audio.currentTime);
        document.getElementById('total-time').textContent = formatTime(audio.duration);
    }
});

audio.addEventListener('ended', () => {
    updatePlayButtons();
});

audio.addEventListener('play', updatePlayButtons);
audio.addEventListener('pause', updatePlayButtons);

function togglePlayPause() {
    if (audio.paused) {
        audio.play();
    } else {
        audio.pause();
    }
}

function updatePlayButtons() {
    const isPlaying = !audio.paused;
    document.getElementById('play-icon').classList.toggle('hidden', isPlaying);
    document.getElementById('pause-icon').classList.toggle('hidden', !isPlaying);

    document.querySelectorAll('.btn-play').forEach(btn => {
        const songId = btn.getAttribute('onclick')?.match(/'([^']+)'/)?.[1];
        if (songId === currentSongId && isPlaying) {
            btn.classList.add('playing');
            btn.innerHTML = '&#9646;&#9646; Playing';
        } else {
            btn.classList.remove('playing');
            btn.innerHTML = '&#9654; Play';
        }
    });
}

function seekAudio(e) {
    const bar = document.getElementById('progress-bar');
    const rect = bar.getBoundingClientRect();
    const pct = (e.clientX - rect.left) / rect.width;
    audio.currentTime = pct * audio.duration;
}

function setVolume(val) {
    audio.volume = val;
}

function formatTime(seconds) {
    const m = Math.floor(seconds / 60);
    const s = Math.floor(seconds % 60);
    return `${m}:${s.toString().padStart(2, '0')}`;
}

function escapeHtml(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// INIT - check for stored session
(function init() {
    const storedToken = localStorage.getItem('token');
    const storedUser = localStorage.getItem('user');

    if (storedToken && storedUser) {
        token = storedToken;
        currentUser = JSON.parse(storedUser);
        showApp();
    }
})();
