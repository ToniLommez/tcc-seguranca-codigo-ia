const storageKey = "stream_music_session";

const state = {
  token: "",
  user: null
};

const elements = {
  tabs: [...document.querySelectorAll(".tab")],
  loginForm: document.getElementById("loginForm"),
  registerForm: document.getElementById("registerForm"),
  authMessage: document.getElementById("authMessage"),
  artistMessage: document.getElementById("artistMessage"),
  userMessage: document.getElementById("userMessage"),
  authPanel: document.getElementById("authPanel"),
  artistDashboard: document.getElementById("artistDashboard"),
  userDashboard: document.getElementById("userDashboard"),
  sessionStatus: document.getElementById("sessionStatus"),
  uploadForm: document.getElementById("uploadForm"),
  artistSongs: document.getElementById("artistSongs"),
  refreshArtistSongs: document.getElementById("refreshArtistSongs"),
  logoutArtist: document.getElementById("logoutArtist"),
  logoutUser: document.getElementById("logoutUser"),
  searchForm: document.getElementById("searchForm"),
  songResults: document.getElementById("songResults"),
  audioPlayer: document.getElementById("audioPlayer"),
  nowPlaying: document.getElementById("nowPlaying")
};

function setMessage(target, message, isError = false) {
  target.textContent = message || "";
  target.style.color = isError ? "#a22b18" : "#8c3d21";
}

function persistSession() {
  localStorage.setItem(storageKey, JSON.stringify({ token: state.token, user: state.user }));
}

function restoreSession() {
  try {
    const parsed = JSON.parse(localStorage.getItem(storageKey) || "null");
    if (parsed?.token && parsed?.user) {
      state.token = parsed.token;
      state.user = parsed.user;
    }
  } catch {
    localStorage.removeItem(storageKey);
  }
}

function clearSession() {
  state.token = "";
  state.user = null;
  localStorage.removeItem(storageKey);
  elements.audioPlayer.pause();
  elements.audioPlayer.removeAttribute("src");
  elements.nowPlaying.textContent = "Choose a track to start streaming";
  updateView();
}

function updateView() {
  const statusPill = elements.sessionStatus.querySelector(".status-pill");

  if (!state.user) {
    statusPill.textContent = "Disconnected";
    elements.sessionStatus.lastElementChild.textContent =
      "Sign in as an artist to publish tracks or as a regular user to listen.";
    elements.authPanel.classList.remove("hidden");
    elements.artistDashboard.classList.add("hidden");
    elements.userDashboard.classList.add("hidden");
    return;
  }

  statusPill.textContent = `${state.user.type} connected`;
  elements.sessionStatus.lastElementChild.textContent = `${state.user.name} is authenticated with ${state.user.email}.`;

  if (state.user.type === "ARTIST") {
    elements.authPanel.classList.add("hidden");
    elements.artistDashboard.classList.remove("hidden");
    elements.userDashboard.classList.add("hidden");
    loadArtistSongs();
  } else {
    elements.authPanel.classList.add("hidden");
    elements.artistDashboard.classList.add("hidden");
    elements.userDashboard.classList.remove("hidden");
    loadSongs();
  }
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
    headers
  });

  const text = await response.text();
  const data = text ? JSON.parse(text) : {};

  if (!response.ok) {
    throw new Error(data.error || "Request failed.");
  }

  return data;
}

function renderArtistSongs(songs) {
  if (!songs.length) {
    elements.artistSongs.innerHTML = "<p>No songs uploaded yet.</p>";
    return;
  }

  elements.artistSongs.innerHTML = songs
    .map(
      (song) => `
        <article class="song-card">
          <p class="song-meta">${song.genre}</p>
          <h3>${song.title}</h3>
          <p>${song.description || "No description provided."}</p>
          <p>Uploaded at ${new Date(song.uploadedAt).toLocaleString()}</p>
        </article>
      `
    )
    .join("");
}

