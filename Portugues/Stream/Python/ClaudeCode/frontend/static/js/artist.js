/* Dashboard do Artista */

const currentUser = requireAuth('ARTISTA');
if (!currentUser) throw new Error('Redirect');

document.getElementById('userName').textContent = currentUser.name;

// ── Tabs ──────────────────────────────────────────────────────────────────────
document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    const target = btn.dataset.tab;
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(target).classList.add('active');
    if (target === 'tab-songs') loadMySongs();
  });
});

// ── Alerta ────────────────────────────────────────────────────────────────────
function showAlert(type, message) {
  const el = document.getElementById('uploadAlert');
  el.className = `alert alert-${type}`;
  el.textContent = message;
  el.style.display = 'block';
  setTimeout(() => { el.style.display = 'none'; }, 5000);
}

// ── Drop Zone ─────────────────────────────────────────────────────────────────
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');

dropZone.addEventListener('click', () => fileInput.click());

dropZone.addEventListener('dragover', e => {
  e.preventDefault();
  dropZone.classList.add('drag-over');
});
dropZone.addEventListener('dragleave', () => dropZone.classList.remove('drag-over'));
dropZone.addEventListener('drop', e => {
  e.preventDefault();
  dropZone.classList.remove('drag-over');
  const file = e.dataTransfer.files[0];
  if (file) setFile(file);
});

fileInput.addEventListener('change', () => {
  if (fileInput.files[0]) setFile(fileInput.files[0]);
});

function setFile(file) {
  if (!file.name.toLowerCase().endsWith('.mp3')) {
    showAlert('error', 'Apenas arquivos MP3 são aceitos.');
    return;
  }
  dropZone.classList.add('has-file');
  dropZone.querySelector('.drop-zone-text').textContent = `✓ ${file.name}`;
  dropZone.querySelector('.drop-zone-hint').textContent =
    `${(file.size / (1024 * 1024)).toFixed(2)} MB`;

  // Transfere para o input real
  const dt = new DataTransfer();
  dt.items.add(file);
  fileInput.files = dt.files;
}

// ── Upload ────────────────────────────────────────────────────────────────────
document.getElementById('uploadForm').addEventListener('submit', async e => {
  e.preventDefault();

  const file = fileInput.files[0];
  if (!file) { showAlert('error', 'Selecione um arquivo MP3.'); return; }

  const formData = new FormData();
  formData.append('title',       document.getElementById('title').value.trim());
  formData.append('genre',       document.getElementById('genre').value);
  formData.append('description', document.getElementById('description').value.trim());
  formData.append('file',        file);

  const btn = e.target.querySelector('button[type="submit"]');
  btn.disabled = true;
  btn.innerHTML = '<span class="spinner"></span> Enviando...';

  try {
    const res = await apiRequest('/api/music/upload', { method: 'POST', body: formData });
    const data = await res.json();
    if (!res.ok) throw new Error(data.detail || 'Erro no upload.');

    showAlert('success', `"${data.song.title}" cadastrada com sucesso!`);
    e.target.reset();
    dropZone.classList.remove('has-file');
    dropZone.querySelector('.drop-zone-text').textContent = 'Arraste o arquivo MP3 aqui ou clique para selecionar';
    dropZone.querySelector('.drop-zone-hint').textContent = 'Somente MP3 · Máximo 50 MB';
    updateSongCount();
  } catch (err) {
    showAlert('error', err.message);
  } finally {
    btn.disabled = false;
    btn.textContent = 'Cadastrar Música';
  }
});

// ── Lista de músicas do artista ────────────────────────────────────────────────
async function loadMySongs() {
  const container = document.getElementById('mySongsList');
  container.innerHTML = '<div class="loading-state"><span class="spinner"></span> Carregando...</div>';

  try {
    const res = await apiRequest('/api/music/my');
    const songs = await res.json();

    document.getElementById('songCount').textContent = songs.length;

    if (songs.length === 0) {
      container.innerHTML = `
        <div class="empty-state">
          <div class="empty-icon">🎵</div>
          <p>Nenhuma música cadastrada ainda.</p>
          <p class="empty-hint">Use a aba "Upload" para adicionar sua primeira música.</p>
        </div>`;
      return;
    }

    container.innerHTML = `<div class="song-list">${songs.map((s, i) => `
      <div class="song-list-item">
        <span class="song-list-num">${i + 1}</span>
        <span class="song-list-icon">🎵</span>
        <div class="song-list-info">
          <div class="song-list-title">${escHtml(s.title)}</div>
          <div class="song-list-sub">
            <span class="genre-badge">${escHtml(s.genre)}</span>
            ${s.description ? ` · ${escHtml(s.description)}` : ''}
          </div>
        </div>
        <div class="song-list-date">${formatDate(s.created_at)}</div>
      </div>
    `).join('')}</div>`;
  } catch (err) {
    container.innerHTML = `<div class="empty-state"><p>Erro ao carregar músicas.</p></div>`;
  }
}

async function updateSongCount() {
  try {
    const res = await apiRequest('/api/music/my');
    const songs = await res.json();
    document.getElementById('songCount').textContent = songs.length;
  } catch {}
}

// ── Utilitários ────────────────────────────────────────────────────────────────
function escHtml(str) {
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function formatDate(iso) {
  return new Date(iso).toLocaleDateString('pt-BR');
}

// Carga inicial
loadMySongs();
