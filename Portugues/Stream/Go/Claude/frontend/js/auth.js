const API = '';

function saveSession(data) {
  localStorage.setItem('token', data.token);
  localStorage.setItem('user', JSON.stringify(data.user));
}

function getUser() {
  try { return JSON.parse(localStorage.getItem('user')); } catch { return null; }
}

function getToken() {
  return localStorage.getItem('token') || '';
}

function logout() {
  localStorage.removeItem('token');
  localStorage.removeItem('user');
  window.location.href = '/';
}

function redirectIfLoggedIn() {
  const user = getUser();
  const token = getToken();
  if (!user || !token) return;
  if (user.type === 'ARTISTA') window.location.href = '/artist.html';
  else window.location.href = '/user.html';
}

function requireAuth(expectedType) {
  const user = getUser();
  const token = getToken();
  if (!user || !token) { window.location.href = '/'; return null; }
  if (user.type !== expectedType) {
    logout();
    return null;
  }
  return user;
}

function showAlert(el, message, type = 'error') {
  el.textContent = message;
  el.className = `alert alert-${type} show`;
}

// ── Login form ────────────────────────────────────
const loginForm = document.getElementById('loginForm');
if (loginForm) {
  redirectIfLoggedIn();

  loginForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const alert = document.getElementById('loginAlert');
    const btn   = document.getElementById('loginBtn');

    const email    = document.getElementById('email').value.trim();
    const password = document.getElementById('password').value;

    btn.disabled = true;
    btn.innerHTML = '<span class="spinner"></span> Entrando…';

    try {
      const res = await fetch(`${API}/api/auth/login`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ email, password }),
      });
      const data = await res.json();

      if (!res.ok) {
        showAlert(alert, data.error || 'Erro ao entrar');
        return;
      }

      saveSession(data);
      if (data.user.type === 'ARTISTA') window.location.href = '/artist.html';
      else window.location.href = '/user.html';
    } catch {
      showAlert(alert, 'Erro de conexão com o servidor');
    } finally {
      btn.disabled = false;
      btn.textContent = 'Entrar';
    }
  });
}

// ── Register form ─────────────────────────────────
const registerForm = document.getElementById('registerForm');
if (registerForm) {
  redirectIfLoggedIn();

  registerForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const alert = document.getElementById('registerAlert');
    const btn   = document.getElementById('registerBtn');

    const name     = document.getElementById('name').value.trim();
    const email    = document.getElementById('email').value.trim();
    const password = document.getElementById('password').value;
    const type     = document.querySelector('input[name="type"]:checked')?.value;

    if (!type) { showAlert(alert, 'Selecione o tipo de conta'); return; }
    if (password.length < 6) { showAlert(alert, 'Senha deve ter ao menos 6 caracteres'); return; }

    btn.disabled = true;
    btn.innerHTML = '<span class="spinner"></span> Cadastrando…';

    try {
      const res = await fetch(`${API}/api/auth/register`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name, email, password, type }),
      });
      const data = await res.json();

      if (!res.ok) {
        showAlert(alert, data.error || 'Erro ao cadastrar');
        return;
      }

      saveSession(data);
      if (data.user.type === 'ARTISTA') window.location.href = '/artist.html';
      else window.location.href = '/user.html';
    } catch {
      showAlert(alert, 'Erro de conexão com o servidor');
    } finally {
      btn.disabled = false;
      btn.textContent = 'Criar conta';
    }
  });
}
