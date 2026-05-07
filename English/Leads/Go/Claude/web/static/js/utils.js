function showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    container.appendChild(toast);
    setTimeout(() => toast.remove(), 4000);
}

function formatDate(dateStr) {
    if (!dateStr) return '—';
    const d = new Date(dateStr);
    if (isNaN(d)) return '—';
    return d.toLocaleDateString('en-US', { year: 'numeric', month: 'short', day: 'numeric' });
}

function formatDateTime(dateStr) {
    if (!dateStr) return '—';
    const d = new Date(dateStr);
    if (isNaN(d)) return '—';
    return d.toLocaleString('en-US', { year: 'numeric', month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit' });
}

function initials(name) {
    if (!name) return '?';
    return name.split(' ').map(p => p[0]).join('').toUpperCase().slice(0, 2);
}

function statusBadge(status) {
    const labels = { new: 'New', contacted: 'Contacted', qualified: 'Qualified', lost: 'Lost' };
    return `<span class="status-badge ${status}">${labels[status] || status}</span>`;
}

function requireAuth() {
    if (!getToken()) {
        window.location.href = '/';
        return false;
    }
    return true;
}

function logout() {
    clearAuth();
    window.location.href = '/';
}

function populateSidebar() {
    const user = getUser();
    if (!user) return;
    const el = document.getElementById('sidebar-user');
    if (el) {
        el.innerHTML = `
            <div class="user-name">${user.name || 'User'}</div>
            <div class="user-email">${user.email || ''}</div>
            <button onclick="logout()">Sign Out</button>
        `;
    }
    const currentPage = window.location.pathname;
    document.querySelectorAll('.sidebar-nav a').forEach(a => {
        a.classList.toggle('active', a.getAttribute('href') === currentPage);
    });
}

function debounce(fn, delay) {
    let t;
    return (...args) => { clearTimeout(t); t = setTimeout(() => fn(...args), delay); };
}
