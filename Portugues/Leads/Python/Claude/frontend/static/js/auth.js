document.addEventListener('DOMContentLoaded', () => {
  if (getToken()) { window.location.href = '/dashboard'; return; }

  document.querySelectorAll('.auth-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.auth-tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.auth-pane').forEach(p => p.classList.remove('active'));
      tab.classList.add('active');
      document.getElementById(tab.dataset.pane).classList.add('active');
    });
  });

  // Login
  document.getElementById('login-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const alert = document.getElementById('login-alert');
    const btn = e.target.querySelector('button[type=submit]');
    btn.disabled = true; btn.textContent = 'Entrando…';
    try {
      const data = await apiFetch('/api/auth/login', {
        method: 'POST',
        body: JSON.stringify({
          email: document.getElementById('login-email').value,
          password: document.getElementById('login-password').value,
        }),
      });
      setToken(data.access_token);
      setUser(data.user);
      window.location.href = '/dashboard';
    } catch (err) {
      showAlert(alert, err.message);
      btn.disabled = false; btn.textContent = 'Entrar';
    }
  });

  // Register
  document.getElementById('register-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const alert = document.getElementById('register-alert');
    const btn = e.target.querySelector('button[type=submit]');
    const pwd = document.getElementById('register-password').value;
    const confirm = document.getElementById('register-confirm').value;
    if (pwd !== confirm) { showAlert(alert, 'As senhas não coincidem'); return; }
    btn.disabled = true; btn.textContent = 'Criando conta…';
    try {
      const data = await apiFetch('/api/auth/register', {
        method: 'POST',
        body: JSON.stringify({
          name: document.getElementById('register-name').value,
          email: document.getElementById('register-email').value,
          password: pwd,
        }),
      });
      setToken(data.access_token);
      setUser(data.user);
      window.location.href = '/dashboard';
    } catch (err) {
      showAlert(alert, err.message);
      btn.disabled = false; btn.textContent = 'Criar conta';
    }
  });
});
