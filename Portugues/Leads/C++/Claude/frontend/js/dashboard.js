// dashboard.js — Dashboard page logic

document.addEventListener('DOMContentLoaded', async () => {
  if (!requireAuth()) return;
  populateSidebarUser();
  await loadDashboard();
});

async function loadDashboard() {
  try {
    const stats = await Leads.stats();
    renderStats(stats);
    renderCharts(stats);
    await loadRecentLeads();
    document.getElementById('loading').classList.add('hidden');
    document.getElementById('dashboard-content').classList.remove('hidden');
  } catch (err) {
    showError('Erro ao carregar dashboard: ' + err.message);
    document.getElementById('loading').innerHTML =
      `<p style="color:var(--danger)">Erro: ${err.message}</p>`;
  }
}

function renderStats(stats) {
  const byStatus = stats.by_status || {};
  document.getElementById('stat-total').textContent      = stats.total || 0;
  document.getElementById('stat-qualified').textContent  = byStatus.qualified  || 0;
  document.getElementById('stat-in-contact').textContent = byStatus.in_contact || 0;
  document.getElementById('stat-lost').textContent       = byStatus.lost       || 0;
}

function renderBar(containerId, data, colorMap) {
  const container = document.getElementById(containerId);
  if (!container) return;

  const entries = Object.entries(data).filter(([, v]) => v > 0);
  if (entries.length === 0) {
    container.innerHTML = '<p class="text-muted text-sm">Sem dados</p>';
    return;
  }

  const max = Math.max(...entries.map(([, v]) => v), 1);
  container.innerHTML = entries.map(([key, val]) => {
    const pct   = Math.round((val / max) * 100);
    const color = (colorMap && colorMap[key]) || 'var(--primary)';
    const label = STATUS_LABELS[key] || SOURCE_LABELS[key] || key;
    return `
      <div class="bar-row">
        <span class="bar-label">${label}</span>
        <div class="bar-track">
          <div class="bar-fill" style="width:${pct}%;background:${color}"></div>
        </div>
        <span class="bar-value">${val}</span>
      </div>`;
  }).join('');
}

function renderCharts(stats) {
  const statusColors = {
    new:        'var(--primary)',
    in_contact: 'var(--warning)',
    qualified:  'var(--success)',
    lost:       'var(--text-muted)',
  };
  const sourceColors = {
    site:         'var(--primary)',
    indication:   'var(--info)',
    event:        'var(--warning)',
    social_media: '#7C3AED',
    other:        'var(--text-muted)',
  };

  renderBar('status-chart', stats.by_status || {}, statusColors);
  renderBar('source-chart', stats.by_source || {}, sourceColors);

  // Monthly chart: sort by month key
  const monthly = (stats.monthly || [])
    .sort((a, b) => a.month.localeCompare(b.month))
    .slice(-12);  // last 12 months

  const monthlyObj = {};
  monthly.forEach(m => {
    const [year, mon] = m.month.split('-');
    const label = new Date(+year, +mon - 1).toLocaleDateString('pt-BR', {
      month: 'short', year: '2-digit',
    });
    monthlyObj[label] = m.count;
  });
  renderBar('monthly-chart', monthlyObj, null);
}

async function loadRecentLeads() {
  const tbody = document.getElementById('recent-leads-body');
  try {
    const result = await Leads.list({ limit: 5, sort_by: 'created_at', sort_dir: 'desc' });
    const leads = result.data || [];

    if (leads.length === 0) {
      tbody.innerHTML = `
        <tr><td colspan="6">
          <div class="empty-state" style="padding:30px">
            <div class="empty-icon">👥</div>
            <p>Nenhum lead cadastrado ainda.</p>
            <a href="/leads" class="btn btn-primary btn-sm">Cadastrar primeiro lead</a>
          </div>
        </td></tr>`;
      return;
    }

    tbody.innerHTML = leads.map(lead => `
      <tr>
        <td><a href="/leads/${lead.id}" style="font-weight:600">${esc(lead.full_name)}</a></td>
        <td class="text-muted">${esc(lead.company || '—')}</td>
        <td>${statusBadge(lead.status)}</td>
        <td>${sourceBadge(lead.source)}</td>
        <td class="text-muted text-sm">${formatDate(lead.capture_date)}</td>
        <td><a href="/leads/${lead.id}" class="btn btn-outline btn-sm">Ver</a></td>
      </tr>`).join('');
  } catch (err) {
    tbody.innerHTML = `<tr><td colspan="6" style="color:var(--danger);padding:20px">
      Erro ao carregar leads: ${err.message}</td></tr>`;
  }
}

function esc(str) {
  return String(str || '').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}
