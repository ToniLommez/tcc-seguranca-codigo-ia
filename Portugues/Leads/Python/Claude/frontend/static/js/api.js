const API_BASE = '';

function getToken() {
  return localStorage.getItem('access_token');
}

function setToken(token) {
  localStorage.setItem('access_token', token);
}

function setUser(user) {
  localStorage.setItem('current_user', JSON.stringify(user));
}

function getUser() {
  try { return JSON.parse(localStorage.getItem('current_user')); } catch { return null; }
}

function logout() {
  localStorage.removeItem('access_token');
  localStorage.removeItem('current_user');
  window.location.href = '/';
}

function requireAuth() {
  if (!getToken()) { window.location.href = '/'; }
}

async function apiFetch(path, options = {}) {
  const token = getToken();
  const headers = { 'Content-Type': 'application/json', ...(options.headers || {}) };
  if (token) headers['Authorization'] = `Bearer ${token}`;
  if (options.body instanceof FormData) delete headers['Content-Type'];

  const res = await fetch(API_BASE + path, { ...options, headers });

  if (res.status === 401) { logout(); return; }

  if (!res.ok) {
    let msg = `Erro ${res.status}`;
    try { const d = await res.json(); msg = d.detail || JSON.stringify(d); } catch {}
    throw new Error(msg);
  }

  if (res.status === 204) return null;
  return res.json();
}

async function apiDownload(path, filename) {
  const token = getToken();
  const res = await fetch(API_BASE + path, { headers: { Authorization: `Bearer ${token}` } });
  if (!res.ok) throw new Error(`Erro ${res.status}`);
  const blob = await res.blob();
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = filename; a.click();
  URL.revokeObjectURL(url);
}

const statusLabels = {
  novo: 'Novo', em_contato: 'Em Contato', qualificado: 'Qualificado', perdido: 'Perdido',
};
const fonteLabels = {
  indicacao: 'Indicação', site: 'Site', evento: 'Evento',
  redes_sociais: 'Redes Sociais', email_marketing: 'E-mail Marketing', outros: 'Outros',
};
const tipoLabels = {
  ligacao: '📞 Ligação', email: '📧 E-mail', reuniao: '🤝 Reunião',
  mensagem: '💬 Mensagem', outro: '📝 Outro',
};

function statusBadge(status) {
  return `<span class="badge badge-${status}">${statusLabels[status] || status}</span>`;
}
function fonteBadge(fonte) {
  return `<span class="badge badge-${fonte}">${fonteLabels[fonte] || fonte}</span>`;
}
function fmtDate(d) {
  if (!d) return '—';
  try { return new Date(d).toLocaleDateString('pt-BR'); } catch { return d; }
}
function fmtDateTime(d) {
  if (!d) return '—';
  try { return new Date(d).toLocaleString('pt-BR'); } catch { return d; }
}
function showAlert(el, msg, type = 'danger') {
  el.className = `alert alert-${type}`;
  el.textContent = msg;
  el.classList.remove('hidden');
  setTimeout(() => el.classList.add('hidden'), 5000);
}
