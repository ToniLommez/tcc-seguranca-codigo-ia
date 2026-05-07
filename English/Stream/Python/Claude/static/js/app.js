const API = "";
let token = localStorage.getItem("token");
let user = JSON.parse(localStorage.getItem("user") || "null");
let audio = new Audio();
let currentSong = null;

// --- Helpers ---
async function api(path, options = {}) {
    const headers = options.headers || {};
    if (token) headers["Authorization"] = `Bearer ${token}`;
    if (!(options.body instanceof FormData)) {
        headers["Content-Type"] = "application/json";
    }
    const res = await fetch(`${API}${path}`, { ...options, headers });
    const data = await res.json();
    if (!res.ok) throw new Error(data.error || "Request failed");
    return data;
}

function $(id) { return document.getElementById(id); }
function show(el) { el.classList.remove("hidden"); }
function hide(el) { el.classList.add("hidden"); }

function showAlert(containerId, message, type = "error") {
    const container = $(containerId);
    container.innerHTML = `<div class="alert alert-${type}">${message}</div>`;
    setTimeout(() => container.innerHTML = "", 5000);
}

function formatTime(secs) {
    if (isNaN(secs)) return "0:00";
    const m = Math.floor(secs / 60);
    const s = Math.floor(secs % 60);
    return `${m}:${s.toString().padStart(2, "0")}`;
}

// --- Auth ---
function initAuth() {
    $("tab-login").addEventListener("click", () => {
        $("tab-login").classList.add("active");
        $("tab-register").classList.remove("active");
        show($("login-form"));
        hide($("register-form"));
    });
    $("tab-register").addEventListener("click", () => {
        $("tab-register").classList.add("active");
        $("tab-login").classList.remove("active");
        hide($("login-form"));
        show($("register-form"));
    });

    $("login-form").addEventListener("submit", async (e) => {
        e.preventDefault();
        try {
            const data = await api("/api/auth/login", {
                method: "POST",
                body: JSON.stringify({
                    email: $("login-email").value,
                    password: $("login-password").value
                })
            });
            token = data.token;
            user = data.user;
            localStorage.setItem("token", token);
            localStorage.setItem("user", JSON.stringify(user));
            enterApp();
        } catch (err) {
            showAlert("auth-alert", err.message);
        }
    });

    $("register-form").addEventListener("submit", async (e) => {
        e.preventDefault();
        try {
            await api("/api/auth/register", {
                method: "POST",
                body: JSON.stringify({
                    name: $("reg-name").value,
                    email: $("reg-email").value,
                    password: $("reg-password").value,
                    user_type: $("reg-type").value
                })
            });
            showAlert("auth-alert", "Registration successful! Please login.", "success");
            $("tab-login").click();
        } catch (err) {
            showAlert("auth-alert", err.message);
        }
    });
}

function logout() {
    token = null;
    user = null;
    localStorage.removeItem("token");
    localStorage.removeItem("user");
    audio.pause();
    audio.src = "";
    currentSong = null;
    show($("auth-page"));
    hide($("app-page"));
    hide($("player-bar"));
}

// --- App ---
function enterApp() {
    hide($("auth-page"));
    show($("app-page"));

    $("user-name").textContent = user.name;
    $("user-type").textContent = user.user_type;
    $("user-avatar-letter").textContent = user.name.charAt(0).toUpperCase();

    const isArtist = user.user_type === "ARTIST";

    if (isArtist) {
        show($("nav-upload"));
        show($("nav-my-songs"));
        hide($("nav-browse"));
        hide($("player-bar"));
        navigateTo("upload");
    } else {
        hide($("nav-upload"));
        hide($("nav-my-songs"));
        show($("nav-browse"));
        show($("player-bar"));
        navigateTo("browse");
    }
}

function navigateTo(page) {
    document.querySelectorAll(".nav-item").forEach(n => n.classList.remove("active"));
    document.querySelectorAll(".content-section").forEach(s => hide(s));

    const navEl = $(`nav-${page}`);
    if (navEl) navEl.classList.add("active");
    show($(`section-${page}`));

    if (page === "browse") loadSongs();
    if (page === "my-songs") loadMySongs();
}

// --- Artist: Upload ---
function initUpload() {
    const fileInput = $("upload-file");
    const fileName = $("file-name");

    fileInput.addEventListener("change", () => {
        if (fileInput.files.length > 0) {
            fileName.textContent = fileInput.files[0].name;
        } else {
            fileName.textContent = "";
        }
    });

    $("upload-form").addEventListener("submit", async (e) => {
        e.preventDefault();
        const form = new FormData();
        form.append("title", $("upload-title").value);
        form.append("genre", $("upload-genre").value);
        form.append("description", $("upload-desc").value);
        form.append("file", fileInput.files[0]);

        try {
            const data = await api("/api/artist/songs", {
                method: "POST",
                body: form
            });
            showAlert("upload-alert", `"${data.title}" uploaded successfully!`, "success");
            $("upload-form").reset();
            fileName.textContent = "";
        } catch (err) {
            showAlert("upload-alert", err.message);
        }
    });
}

