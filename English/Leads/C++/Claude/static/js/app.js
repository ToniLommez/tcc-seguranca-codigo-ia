const API = '';
let token = localStorage.getItem('token');
let currentUser = JSON.parse(localStorage.getItem('user') || 'null');
let currentPage = 1;
let currentSort = { field: 'created_at', order: 'desc' };
let chartStatus = null, chartSource = null, chartTimeline = null;
let currentDetailId = null;

async function api(path, options = {}) {
    const headers = { 'Content-Type': 'application/json', ...options.headers };
    if (token) headers['Authorization'] = `Bearer ${token}`;

    const res = await fetch(API + path, { ...options, headers });

    if (res.status === 401) {
        logout();
        throw new Error('Session expired');
    }

    const contentType = res.headers.get('content-type') || '';
    if (contentType.includes('text/csv')) {
        return { csv: await res.text(), ok: res.ok };
    }

    const data = await res.json();
    if (!res.ok) throw new Error(data.error || 'Request failed');
    return data;
}

function showToast(message, type = 'info') {
    const toast = document.getElementById('toast');
    toast.textContent = message;
    toast.className = `toast ${type}`;
    toast.style.display = 'block';
    setTimeout(() => { toast.style.display = 'none'; }, 3000);
}

function showScreen(name) {
    document.getElementById('auth-screen').style.display = name === 'auth' ? '' : 'none';
    document.getElementById('main-screen').style.display = name === 'main' ? '' : 'none';
}

function showPage(name) {
    document.querySelectorAll('.page').forEach(p => p.style.display = 'none');
    const page = document.getElementById(`page-${name}`);
    if (page) page.style.display = '';

    document.querySelectorAll('.nav-link').forEach(l => l.classList.remove('active'));
    const link = document.querySelector(`.nav-link[data-page="${name}"]`);
    if (link) link.classList.add('active');

    if (name === 'dashboard') loadDashboard();
    if (name === 'leads') loadLeads();
}

// Auth
document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        const tab = btn.dataset.tab;
        document.getElementById('login-form').style.display = tab === 'login' ? '' : 'none';
        document.getElementById('register-form').style.display = tab === 'register' ? '' : 'none';
        document.getElementById('auth-error').style.display = 'none';
    });
});

document.getElementById('login-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    try {
        const data = await api('/api/auth/login', {
            method: 'POST',
            body: JSON.stringify({
                email: document.getElementById('login-email').value,
                password: document.getElementById('login-password').value
            })
        });
        token = data.token;
        currentUser = data.user;
        localStorage.setItem('token', token);
        localStorage.setItem('user', JSON.stringify(currentUser));
        document.getElementById('user-name').textContent = currentUser.name;
        showScreen('main');
        showPage('dashboard');
    } catch (err) {
        const errEl = document.getElementById('auth-error');
        errEl.textContent = err.message;
        errEl.style.display = '';
    }
});

document.getElementById('register-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    try {
        const data = await api('/api/auth/signup', {
            method: 'POST',
            body: JSON.stringify({
                name: document.getElementById('reg-name').value,
                email: document.getElementById('reg-email').value,
                password: document.getElementById('reg-password').value
            })
        });
        token = data.token;
        currentUser = data.user;
        localStorage.setItem('token', token);
        localStorage.setItem('user', JSON.stringify(currentUser));
        document.getElementById('user-name').textContent = currentUser.name;
        showScreen('main');
        showPage('dashboard');
        showToast('Account created successfully!', 'success');
    } catch (err) {
        const errEl = document.getElementById('auth-error');
        errEl.textContent = err.message;
        errEl.style.display = '';
    }
});

function logout() {
    token = null;
    currentUser = null;
    localStorage.removeItem('token');
    localStorage.removeItem('user');
    showScreen('auth');
}

document.getElementById('logout-btn').addEventListener('click', logout);

// Navigation
document.querySelectorAll('.nav-link').forEach(link => {
    link.addEventListener('click', (e) => {
        e.preventDefault();
        showPage(link.dataset.page);
    });
});

// Dashboard
async function loadDashboard() {
    try {
        const data = await api('/api/dashboard');

        document.getElementById('stat-total').textContent = data.total_leads || 0;
        document.getElementById('stat-new').textContent = (data.by_status && data.by_status.new) || 0;
        document.getElementById('stat-contacted').textContent = (data.by_status && data.by_status.contacted) || 0;
        document.getElementById('stat-qualified').textContent = (data.by_status && data.by_status.qualified) || 0;
        document.getElementById('stat-lost').textContent = (data.by_status && data.by_status.lost) || 0;

        renderCharts(data);
    } catch (err) {
        showToast('Failed to load dashboard: ' + err.message, 'error');
    }
}

