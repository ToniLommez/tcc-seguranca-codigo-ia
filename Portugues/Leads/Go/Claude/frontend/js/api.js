// ─── Lead Manager API Client ────────────────────────────────────────────────
const API_BASE = '/api';

const api = {
  _token() {
    return localStorage.getItem('lm_token');
  },

  _headers(isForm = false) {
    const h = { 'Authorization': 'Bearer ' + this._token() };
    if (!isForm) h['Content-Type'] = 'application/json';
    return h;
  },

  _handleUnauth(res) {
    if (res.status === 401) {
      localStorage.clear();
      window.location.href = '/app/index.html';
      return true;
    }
    return false;
  },

  async request(method, path, body = null, isForm = false) {
    const opts = { method, headers: this._headers(isForm) };
    if (body) opts.body = isForm ? body : JSON.stringify(body);
    const res = await fetch(API_BASE + path, opts);
    if (this._handleUnauth(res)) return null;
    const data = await res.json().catch(() => ({}));
    if (!res.ok) throw new Error(data.error || 'Erro desconhecido');
    return data;
  },

  // Auth
  register: (name, email, password) =>
    fetch(API_BASE + '/auth/register', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name, email, password })
    }).then(r => r.json()),

  login: (email, password) =>
    fetch(API_BASE + '/auth/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ email, password })
    }).then(r => r.json()),

  // Dashboard
  getDashboard: () => api.request('GET', '/dashboard'),

  // Leads
  listLeads: (params = {}) => {
    const q = new URLSearchParams(params).toString();
    return api.request('GET', '/leads' + (q ? '?' + q : ''));
  },
  createLead: (data) => api.request('POST', '/leads', data),
  getLead: (id) => api.request('GET', '/leads/' + id),
  updateLead: (id, data) => api.request('PUT', '/leads/' + id, data),
  deleteLead: (id) => api.request('DELETE', '/leads/' + id),
  addInteraction: (id, data) => api.request('POST', '/leads/' + id + '/interactions', data),

  exportLeads: (format = 'csv') => {
    const url = API_BASE + '/leads/export?format=' + format;
    const a = document.createElement('a');
    a.href = url;
    a.setAttribute('download', '');
    // Adiciona token via custom header não é possível em <a>, usar fetch blob
    fetch(url, { headers: { 'Authorization': 'Bearer ' + api._token() } })
      .then(r => r.blob())
      .then(blob => {
        const ext = format === 'xlsx' ? '.xlsx' : '.csv';
        const filename = 'leads_' + new Date().toISOString().slice(0,10) + ext;
        const url2 = URL.createObjectURL(blob);
        const link = document.createElement('a');
        link.href = url2;
        link.download = filename;
        link.click();
        URL.revokeObjectURL(url2);
      });
  },

  importLeads: (file) => {
    const form = new FormData();
    form.append('file', file);
    return fetch(API_BASE + '/leads/import', {
      method: 'POST',
      headers: { 'Authorization': 'Bearer ' + api._token() },
      body: form
    }).then(r => r.json());
  }
};

// ─── Auth Guard ──────────────────────────────────────────────────────────────
function requireAuth() {
  const token = localStorage.getItem('lm_token');
  if (!token) {
    window.location.href = '/app/index.html';
    return false;
  }
  return true;
}

function getUser() {
  try {
    const u = localStorage.getItem('lm_user');
    return u ? JSON.parse(u) : null;
  } catch { return null; }
}

function logout() {
  localStorage.clear();
  window.location.href = '/app/index.html';
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
const statusConfig = {
  novo:        { label: 'Novo',        color: '#3b82f6', bg: '#1e3a5f' },
  em_contato:  { label: 'Em Contato',  color: '#f59e0b', bg: '#451a03' },
  qualificado: { label: 'Qualificado', color: '#22c55e', bg: '#14532d' },
  perdido:     { label: 'Perdido',     color: '#ef4444', bg: '#450a0a' },
};
const sourceConfig = {
  indicacao: { label: 'Indicação',  icon: '👥' },
  site:      { label: 'Site',       icon: '🌐' },
  evento:    { label: 'Evento',     icon: '📅' },
  linkedin:  { label: 'LinkedIn',   icon: '💼' },
  email:     { label: 'E-mail',     icon: '📧' },
  outro:     { label: 'Outro',      icon: '📌' },
};
const interactionConfig = {
  ligacao:  { label: 'Ligação',  icon: '📞' },
  email:    { label: 'E-mail',   icon: '📧' },
  reuniao:  { label: 'Reunião',  icon: '🤝' },
  nota:     { label: 'Nota',     icon: '📝' },
};

function statusBadge(status) {
  const s = statusConfig[status] || { label: status, color: '#94a3b8', bg: '#1e293b' };
  return `<span class="badge" style="color:${s.color};background:${s.bg};border:1px solid ${s.color}33">${s.label}</span>`;
}
function sourceBadge(source) {
  const s = sourceConfig[source] || { label: source, icon: '📌' };
  return `<span class="source-tag">${s.icon} ${s.label}</span>`;
}
function formatDate(d) {
  if (!d) return '—';
  const date = typeof d === 'string' ? new Date(d) : new Date(d._seconds ? d._seconds * 1000 : d);
  return date.toLocaleDateString('pt-BR', { day: '2-digit', month: '2-digit', year: 'numeric' });
}
function formatDatetime(d) {
  if (!d) return '—';
  const date = typeof d === 'string' ? new Date(d) : new Date(d._seconds ? d._seconds * 1000 : d);
  return date.toLocaleString('pt-BR', { day: '2-digit', month: '2-digit', year: 'numeric', hour: '2-digit', minute: '2-digit' });
}
function initials(name) {
  if (!name) return '?';
  return name.split(' ').slice(0,2).map(n => n[0]).join('').toUpperCase();
}

function showToast(msg, type = 'success') {
  const existing = document.querySelector('.lm-toast');
  if (existing) existing.remove();
  const t = document.createElement('div');
  t.className = 'lm-toast lm-toast-' + type;
  t.textContent = msg;
  document.body.appendChild(t);
  requestAnimationFrame(() => t.classList.add('show'));
  setTimeout(() => { t.classList.remove('show'); setTimeout(() => t.remove(), 300); }, 3000);
}

// ─── Shared Styles ────────────────────────────────────────────────────────────
function injectSharedStyles() {
  const style = document.createElement('style');
  style.textContent = `
    .badge {
      display:inline-block; padding:2px 10px; border-radius:3px;
      font-size:11px; font-weight:600; letter-spacing:.5px; text-transform:uppercase;
      font-family:'IBM Plex Mono',monospace;
    }
    .source-tag {
      display:inline-block; padding:2px 8px; border-radius:3px;
      font-size:12px; color:#94a3b8; background:#1e293b; border:1px solid #334155;
    }
    .lm-toast {
      position:fixed; bottom:24px; right:24px; padding:12px 20px;
      border-radius:4px; font-size:13px; font-weight:500; z-index:9999;
      opacity:0; transform:translateY(10px); transition:all .25s ease;
      font-family:'IBM Plex Sans',sans-serif;
    }
    .lm-toast.show { opacity:1; transform:translateY(0); }
    .lm-toast-success { background:#14532d; color:#86efac; border:1px solid #22c55e44; }
    .lm-toast-error   { background:#450a0a; color:#fca5a5; border:1px solid #ef444444; }
    .lm-toast-info    { background:#1e3a5f; color:#93c5fd; border:1px solid #3b82f644; }
  `;
  document.head.appendChild(style);
}