// --- Artist: My Songs ---
async function loadMySongs() {
    const container = $("my-songs-container");
    container.innerHTML = '<div style="text-align:center;padding:40px"><div class="loading-spinner"></div></div>';

    try {
        const songs = await api("/api/artist/songs");
        if (songs.length === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 18V5l12-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="18" cy="16" r="3"/></svg>
                    <h3>No songs yet</h3>
                    <p>Upload your first song to get started</p>
                </div>`;
            return;
        }
        container.innerHTML = '<div class="my-songs-list">' + songs.map(s => `
            <div class="my-song-row">
                <div class="my-song-info">
                    <div class="my-song-icon">
                        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 18V5l12-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="18" cy="16" r="3"/></svg>
                    </div>
                    <div class="my-song-details">
                        <h4>${esc(s.title)}</h4>
                        <span>${esc(s.genre)}${s.description ? " — " + esc(s.description) : ""}</span>
                    </div>
                </div>
                <button class="btn-delete-song" onclick="deleteSong('${s.id}')">
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="14" height="14"><path d="M3 6h18M19 6v14a2 2 0 01-2 2H7a2 2 0 01-2-2V6m3 0V4a2 2 0 012-2h4a2 2 0 012 2v2"/></svg>
                    Delete
                </button>
            </div>
        `).join("") + '</div>';
    } catch (err) {
        container.innerHTML = `<div class="alert alert-error">${err.message}</div>`;
    }
}

async function deleteSong(id) {
    if (!confirm("Are you sure you want to delete this song?")) return;
    try {
        await api(`/api/artist/songs/${id}`, { method: "DELETE" });
        loadMySongs();
    } catch (err) {
        alert(err.message);
    }
}

// --- User: Browse Songs ---
async function loadSongs() {
    const container = $("songs-container");
    container.innerHTML = '<div style="text-align:center;padding:40px"><div class="loading-spinner"></div></div>';

    const search = $("search-input").value;
    const genre = $("genre-filter").value;

    let query = "/api/songs?";
    if (search) query += `search=${encodeURIComponent(search)}&`;
    if (genre) query += `genre=${encodeURIComponent(genre)}`;

    try {
        const songs = await api(query);
        loadGenres();

        if (songs.length === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="8"/><path d="M21 21l-4.35-4.35"/></svg>
                    <h3>No songs found</h3>
                    <p>Try a different search or filter</p>
                </div>`;
            return;
        }

        container.innerHTML = '<div class="songs-grid">' + songs.map(s => `
            <div class="song-card">
                <div class="song-card-header">
                    <div>
                        <div class="song-title">${esc(s.title)}</div>
                        <div class="song-artist">${esc(s.artist_name)}</div>
                    </div>
                    <span class="song-genre">${esc(s.genre)}</span>
                </div>
                ${s.description ? `<div class="song-description">${esc(s.description)}</div>` : ""}
                <div class="song-actions">
                    <button class="btn-play" onclick='playSong(${JSON.stringify(s).replace(/'/g, "&#39;")})'>
                        <svg viewBox="0 0 24 24" fill="currentColor"><polygon points="5 3 19 12 5 21 5 3"/></svg>
                        Play
                    </button>
                </div>
            </div>
        `).join("") + '</div>';
    } catch (err) {
        container.innerHTML = `<div class="alert alert-error">${err.message}</div>`;
    }
}

async function loadGenres() {
    try {
        const genres = await api("/api/songs/genres");
        const select = $("genre-filter");
        const current = select.value;
        select.innerHTML = '<option value="">All Genres</option>' +
            genres.map(g => `<option value="${esc(g)}" ${g === current ? "selected" : ""}>${esc(g)}</option>`).join("");
    } catch (_) {}
}

// --- Player ---
function initPlayer() {
    const progress = $("player-seek");
    const volume = $("player-volume");

    audio.addEventListener("timeupdate", () => {
        if (audio.duration) {
            progress.value = (audio.currentTime / audio.duration) * 100;
            $("player-current").textContent = formatTime(audio.currentTime);
        }
    });

    audio.addEventListener("loadedmetadata", () => {
        $("player-duration").textContent = formatTime(audio.duration);
    });

    audio.addEventListener("ended", () => {
        $("play-pause-icon").innerHTML = '<polygon points="5 3 19 12 5 21 5 3"/>';
    });

    progress.addEventListener("input", () => {
        if (audio.duration) {
            audio.currentTime = (progress.value / 100) * audio.duration;
        }
    });

    volume.addEventListener("input", () => {
        audio.volume = volume.value / 100;
    });

    audio.volume = 0.7;
    volume.value = 70;
}

function playSong(song) {
    currentSong = song;
    $("player-title").textContent = song.title;
    $("player-artist").textContent = song.artist_name;
    show($("player-bar"));

    audio.src = `/api/stream/${song.id}?token=${token}`;
    audio.play();
    $("play-pause-icon").innerHTML = '<rect x="6" y="4" width="4" height="16"/><rect x="14" y="4" width="4" height="16"/>';
}

function togglePlayPause() {
    if (!currentSong) return;
    if (audio.paused) {
        audio.play();
        $("play-pause-icon").innerHTML = '<rect x="6" y="4" width="4" height="16"/><rect x="14" y="4" width="4" height="16"/>';
    } else {
        audio.pause();
        $("play-pause-icon").innerHTML = '<polygon points="5 3 19 12 5 21 5 3"/>';
    }
}

function esc(str) {
    const div = document.createElement("div");
    div.textContent = str || "";
    return div.innerHTML;
}

// --- Search ---
function initSearch() {
    let debounce;
    $("search-input").addEventListener("input", () => {
        clearTimeout(debounce);
        debounce = setTimeout(loadSongs, 400);
    });
    $("genre-filter").addEventListener("change", loadSongs);
}

// --- Init ---
document.addEventListener("DOMContentLoaded", () => {
    initAuth();
    initUpload();
    initPlayer();
    initSearch();

    if (token && user) {
        enterApp();
    }
});
