/**
 * api.js — centralised API client for Lead Manager
 */

const API_BASE = '/api';

function getToken() {
  return localStorage.getItem('lm_token');
}

function setToken(token) {
  localStorage.setItem('lm_token', token);
}

function clearAuth() {
  localStorage.removeItem('lm_token');
  localStorage.removeItem('lm_user');
}

function getUser() {
  try {
    return JSON.parse(localStorage.getItem('lm_user') || 'null');
  } catch { return null; }
}

function setUser(user) {
  localStorage.setItem('lm_user', JSON.stringify(user));
}

// Redirect to login if no token
function requireAuth() {
  if (!getToken()) {
    window.location.href = '/';
    return false;
  }
  return true;
}

async function apiFetch(path, options = {}) {
  const token = getToken();
  const headers = {
    'Content-Type': 'application/json',
    ...(token ? { 'Authorization': `Bearer ${token}` } : {}),
    ...(options.headers || {}),
  };

  const res = await fetch(API_BASE + path, {
    ...options,
    headers,
  });

  // Handle 401: redirect to login
  if (res.status === 401) {
    clearAuth();
    window.location.href = '/';
    throw new Error('Sessão expirada');
  }

  let data;
  const ct = res.headers.get('Content-Type') || '';
  if (ct.includes('application/json')) {
    data = await res.json();
  } else {
    data = await res.text();
  }

  if (!res.ok) {
    const msg = (data && data.error) ? data.error : `HTTP ${res.status}`;
    throw new Error(msg);
  }

  return data;
}

// ---- Auth API ----------------------------------------------------------
const Auth = {
  async register(name, email, password) {
    const data = await apiFetch('/auth/register', {
      method: 'POST',
      body: JSON.stringify({ name, email, password }),
    });
    setToken(data.token);
    setUser(data.user);
    return data;
  },

  async login(email, password) {
    const data = await apiFetch('/auth/login', {
      method: 'POST',
      body: JSON.stringify({ email, password }),
    });
    setToken(data.token);
    setUser(data.user);
    return data;
  },

  async me() {
    return apiFetch('/auth/me');
  },

  logout() {
    clearAuth();
    window.location.href = '/';
  },
};

// ---- Leads API ---------------------------------------------------------
const Leads = {
  async list(params = {}) {
    const q = new URLSearchParams();
    Object.entries(params).forEach(([k, v]) => {
      if (v !== undefined && v !== null && v !== '') q.set(k, v);
    });
    const qs = q.toString() ? '?' + q.toString() : '';
    return apiFetch('/leads' + qs);
  },

  async get(id) {
    return apiFetch(`/leads/${id}`);
  },

  async create(lead) {
    return apiFetch('/leads', {
      method: 'POST',
      body: JSON.stringify(lead),
    });
  },

  async update(id, lead) {
    return apiFetch(`/leads/${id}`, {
      method: 'PUT',
      body: JSON.stringify(lead),
    });
  },

  async delete(id) {
    return apiFetch(`/leads/${id}`, { method: 'DELETE' });
  },

  async addInteraction(leadId, interaction) {
    return apiFetch(`/leads/${leadId}/interactions`, {
      method: 'POST',
      body: JSON.stringify(interaction),
    });
  },

  async stats() {
    return apiFetch('/leads/stats');
  },

  async exportCsv() {
    const token = getToken();
    const res = await fetch(API_BASE + '/leads/export', {
      headers: { 'Authorization': `Bearer ${token}` },
    });
    if (!res.ok) throw new Error('Erro ao exportar');
    const blob = await res.blob();
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href     = url;
    a.download = 'leads.csv';
    a.click();
    URL.revokeObjectURL(url);
  },

  async importCsv(file) {
    const token   = getToken();
    const formData = new FormData();
    formData.append('file', file);
    const res = await fetch(API_BASE + '/leads/import', {
      method: 'POST',
      headers: { 'Authorization': `Bearer ${token}` },
      body: formData,
    });
    const data = await res.json();
    if (!res.ok) throw new Error(data.error || 'Erro ao importar');
    return data;
  },
};

// ---- UI helpers --------------------------------------------------------
function showToast(message, type = 'success') {
  let container = document.querySelector('.toast-container');
  if (!container) {
    container = document.createElement('div');
    container.className = 'toast-container';
    document.body.appendChild(container);
  }
  const toast = document.createElement('div');
  toast.className = `toast ${type}`;
  toast.innerHTML = `
    <span>${type === 'success' ? '✓' : '✕'}</span>
    <span>${message}</span>
  `;
  container.appendChild(toast);
  setTimeout(() => toast.remove(), 3500);
}

function showError(message)   { showToast(message, 'error'); }
function showSuccess(message) { showToast(message, 'success'); }

function formatDate(iso) {
  if (!iso) return '—';
  const d = new Date(iso);
  if (isNaN(d)) return iso;
  return d.toLocaleDateString('pt-BR', { day: '2-digit', month: '2-digit', year: 'numeric' });
}

function formatDateTime(iso) {
  if (!iso) return '—';
  const d = new Date(iso);
  if (isNaN(d)) return iso;
  return d.toLocaleString('pt-BR', {
    day: '2-digit', month: '2-digit', year: 'numeric',
    hour: '2-digit', minute: '2-digit',
  });
}

const STATUS_LABELS = {
  new:        'Novo',
  in_contact: 'Em Contato',
  qualified:  'Qualificado',
  lost:       'Perdido',
};
const SOURCE_LABELS = {
  indication:   'Indicação',
  site:         'Site',
  event:        'Evento',
  social_media: 'Redes Sociais',
  other:        'Outro',
};

function statusBadge(status) {
  const label = STATUS_LABELS[status] || status;
  return `<span class="badge badge-${status}">${label}</span>`;
}
function sourceBadge(source) {
  const label = SOURCE_LABELS[source] || source;
  return `<span class="badge badge-${source}">${label}</span>`;
}

// Populate user info in sidebar
function populateSidebarUser() {
  const user = getUser();
  if (!user) return;
  const nameEl  = document.getElementById('sidebar-user-name');
  const emailEl = document.getElementById('sidebar-user-email');
  const avatarEl= document.getElementById('sidebar-user-avatar');
  if (nameEl)   nameEl.textContent  = user.name  || user.email;
  if (emailEl)  emailEl.textContent = user.email || '';
  if (avatarEl) avatarEl.textContent = (user.name || user.email || 'U')[0].toUpperCase();
}