function renderCharts(data) {
    const statusColors = {
        new: '#3b82f6', contacted: '#f59e0b',
        qualified: '#22c55e', lost: '#ef4444'
    };
    const statusLabels = Object.keys(data.by_status || {});
    const statusValues = statusLabels.map(k => data.by_status[k]);

    if (chartStatus) chartStatus.destroy();
    chartStatus = new Chart(document.getElementById('chart-status'), {
        type: 'doughnut',
        data: {
            labels: statusLabels.map(s => s.charAt(0).toUpperCase() + s.slice(1)),
            datasets: [{
                data: statusValues,
                backgroundColor: statusLabels.map(s => statusColors[s] || '#94a3b8')
            }]
        },
        options: { responsive: true, plugins: { legend: { position: 'bottom' } } }
    });

    const sourceLabels = Object.keys(data.by_source || {});
    const sourceValues = sourceLabels.map(k => data.by_source[k]);
    const sourceColors = ['#6366f1', '#ec4899', '#14b8a6', '#f97316', '#8b5cf6', '#64748b'];

    if (chartSource) chartSource.destroy();
    chartSource = new Chart(document.getElementById('chart-source'), {
        type: 'pie',
        data: {
            labels: sourceLabels.map(s => s.replace('_', ' ').replace(/\b\w/g, l => l.toUpperCase())),
            datasets: [{
                data: sourceValues,
                backgroundColor: sourceColors.slice(0, sourceLabels.length)
            }]
        },
        options: { responsive: true, plugins: { legend: { position: 'bottom' } } }
    });

    const timeLabels = Object.keys(data.captures_over_time || {}).sort();
    const timeValues = timeLabels.map(k => data.captures_over_time[k]);

    if (chartTimeline) chartTimeline.destroy();
    chartTimeline = new Chart(document.getElementById('chart-timeline'), {
        type: 'bar',
        data: {
            labels: timeLabels,
            datasets: [{
                label: 'Leads Captured',
                data: timeValues,
                backgroundColor: '#6366f1',
                borderRadius: 4
            }]
        },
        options: {
            responsive: true,
            scales: { y: { beginAtZero: true, ticks: { stepSize: 1 } } },
            plugins: { legend: { display: false } }
        }
    });
}

// Leads
async function loadLeads() {
    const tbody = document.getElementById('leads-tbody');
    tbody.innerHTML = '<tr><td colspan="7"><div class="loading"><div class="spinner"></div>Loading leads...</div></td></tr>';

    try {
        const params = new URLSearchParams({
            page: currentPage,
            page_size: 20,
            sort_by: currentSort.field,
            sort_order: currentSort.order
        });

        const search = document.getElementById('filter-search').value;
        const status = document.getElementById('filter-status').value;
        const source = document.getElementById('filter-source').value;
        const dateFrom = document.getElementById('filter-date-from').value;
        const dateTo = document.getElementById('filter-date-to').value;

        if (search) params.set('search', search);
        if (status) params.set('status', status);
        if (source) params.set('source', source);
        if (dateFrom) params.set('date_from', dateFrom);
        if (dateTo) params.set('date_to', dateTo);

        const data = await api(`/api/leads?${params}`);
        renderLeadsTable(data.leads, data.pagination);
    } catch (err) {
        tbody.innerHTML = `<tr><td colspan="7"><div class="empty-state"><h3>Error</h3><p>${err.message}</p></div></td></tr>`;
    }
}

function renderLeadsTable(leads, pagination) {
    const tbody = document.getElementById('leads-tbody');

    if (!leads || leads.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7"><div class="empty-state"><h3>No leads found</h3><p>Create your first lead or adjust your filters.</p></div></td></tr>';
        document.getElementById('pagination').innerHTML = '';
        return;
    }

    tbody.innerHTML = leads.map(lead => `
        <tr>
            <td><strong>${esc(lead.full_name)}</strong></td>
            <td>${esc(lead.email || '-')}</td>
            <td>${esc(lead.company || '-')}</td>
            <td>${esc(formatSource(lead.source))}</td>
            <td><span class="status-badge status-${lead.status}">${esc(lead.status)}</span></td>
            <td>${esc(lead.capture_date || '-')}</td>
            <td class="action-btns">
                <button class="btn btn-sm btn-outline" onclick="viewLead('${lead.id}')">View</button>
                <button class="btn btn-sm btn-outline" onclick="editLead('${lead.id}')">Edit</button>
                <button class="btn btn-sm btn-danger" onclick="deleteLead('${lead.id}')">Del</button>
            </td>
        </tr>
    `).join('');

    renderPagination(pagination);
}

