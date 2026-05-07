document.addEventListener('DOMContentLoaded', async () => {
  requireAuth();
  const user = getUser();
  if (user) {
    document.getElementById('user-name').textContent = user.name;
    document.getElementById('user-email').textContent = user.email;
  }
  document.getElementById('logout-btn').addEventListener('click', logout);

  try {
    const stats = await apiFetch('/api/leads/stats');
    renderStats(stats);
    renderCharts(stats);
    renderRecent(stats.recent || []);
  } catch (err) {
    console.error(err);
  }
});

function renderStats(stats) {
  const s = stats.by_status || {};
  document.getElementById('stat-total').textContent = stats.total || 0;
  document.getElementById('stat-novo').textContent = s.novo || 0;
  document.getElementById('stat-qualificado').textContent = s.qualificado || 0;
  document.getElementById('stat-perdido').textContent = s.perdido || 0;
}

function renderCharts(stats) {
  const statusColors = {
    novo: '#4f8ef7', em_contato: '#f59e0b', qualificado: '#10b981', perdido: '#ef4444',
  };
  const fonteColors = ['#4f8ef7','#10b981','#f59e0b','#8b5cf6','#ef4444','#6b7280'];

  const byStatus = stats.by_status || {};
  const byFonte = stats.by_fonte || {};
  const byDay = stats.by_day || {};

  // Status doughnut
  new Chart(document.getElementById('chart-status'), {
    type: 'doughnut',
    data: {
      labels: Object.keys(byStatus).map(k => statusLabels[k] || k),
      datasets: [{ data: Object.values(byStatus), backgroundColor: Object.keys(byStatus).map(k => statusColors[k] || '#6b7280'), borderWidth: 0 }],
    },
    options: { plugins: { legend: { position: 'bottom' } }, cutout: '65%' },
  });

  // Fonte bar
  new Chart(document.getElementById('chart-fonte'), {
    type: 'bar',
    data: {
      labels: Object.keys(byFonte).map(k => fonteLabels[k] || k),
      datasets: [{ label: 'Leads', data: Object.values(byFonte), backgroundColor: fonteColors, borderRadius: 6 }],
    },
    options: { plugins: { legend: { display: false } }, scales: { y: { beginAtZero: true, ticks: { precision: 0 } } } },
  });

  // Capturas por dia (line chart)
  const days = Object.keys(byDay).slice(-30);
  const counts = days.map(d => byDay[d]);
  new Chart(document.getElementById('chart-timeline'), {
    type: 'line',
    data: {
      labels: days.map(d => fmtDate(d)),
      datasets: [{
        label: 'Capturas', data: counts, borderColor: '#4f8ef7',
        backgroundColor: 'rgba(79,142,247,.1)', fill: true, tension: 0.4, pointRadius: 3,
      }],
    },
    options: { plugins: { legend: { display: false } }, scales: { y: { beginAtZero: true, ticks: { precision: 0 } } } },
  });
}

function renderRecent(leads) {
  const tbody = document.getElementById('recent-tbody');
  if (!leads.length) {
    tbody.innerHTML = '<tr><td colspan="5" class="empty-state"><p>Nenhum lead ainda</p></td></tr>';
    return;
  }
  tbody.innerHTML = leads.map(l => `
    <tr style="cursor:pointer" onclick="location.href='/lead-detail?id=${l.id}'">
      <td class="fw-bold">${l.nome_completo}</td>
      <td>${l.empresa || '—'}</td>
      <td>${fonteBadge(l.fonte)}</td>
      <td>${statusBadge(l.status)}</td>
      <td class="text-muted">${fmtDate(l.data_captura)}</td>
    </tr>
  `).join('');
}
