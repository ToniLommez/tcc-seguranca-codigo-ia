/* Dashboard do Usuário */

const currentUser = requireAuth('USUARIO');
if (!currentUser) throw new Error('Redirect');

document.getElementById('userName').textContent = currentUser.name;

// ── Estado ─────────────────────────────────────────────────────────────────────
let allSongs = [];
let currentSong = null;
let isPlaying = false;

// ── Elemento de áudio ──────────────────────────────────────────────────────────
const audio = new Audio();
audio.preload = 'none';

// ── Referências do player ──────────────────────────────────────────────────────
const playerBar     = document.getElementById('playerBar');
const mainArea      = document.getElementById('mainArea');
const playPauseBtn  = document.getElementById('playPauseBtn');
const prevBtn       = document.getElementById('prevBtn');
const nextBtn       = document.getElementById('nextBtn');
const progressTrack = document.getElementById('progressTrack');
const progressFill  = document.getElementById('progressFill');
const elCurrentTime = document.getElementById('currentTime');
const elTotalTime   = document.getElementById('totalTime');
const volumeSlider  = document.getElementById('volumeSlider');
const playerTitle   = document.getElementById('playerTitle');
const playerArtist  = document.getElementById('playerArtist');

// ── Player — eventos de áudio ──────────────────────────────────────────────────
audio.addEventListener('timeupdate', () => {
  if (!audio.duration) return;
  const pct = (audio.currentTime / audio.duration) * 100;
  progressFill.style.width = `${pct}%`;
  elCurrentTime.textContent = fmtTime(audio.currentTime);
});

audio.addEventListener('loadedmetadata', () => {
  elTotalTime.textContent = fmtTime(audio.duration);
});

audio.addEventListener('ended', () => {
  setPlayState(false);
  skipTrack(1);
});

audio.addEventListener('error', () => {
  setPlayState(false);
});

// ── Player — controles ─────────────────────────────────────────────────────────
playPauseBtn.addEventListener('click', togglePlayPause);
prevBtn.addEventListener('click', () => skipTrack(-1));
nextBtn.addEventListener('click', () => skipTrack(1));

progressTrack.addEventListener('click', e => {
  if (!audio.duration) return;
  const rect = progressTrack.getBoundingClientRect();
  audio.currentTime = ((e.clientX - rect.left) / rect.width) * audio.duration;
});

volumeSlider.addEventListener('input', () => {
  audio.volume = volumeSlider.value;
  document.getElementById('volIcon').textContent =
    audio.volume === 0 ? '🔇' : audio.volume < 0.5 ? '🔉' : '🔊';
});

function togglePlayPause() {
  if (!currentSong) return;
  if (audio.paused) {
    audio.play().then(() => setPlayState(true)).catch(() => {});
  } else {
    audio.pause();
    setPlayState(false);
  }
}

function setPlayState(playing) {
  isPlaying = playing;
  playPauseBtn.textContent = playing ? '⏸' : '▶';
  playPauseBtn.title = playing ? 'Pausar' : 'Reproduzir';
}

function skipTrack(dir) {
  if (!allSongs.length || !currentSong) return;
  const idx = allSongs.findIndex(s => s.id === currentSong.id);
  const next = allSongs[idx + dir];
  if (next) playSong(next);
}

function playSong(song) {
  currentSong = song;
  const token = getToken();

  audio.src = `/api/music/stream/${song.id}?token=${encodeURIComponent(token)}`;
  audio.load();
  audio.play()
    .then(() => setPlayState(true))
    .catch(() => setPlayState(false));

  playerTitle.textContent  = song.title;
  playerArtist.textContent = song.artist_name;
  playerBar.classList.add('visible');
  mainArea.classList.add('has-player');

  // Destaca o card ativo
  document.querySelectorAll('.song-card').forEach(c => {
    c.classList.toggle('playing', parseInt(c.dataset.id) === song.id);
  });
}

// ── Busca ──────────────────────────────────────────────────────────────────────
let debounceTimer;

