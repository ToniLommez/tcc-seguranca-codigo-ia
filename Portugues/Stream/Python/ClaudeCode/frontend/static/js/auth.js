/* Funções compartilhadas de autenticação — incluído em todas as páginas */

const API_BASE = '';

function getToken() {
  return localStorage.getItem('ss_token');
}

function getUser() {
  try {
    return JSON.parse(localStorage.getItem('ss_user') || 'null');
  } catch {
    return null;
  }
}

function setSession(token, user) {
  localStorage.setItem('ss_token', token);
  localStorage.setItem('ss_user', JSON.stringify(user));
}

function clearSession() {
  localStorage.removeItem('ss_token');
  localStorage.removeItem('ss_user');
}

function logout() {
  clearSession();
  window.location.href = '/';
}

async function apiRequest(url, options = {}) {
  const token = getToken();
  const headers = { ...options.headers };
  if (token) headers['Authorization'] = `Bearer ${token}`;

  const res = await fetch(API_BASE + url, { ...options, headers });

  if (res.status === 401) {
    clearSession();
    window.location.href = '/';
    return null;
  }
  return res;
}

async function login(email, password) {
  const res = await fetch(`${API_BASE}/api/auth/login`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ email, password }),
  });
  const data = await res.json();
  if (!res.ok) throw new Error(data.detail || 'Erro ao fazer login.');
  setSession(data.access_token, data.user);
  redirectByRole(data.user);
}

async function register(name, email, password, user_type) {
  const res = await fetch(`${API_BASE}/api/auth/register`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ name, email, password, user_type }),
  });
  const data = await res.json();
  if (!res.ok) throw new Error(data.detail || 'Erro ao cadastrar.');
  setSession(data.access_token, data.user);
  redirectByRole(data.user);
}

function redirectByRole(user) {
  if (user.user_type === 'ARTISTA') {
    window.location.href = '/artist';
  } else {
    window.location.href = '/user';
  }
}

function requireAuth(expectedType) {
  const user = getUser();
  if (!user || !getToken()) {
    window.location.href = '/';
    return null;
  }
  if (expectedType && user.user_type !== expectedType) {
    redirectByRole(user);
    return null;
  }
  return user;
}
