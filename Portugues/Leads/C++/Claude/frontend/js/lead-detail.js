// lead-detail.js — Lead detail page logic

let currentLead = null;

document.addEventListener('DOMContentLoaded', async () => {
  if (!requireAuth()) return;
  populateSidebarUser();

  // Extract lead ID from URL path: /leads/{id}
  const parts = window.location.pathname.split('/');
  const leadId = parts[parts.length - 1];

  if (!leadId || leadId === 'leads') {
    window.location.href = '/leads';
    return;
  }

  await loadLead(leadId);
});

async function loadLead(id) {
  try {
    currentLead = await Leads.get(id);
    renderLead(currentLead);
    document.getElementById('loading').classList.add('hidden');
    document.getElementById('detail-content').classList.remove('hidden');
  } catch (err) {
    document.getElementById('loading').innerHTML =
      `<div class="empty-state">
        <div class="empty-icon">❌</div>
        <h3>Lead não encontrado</h3>
        <p>${err.message}</p>
        <a href="/leads" class="btn btn-primary btn-sm">← Voltar para Leads</a>
      </div>`;
  }
}

function renderLead(lead) {
  document.title = `${lead.full_name} — Lead Manager`;

  document.getElementById('topbar-name').textContent = lead.full_name;
  document.getElementById('detail-name').textContent = lead.full_name;
  document.getElementById('detail-avatar').textContent =
    (lead.full_name || 'L')[0].toUpperCase();

  // Position / company
  const parts = [lead.position, lead.company].filter(Boolean);
  document.getElementById('detail-position-company').textContent =
    parts.length > 0 ? parts.join(' · ') : '—';

  document.getElementById('detail-status-badge').innerHTML = statusBadge(lead.status);
  document.getElementById('detail-source-badge').innerHTML = sourceBadge(lead.source);

  document.getElementById('detail-email').innerHTML =
    lead.email ? `<a href="mailto:${esc(lead.email)}">${esc(lead.email)}</a>` : '—';
  document.getElementById('detail-phone').innerHTML =
    lead.phone ? `<a href="tel:${esc(lead.phone)}">${esc(lead.phone)}</a>` : '—';
  document.getElementById('detail-capture-date').textContent =
    formatDate(lead.capture_date) || '—';

  document.getElementById('detail-notes').textContent =
    lead.notes || 'Nenhuma observação registrada.';

  document.getElementById('detail-id').textContent      = lead.id || '—';
  document.getElementById('detail-created').textContent = formatDateTime(lead.created_at);
  document.getElementById('detail-updated').textContent = formatDateTime(lead.updated_at);

  renderTimeline(lead.interactions || []);
}

function renderTimeline(interactions) {
  const container = document.getElementById('timeline-container');

  if (!interactions || interactions.length === 0) {
    container.innerHTML = `
      <div class="empty-state" style="padding:24px 10px">
        <div class="empty-icon">💬</div>
        <p style="font-size:.8rem">Nenhuma interação registrada ainda.</p>
      </div>`;
    return;
  }

  // Sort newest first
  const sorted = [...interactions].sort((a, b) =>
    new Date(b.date) - new Date(a.date));

  const typeIcons = {
    call:     '📞',
    email:    '📧',
    meeting:  '🤝',
    whatsapp: '💬',
    note:     '📝',
  };
  const typeLabels = {
    call:     'Ligação',
    email:    'E-mail',
    meeting:  'Reunião',
    whatsapp: 'WhatsApp',
    note:     'Anotação',
  };

  container.innerHTML = `<div class="timeline">` +
    sorted.map(i => `
      <div class="timeline-item">
        <div class="timeline-dot"></div>
        <div class="timeline-content" style="flex:1">
          <div style="display:flex;justify-content:space-between;align-items:center">
            <span style="font-size:.8rem;font-weight:700;color:var(--text-secondary)">
              ${typeIcons[i.type] || '•'} ${typeLabels[i.type] || i.type}
            </span>
            <span class="time">${formatDateTime(i.date)}</span>
          </div>
          <div class="desc">${esc(i.description)}</div>
        </div>
      </div>`).join('') +
    `</div>`;
}

// ---- Add interaction ---------------------------------------------------
async function addInteraction() {
  const type = document.getElementById('interaction-type').value;
  const desc = document.getElementById('interaction-desc').value.trim();

  if (!desc) {
    showError('Informe a descrição da interação');
    document.getElementById('interaction-desc').focus();
    return;
  }

  try {
    const updated = await Leads.addInteraction(currentLead.id, {
      type,
      description: desc,
    });
    currentLead = updated;
    renderTimeline(updated.interactions || []);
    document.getElementById('interaction-desc').value = '';
    showSuccess('Interação registrada!');
  } catch (err) {
    showError('Erro ao registrar: ' + err.message);
  }
}

// ---- Edit modal --------------------------------------------------------
function openEditModal() {
  if (!currentLead) return;
  const lead = currentLead;

  document.getElementById('e-full-name').value    = lead.full_name    || '';
  document.getElementById('e-email').value        = lead.email        || '';
  document.getElementById('e-phone').value        = lead.phone        || '';
  document.getElementById('e-company').value      = lead.company      || '';
  document.getElementById('e-position').value     = lead.position     || '';
  document.getElementById('e-source').value       = lead.source       || 'site';
  document.getElementById('e-status').value       = lead.status       || 'new';
  document.getElementById('e-capture-date').value = lead.capture_date || '';
  document.getElementById('e-notes').value        = lead.notes        || '';

  document.getElementById('edit-alert').classList.add('hidden');
  document.getElementById('edit-modal-overlay').classList.remove('hidden');
  document.getElementById('e-full-name').focus();
}

function closeEditModal() {
  document.getElementById('edit-modal-overlay').classList.add('hidden');
}

function closeEditOnBackdrop(e) {
  if (e.target.id === 'edit-modal-overlay') closeEditModal();
}

async function saveEdit(e) {
  e.preventDefault();
  const data = {
    full_name:    document.getElementById('e-full-name').value,
    email:        document.getElementById('e-email').value,
    phone:        document.getElementById('e-phone').value,
    company:      document.getElementById('e-company').value,
    position:     document.getElementById('e-position').value,
    source:       document.getElementById('e-source').value,
    status:       document.getElementById('e-status').value,
    capture_date: document.getElementById('e-capture-date').value,
    notes:        document.getElementById('e-notes').value,
  };

  try {
    currentLead = await Leads.update(currentLead.id, data);
    renderLead(currentLead);
    closeEditModal();
    showSuccess('Lead atualizado com sucesso!');
  } catch (err) {
    const alertEl = document.getElementById('edit-alert');
    alertEl.textContent = err.message;
    alertEl.classList.remove('hidden');
  }
}

// ---- Delete ------------------------------------------------------------
function confirmDelete() {
  document.getElementById('delete-modal-overlay').classList.remove('hidden');
}

async function deleteLead() {
  try {
    await Leads.delete(currentLead.id);
    showSuccess('Lead excluído!');
    setTimeout(() => { window.location.href = '/leads'; }, 1000);
  } catch (err) {
    showError('Erro ao excluir: ' + err.message);
    document.getElementById('delete-modal-overlay').classList.add('hidden');
  }
}

function esc(str) {
  return String(str || '')
    .replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}
