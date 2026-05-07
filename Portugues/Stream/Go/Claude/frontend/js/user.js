// Requires auth.js to be loaded first (API, getToken, getUser, etc. come from there).

let currentUser  = null;
let allMusics    = [];
let currentMusic = null;

document.addEventListener('DOMContentLoaded', () => {
  currentUser = requireAuth('USUARIO');
  if (!currentUser) return;

  renderUserInfo();
  loadMusic();
  initSearch();
  document.getElementById('logoutBtn').addEventListener('click', logout);
});

function renderUserInfo() {
  document.getElementById('userName').textContent = currentUser.name;
  document.getElementById('userAvatar').textContent = currentUser.name.charAt(0).toUpperCase();
}

// ── Load all music ────────────────────────────────
async function loadMusic(params = {}) {
  const grid      = document.getElementById('musicGrid');
  const emptyState = document.getElementById('emptyState');
  const sectionTitle = document.getElementById('sectionTitle');

  grid.innerHTML = '<div style="grid-column:1/-1;text-align:center;color:var(--muted);padding:48px"><span class="spinner"></span> Carregando músicas…</div>';
  emptyState.style.display = 'none';

  try {
    let url;
    const hasSearch = params.title || params.artist || params.genre;

    if (hasSearch) {
      const q = new URLSearchParams();
      if (params.title)  q.set('title',  params.title);
      if (params.artist) q.set('artist', params.artist);
      if (params.genre)  q.set('genre',  params.genre);
      url = `${API}/api/music/search?${q}`;
      sectionTitle.textContent = 'Resultados da busca';
    } else {
      url = `${API}/api/music`;
      sectionTitle.textContent = 'Todas as músicas';
    }

    const res = await fetch(url, {
      headers: { Authorization: `Bearer ${getToken()}` },
    });

    if (res.status === 401 || res.status === 403) { logout(); return; }

    const musics = await res.json();
    allMusics = musics || [];

    if (allMusics.length === 0) {
      grid.innerHTML = '';
      emptyState.style.display = 'block';
      emptyState.querySelector('p').textContent = hasSearch
        ? 'Nenhuma música encontrada para essa busca.'
        : 'Nenhuma música cadastrada ainda.';
      return;
    }

    renderGrid(allMusics);
  } catch {
    grid.innerHTML = '<div style="grid-column:1/-1;text-align:center;color:var(--danger);padding:48px">Erro ao carregar músicas.</div>';
  }
}

function renderGrid(musics) {
  const grid = document.getElementById('musicGrid');
  grid.innerHTML = musics.map(m => `
    <div class="music-card ${currentMusic?.id === m.id ? 'playing' : ''}"
         onclick="playMusic(${JSON.stringify(m).replace(/"/g, '&quot;')})"
         data-id="${m.id}">
      <div class="music-card-art">🎵</div>
      <div class="music-card-title" title="${escHtml(m.title)}">${escHtml(m.title)}</div>
      <div class="music-card-artist">${escHtml(m.artist_name)}</div>
      <span class="music-card-genre">${escHtml(m.genre)}</span>
    </div>
  `).join('');
}

// ── Player ────────────────────────────────────────
function playMusic(music) {
  currentMusic = music;
  const token  = getToken();
  const audio  = document.getElementById('audioPlayer');

  // Update card highlight.
  document.querySelectorAll('.music-card').forEach(c => {
    c.classList.toggle('playing', c.dataset.id === music.id);
  });

  // Update player info.
  document.getElementById('playerTrackName').textContent   = music.title;
  document.getElementById('playerTrackArtist').textContent = music.artist_name;
  document.getElementById('playerIdle').style.display  = 'none';
  document.getElementById('playerInfo').style.display  = 'flex';
  document.getElementById('playerAudio').style.display = 'flex';

  // Token in URL for audio element (can't set headers directly).
  audio.src = `${API}/api/music/${music.id}/stream?token=${encodeURIComponent(token)}`;
  audio.load();
  audio.play().catch(() => {});
}

// ── Search ────────────────────────────────────────
function initSearch() {
  const btnSearch = document.getElementById('btnSearch');
  const btnClear  = document.getElementById('btnClear');

  btnSearch.addEventListener('click', runSearch);
  btnClear.addEventListener('click', () => {
    document.getElementById('searchTitle').value  = '';
    document.getElementById('searchArtist').value = '';
    document.getElementById('searchGenre').value  = '';
    loadMusic();
  });

  // Search on Enter key.
  ['searchTitle', 'searchArtist', 'searchGenre'].forEach(id => {
    document.getElementById(id).addEventListener('keydown', (e) => {
      if (e.key === 'Enter') runSearch();
    });
  });
}

function runSearch() {
  loadMusic({
    title:  document.getElementById('searchTitle').value.trim(),
    artist: document.getElementById('searchArtist').value.trim(),
    genre:  document.getElementById('searchGenre').value.trim(),
  });
}

// ── Helpers ───────────────────────────────────────
function escHtml(str) {
  return String(str).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}
