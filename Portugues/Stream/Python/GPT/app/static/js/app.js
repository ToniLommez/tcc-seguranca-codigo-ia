const state = {
    token: localStorage.getItem("streaming_token"),
    user: null,
    songsById: {},
};

const authView = document.getElementById("auth-view");
const dashboardView = document.getElementById("dashboard-view");
const artistDashboard = document.getElementById("artist-dashboard");
const userDashboard = document.getElementById("user-dashboard");
const toast = document.getElementById("toast");
const welcomeTitle = document.getElementById("welcome-title");
const welcomeSubtitle = document.getElementById("welcome-subtitle");
const artistSongList = document.getElementById("artist-song-list");
const catalogList = document.getElementById("catalog-list");
const audioPlayer = document.getElementById("audio-player");
const playerPanel = document.getElementById("player-panel");
const playerEmpty = document.getElementById("player-empty");
const playerTitle = document.getElementById("player-title");
const playerMeta = document.getElementById("player-meta");
const playerDescription = document.getElementById("player-description");

function showToast(message, variant = "") {
    toast.textContent = message;
    toast.className = `toast ${variant}`.trim();
    setTimeout(() => toast.classList.add("hidden"), 3200);
    toast.classList.remove("hidden");
}

function escapeHtml(value = "") {
    return String(value)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#039;");
}

async function api(path, options = {}) {
    const headers = new Headers(options.headers || {});
    if (!(options.body instanceof FormData)) {
        headers.set("Content-Type", "application/json");
    }
    if (state.token) {
        headers.set("Authorization", `Bearer ${state.token}`);
    }

    const response = await fetch(path, { ...options, headers });
    if (!response.ok) {
        const data = await response.json().catch(() => ({ detail: "Erro inesperado." }));
        throw new Error(data.detail || "Erro inesperado.");
    }
    return response.status === 204 ? null : response.json();
}

function switchTab(targetTab) {
    document.querySelectorAll(".tab-button").forEach((button) => {
        button.classList.toggle("active", button.dataset.tab === targetTab);
    });
    document.querySelectorAll(".tab-content").forEach((content) => {
        content.classList.toggle("active", content.id === `${targetTab}-form`);
    });
}

function setAuthenticatedLayout() {
    authView.classList.add("hidden");
    dashboardView.classList.remove("hidden");
    welcomeTitle.textContent = `Ola, ${state.user.name}`;
    welcomeSubtitle.textContent = `${state.user.email} • ${state.user.user_type}`;
    artistDashboard.classList.toggle("hidden", state.user.user_type !== "ARTISTA");
    userDashboard.classList.toggle("hidden", state.user.user_type !== "USUARIO");
}

function setLoggedOutLayout() {
    state.user = null;
    state.token = null;
    localStorage.removeItem("streaming_token");
    authView.classList.remove("hidden");
    dashboardView.classList.add("hidden");
    artistDashboard.classList.add("hidden");
    userDashboard.classList.add("hidden");
}

function renderSongList(container, songs, canPlay = false) {
    state.songsById = songs.reduce((accumulator, song) => {
        accumulator[song.id] = song;
        return accumulator;
    }, {});

    if (!songs.length) {
        container.innerHTML = `<div class="empty-state">Nenhuma musica encontrada.</div>`;
        return;
    }

    container.innerHTML = songs
        .map((song) => {
            const description = song.description ? `<p>${escapeHtml(song.description)}</p>` : "";
            const button = canPlay
                ? `<button class="secondary-button play-button" data-song-id="${song.id}">Ouvir agora</button>`
                : "";
            return `
                <article class="song-item">
                    <h4>${escapeHtml(song.title)}</h4>
                    <p><strong>Artista:</strong> ${escapeHtml(song.artist_name)}</p>
                    <p><strong>Genero:</strong> ${escapeHtml(song.genre)}</p>
                    ${description}
                    ${button}
                </article>
            `;
        })
        .join("");
}

