// leads.js — Leads list page logic

let currentPage    = 1;
let currentSortBy  = 'created_at';
let currentSortDir = 'desc';
let searchTimer    = null;
let deleteTargetId = null;

document.addEventListener('DOMContentLoaded', () => {
  if (!requireAuth()) return;
  populateSidebarUser();
  // Default capture date to today
  document.getElementById('f-capture-date').value = new Date().toISOString().slice(0, 10);
  loadLeads();
});

// ---- Data loading ------------------------------------------------------
async function loadLeads() {
  const tbody = document.getElementById('leads-body');
  tbody.innerHTML = `<tr><td colspan="8">
    <div class="loading"><div class="spinner"></div><p>Carregando...</p></div>
  </td></tr>`;

  const params = {
    page:      currentPage,
    limit:     20,
    sort_by:   currentSortBy,
    sort_dir:  currentSortDir,
    status:    document.getElementById('filter-status').value,
    source:    document.getElementById('filter-source').value,
    search:    document.getElementById('filter-search').value,
    date_from: document.getElementById('filter-date-from').value,
    date_to:   document.getElementById('filter-date-to').value,
  };

  try {
    const result = await Leads.list(params);
    renderLeads(result);
  } catch (err) {
    tbody.innerHTML = `<tr><td colspan="8" style="color:var(--danger);padding:24px;text-align:center">
      Erro: ${err.message}</td></tr>`;
  }
}

function renderLeads(result) {
  const tbody = document.getElementById('leads-body');
  const leads = result.data || [];

  if (leads.length === 0) {
    tbody.innerHTML = `
      <tr><td colspan="8">
        <div class="empty-state" style="padding:50px">
          <div class="empty-icon">👥</div>
          <h3>Nenhum lead encontrado</h3>
          <p>Tente ajustar os filtros ou cadastre um novo lead.</p>
          <button class="btn btn-primary btn-sm" onclick="openLeadModal()">+ Novo Lead</button>
        </div>
      </td></tr>`;
  } else {
    tbody.innerHTML = leads.map(lead => `
      <tr>
        <td>
          <a href="/leads/${esc(lead.id)}" style="font-weight:600;color:var(--text-primary)">
            ${esc(lead.full_name)}
          </a>
        </td>
        <td class="text-muted text-sm">${esc(lead.email || '—')}</td>
        <td class="text-muted text-sm">${esc(lead.phone || '—')}</td>
        <td>${esc(lead.company || '—')}</td>
        <td>${statusBadge(lead.status)}</td>
        <td>${sourceBadge(lead.source)}</td>
        <td class="text-muted text-sm">${formatDate(lead.capture_date)}</td>
        <td>
          <div style="display:flex;gap:6px">
            <a href="/leads/${esc(lead.id)}" class="btn-icon" title="Ver detalhes">👁</a>
            <button class="btn-icon" title="Editar" onclick="editLead('${esc(lead.id)}')">✏</button>
            <button class="btn-icon" title="Excluir"
                    style="color:var(--danger)"
                    onclick="openDeleteModal('${esc(lead.id)}','${esc(lead.full_name)}')">🗑</button>
          </div>
        </td>
      </tr>`).join('');
  }

  // Pagination
  const total      = result.total      || 0;
  const totalPages = result.totalPages || 1;
  const page       = result.page       || 1;
  const from       = total === 0 ? 0 : (page - 1) * 20 + 1;
  const to         = Math.min(page * 20, total);

  document.getElementById('pagination-info').textContent =
    total === 0 ? 'Nenhum resultado' : `Exibindo ${from}–${to} de ${total} leads`;

  renderPagination(page, totalPages);
  updateSortIndicators();
}

