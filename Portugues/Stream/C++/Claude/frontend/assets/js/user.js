document.addEventListener('DOMContentLoaded', async () => {
    const user = Api.requireAuth('USUARIO');
    if (!user) return;

    document.getElementById('userName').textContent    = user.name;
    document.getElementById('userEmail').textContent   = user.email;
    document.getElementById('userInitial').textContent = user.name[0].toUpperCase();

    document.getElementById('logoutBtn').addEventListener('click', () => {
        Api.clearSession();
        window.location.href = '/index.html';
    });

    // ── Audio engine ──
    const audio = new Audio();
    audio.preload = 'metadata';
    let currentSongId = null;
    let allSongs = [];
    let currentIndex = -1;

    const playerBar     = document.getElementById('playerBar');
    const nowTitle      = document.getElementById('nowTitle');
    const nowArtist     = document.getElementById('nowArtist');
    const progressFill  = document.getElementById('progressFill');
    const progressBar   = document.getElementById('progressBar');
    const currentTime   = document.getElementById('currentTime');
    const totalTime     = document.getElementById('totalTime');
    const playPauseBtn  = document.getElementById('playPauseBtn');
    const volumeSlider  = document.getElementById('volumeSlider');

    function updatePlayIcon(playing) {
        playPauseBtn.innerHTML = playing
            ? `<svg width="20" height="20" fill="currentColor" viewBox="0 0 24 24">
                   <path d="M6 19h4V5H6v14zm8-14v14h4V5h-4z"/>
               </svg>`
            : `<svg width="20" height="20" fill="currentColor" viewBox="0 0 24 24">
                   <path d="M8 5v14l11-7z"/>
               </svg>`;
    }

    function playSong(song, index) {
        currentSongId = song.id;
        currentIndex  = index;

        const url = Api.getStreamUrl(song.id) + '?t=' + Api.getToken();
        audio.src = url;
        audio.load();
        audio.play().catch(() => {});

        nowTitle.textContent  = song.title;
        nowArtist.textContent = song.artistName;
        playerBar.classList.remove('hidden');

        // Mark playing in list
        document.querySelectorAll('.song-item').forEach(el => {
            el.classList.toggle('playing', el.dataset.songId === song.id);
        });
        document.querySelectorAll('.btn-play').forEach(btn => {
            const isCurrent = btn.dataset.songId === song.id;
            btn.innerHTML = isCurrent
                ? `<svg width="16" height="16" fill="currentColor" viewBox="0 0 24 24"><path d="M6 19h4V5H6v14zm8-14v14h4V5h-4z"/></svg>`
                : `<svg width="16" height="16" fill="currentColor" viewBox="0 0 24 24"><path d="M8 5v14l11-7z"/></svg>`;
        });
    }

    audio.addEventListener('timeupdate', () => {
        if (!audio.duration) return;
        const pct = (audio.currentTime / audio.duration) * 100;
        progressFill.style.width = pct + '%';
        currentTime.textContent  = formatTime(audio.currentTime);
    });
    audio.addEventListener('loadedmetadata', () => {
        totalTime.textContent = formatTime(audio.duration);
    });
    audio.addEventListener('play',  () => updatePlayIcon(true));
    audio.addEventListener('pause', () => updatePlayIcon(false));
    audio.addEventListener('ended', () => {
        if (currentIndex + 1 < allSongs.length) playSong(allSongs[currentIndex + 1], currentIndex + 1);
        else updatePlayIcon(false);
    });

    progressBar.addEventListener('click', e => {
        if (!audio.duration) return;
        const rect = progressBar.getBoundingClientRect();
        audio.currentTime = ((e.clientX - rect.left) / rect.width) * audio.duration;
    });

    playPauseBtn.addEventListener('click', () => {
        if (audio.paused) audio.play();
        else audio.pause();
    });

    document.getElementById('prevBtn').addEventListener('click', () => {
        if (currentIndex > 0) playSong(allSongs[currentIndex - 1], currentIndex - 1);
    });
    document.getElementById('nextBtn').addEventListener('click', () => {
        if (currentIndex + 1 < allSongs.length) playSong(allSongs[currentIndex + 1], currentIndex + 1);
    });

    volumeSlider.addEventListener('input', () => {
        audio.volume = volumeSlider.value / 100;
    });
    audio.volume = 0.8;
    volumeSlider.value = 80;

    // ── Song rendering ──
    function escHtml(s) {
        return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
    }

    function renderSongs(songs, container) {
        if (!songs.length) {
            container.innerHTML = `<div class="empty-state">
                <svg width="48" height="48" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="1.5"
                          d="M9 19V6l12-3v13M9 19c0 1.105-1.343 2-3 2s-3-.895-3-2 1.343-2 3-2 3 .895 3 2z"/>
                </svg>
                <p>Nenhuma música encontrada.</p>
            </div>`;
            return;
        }

        container.innerHTML = '<div class="song-list">' + songs.map((s, i) => `
            <div class="song-item" data-song-id="${s.id}" data-index="${i}">
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
                <div class="song-actions">
                    <button class="btn-icon btn-play" data-song-id="${s.id}" data-index="${i}" title="Reproduzir">
                        <svg width="16" height="16" fill="currentColor" viewBox="0 0 24 24">
                            <path d="M8 5v14l11-7z"/>
                        </svg>
                    </button>
                </div>
            </div>`).join('') + '</div>';

        container.querySelectorAll('.btn-play').forEach(btn => {
            btn.addEventListener('click', e => {
                e.stopPropagation();
                const idx = parseInt(btn.dataset.index);
                playSong(songs[idx], idx);
            });
        });
        container.querySelectorAll('.song-item').forEach(item => {
            item.addEventListener('click', () => {
                const idx = parseInt(item.dataset.index);
                playSong(songs[idx], idx);
            });
        });

        // Mark currently playing
        if (currentSongId) {
            const playing = container.querySelector(`[data-song-id="${currentSongId}"]`);
            if (playing) playing.classList.add('playing');
        }
    }

    // ── Load all songs ──
    async function loadAllSongs() {
        const container = document.getElementById('songListContainer');
        container.innerHTML = '<div class="spinner" style="margin-top:40px"></div>';
        try {
            allSongs = await Api.getSongs();
            document.getElementById('totalSongsCount').textContent = allSongs.length;
            document.getElementById('resultsLabel').textContent =
                `${allSongs.length} música${allSongs.length !== 1 ? 's' : ''} disponível${allSongs.length !== 1 ? 'is' : ''}`;
            renderSongs(allSongs, container);
        } catch (err) {
            container.innerHTML = `<p style="color:var(--text-muted);padding:20px">${err.message}</p>`;
        }
    }

    // ── Search ──
    let searchTimeout;
    const searchInput = document.getElementById('searchInput');
    const genreFilter = document.getElementById('genreFilter');

    async function doSearch() {
        const title  = searchInput.value.trim();
        const genre  = genreFilter.value;
        const container = document.getElementById('songListContainer');
        container.innerHTML = '<div class="spinner" style="margin-top:40px"></div>';
        try {
            const songs = await Api.searchSongs(title, '', genre);
            allSongs = songs;
            document.getElementById('resultsLabel').textContent =
                `${songs.length} resultado${songs.length !== 1 ? 's' : ''} encontrado${songs.length !== 1 ? 's' : ''}`;
            renderSongs(songs, container);
        } catch {
            container.innerHTML = '<p style="color:var(--text-muted);padding:20px">Erro ao buscar.</p>';
        }
    }

    searchInput.addEventListener('input', () => {
        clearTimeout(searchTimeout);
        if (!searchInput.value.trim() && !genreFilter.value) {
            loadAllSongs(); return;
        }
        searchTimeout = setTimeout(doSearch, 400);
    });

    genreFilter.addEventListener('change', () => {
        if (!searchInput.value.trim() && !genreFilter.value) { loadAllSongs(); return; }
        doSearch();
    });

    document.getElementById('clearSearch').addEventListener('click', () => {
        searchInput.value = '';
        genreFilter.value = '';
        loadAllSongs();
    });

    await loadAllSongs();
});
