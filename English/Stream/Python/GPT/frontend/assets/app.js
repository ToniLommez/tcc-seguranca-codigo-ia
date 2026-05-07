const state = {
    token: localStorage.getItem("stream_token") || "",
    user: JSON.parse(localStorage.getItem("stream_user") || "null"),
};

const authPanel = document.getElementById("auth-panel");
const dashboard = document.getElementById("dashboard");
const loginForm = document.getElementById("login-form");
const registerForm = document.getElementById("register-form");
const showLoginButton = document.getElementById("show-login");
const showRegisterButton = document.getElementById("show-register");
const alertBox = document.getElementById("alert");
const dashboardTitle = document.getElementById("dashboard-title");
const dashboardSubtitle = document.getElementById("dashboard-subtitle");
const artistView = document.getElementById("artist-view");
const userView = document.getElementById("user-view");
const artistSongList = document.getElementById("artist-song-list");
const userSongList = document.getElementById("user-song-list");
const uploadForm = document.getElementById("upload-form");
const searchForm = document.getElementById("search-form");
const logoutButton = document.getElementById("logout-button");
const refreshArtistSongsButton = document.getElementById("refresh-artist-songs");
const refreshUserSongsButton = document.getElementById("refresh-user-songs");
const audioPlayer = document.getElementById("audio-player");
const nowPlaying = document.getElementById("now-playing");

function showAlert(message, type = "success") {
    alertBox.textContent = message;
    alertBox.className = `alert ${type}`;
}

function hideAlert() {
    alertBox.textContent = "";
    alertBox.className = "alert hidden";
}

function setTab(mode) {
    const isLogin = mode === "login";
    loginForm.classList.toggle("hidden", !isLogin);
    registerForm.classList.toggle("hidden", isLogin);
    showLoginButton.classList.toggle("active", isLogin);
    showRegisterButton.classList.toggle("active", !isLogin);
    hideAlert();
}

function persistSession(token, user) {
    state.token = token;
    state.user = user;
    localStorage.setItem("stream_token", token);
    localStorage.setItem("stream_user", JSON.stringify(user));
}

function clearSession() {
    state.token = "";
    state.user = null;
    localStorage.removeItem("stream_token");
    localStorage.removeItem("stream_user");
    audioPlayer.pause();
    audioPlayer.removeAttribute("src");
    nowPlaying.textContent = "Select a song to start playback.";
}

async function apiRequest(path, options = {}) {
    const headers = new Headers(options.headers || {});
    if (!(options.body instanceof FormData)) {
        headers.set("Content-Type", "application/json");
    }
    if (state.token) {
        headers.set("Authorization", `Bearer ${state.token}`);
    }

    const response = await fetch(path, { ...options, headers });
    if (!response.ok) {
        const payload = await response.json().catch(() => ({}));
        if (response.status === 401) {
            clearSession();
            renderDashboard();
            setTab("login");
        }
        throw new Error(payload.detail || "Request failed");
    }

    return response.status === 204 ? null : response.json();
}

function renderDashboard() {
    if (!state.user) {
        authPanel.classList.remove("hidden");
        dashboard.classList.add("hidden");
        artistView.classList.add("hidden");
        userView.classList.add("hidden");
        return;
    }

    authPanel.classList.add("hidden");
    dashboard.classList.remove("hidden");
    dashboardTitle.textContent = `Hello, ${state.user.name}`;
    dashboardSubtitle.textContent = `Logged in as ${state.user.type}.`;

    const isArtist = state.user.type === "ARTIST";
    artistView.classList.toggle("hidden", !isArtist);
    userView.classList.toggle("hidden", isArtist);

    if (isArtist) {
        loadArtistSongs();
    } else {
        loadUserSongs();
    }
}

function renderEmptyState(target, message) {
    target.innerHTML = `<div class="song-item"><p class="muted">${message}</p></div>`;
}