function renderPagination(p) {
    const el = document.getElementById('pagination');
    if (!p || p.total_pages <= 1) { el.innerHTML = ''; return; }

    let html = `<button ${p.page <= 1 ? 'disabled' : ''} onclick="goToPage(${p.page - 1})">Prev</button>`;

    for (let i = 1; i <= p.total_pages; i++) {
        if (i === 1 || i === p.total_pages || Math.abs(i - p.page) <= 2) {
            html += `<button class="${i === p.page ? 'active' : ''}" onclick="goToPage(${i})">${i}</button>`;
        } else if (Math.abs(i - p.page) === 3) {
            html += '<span class="page-info">...</span>';
        }
    }

    html += `<button ${p.page >= p.total_pages ? 'disabled' : ''} onclick="goToPage(${p.page + 1})">Next</button>`;
    html += `<span class="page-info">${p.total} total leads</span>`;
    el.innerHTML = html;
}

function goToPage(page) {
    currentPage = page;
    loadLeads();
}

// Sorting
document.querySelectorAll('.data-table th[data-sort]').forEach(th => {
    th.addEventListener('click', () => {
        const field = th.dataset.sort;
        if (currentSort.field === field) {
            currentSort.order = currentSort.order === 'asc' ? 'desc' : 'asc';
        } else {
            currentSort.field = field;
            currentSort.order = 'asc';
        }
        currentPage = 1;
        loadLeads();
    });
});

// Filters
let filterTimeout;
document.getElementById('filter-search').addEventListener('input', () => {
    clearTimeout(filterTimeout);
    filterTimeout = setTimeout(() => { currentPage = 1; loadLeads(); }, 300);
});

['filter-status', 'filter-source', 'filter-date-from', 'filter-date-to'].forEach(id => {
    document.getElementById(id).addEventListener('change', () => { currentPage = 1; loadLeads(); });
});

document.getElementById('btn-clear-filters').addEventListener('click', () => {
    document.getElementById('filter-search').value = '';
    document.getElementById('filter-status').value = '';
    document.getElementById('filter-source').value = '';
    document.getElementById('filter-date-from').value = '';
    document.getElementById('filter-date-to').value = '';
    currentPage = 1;
    loadLeads();
});

// Lead CRUD
function openLeadModal(lead = null) {
    const modal = document.getElementById('lead-modal');
    const title = document.getElementById('modal-title');

    if (lead) {
        title.textContent = 'Edit Lead';
        document.getElementById('lead-id').value = lead.id;
        document.getElementById('lead-fullname').value = lead.full_name || '';
        document.getElementById('lead-email').value = lead.email || '';
        document.getElementById('lead-phone').value = lead.phone || '';
        document.getElementById('lead-company').value = lead.company || '';
        document.getElementById('lead-position').value = lead.position || '';
        document.getElementById('lead-source').value = lead.source || 'website';
        document.getElementById('lead-status').value = lead.status || 'new';
        document.getElementById('lead-capture-date').value = lead.capture_date || '';
        document.getElementById('lead-notes').value = lead.notes || '';
    } else {
        title.textContent = 'New Lead';
        document.getElementById('lead-form').reset();
        document.getElementById('lead-id').value = '';
        document.getElementById('lead-capture-date').value = new Date().toISOString().split('T')[0];
    }
    modal.style.display = '';
}

function closeLeadModal() {
    document.getElementById('lead-modal').style.display = 'none';
}

document.getElementById('btn-new-lead').addEventListener('click', () => openLeadModal());
document.getElementById('modal-close').addEventListener('click', closeLeadModal);
document.getElementById('btn-cancel-lead').addEventListener('click', closeLeadModal);

document.getElementById('lead-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const id = document.getElementById('lead-id').value;
    const body = {
        full_name: document.getElementById('lead-fullname').value,
        email: document.getElementById('lead-email').value,
        phone: document.getElementById('lead-phone').value,
        company: document.getElementById('lead-company').value,
        position: document.getElementById('lead-position').value,
        source: document.getElementById('lead-source').value,
        status: document.getElementById('lead-status').value,
        capture_date: document.getElementById('lead-capture-date').value,
        notes: document.getElementById('lead-notes').value
    };

    try {
        if (id) {
            await api(`/api/leads/${id}`, { method: 'PUT', body: JSON.stringify(body) });
            showToast('Lead updated successfully', 'success');
        } else {
            await api('/api/leads', { method: 'POST', body: JSON.stringify(body) });
            showToast('Lead created successfully', 'success');
        }
        closeLeadModal();
        loadLeads();
    } catch (err) {
        showToast(err.message, 'error');
    }
});

