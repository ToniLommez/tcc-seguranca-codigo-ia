const authPanel = document.getElementById("auth-panel");
const artistDashboard = document.getElementById("artist-dashboard");
const userDashboard = document.getElementById("user-dashboard");
const loginForm = document.getElementById("login-form");
const registerForm = document.getElementById("register-form");
const uploadForm = document.getElementById("upload-form");
const searchForm = document.getElementById("search-form");
const artistSongList = document.getElementById("artist-song-list");
const userSongList = document.getElementById("user-song-list");
const statusPanel = document.getElementById("status-panel");
const sessionBox = document.getElementById("session-box");
const sessionUser = document.getElementById("session-user");
const logoutButton = document.getElementById("logout-button");
const workspaceTitle = document.getElementById("workspace-title");
const playerTitle = document.getElementById("player-title");
const playerSubtitle = document.getElementById("player-subtitle");
const audioPlayer = document.getElementById("audio-player");
const loginTab = document.getElementById("login-tab");
const registerTab = document.getElementById("register-tab");

let authToken = localStorage.getItem("authToken") || "";
let currentUser = null;

loginTab.addEventListener("click", () => switchTab("login"));
registerTab.addEventListener("click", () => switchTab("register"));
logoutButton.addEventListener("click", logout);
loginForm.addEventListener("submit", handleLogin);
registerForm.addEventListener("submit", handleRegister);
uploadForm.addEventListener("submit", handleUpload);
searchForm.addEventListener("submit", handleSearch);

bootstrap();

async function bootstrap() {
  if (!authToken) {
    renderLoggedOut();
    return;
  }

  try {
    const response = await api("/api/auth/me");
    currentUser = response.user;
    renderLoggedIn();
  } catch (error) {
    localStorage.removeItem("authToken");
    authToken = "";
    renderLoggedOut();
    showStatus(error.message, true);
  }
}

function switchTab(tab) {
  const isLogin = tab === "login";
  loginTab.classList.toggle("active", isLogin);
  registerTab.classList.toggle("active", !isLogin);
  loginForm.classList.toggle("hidden", !isLogin);
  registerForm.classList.toggle("hidden", isLogin);
}

async function handleLogin(event) {
  event.preventDefault();
  const formData = new FormData(loginForm);

  try {
    const response = await api("/api/auth/login", {
      method: "POST",
      body: JSON.stringify({
        email: formData.get("email"),
        password: formData.get("password"),
      }),
    });

    authToken = response.token;
    currentUser = response.user;
    localStorage.setItem("authToken", authToken);
    loginForm.reset();
    renderLoggedIn();
    showStatus("Login successful. Session is ready.");
  } catch (error) {
    showStatus(error.message, true);
  }
}

async function handleRegister(event) {
  event.preventDefault();
  const formData = new FormData(registerForm);

  try {
    const response = await api("/api/auth/register", {
      method: "POST",
      body: JSON.stringify({
        name: formData.get("name"),
        email: formData.get("email"),
        password: formData.get("password"),
        type: formData.get("type"),
      }),
    });

    registerForm.reset();
    switchTab("login");
    showStatus(`${response.user.name} created successfully. You can login now.`);
  } catch (error) {
    showStatus(error.message, true);
  }
}

async function handleUpload(event) {
  event.preventDefault();
  const formData = new FormData(uploadForm);

  try {
    const response = await fetch("/api/artist/songs", {
      method: "POST",
      headers: authHeaders(),
      credentials: "include",
      body: formData,
    });

    const payload = await response.json();
    if (!response.ok) {
      throw new Error(payload.error || "Upload failed");
    }

    uploadForm.reset();
    showStatus(`"${payload.song.title}" uploaded successfully.`);
    await loadArtistSongs();
  } catch (error) {
    showStatus(error.message, true);
  }
}

async function handleSearch(event) {
  event.preventDefault();
  await loadUserSongs(new FormData(searchForm));
}

