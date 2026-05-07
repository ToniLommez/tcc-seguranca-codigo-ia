document.addEventListener('DOMContentLoaded', async () => {
    const user = Api.requireAuth('ARTISTA');
    if (!user) return;

    // User info sidebar
    document.getElementById('userName').textContent  = user.name;
    document.getElementById('userEmail').textContent = user.email;
    document.getElementById('userInitial').textContent = user.name[0].toUpperCase();

    document.getElementById('logoutBtn').addEventListener('click', () => {
        Api.clearSession();
        window.location.href = '/index.html';
    });

    // ── Navigation ──
    const views = document.querySelectorAll('.view');
    const navItems = document.querySelectorAll('.nav-item[data-view]');

    function showView(name) {
        views.forEach(v => v.style.display = v.dataset.view === name ? 'block' : 'none');
        navItems.forEach(n => n.classList.toggle('active', n.dataset.view === name));
    }

    navItems.forEach(n => n.addEventListener('click', () => showView(n.dataset.view)));
    showView('upload');

    // ── Upload form ──
    const dropZone   = document.getElementById('dropZone');
    const fileInput  = document.getElementById('fileInput');
    const fileLabel  = document.getElementById('fileLabel');
    const uploadForm = document.getElementById('uploadForm');
    const uploadMsg  = document.getElementById('uploadMsg');
    let selectedFile = null;

    dropZone.addEventListener('click', () => fileInput.click());
    dropZone.addEventListener('dragover', e => { e.preventDefault(); dropZone.classList.add('drag-over'); });
    dropZone.addEventListener('dragleave', () => dropZone.classList.remove('drag-over'));
    dropZone.addEventListener('drop', e => {
        e.preventDefault();
        dropZone.classList.remove('drag-over');
        const f = e.dataTransfer.files[0];
        if (f) setFile(f);
    });
    fileInput.addEventListener('change', () => {
        if (fileInput.files[0]) setFile(fileInput.files[0]);
    });

    function setFile(f) {
        if (!f.name.toLowerCase().endsWith('.mp3')) {
            showAlert('uploadMsg', 'Apenas arquivos MP3 são aceitos.'); return;
        }
        selectedFile = f;
        fileLabel.textContent = f.name;
        fileLabel.className = 'file-selected';
        uploadMsg.innerHTML = '';
    }

    uploadForm.addEventListener('submit', async e => {
        e.preventDefault();
        if (!selectedFile) { showAlert('uploadMsg', 'Selecione um arquivo MP3.'); return; }

        const title = document.getElementById('titleInput').value.trim();
        const genre = document.getElementById('genreInput').value;
        const desc  = document.getElementById('descInput').value.trim();

        if (!title) { showAlert('uploadMsg', 'Título é obrigatório.'); return; }

        const fd = new FormData();
        fd.append('title', title);
        fd.append('genre', genre);
        fd.append('description', desc);
        fd.append('artistName', user.name);
        fd.append('file', selectedFile);

        const btn = uploadForm.querySelector('button[type="submit"]');
        btn.disabled = true;
        btn.textContent = 'Enviando...';

        try {
            await Api.uploadSong(fd);
            showAlert('uploadMsg', 'Música enviada com sucesso!', 'success');
            uploadForm.reset();
            selectedFile = null;
            fileLabel.textContent = 'Clique ou arraste um arquivo MP3';
            fileLabel.className = '';
            loadMySongs();
        } catch (err) {
            showAlert('uploadMsg', err.message || 'Erro ao enviar música.');
        } finally {
            btn.disabled = false;
            btn.textContent = 'Enviar Música';
        }
    });

    // ── My songs ──
    async function loadMySongs() {
        const container = document.getElementById('mySongsList');
        container.innerHTML = '<div class="spinner" style="margin-top:40px"></div>';
        try {
            const songs = await Api.getArtistSongs();
            document.getElementById('songCount').textContent = songs.length;
            renderMySongs(songs, container);
        } catch {
            container.innerHTML = '<p style="color:var(--text-muted);padding:20px">Erro ao carregar músicas.</p>';
        }
    }

    function renderMySongs(songs, container) {
        if (!songs.length) {
            container.innerHTML = `<div class="empty-state">
                <svg width="48" height="48" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"
                          d="M9 19V6l12-3v13M9 19c0 1.105-1.343 2-3 2s-3-.895-3-2 1.343-2 3-2 3 .895 3 2zm12-3c0 1.105-1.343 2-3 2s-3-.895-3-2 1.343-2 3-2 3 .895 3 2z"/>
                </svg>
                <p>Nenhuma música cadastrada ainda.<br>Use o formulário acima para adicionar músicas.</p>
            </div>`;
            return;
        }

        container.innerHTML = '<div class="song-list">' + songs.map((s, i) => `
            <div class="song-item">
                <span class="song-num">${i + 1}</span>
                <div class="song-cover">
                    <svg width="20" height="20" fill="none" stroke="white" viewBox="0 0 24 24">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2"
                              d="M9 19V6l12-3v13M9 19c0 1.105-1.343 2-3 2s-3-.895-3-2 1.343-2 3-2 3 .895 3 2z"/>
                    </svg>
                </div>
                <div class="song-info">
                    <div class="song-title">${escHtml(s.title)}</div>
                    <div class="song-artist">${escHtml(s.artistName)}</div>
                </div>
                <span class="song-genre">${escHtml(s.genre)}</span>
            </div>`).join('') + '</div>';
    }

    function escHtml(s) {
        return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
    }

    await loadMySongs();
});