function renderUserSongs(songs) {
  if (!songs.length) {
    elements.songResults.innerHTML = "<p>No songs matched your filters.</p>";
    return;
  }

  elements.songResults.innerHTML = songs
    .map(
      (song) => `
        <article class="song-card">
          <p class="song-meta">${song.genre}</p>
          <h3>${song.title}</h3>
          <p>${song.description || "No description provided."}</p>
          <p>Artist: ${song.artistName}</p>
          <button class="primary" data-song-id="${song.id}" data-song-title="${song.title}" data-artist-name="${song.artistName}">
            Play in browser
          </button>
        </article>
      `
    )
    .join("");

  [...elements.songResults.querySelectorAll("button[data-song-id]")].forEach((button) => {
    button.addEventListener("click", () => {
      const { songId, songTitle, artistName } = button.dataset;
      elements.audioPlayer.src = `/api/songs/${songId}/stream?token=${encodeURIComponent(state.token)}`;
      elements.audioPlayer.play();
      elements.nowPlaying.textContent = `${songTitle} by ${artistName}`;
    });
  });
}

async function loadArtistSongs() {
  try {
    const data = await api("/api/artist/songs");
    renderArtistSongs(data.songs || []);
  } catch (error) {
    setMessage(elements.artistMessage, error.message, true);
  }
}

async function loadSongs(filters = {}) {
  try {
    const params = new URLSearchParams(filters);
    const suffix = params.toString() ? `?${params.toString()}` : "";
    const data = await api(`/api/songs${suffix}`, { method: "GET" });
    renderUserSongs(data.songs || []);
  } catch (error) {
    setMessage(elements.userMessage, error.message, true);
  }
}

elements.tabs.forEach((tab) => {
  tab.addEventListener("click", () => {
    elements.tabs.forEach((item) => item.classList.toggle("active", item === tab));
    elements.loginForm.classList.toggle("active", tab.dataset.tab === "login");
    elements.registerForm.classList.toggle("active", tab.dataset.tab === "register");
    setMessage(elements.authMessage, "");
  });
});

elements.loginForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  setMessage(elements.authMessage, "");

  const formData = new FormData(elements.loginForm);

  try {
    const data = await api("/api/auth/login", {
      method: "POST",
      body: JSON.stringify({
        email: formData.get("email"),
        password: formData.get("password")
      })
    });

    state.token = data.token;
    state.user = data.user;
    persistSession();
    elements.loginForm.reset();
    updateView();
  } catch (error) {
    setMessage(elements.authMessage, error.message, true);
  }
});

elements.registerForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  setMessage(elements.authMessage, "");

  const formData = new FormData(elements.registerForm);

  try {
    await api("/api/auth/register", {
      method: "POST",
      body: JSON.stringify({
        name: formData.get("name"),
        email: formData.get("email"),
        password: formData.get("password"),
        type: formData.get("type")
      })
    });

    elements.registerForm.reset();
    setMessage(elements.authMessage, "Registration complete. You can now log in.");
    elements.tabs[0].click();
  } catch (error) {
    setMessage(elements.authMessage, error.message, true);
  }
});

elements.uploadForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  setMessage(elements.artistMessage, "");

  const formData = new FormData(elements.uploadForm);

  try {
    await api("/api/artist/songs", {
      method: "POST",
      body: formData
    });

    elements.uploadForm.reset();
    setMessage(elements.artistMessage, "Song uploaded successfully.");
    await loadArtistSongs();
  } catch (error) {
    setMessage(elements.artistMessage, error.message, true);
  }
});

elements.refreshArtistSongs.addEventListener("click", loadArtistSongs);
elements.logoutArtist.addEventListener("click", clearSession);
elements.logoutUser.addEventListener("click", clearSession);

elements.searchForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  setMessage(elements.userMessage, "");

  const formData = new FormData(elements.searchForm);
  await loadSongs({
    title: formData.get("title") || "",
    artist: formData.get("artist") || "",
    genre: formData.get("genre") || ""
  });
});

restoreSession();
updateView();