async function loadCurrentUser() {
    if (!state.token) {
        setLoggedOutLayout();
        return;
    }

    try {
        state.user = await api("/api/auth/me");
        setAuthenticatedLayout();
        if (state.user.user_type === "ARTISTA") {
            await loadArtistSongs();
        } else {
            await loadSongs();
        }
    } catch (error) {
        setLoggedOutLayout();
        showToast(error.message);
    }
}

async function loadArtistSongs() {
    const songs = await api("/api/artists/songs/mine");
    renderSongList(artistSongList, songs, false);
}

async function loadSongs(filters = {}) {
    const params = new URLSearchParams();
    Object.entries(filters).forEach(([key, value]) => {
        if (value) {
            params.append(key, value);
        }
    });

    const query = params.toString() ? `?${params.toString()}` : "";
    const songs = await api(`/api/songs${query}`);
    renderSongList(catalogList, songs, true);
}

function playSong(songId) {
    const song = state.songsById[songId];
    if (!song) {
        return;
    }

    playerTitle.textContent = song.title;
    playerMeta.textContent = `${song.artist_name} • ${song.genre}`;
    playerDescription.textContent = song.description || "Sem descricao informada.";
    playerEmpty.classList.add("hidden");
    playerPanel.classList.remove("hidden");
    audioPlayer.src = `/api/songs/${songId}/stream?token=${encodeURIComponent(state.token)}`;
    audioPlayer.load();
    audioPlayer.play().catch(() => {
        showToast("Clique no player para iniciar a reproducao.");
    });
}

document.querySelectorAll(".tab-button").forEach((button) => {
    button.addEventListener("click", () => switchTab(button.dataset.tab));
});

document.getElementById("register-form").addEventListener("submit", async (event) => {
    event.preventDefault();
    const formData = new FormData(event.currentTarget);
    const payload = Object.fromEntries(formData.entries());

    try {
        await api("/api/auth/register", {
            method: "POST",
            body: JSON.stringify(payload),
        });
        showToast("Cadastro concluido com sucesso.", "success");
        switchTab("login");
        event.currentTarget.reset();
    } catch (error) {
        showToast(error.message);
    }
});

document.getElementById("login-form").addEventListener("submit", async (event) => {
    event.preventDefault();
    const formData = new FormData(event.currentTarget);
    const payload = Object.fromEntries(formData.entries());

    try {
        const data = await api("/api/auth/login", {
            method: "POST",
            body: JSON.stringify(payload),
        });
        state.token = data.access_token;
        state.user = data.user;
        localStorage.setItem("streaming_token", state.token);
        setAuthenticatedLayout();
        showToast("Login realizado com sucesso.", "success");
        event.currentTarget.reset();
        if (state.user.user_type === "ARTISTA") {
            await loadArtistSongs();
        } else {
            await loadSongs();
        }
    } catch (error) {
        showToast(error.message);
    }
});

document.getElementById("upload-form").addEventListener("submit", async (event) => {
    event.preventDefault();
    const formData = new FormData(event.currentTarget);

    try {
        await api("/api/artists/songs", {
            method: "POST",
            body: formData,
        });
        showToast("Musica cadastrada com sucesso.", "success");
        event.currentTarget.reset();
        await loadArtistSongs();
    } catch (error) {
        showToast(error.message);
    }
});

document.getElementById("search-form").addEventListener("submit", async (event) => {
    event.preventDefault();
    const formData = new FormData(event.currentTarget);
    const filters = Object.fromEntries(formData.entries());

    try {
        await loadSongs(filters);
    } catch (error) {
        showToast(error.message);
    }
});

document.getElementById("logout-button").addEventListener("click", () => {
    audioPlayer.pause();
    audioPlayer.removeAttribute("src");
    setLoggedOutLayout();
    showToast("Sessao encerrada.", "success");
});

catalogList.addEventListener("click", (event) => {
    const button = event.target.closest(".play-button");
    if (!button) {
        return;
    }
    playSong(button.dataset.songId);
});

loadCurrentUser();