async function renderLoggedIn() {
  authPanel.classList.add("hidden");
  sessionBox.classList.remove("hidden");
  sessionUser.textContent = `${currentUser.name} (${currentUser.type})`;

  if (currentUser.type === "ARTIST") {
    workspaceTitle.textContent = "Artist Upload Dashboard";
    artistDashboard.classList.remove("hidden");
    userDashboard.classList.add("hidden");
    audioPlayer.pause();
    audioPlayer.removeAttribute("src");
    await loadArtistSongs();
  } else {
    workspaceTitle.textContent = "User Streaming Dashboard";
    artistDashboard.classList.add("hidden");
    userDashboard.classList.remove("hidden");
    await loadUserSongs();
  }
}

function renderLoggedOut() {
  currentUser = null;
  authPanel.classList.remove("hidden");
  artistDashboard.classList.add("hidden");
  userDashboard.classList.add("hidden");
  sessionBox.classList.add("hidden");
  workspaceTitle.textContent = "Access Portal";
  audioPlayer.pause();
  audioPlayer.removeAttribute("src");
  playerTitle.textContent = "Select a song to play";
  playerSubtitle.textContent = "Playback uses the backend streaming endpoint.";
}

async function loadArtistSongs() {
  try {
    const response = await api("/api/artist/songs");
    artistSongList.innerHTML = renderSongs(response.songs, { showPlay: false });
  } catch (error) {
    showStatus(error.message, true);
  }
}

async function loadUserSongs(formData) {
  try {
    const params = new URLSearchParams();
    if (formData) {
      for (const [key, value] of formData.entries()) {
        if (String(value).trim()) {
          params.set(key, value);
        }
      }
    }

    const query = params.toString() ? `?${params.toString()}` : "";
    const response = await api(`/api/songs${query}`);
    userSongList.innerHTML = renderSongs(response.songs, { showPlay: true });
    bindPlayButtons();
  } catch (error) {
    showStatus(error.message, true);
  }
}

function renderSongs(songs = [], options = {}) {
  if (!songs.length) {
    return `<div class="song-card"><div><h4>No songs found</h4><p>Try another search or add a new upload.</p></div></div>`;
  }

  return songs.map((song) => `
    <article class="song-card">
      <div>
        <h4>${escapeHtml(song.title)}</h4>
        <p><strong>Artist:</strong> ${escapeHtml(song.artistName)}</p>
        <p><strong>Genre:</strong> ${escapeHtml(song.genre)}</p>
        <p>${escapeHtml(song.description || "No description provided.")}</p>
      </div>
      ${options.showPlay ? `<button class="primary-button play-button" data-id="${song.id}" data-title="${escapeHtml(song.title)}" data-artist="${escapeHtml(song.artistName)}">Play</button>` : `<span class="side-note">Upload only</span>`}
    </article>
  `).join("");
}

function bindPlayButtons() {
  document.querySelectorAll(".play-button").forEach((button) => {
    button.addEventListener("click", () => {
      const songId = button.dataset.id;
      const title = button.dataset.title;
      const artist = button.dataset.artist;

      audioPlayer.src = `/api/songs/${songId}/stream`;
      audioPlayer.play().catch(() => {
        showStatus("Browser blocked autoplay. Press play on the audio control.", true);
      });

      playerTitle.textContent = title;
      playerSubtitle.textContent = `Now streaming ${artist}`;
    });
  });
}

async function logout() {
  try {
    await api("/api/auth/logout", { method: "POST" });
  } catch (error) {
    showStatus(error.message, true);
  } finally {
    localStorage.removeItem("authToken");
    authToken = "";
    renderLoggedOut();
    showStatus("Session closed.");
  }
}

async function api(url, options = {}) {
  const headers = {
    "Content-Type": "application/json",
    ...authHeaders(),
    ...(options.headers || {}),
  };

  if (options.body instanceof FormData) {
    delete headers["Content-Type"];
  }

  const response = await fetch(url, {
    credentials: "include",
    ...options,
    headers,
  });

  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(payload.error || "Request failed");
  }

  return payload;
}

function authHeaders() {
  return authToken ? { Authorization: `Bearer ${authToken}` } : {};
}

function showStatus(message, isError = false) {
  statusPanel.textContent = message;
  statusPanel.classList.remove("hidden", "error");
  if (isError) {
    statusPanel.classList.add("error");
  }
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}