document.getElementById('quickSearch').addEventListener('input', e => {
  clearTimeout(debounceTimer);
  debounceTimer = setTimeout(() => {
    const q = e.target.value.trim();
    if (q) loadSongs({ title: q });
    else loadSongs();
  }, 320);
});

document.getElementById('advSearchBtn').addEventListener('click', () => {
  const panel = document.getElementById('advPanel');
  panel.classList.toggle('open');
});

document.getElementById('applySearch').addEventListener('click', () => {
  loadSongs({
    title:  document.getElementById('sTitle').value.trim(),
    artist: document.getElementById('sArtist').value.trim(),
    genre:  document.getElementById('sGenre').value.trim(),
  });
});

document.getElementById('clearSearch').addEventListener('click', () => {
  ['sTitle', 'sArtist', 'sGenre', 'quickSearch'].forEach(id => {
    document.getElementById(id).value = '';
  });
  document.querySelectorAll('.genre-chip').forEach(c => c.classList.remove('active'));
  document.querySelector('.genre-chip[data-genre=""]').classList.add('active');
  loadSongs();
});

// Genre chips
document.querySelectorAll('.genre-chip').forEach(chip => {
  chip.addEventListener('click', () => {
    document.querySelectorAll('.genre-chip').forEach(c => c.classList.remove('active'));
    chip.classList.add('active');
    const genre = chip.dataset.genre;
    if (genre) loadSongs({ genre });
    else loadSongs();
  });
});

// ── Carregamento de músicas ────────────────────────────────────────────────────
async function loadSongs(params = {}) {
  const grid = document.getElementById('songsGrid');
  grid.innerHTML = '<div class="loading-state"><span class="spinner"></span> Carregando músicas...</div>';

  try {
    const hasFilter = params.title || params.artist || params.genre;
    const qs = new URLSearchParams(
      Object.fromEntries(Object.entries(params).filter(([, v]) => v))
    ).toString();
    const url = hasFilter ? `/api/music/search?${qs}` : '/api/music/list';

    const res = await apiRequest(url);
    if (!res) return;
    allSongs = await res.json();

    document.getElementById('totalCount').textContent = allSongs.length;
    renderSongs(allSongs);
  } catch (err) {
    grid.innerHTML = `<div class="empty-state"><p>Erro ao carregar músicas.</p></div>`;
  }
}

function renderSongs(songs) {
  const grid = document.getElementById('songsGrid');

  if (songs.length === 0) {
    grid.innerHTML = `
      <div class="empty-state">
        <div class="empty-icon">🔍</div>
        <p>Nenhuma música encontrada.</p>
        <p class="empty-hint">Tente outros termos de busca.</p>
      </div>`;
    return;
  }

  grid.innerHTML = songs.map(song => `
    <div class="song-card ${currentSong?.id === song.id ? 'playing' : ''}"
         data-id="${song.id}"
         onclick='handleCardClick(${JSON.stringify(song)})'>
      <div class="song-cover">
        <div class="song-play-overlay">${currentSong?.id === song.id && isPlaying ? '⏸' : '▶'}</div>
        🎵
      </div>
      <div class="song-title">${escHtml(song.title)}</div>
      <div class="song-artist">${escHtml(song.artist_name)}</div>
      <div class="genre-badge">${escHtml(song.genre)}</div>
    </div>
  `).join('');
}

function handleCardClick(song) {
  if (currentSong?.id === song.id) {
    togglePlayPause();
  } else {
    playSong(song);
  }
}

// ── Utilitários ────────────────────────────────────────────────────────────────
function fmtTime(s) {
  if (!s || isNaN(s)) return '0:00';
  const m = Math.floor(s / 60);
  const sec = Math.floor(s % 60).toString().padStart(2, '0');
  return `${m}:${sec}`;
}

function escHtml(str) {
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

// ── Init ───────────────────────────────────────────────────────────────────────
audio.volume = volumeSlider.value;
loadSongs();