async function viewLead(id) {
    try {
        const lead = await api(`/api/leads/${id}`);
        currentDetailId = id;
        const content = document.getElementById('lead-detail-content');
        content.innerHTML = `
            <div class="detail-grid">
                <div class="detail-item">
                    <div class="detail-label">Full Name</div>
                    <div class="detail-value">${esc(lead.full_name)}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">Email</div>
                    <div class="detail-value">${esc(lead.email || '-')}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">Phone</div>
                    <div class="detail-value">${esc(lead.phone || '-')}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">Company</div>
                    <div class="detail-value">${esc(lead.company || '-')}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">Position</div>
                    <div class="detail-value">${esc(lead.position || '-')}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">Source</div>
                    <div class="detail-value">${esc(formatSource(lead.source))}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">Status</div>
                    <div class="detail-value"><span class="status-badge status-${lead.status}">${esc(lead.status)}</span></div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">Capture Date</div>
                    <div class="detail-value">${esc(lead.capture_date || '-')}</div>
                </div>
                <div class="detail-item detail-full">
                    <div class="detail-label">Notes</div>
                    <div class="detail-value">${esc(lead.notes || 'No notes')}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">Created</div>
                    <div class="detail-value">${esc(lead.created_at || '-')}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">Last Updated</div>
                    <div class="detail-value">${esc(lead.updated_at || '-')}</div>
                </div>
            </div>
        `;
        document.getElementById('detail-modal').style.display = '';
    } catch (err) {
        showToast(err.message, 'error');
    }
}

async function editLead(id) {
    try {
        const lead = await api(`/api/leads/${id}`);
        document.getElementById('detail-modal').style.display = 'none';
        openLeadModal(lead);
    } catch (err) {
        showToast(err.message, 'error');
    }
}

async function deleteLead(id) {
    if (!confirm('Are you sure you want to delete this lead?')) return;
    try {
        await api(`/api/leads/${id}`, { method: 'DELETE' });
        showToast('Lead deleted', 'success');
        document.getElementById('detail-modal').style.display = 'none';
        loadLeads();
    } catch (err) {
        showToast(err.message, 'error');
    }
}

document.getElementById('detail-close').addEventListener('click', () => {
    document.getElementById('detail-modal').style.display = 'none';
});

document.getElementById('btn-edit-from-detail').addEventListener('click', () => {
    if (currentDetailId) editLead(currentDetailId);
});

document.getElementById('btn-delete-from-detail').addEventListener('click', () => {
    if (currentDetailId) deleteLead(currentDetailId);
});

// Import / Export
document.getElementById('btn-export').addEventListener('click', async () => {
    try {
        const res = await fetch(API + '/api/leads/export', {
            headers: { 'Authorization': `Bearer ${token}` }
        });
        const blob = await res.blob();
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'leads_export.csv';
        a.click();
        URL.revokeObjectURL(url);
        showToast('Export downloaded', 'success');
    } catch (err) {
        showToast('Export failed: ' + err.message, 'error');
    }
});

document.getElementById('import-file').addEventListener('change', (e) => {
    document.getElementById('btn-import').disabled = !e.target.files.length;
});

document.getElementById('btn-import').addEventListener('click', async () => {
    const fileInput = document.getElementById('import-file');
    if (!fileInput.files.length) return;

    const file = fileInput.files[0];
    const text = await file.text();

    try {
        const data = await api('/api/leads/import', {
            method: 'POST',
            body: text,
            headers: { 'Content-Type': 'text/csv' }
        });
        const resultEl = document.getElementById('import-result');
        resultEl.className = 'import-result success';
        resultEl.textContent = `Imported ${data.imported} of ${data.total} leads. ${data.failed ? data.failed + ' failed.' : ''}`;
        resultEl.style.display = '';
        showToast('Import completed', 'success');
        fileInput.value = '';
        document.getElementById('btn-import').disabled = true;
    } catch (err) {
        const resultEl = document.getElementById('import-result');
        resultEl.className = 'import-result error';
        resultEl.textContent = 'Import failed: ' + err.message;
        resultEl.style.display = '';
    }
});

// Close modals on backdrop click
['lead-modal', 'detail-modal'].forEach(id => {
    document.getElementById(id).addEventListener('click', (e) => {
        if (e.target.classList.contains('modal')) {
            document.getElementById(id).style.display = 'none';
        }
    });
});

// Utilities
function esc(str) {
    if (!str) return '';
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
}

function formatSource(source) {
    if (!source) return '-';
    return source.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase());
}

// Init
if (token && currentUser) {
    document.getElementById('user-name').textContent = currentUser.name;
    showScreen('main');
    showPage('dashboard');
} else {
    showScreen('auth');
}
