const leadId = new URLSearchParams(location.search).get('id');
let leadData = null;

document.addEventListener('DOMContentLoaded', async () => {
  requireAuth();
  if (!leadId) { location.href = '/leads'; return; }

  const user = getUser();
  if (user) {
    document.getElementById('user-name').textContent = user.name;
    document.getElementById('user-email').textContent = user.email;
  }
  document.getElementById('logout-btn').addEventListener('click', logout);

  await Promise.all([loadLead(), loadInteractions()]);

  document.getElementById('btn-edit').addEventListener('click', enterEditMode);
  document.getElementById('btn-cancel').addEventListener('click', exitEditMode);
  document.getElementById('edit-form').addEventListener('submit', handleUpdate);
  document.getElementById('btn-delete').addEventListener('click', handleDelete);
  document.getElementById('interaction-form').addEventListener('submit', handleAddInteraction);
});

async function loadLead() {
  try {
    leadData = await apiFetch(`/api/leads/${leadId}`);
    renderView(leadData);
    fillEditForm(leadData);
  } catch (err) {
    document.getElementById('lead-view').innerHTML =
      `<div class="alert alert-danger">${err.message}</div>`;
  }
}

function renderView(l) {
  document.getElementById('lead-fullname').textContent = l.nome_completo;
  document.getElementById('lead-badges').innerHTML = `${statusBadge(l.status)} ${fonteBadge(l.fonte)}`;
  document.getElementById('info-email').textContent = l.email || '—';
  document.getElementById('info-telefone').textContent = l.telefone || '—';
  document.getElementById('info-empresa').textContent = l.empresa || '—';
  document.getElementById('info-cargo').textContent = l.cargo || '—';
  document.getElementById('info-data').textContent = fmtDate(l.data_captura);
  document.getElementById('info-criado').textContent = fmtDateTime(l.created_at);
  document.getElementById('info-atualizado').textContent = fmtDateTime(l.updated_at);
  document.getElementById('info-notas').textContent = l.notas || '—';
}

function fillEditForm(l) {
  document.getElementById('e-nome').value = l.nome_completo || '';
  document.getElementById('e-email').value = l.email || '';
  document.getElementById('e-telefone').value = l.telefone || '';
  document.getElementById('e-empresa').value = l.empresa || '';
  document.getElementById('e-cargo').value = l.cargo || '';
  document.getElementById('e-fonte').value = l.fonte || 'outros';
  document.getElementById('e-status').value = l.status || 'novo';
  document.getElementById('e-data').value = l.data_captura || '';
  document.getElementById('e-notas').value = l.notas || '';
}

function enterEditMode() {
  document.getElementById('lead-view').classList.add('hidden');
  document.getElementById('lead-edit').classList.remove('hidden');
  document.getElementById('btn-edit').classList.add('hidden');
}

function exitEditMode() {
  document.getElementById('lead-edit').classList.add('hidden');
  document.getElementById('lead-view').classList.remove('hidden');
  document.getElementById('btn-edit').classList.remove('hidden');
}

async function handleUpdate(e) {
  e.preventDefault();
  const btn = e.target.querySelector('button[type=submit]');
  btn.disabled = true; btn.textContent = 'Salvando…';
  const alertEl = document.getElementById('edit-alert');

  const body = {
    nome_completo: document.getElementById('e-nome').value,
    email: document.getElementById('e-email').value,
    telefone: document.getElementById('e-telefone').value || null,
    empresa: document.getElementById('e-empresa').value || null,
    cargo: document.getElementById('e-cargo').value || null,
    fonte: document.getElementById('e-fonte').value,
    status: document.getElementById('e-status').value,
    data_captura: document.getElementById('e-data').value || null,
    notas: document.getElementById('e-notas').value || null,
  };

  try {
    leadData = await apiFetch(`/api/leads/${leadId}`, { method: 'PUT', body: JSON.stringify(body) });
    renderView(leadData);
    exitEditMode();
    showAlert(alertEl, 'Lead atualizado com sucesso!', 'success');
    setTimeout(() => alertEl.classList.add('hidden'), 3000);
  } catch (err) {
    showAlert(alertEl, err.message);
    btn.disabled = false; btn.textContent = 'Salvar Alterações';
  }
}

async function handleDelete() {
  if (!confirm('Excluir este lead permanentemente?')) return;
  try {
    await apiFetch(`/api/leads/${leadId}`, { method: 'DELETE' });
    location.href = '/leads';
  } catch (err) { alert(err.message); }
}

async function loadInteractions() {
  try {
    const interactions = await apiFetch(`/api/leads/${leadId}/interactions`);
    renderInteractions(interactions);
  } catch {}
}

function renderInteractions(list) {
  const container = document.getElementById('interactions-list');
  if (!list.length) {
    container.innerHTML = '<div class="empty-state"><p>Nenhuma interação registrada</p></div>';
    return;
  }
  const icons = { ligacao: '📞', email: '📧', reuniao: '🤝', mensagem: '💬', outro: '📝' };
  container.innerHTML = list.map(i => `
    <div class="interaction-item">
      <div class="interaction-dot">${icons[i.tipo] || '📝'}</div>
      <div style="flex:1">
        <div style="display:flex;justify-content:space-between;align-items:center">
          <span class="fw-bold" style="font-size:13px">${tipoLabels[i.tipo] || i.tipo}</span>
          <div style="display:flex;gap:8px;align-items:center">
            <span class="text-muted" style="font-size:11px">${fmtDate(i.data)}</span>
            <button class="btn btn-sm btn-danger btn-icon" onclick="deleteInteraction('${i.id}')">🗑</button>
          </div>
        </div>
        <div style="font-size:13px;margin-top:4px;color:#374151">${i.descricao}</div>
      </div>
    </div>
  `).join('');
}

async function handleAddInteraction(e) {
  e.preventDefault();
  const btn = e.target.querySelector('button[type=submit]');
  btn.disabled = true; btn.textContent = 'Salvando…';

  const body = {
    tipo: document.getElementById('i-tipo').value,
    descricao: document.getElementById('i-descricao').value,
    data: document.getElementById('i-data').value || undefined,
  };

  try {
    await apiFetch(`/api/leads/${leadId}/interactions`, { method: 'POST', body: JSON.stringify(body) });
    e.target.reset();
    await loadInteractions();
  } catch (err) { alert(err.message); }
  btn.disabled = false; btn.textContent = 'Registrar';
}

async function deleteInteraction(intId) {
  if (!confirm('Excluir esta interação?')) return;
  try {
    await apiFetch(`/api/leads/${leadId}/interactions/${intId}`, { method: 'DELETE' });
    await loadInteractions();
  } catch (err) { alert(err.message); }
}
