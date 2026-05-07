let currentPage = 1;
let currentFilters = {};
let currentOrder = { by: 'created_at', dir: 'desc' };
let totalPages = 1;

document.addEventListener('DOMContentLoaded', () => {
  requireAuth();
  const user = getUser();
  if (user) {
    document.getElementById('user-name').textContent = user.name;
    document.getElementById('user-email').textContent = user.email;
  }
  document.getElementById('logout-btn').addEventListener('click', logout);

  loadLeads();

  // Filters
  document.getElementById('btn-search').addEventListener('click', applyFilters);
  document.getElementById('search-q').addEventListener('keydown', e => { if (e.key === 'Enter') applyFilters(); });
  document.getElementById('filter-status').addEventListener('change', applyFilters);
  document.getElementById('filter-fonte').addEventListener('change', applyFilters);
  document.getElementById('filter-data-inicio').addEventListener('change', applyFilters);
  document.getElementById('filter-data-fim').addEventListener('change', applyFilters);
  document.getElementById('btn-clear-filters').addEventListener('click', clearFilters);

  // Export
  document.getElementById('btn-export-csv').addEventListener('click', () =>
    apiDownload(`/api/leads/export?fmt=csv`, 'leads.csv').catch(e => alert(e.message)));
  document.getElementById('btn-export-excel').addEventListener('click', () =>
    apiDownload(`/api/leads/export?fmt=excel`, 'leads.xlsx').catch(e => alert(e.message)));

  // Import
  document.getElementById('btn-import').addEventListener('click', () =>
    document.getElementById('import-input').click());
  document.getElementById('import-input').addEventListener('change', handleImport);

  // New lead modal
  document.getElementById('btn-new-lead').addEventListener('click', openNewLeadModal);
  document.getElementById('modal-close').addEventListener('click', closeModal);
  document.getElementById('lead-form').addEventListener('submit', handleLeadSubmit);
  document.getElementById('modal-overlay').addEventListener('click', e => {
    if (e.target === document.getElementById('modal-overlay')) closeModal();
  });
});

function applyFilters() {
  currentFilters = {
    q: document.getElementById('search-q').value.trim() || undefined,
    status: document.getElementById('filter-status').value || undefined,
    fonte: document.getElementById('filter-fonte').value || undefined,
    data_inicio: document.getElementById('filter-data-inicio').value || undefined,
    data_fim: document.getElementById('filter-data-fim').value || undefined,
  };
  currentPage = 1;
  loadLeads();
}

function clearFilters() {
  document.getElementById('search-q').value = '';
  document.getElementById('filter-status').value = '';
  document.getElementById('filter-fonte').value = '';
  document.getElementById('filter-data-inicio').value = '';
  document.getElementById('filter-data-fim').value = '';
  currentFilters = {};
  currentPage = 1;
  loadLeads();
}

async function loadLeads() {
  const params = new URLSearchParams({
    page: currentPage, page_size: 20,
    order_by: currentOrder.by, order_dir: currentOrder.dir,
    ...Object.fromEntries(Object.entries(currentFilters).filter(([,v]) => v !== undefined)),
  });
  try {
    const data = await apiFetch(`/api/leads?${params}`);
    totalPages = data.total_pages;
    renderTable(data.items);
    renderPagination(data.total, data.page, data.total_pages);
  } catch (err) {
    document.getElementById('leads-tbody').innerHTML =
      `<tr><td colspan="7"><div class="alert alert-danger">${err.message}</div></td></tr>`;
  }
}

