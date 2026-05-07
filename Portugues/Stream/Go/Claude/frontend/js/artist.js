// Requires auth.js to be loaded first (API, getToken, getUser, etc. come from there).

let currentUser = null;

document.addEventListener('DOMContentLoaded', () => {
  currentUser = requireAuth('ARTISTA');
  if (!currentUser) return;

  renderUserInfo();
  loadMyMusic();
  initUploadForm();
  document.getElementById('logoutBtn').addEventListener('click', logout);
});

function renderUserInfo() {
  document.getElementById('userName').textContent = currentUser.name;
  document.getElementById('userAvatar').textContent = currentUser.name.charAt(0).toUpperCase();
}

// ── Load artist's own music ───────────────────────
async function loadMyMusic() {
  const container = document.getElementById('musicTableBody');
  const emptyState = document.getElementById('emptyState');
  container.innerHTML = '<tr><td colspan="4" style="text-align:center;color:var(--muted);padding:32px"><span class="spinner"></span> Carregando…</td></tr>';

  try {
    const res = await fetch(`${API}/api/artist/music`, {
      headers: { Authorization: `Bearer ${getToken()}` },
    });

    if (res.status === 401 || res.status === 403) { logout(); return; }

    const musics = await res.json();

    if (!musics || musics.length === 0) {
      container.innerHTML = '';
      emptyState.style.display = 'block';
      return;
    }

    emptyState.style.display = 'none';
    container.innerHTML = musics.map(m => `
      <tr>
        <td><strong>${escHtml(m.title)}</strong></td>
        <td><span class="genre-pill">${escHtml(m.genre)}</span></td>
        <td style="color:var(--muted);font-size:13px">${escHtml(m.description || '—')}</td>
        <td style="color:var(--muted);font-size:12px">${formatDate(m.created_at)}</td>
      </tr>
    `).join('');
  } catch {
    container.innerHTML = '<tr><td colspan="4" style="color:var(--danger);text-align:center;padding:24px">Erro ao carregar músicas</td></tr>';
  }
}

// ── Upload form ───────────────────────────────────
function initUploadForm() {
  const fileInput  = document.getElementById('fileInput');
  const dropArea   = document.getElementById('dropArea');
  const fileLabel  = document.getElementById('fileSelectedName');
  const uploadForm = document.getElementById('uploadForm');
  const progressWrap = document.getElementById('progressWrap');
  const progressBar  = document.getElementById('progressBar');
  const uploadAlert  = document.getElementById('uploadAlert');

  // Drag and drop visual feedback.
  dropArea.addEventListener('dragover', (e) => { e.preventDefault(); dropArea.classList.add('dragover'); });
  dropArea.addEventListener('dragleave', () => dropArea.classList.remove('dragover'));
  dropArea.addEventListener('drop', (e) => {
    e.preventDefault();
    dropArea.classList.remove('dragover');
    if (e.dataTransfer.files[0]) {
      fileInput.files = e.dataTransfer.files;
      updateFileName();
    }
  });

  fileInput.addEventListener('change', updateFileName);

  function updateFileName() {
    const f = fileInput.files[0];
    if (f) {
      if (!f.name.toLowerCase().endsWith('.mp3')) {
        showUploadAlert('Apenas arquivos MP3 são permitidos', 'error');
        fileInput.value = '';
        fileLabel.textContent = '';
        return;
      }
      fileLabel.textContent = `✓ ${f.name}`;
      uploadAlert.classList.remove('show');
    }
  }

  uploadForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const btn = document.getElementById('uploadBtn');

    const title       = document.getElementById('title').value.trim();
    const genre       = document.getElementById('genre').value;
    const description = document.getElementById('description').value.trim();
    const file        = fileInput.files[0];

    if (!title)  { showUploadAlert('Informe o título da música', 'error'); return; }
    if (!genre)  { showUploadAlert('Selecione um gênero', 'error'); return; }
    if (!file)   { showUploadAlert('Selecione um arquivo MP3', 'error'); return; }

    const formData = new FormData();
    formData.append('title', title);
    formData.append('genre', genre);
    formData.append('description', description);
    formData.append('file', file);

    btn.disabled = true;
    btn.innerHTML = '<span class="spinner"></span> Enviando…';
    progressWrap.classList.add('show');
    progressBar.style.width = '30%';
    uploadAlert.classList.remove('show');

    try {
      const res = await fetch(`${API}/api/artist/music`, {
        method: 'POST',
        headers: { Authorization: `Bearer ${getToken()}` },
        body: formData,
      });

      progressBar.style.width = '100%';

      const data = await res.json();
      if (!res.ok) {
        showUploadAlert(data.error || 'Erro ao enviar música', 'error');
        return;
      }

      showUploadAlert('Música cadastrada com sucesso!', 'success');
      uploadForm.reset();
      fileLabel.textContent = '';
      setTimeout(() => progressWrap.classList.remove('show'), 800);
      loadMyMusic();
    } catch {
      showUploadAlert('Erro de conexão com o servidor', 'error');
    } finally {
      btn.disabled = false;
      btn.textContent = 'Enviar música';
      setTimeout(() => { progressBar.style.width = '0%'; }, 900);
    }
  });
}

function showUploadAlert(msg, type) {
  const el = document.getElementById('uploadAlert');
  el.textContent = msg;
  el.className = `alert alert-${type} show`;
}

// ── Helpers ───────────────────────────────────────
function escHtml(str) {
  return String(str).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}

function formatDate(iso) {
  if (!iso) return '—';
  return new Date(iso).toLocaleDateString('pt-BR', { day: '2-digit', month: 'short', year: 'numeric' });
}