function renderPagination(page, totalPages) {
  const container = document.getElementById('pagination-controls');
  if (totalPages <= 1) { container.innerHTML = ''; return; }

  let html = `<button class="page-btn" onclick="goPage(${page-1})" ${page===1?'disabled':''}>‹</button>`;

  // Show at most 7 page buttons
  let start = Math.max(1, page - 3);
  let end   = Math.min(totalPages, start + 6);
  start     = Math.max(1, end - 6);

  if (start > 1) html += `<button class="page-btn" onclick="goPage(1)">1</button>
    ${start > 2 ? '<span style="padding:0 4px">…</span>' : ''}`;

  for (let i = start; i <= end; i++)
    html += `<button class="page-btn ${i===page?'active':''}" onclick="goPage(${i})">${i}</button>`;

  if (end < totalPages)
    html += `${end < totalPages-1 ? '<span style="padding:0 4px">…</span>' : ''}
    <button class="page-btn" onclick="goPage(${totalPages})">${totalPages}</button>`;

  html += `<button class="page-btn" onclick="goPage(${page+1})" ${page===totalPages?'disabled':''}>›</button>`;
  container.innerHTML = html;
}

function goPage(p) {
  currentPage = p;
  loadLeads();
  window.scrollTo(0, 0);
}

// ---- Sorting -----------------------------------------------------------
function sortBy(field) {
  if (currentSortBy === field)
    currentSortDir = currentSortDir === 'asc' ? 'desc' : 'asc';
  else {
    currentSortBy  = field;
    currentSortDir = 'asc';
  }
  currentPage = 1;
  loadLeads();
}

function updateSortIndicators() {
  ['full_name','company','capture_date'].forEach(f => {
    const el = document.getElementById('sort-' + f);
    if (!el) return;
    el.textContent = currentSortBy === f
      ? (currentSortDir === 'asc' ? ' ↑' : ' ↓') : '';
  });
}

// ---- Filters -----------------------------------------------------------
function debounceSearch() {
  clearTimeout(searchTimer);
  searchTimer = setTimeout(() => { currentPage = 1; loadLeads(); }, 400);
}

function clearFilters() {
  document.getElementById('filter-search').value    = '';
  document.getElementById('filter-status').value    = '';
  document.getElementById('filter-source').value    = '';
  document.getElementById('filter-date-from').value = '';
  document.getElementById('filter-date-to').value   = '';
  currentPage = 1;
  loadLeads();
}

// ---- Create / Edit modal -----------------------------------------------
function openLeadModal(lead = null) {
  document.getElementById('modal-title').textContent =
    lead ? 'Editar Lead' : 'Novo Lead';
  document.getElementById('modal-alert').classList.add('hidden');

  document.getElementById('lead-id').value         = lead?.id         || '';
  document.getElementById('f-full-name').value     = lead?.full_name  || '';
  document.getElementById('f-email').value         = lead?.email      || '';
  document.getElementById('f-phone').value         = lead?.phone      || '';
  document.getElementById('f-company').value       = lead?.company    || '';
  document.getElementById('f-position').value      = lead?.position   || '';
  document.getElementById('f-source').value        = lead?.source     || 'site';
  document.getElementById('f-status').value        = lead?.status     || 'new';
  document.getElementById('f-notes').value         = lead?.notes      || '';
  document.getElementById('f-capture-date').value  =
    lead?.capture_date || new Date().toISOString().slice(0, 10);

  document.getElementById('lead-modal-overlay').classList.remove('hidden');
  document.getElementById('f-full-name').focus();
}

function closeLeadModal() {
  document.getElementById('lead-modal-overlay').classList.add('hidden');
}

function closeModalOnBackdrop(e) {
  if (e.target.id === 'lead-modal-overlay') closeLeadModal();
}

async function editLead(id) {
  try {
    const lead = await Leads.get(id);
    openLeadModal(lead);
  } catch (err) {
    showError('Erro ao carregar lead: ' + err.message);
  }
}

