const state = {
  token: localStorage.getItem("token") || "",
  user: loadUser(),
};

const authView = document.getElementById("authView");
const dashboardView = document.getElementById("dashboardView");
const artistPanel = document.getElementById("artistPanel");
const userPanel = document.getElementById("userPanel");
const welcomeTitle = document.getElementById("welcomeTitle");
const toast = document.getElementById("toast");
const audioPlayer = document.getElementById("audioPlayer");
const playerTitle = document.getElementById("playerTitle");
const playerSubtitle = document.getElementById("playerSubtitle");
let currentAudioUrl = "";

document.querySelectorAll(".tab").forEach((button) => {
  button.addEventListener("click", () => switchTab(button.dataset.tab));
});

document.getElementById("loginForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);
  await authRequest("/api/auth/login", {
    email: form.get("email"),
    password: form.get("password"),
  });
});

document.getElementById("registerForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);
  await authRequest("/api/auth/register", {
    name: form.get("name"),
    email: form.get("email"),
    password: form.get("password"),
    type: form.get("type"),
  });
});

document.getElementById("uploadForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  const form = new FormData(event.currentTarget);

  const response = await fetch("/api/artist/songs", {
    method: "POST",
    headers: authHeaders(),
    body: form,
  });
  const payload = await response.json();

  if (!response.ok) {
    showToast(payload.error || "Falha ao enviar musica");
    return;
  }

  event.currentTarget.reset();
  showToast("Musica cadastrada com sucesso");
  loadArtistSongs();
});

document.getElementById("searchForm").addEventListener("submit", async (event) => {
  event.preventDefault();
  await loadSongs();
});

document.getElementById("logoutButton").addEventListener("click", logout);

boot();

async function boot() {
  if (!state.token || !state.user) {
    render();
    return;
  }

  const response = await fetch("/api/me", { headers: authHeaders() });
  if (!response.ok) {
    logout();
    return;
  }

  render();
  await hydrateDashboard();
}

async function authRequest(url, data) {
  const response = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(data),
  });
  const payload = await response.json();

  if (!response.ok) {
    showToast(payload.error || "Erro na autenticacao");
    return;
  }

  state.token = payload.token;
  state.user = payload.user;
  localStorage.setItem("token", state.token);
  localStorage.setItem("user", JSON.stringify(state.user));

  render();
  await hydrateDashboard();
  showToast("Autenticacao realizada com sucesso");
}

function render() {
  const logged = Boolean(state.token && state.user);
  authView.classList.toggle("hidden", logged);
  dashboardView.classList.toggle("hidden", !logged);

  if (!logged) {
    return;
  }

  welcomeTitle.textContent = `${state.user.name} • ${state.user.type}`;

  const isArtist = state.user.type === "ARTISTA";
  artistPanel.classList.toggle("hidden", !isArtist);
  userPanel.classList.toggle("hidden", isArtist);
}

async function hydrateDashboard() {
  if (state.user.type === "ARTISTA") {
    await loadArtistSongs();
    return;
  }

  await loadSongs();
}

async function loadArtistSongs() {
  const response = await fetch("/api/artist/songs", { headers: authHeaders() });
  const payload = await response.json();
  const table = document.getElementById("artistSongsTable");

  if (!response.ok) {
    table.innerHTML = `<tr><td colspan="4">Nao foi possivel carregar as musicas.</td></tr>`;
    return;
  }

  const items = payload.items || [];
  if (!items.length) {
    table.innerHTML = `<tr><td colspan="4">Nenhuma musica cadastrada ate agora.</td></tr>`;
    return;
  }

  table.innerHTML = items
    .map(
      (song) => `
        <tr>
          <td>${escapeHtml(song.title)}</td>
          <td>${escapeHtml(song.genre)}</td>
          <td>${escapeHtml(song.description || "-")}</td>
          <td>${formatDate(song.createdAt)}</td>
        </tr>
      `
    )
    .join("");
}

async function loadSongs() {
  const form = new FormData(document.getElementById("searchForm"));
  const query = new URLSearchParams({
    name: form.get("name") || "",
    artist: form.get("artist") || "",
    genre: form.get("genre") || "",
  });

  const response = await fetch(`/api/songs?${query.toString()}`, {
    headers: authHeaders(),
  });
  const payload = await response.json();
  const grid = document.getElementById("songsGrid");

  if (!response.ok) {
    grid.innerHTML = `<article class="song-card"><p>Nao foi possivel carregar o catalogo.</p></article>`;
    return;
  }

  const items = payload.items || [];
  if (!items.length) {
    grid.innerHTML = `<article class="song-card"><p>Nenhuma musica encontrada para os filtros informados.</p></article>`;
    return;
  }

  grid.innerHTML = items
    .map(
      (song) => `
        <article class="song-card">
          <span class="pill">${escapeHtml(song.genre)}</span>
          <h4>${escapeHtml(song.title)}</h4>
          <p class="muted">${escapeHtml(song.artistName)}</p>
          <p>${escapeHtml(song.description || "Sem descricao")}</p>
          <button class="primary-btn" data-song-id="${song.id}">Reproduzir</button>
        </article>
      `
    )
    .join("");

  grid.querySelectorAll("button[data-song-id]").forEach((button) => {
    button.addEventListener("click", () => {
      const song = items.find((item) => String(item.id) === button.dataset.songId);
      if (!song) {
        return;
      }
      playSong(song);
    });
  });
}

async function playSong(song) {
  playerTitle.textContent = song.title;
  playerSubtitle.textContent = `${song.artistName} • ${song.genre}`;

  try {
    const response = await fetch(`/api/songs/${song.id}/stream`, {
      headers: authHeaders(),
    });

    if (!response.ok) {
      showToast("Nao foi possivel reproduzir a musica");
      return;
    }

    const audioBlob = await response.blob();
    releaseCurrentAudioUrl();
    currentAudioUrl = URL.createObjectURL(audioBlob);
    audioPlayer.src = currentAudioUrl;
    audioPlayer.load();
    audioPlayer.play().catch(() => {});
  } catch {
    showToast("Nao foi possivel reproduzir a musica");
  }
}

function logout() {
  state.token = "";
  state.user = null;
  localStorage.removeItem("token");
  localStorage.removeItem("user");
  audioPlayer.pause();
  audioPlayer.removeAttribute("src");
  releaseCurrentAudioUrl();
  render();
}

function switchTab(tab) {
  document.querySelectorAll(".tab").forEach((button) => {
    button.classList.toggle("active", button.dataset.tab === tab);
  });
  document.getElementById("loginForm").classList.toggle("hidden", tab !== "login");
  document.getElementById("registerForm").classList.toggle("hidden", tab !== "register");
}

function authHeaders() {
  return {
    Authorization: `Bearer ${state.token}`,
  };
}

function releaseCurrentAudioUrl() {
  if (!currentAudioUrl) {
    return;
  }

  URL.revokeObjectURL(currentAudioUrl);
  currentAudioUrl = "";
}

function loadUser() {
  try {
    const raw = localStorage.getItem("user");
    return raw ? JSON.parse(raw) : null;
  } catch {
    return null;
  }
}

function formatDate(value) {
  return new Date(value).toLocaleString("pt-BR");
}

function showToast(message) {
  toast.textContent = message;
  toast.classList.remove("hidden");
  window.clearTimeout(showToast.timer);
  showToast.timer = window.setTimeout(() => toast.classList.add("hidden"), 3200);
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}
