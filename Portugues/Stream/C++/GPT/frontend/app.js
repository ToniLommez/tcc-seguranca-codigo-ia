const authView = document.querySelector("#auth-view");
const dashboardView = document.querySelector("#dashboard-view");
const artistArea = document.querySelector("#artist-area");
const userArea = document.querySelector("#user-area");
const welcomeTitle = document.querySelector("#welcome-title");
const userBadge = document.querySelector("#user-badge");
const toast = document.querySelector("#toast");
const audioPlayer = document.querySelector("#audio-player");
const playerCaption = document.querySelector("#player-caption");

const state = {
  token: localStorage.getItem("stream_token") || "",
  user: null,
};

function showToast(message, isError = false) {
  toast.textContent = message;
  toast.style.background = isError ? "#8a3413" : "#112434";
  toast.classList.remove("hidden");
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => toast.classList.add("hidden"), 3500);
}

async function api(path, options = {}) {
  const headers = new Headers(options.headers || {});
  if (!(options.body instanceof FormData)) {
    headers.set("Content-Type", "application/json");
  }
  if (state.token) {
    headers.set("Authorization", `Bearer ${state.token}`);
  }

  const response = await fetch(path, {
    ...options,
    headers,
  });

  const contentType = response.headers.get("Content-Type") || "";
  const data = contentType.includes("application/json") ? await response.json() : await response.text();

  if (!response.ok) {
    const message = typeof data === "string" ? data : data.error || "Erro inesperado.";
    throw new Error(message);
  }

  return data;
}

function saveSession(token, user) {
  state.token = token;
  state.user = user;
  localStorage.setItem("stream_token", token);
}

function clearSession() {
  state.token = "";
  state.user = null;
  localStorage.removeItem("stream_token");
  audioPlayer.pause();
  audioPlayer.removeAttribute("src");
  audioPlayer.load();
  render();
}

function renderArtistSongs(songs) {
  const container = document.querySelector("#artist-song-list");
  if (!songs.length) {
    container.className = "song-list empty-state";
    container.textContent = "Nenhuma musica cadastrada ainda.";
    return;
  }

  container.className = "song-list";
  container.innerHTML = songs
    .map(
      (song) => `
        <article class="song-item">
          <h4>${song.title}</h4>
          <div class="song-meta">
            <span>${song.genre}</span>
            <span>${new Date(song.createdAt).toLocaleString("pt-BR")}</span>
          </div>
          <p class="song-description">${song.description || "Sem descricao."}</p>
        </article>
      `
    )
    .join("");
}

function renderUserSongs(songs) {
  const container = document.querySelector("#user-song-list");
  if (!songs.length) {
    container.className = "song-list empty-state";
    container.textContent = "Nenhuma musica encontrada.";
    return;
  }

  container.className = "song-list";
  container.innerHTML = songs
    .map(
      (song) => `
        <article class="song-item">
          <h4>${song.title}</h4>
          <div class="song-meta">
            <span>${song.artistName}</span>
            <span>${song.genre}</span>
          </div>
          <p class="song-description">${song.description || "Sem descricao."}</p>
          <button class="secondary" data-play="${song.id}" data-title="${song.title}" data-artist="${song.artistName}">
            Reproduzir
          </button>
        </article>
      `
    )
    .join("");

  container.querySelectorAll("[data-play]").forEach((button) => {
    button.addEventListener("click", () => {
      const { play, title, artist } = button.dataset;
      audioPlayer.src = `/api/songs/${play}/stream?token=${encodeURIComponent(state.token)}`;
      audioPlayer.play().catch(() => undefined);
      playerCaption.textContent = `${title} - ${artist}`;
    });
  });
}

async function loadArtistSongs() {
  const data = await api("/api/songs/mine");
  renderArtistSongs(data.songs || []);
}

async function loadUserSongs() {
  const form = new FormData(document.querySelector("#search-form"));
  const params = new URLSearchParams();
  for (const [key, value] of form.entries()) {
    if (value) {
      params.append(key, value.toString());
    }
  }
  params.append("token", state.token);
  const data = await api(`/api/songs?${params.toString()}`);
  renderUserSongs(data.songs || []);
}

function render() {
  const loggedIn = Boolean(state.user);
  authView.classList.toggle("hidden", loggedIn);
  dashboardView.classList.toggle("hidden", !loggedIn);

  if (!loggedIn) {
    artistArea.classList.add("hidden");
    userArea.classList.add("hidden");
    return;
  }

  welcomeTitle.textContent = `Bem-vindo, ${state.user.name}`;
  userBadge.textContent = state.user.type;

  const isArtist = state.user.type === "ARTISTA";
  artistArea.classList.toggle("hidden", !isArtist);
  userArea.classList.toggle("hidden", isArtist);
}

async function bootstrapSession() {
  if (!state.token) {
    render();
    return;
  }

  try {
    const data = await api("/api/me");
    state.user = data.user;
    render();
    if (state.user.type === "ARTISTA") {
      await loadArtistSongs();
    } else {
      await loadUserSongs();
    }
  } catch {
    clearSession();
  }
}

document.querySelector("#login-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);

  try {
    const data = await api("/api/auth/login", {
      method: "POST",
      body: JSON.stringify({
        email: form.get("email"),
        password: form.get("password"),
      }),
    });

    saveSession(data.token, data.user);
    render();
    showToast("Login realizado com sucesso.");
    if (state.user.type === "ARTISTA") {
      await loadArtistSongs();
    } else {
      await loadUserSongs();
    }
    event.currentTarget.reset();
  } catch (error) {
    showToast(error.message, true);
  }
});

document.querySelector("#register-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);

  try {
    await api("/api/auth/register", {
      method: "POST",
      body: JSON.stringify({
        name: form.get("name"),
        email: form.get("email"),
        password: form.get("password"),
        type: form.get("type"),
      }),
    });

    showToast("Cadastro criado. Agora faca o login.");
    event.currentTarget.reset();
  } catch (error) {
    showToast(error.message, true);
  }
});

document.querySelector("#upload-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);

  try {
    await api("/api/songs", {
      method: "POST",
      body: form,
    });

    showToast("Musica enviada com sucesso.");
    event.currentTarget.reset();
    await loadArtistSongs();
  } catch (error) {
    showToast(error.message, true);
  }
});

document.querySelector("#search-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  try {
    await loadUserSongs();
  } catch (error) {
    showToast(error.message, true);
  }
});

document.querySelector("#logout-button").addEventListener("click", () => {
  clearSession();
  showToast("Sessao encerrada.");
});

bootstrapSession();
