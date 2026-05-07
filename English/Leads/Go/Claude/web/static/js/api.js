const API_BASE = '/api';

function getToken() {
    return localStorage.getItem('token');
}

function getUser() {
    try {
        return JSON.parse(localStorage.getItem('user') || 'null');
    } catch { return null; }
}

function setAuth(token, user) {
    localStorage.setItem('token', token);
    localStorage.setItem('user', JSON.stringify(user));
}

function clearAuth() {
    localStorage.removeItem('token');
    localStorage.removeItem('user');
}

async function apiFetch(path, options = {}) {
    const token = getToken();
    const headers = { 'Content-Type': 'application/json', ...options.headers };
    if (token) headers['Authorization'] = `Bearer ${token}`;

    const res = await fetch(`${API_BASE}${path}`, { ...options, headers });

    if (res.status === 401) {
        clearAuth();
        window.location.href = '/';
        return;
    }

    const text = await res.text();
    let data;
    try { data = JSON.parse(text); } catch { data = text; }

    if (!res.ok) {
        throw new Error(data?.error || `Request failed: ${res.status}`);
    }
    return data;
}

async function apiUpload(path, formData) {
    const token = getToken();
    const res = await fetch(`${API_BASE}${path}`, {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}` },
        body: formData,
    });

    const text = await res.text();
    let data;
    try { data = JSON.parse(text); } catch { data = text; }

    if (!res.ok) throw new Error(data?.error || `Upload failed: ${res.status}`);
    return data;
}

const auth = {
    signup: (body) => apiFetch('/auth/signup', { method: 'POST', body: JSON.stringify(body) }),
    login:  (body) => apiFetch('/auth/login',  { method: 'POST', body: JSON.stringify(body) }),
};

const leads = {
    list:    (params = {}) => apiFetch('/leads?' + new URLSearchParams(params)),
    create:  (body)        => apiFetch('/leads', { method: 'POST', body: JSON.stringify(body) }),
    get:     (id)          => apiFetch(`/leads/${id}`),
    update:  (id, body)    => apiFetch(`/leads/${id}`, { method: 'PUT', body: JSON.stringify(body) }),
    delete:  (id)          => apiFetch(`/leads/${id}`, { method: 'DELETE' }),
    stats:   ()            => apiFetch('/leads/stats'),
    interactions: {
        list:   (id)       => apiFetch(`/leads/${id}/interactions`),
        create: (id, body) => apiFetch(`/leads/${id}/interactions`, { method: 'POST', body: JSON.stringify(body) }),
    },
    exportCSV:   () => window.location.href = `${API_BASE}/leads/export/csv?token=${getToken()}`,
    exportExcel: () => window.location.href = `${API_BASE}/leads/export/excel?token=${getToken()}`,
    import: (formData) => apiUpload('/leads/import', formData),
};