function createSongMarkup(song, withPlayButton = false) {
    const description = song.description
        ? `<p>${song.description}</p>`
        : `<p class="muted">No description provided.</p>`;
    const playButton = withPlayButton
        ? `<button class="primary-btn small play-button" data-id="${song.id}" data-title="${song.title}" data-artist="${song.artist_name}">Play song</button>`
        : "";

    return `
        <article class="song-item">
            <div>
                <h4>${song.title}</h4>
                ${description}
            </div>
            <div class="song-meta">
                <span><strong>Artist:</strong> ${song.artist_name}</span>
                <span><strong>Genre:</strong> ${song.genre}</span>
                <span><strong>File:</strong> ${song.original_file_name}</span>
            </div>
            <div class="song-actions">
                <span class="muted">${new Date(song.created_at).toLocaleString()}</span>
                ${playButton}
            </div>
        </article>
    `;
}

async function loadArtistSongs() {
    try {
        const songs = await apiRequest("/api/artist/songs");
        if (!songs.length) {
            renderEmptyState(artistSongList, "You have not uploaded any songs yet.");
            return;
        }
        artistSongList.innerHTML = songs.map((song) => createSongMarkup(song)).join("");
    } catch (error) {
        showAlert(error.message, "error");
    }
}

async function loadUserSongs(filters = {}) {
    try {
        const params = new URLSearchParams();
        Object.entries(filters).forEach(([key, value]) => {
            if (value) {
                params.set(key, value);
            }
        });
        const suffix = params.toString() ? `?${params.toString()}` : "";
        const songs = await apiRequest(`/api/songs${suffix}`);
        if (!songs.length) {
            renderEmptyState(userSongList, "No songs found for the current search.");
            return;
        }
        userSongList.innerHTML = songs
            .map((song) => createSongMarkup(song, true))
            .join("");
    } catch (error) {
        showAlert(error.message, "error");
    }
}

function playSong(songId, title, artist) {
    audioPlayer.src = `/api/songs/${songId}/stream?access_token=${encodeURIComponent(state.token)}`;
    audioPlayer.play();
    nowPlaying.textContent = `Now playing: ${title} by ${artist}`;
}

showLoginButton.addEventListener("click", () => setTab("login"));
showRegisterButton.addEventListener("click", () => setTab("register"));

loginForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    hideAlert();

    const formData = new FormData(loginForm);
    try {
        const response = await apiRequest("/api/auth/login", {
            method: "POST",
            body: JSON.stringify({
                email: formData.get("email"),
                password: formData.get("password"),
            }),
        });
        persistSession(response.access_token, response.user);
        loginForm.reset();
        renderDashboard();
    } catch (error) {
        showAlert(error.message, "error");
    }
});

registerForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    hideAlert();

    const formData = new FormData(registerForm);
    try {
        const response = await apiRequest("/api/auth/register", {
            method: "POST",
            body: JSON.stringify({
                name: formData.get("name"),
                email: formData.get("email"),
                password: formData.get("password"),
                type: formData.get("type"),
            }),
        });
        persistSession(response.access_token, response.user);
        registerForm.reset();
        renderDashboard();
    } catch (error) {
        showAlert(error.message, "error");
    }
});

uploadForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    hideAlert();

    const formData = new FormData(uploadForm);
    try {
        await apiRequest("/api/artist/songs", {
            method: "POST",
            body: formData,
        });
        uploadForm.reset();
        showAlert("Song registered successfully.");
        loadArtistSongs();
    } catch (error) {
        showAlert(error.message, "error");
    }
});

searchForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    hideAlert();

    const formData = new FormData(searchForm);
    await loadUserSongs({
        title: formData.get("title")?.toString().trim(),
        artist: formData.get("artist")?.toString().trim(),
        genre: formData.get("genre")?.toString().trim(),
    });
});

userSongList.addEventListener("click", (event) => {
    const button = event.target.closest(".play-button");
    if (!button) {
        return;
    }

    playSong(button.dataset.id, button.dataset.title, button.dataset.artist);
});

refreshArtistSongsButton.addEventListener("click", () => loadArtistSongs());
refreshUserSongsButton.addEventListener("click", () => loadUserSongs());

logoutButton.addEventListener("click", () => {
    clearSession();
    hideAlert();
    renderDashboard();
    setTab("login");
});

setTab("login");
renderDashboard();