function renderTable(leads) {
  const tbody = document.getElementById('leads-tbody');
  if (!leads.length) {
    tbody.innerHTML = `<tr><td colspan="7"><div class="empty-state"><p>Nenhum lead encontrado</p></div></td></tr>`;
    return;
  }
  tbody.innerHTML = leads.map(l => `
    <tr>
      <td>
        <a href="/lead-detail?id=${l.id}" class="fw-bold" style="color:#4f8ef7">${l.nome_completo}</a>
        <div class="text-muted mt-1" style="font-size:11px">${l.email}</div>
      </td>
      <td>${l.empresa || '—'}</td>
      <td>${l.cargo || '—'}</td>
      <td>${fonteBadge(l.fonte)}</td>
      <td>${statusBadge(l.status)}</td>
      <td class="text-muted">${fmtDate(l.data_captura)}</td>
      <td>
        <div style="display:flex;gap:6px">
          <a href="/lead-detail?id=${l.id}" class="btn btn-sm btn-secondary" title="Ver">👁</a>
          <button class="btn btn-sm btn-danger" onclick="deleteLead('${l.id}','${l.nome_completo.replace(/'/g,"\\'")}')">🗑</button>
        </div>
      </td>
    </tr>
  `).join('');
}

function renderPagination(total, page, pages) {
  document.getElementById('page-info').textContent = `${total} lead(s) — Página ${page} de ${pages}`;
  const wrap = document.getElementById('pagination');
  wrap.innerHTML = '';

  const mkBtn = (label, pg, disabled = false) => {
    const btn = document.createElement('button');
    btn.className = 'page-btn' + (pg === page ? ' active' : '');
    btn.textContent = label; btn.disabled = disabled;
    if (!disabled) btn.addEventListener('click', () => { currentPage = pg; loadLeads(); });
    return btn;
  };

  wrap.appendChild(mkBtn('‹', page - 1, page <= 1));
  const start = Math.max(1, page - 2);
  const end = Math.min(pages, page + 2);
  for (let p = start; p <= end; p++) wrap.appendChild(mkBtn(p, p));
  wrap.appendChild(mkBtn('›', page + 1, page >= pages));
}

async function deleteLead(id, name) {
  if (!confirm(`Excluir o lead "${name}"?`)) return;
  try {
    await apiFetch(`/api/leads/${id}`, { method: 'DELETE' });
    loadLeads();
  } catch (err) { alert(err.message); }
}

function openNewLeadModal() {
  document.getElementById('modal-title').textContent = 'Novo Lead';
  document.getElementById('lead-form').reset();
  document.getElementById('lead-form').dataset.editId = '';
  document.getElementById('modal-overlay').classList.remove('hidden');
}

function closeModal() {
  document.getElementById('modal-overlay').classList.add('hidden');
}

async function handleLeadSubmit(e) {
  e.preventDefault();
  const btn = e.target.querySelector('button[type=submit]');
  btn.disabled = true; btn.textContent = 'Salvando…';
  const alertEl = document.getElementById('modal-alert');

  const body = {
    nome_completo: document.getElementById('f-nome').value,
    email: document.getElementById('f-email').value,
    telefone: document.getElementById('f-telefone').value,
    empresa: document.getElementById('f-empresa').value,
    cargo: document.getElementById('f-cargo').value,
    fonte: document.getElementById('f-fonte').value,
    status: document.getElementById('f-status').value,
    data_captura: document.getElementById('f-data').value || undefined,
    notas: document.getElementById('f-notas').value,
  };

  try {
    await apiFetch('/api/leads', { method: 'POST', body: JSON.stringify(body) });
    closeModal();
    loadLeads();
  } catch (err) {
    showAlert(alertEl, err.message);
    btn.disabled = false; btn.textContent = 'Salvar Lead';
  }
}

async function handleImport(e) {
  const file = e.target.files[0];
  if (!file) return;
  const fd = new FormData();
  fd.append('file', file);
  try {
    const res = await apiFetch('/api/leads/import', { method: 'POST', body: fd, headers: {} });
    alert(`Importados: ${res.imported} lead(s)${res.errors.length ? '\nErros:\n' + res.errors.join('\n') : ''}`);
    loadLeads();
  } catch (err) { alert(err.message); }
  e.target.value = '';
}