async function saveLead(e) {
  e.preventDefault();
  const saveBtn = document.getElementById('save-btn');
  saveBtn.disabled = true;
  saveBtn.textContent = 'Salvando...';

  const id = document.getElementById('lead-id').value;
  const data = {
    full_name:    document.getElementById('f-full-name').value,
    email:        document.getElementById('f-email').value,
    phone:        document.getElementById('f-phone').value,
    company:      document.getElementById('f-company').value,
    position:     document.getElementById('f-position').value,
    source:       document.getElementById('f-source').value,
    status:       document.getElementById('f-status').value,
    capture_date: document.getElementById('f-capture-date').value,
    notes:        document.getElementById('f-notes').value,
  };

  try {
    if (id) {
      await Leads.update(id, data);
      showSuccess('Lead atualizado com sucesso!');
    } else {
      await Leads.create(data);
      showSuccess('Lead criado com sucesso!');
    }
    closeLeadModal();
    currentPage = 1;
    loadLeads();
  } catch (err) {
    const alertEl = document.getElementById('modal-alert');
    alertEl.textContent = err.message;
    alertEl.classList.remove('hidden');
  } finally {
    saveBtn.disabled = false;
    saveBtn.textContent = 'Salvar';
  }
}

// ---- Delete ------------------------------------------------------------
function openDeleteModal(id, name) {
  deleteTargetId = id;
  document.getElementById('delete-lead-name').textContent = name;
  document.getElementById('delete-modal-overlay').classList.remove('hidden');

  document.getElementById('confirm-delete-btn').onclick = async () => {
    try {
      await Leads.delete(deleteTargetId);
      closeDeleteModal();
      showSuccess('Lead excluído com sucesso!');
      loadLeads();
    } catch (err) {
      showError('Erro ao excluir: ' + err.message);
    }
  };
}

function closeDeleteModal() {
  document.getElementById('delete-modal-overlay').classList.add('hidden');
  deleteTargetId = null;
}

// ---- CSV Import / Export -----------------------------------------------
function importCsvClick() {
  document.getElementById('import-modal-overlay').classList.remove('hidden');
  document.getElementById('import-result').classList.add('hidden');
  document.getElementById('csv-file-input').value = '';
}

function closeImportModal() {
  document.getElementById('import-modal-overlay').classList.add('hidden');
}

async function doImport() {
  const fileInput = document.getElementById('csv-file-input');
  const file = fileInput.files[0];
  if (!file) { showError('Selecione um arquivo CSV'); return; }

  const btn = document.getElementById('import-btn');
  btn.disabled = true;
  btn.textContent = 'Importando...';

  try {
    const result = await Leads.importCsv(file);
    const resultEl = document.getElementById('import-result');
    resultEl.className = result.errors === 0 ? 'alert alert-success' : 'alert alert-info';
    resultEl.innerHTML = `
      <div>
        <strong>${result.imported} leads importados</strong>
        ${result.errors > 0 ? ` · ${result.errors} erros` : ''}
        ${result.errorList?.length > 0
          ? '<ul style="margin-top:8px;font-size:.8rem">' +
            result.errorList.map(e => `<li>Linha ${e.row}: ${e.error}</li>`).join('') +
            '</ul>'
          : ''}
      </div>`;
    resultEl.classList.remove('hidden');
    if (result.imported > 0) {
      loadLeads();
      setTimeout(closeImportModal, 3000);
    }
  } catch (err) {
    showError('Erro na importação: ' + err.message);
  } finally {
    btn.disabled = false;
    btn.textContent = 'Importar';
  }
}

async function exportCsv() {
  try {
    await Leads.exportCsv();
    showSuccess('CSV exportado com sucesso!');
  } catch (err) {
    showError('Erro ao exportar: ' + err.message);
  }
}

// Legacy handler for hidden file input (kept for compatibility)
async function handleCsvFile(e) {
  const file = e.target.files[0];
  if (!file) return;
  try {
    const result = await Leads.importCsv(file);
    showSuccess(`${result.imported} leads importados!`);
    loadLeads();
  } catch (err) {
    showError(err.message);
  }
}

function esc(str) {
  return String(str || '').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}
