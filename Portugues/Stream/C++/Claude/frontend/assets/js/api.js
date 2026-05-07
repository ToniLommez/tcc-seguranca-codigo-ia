const API_BASE = '';

const Api = {
    getToken() { return localStorage.getItem('token'); },
    getUser()  { return JSON.parse(localStorage.getItem('user') || 'null'); },

    setSession(data) {
        localStorage.setItem('token', data.token);
        localStorage.setItem('user', JSON.stringify({ name: data.name, email: data.email, type: data.type }));
    },

    clearSession() {
        localStorage.removeItem('token');
        localStorage.removeItem('user');
    },

    requireAuth(expectedType = null) {
        const user = this.getUser();
        if (!user || !this.getToken()) {
            window.location.href = '/index.html';
            return null;
        }
        if (expectedType && user.type !== expectedType) {
            if (user.type === 'ARTISTA') window.location.href = '/artist/dashboard.html';
            else                         window.location.href = '/user/dashboard.html';
            return null;
        }
        return user;
    },

    async request(method, path, body = null, isFormData = false) {
        const headers = { 'Authorization': 'Bearer ' + (this.getToken() || '') };
        if (!isFormData && body) headers['Content-Type'] = 'application/json';

        const opts = { method, headers };
        if (body) opts.body = isFormData ? body : JSON.stringify(body);

        const res = await fetch(API_BASE + path, opts);
        const text = await res.text();
        let json;
        try { json = JSON.parse(text); } catch { json = { error: text }; }

        if (!res.ok) throw new Error(json.error || `HTTP ${res.status}`);
        return json;
    },

    async register(name, email, password, type) {
        return this.request('POST', '/api/auth/register', { name, email, password, type });
    },

    async login(email, password) {
        const data = await this.request('POST', '/api/auth/login', { email, password });
        this.setSession(data);
        return data;
    },

    async getSongs()                   { return this.request('GET', '/api/music'); },
    async getArtistSongs()             { return this.request('GET', '/api/music/my-songs'); },
    async searchSongs(title='', artist='', genre='') {
        const params = new URLSearchParams();
        if (title)  params.append('title', title);
        if (artist) params.append('artist', artist);
        if (genre)  params.append('genre', genre);
        return this.request('GET', '/api/music/search?' + params.toString());
    },

    async uploadSong(formData) {
        return this.request('POST', '/api/music/upload', formData, true);
    },

    getStreamUrl(songId) {
        return `${API_BASE}/api/music/${songId}/stream`;
    }
};

function showAlert(container, message, type = 'error') {
    const el = document.getElementById(container);
    if (!el) return;
    el.innerHTML = `<div class="alert alert-${type}">${message}</div>`;
    if (type === 'success') setTimeout(() => { el.innerHTML = ''; }, 4000);
}

function formatTime(sec) {
    if (!isFinite(sec) || isNaN(sec)) return '0:00';
    const m = Math.floor(sec / 60);
    const s = Math.floor(sec % 60);
    return `${m}:${s.toString().padStart(2, '0')}`;
}
