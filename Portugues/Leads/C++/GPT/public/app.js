const state = {
  token: localStorage.getItem("lead_manager_token") || "",
  user: null,
  leads: [],
  pagination: { page: 1, pageSize: 20, total: 0, totalPages: 1 },
  selectedLeadId: null
};

const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => Array.from(document.querySelectorAll(selector));

const loginForm = $("#login-form");
const registerForm = $("#register-form");
const filtersForm = $("#filters-form");
const leadForm = $("#lead-form");
const interactionForm = $("#interaction-form");
const leadDialog = $("#lead-dialog");

function showMessage(message, timeout = 4200) {
  const box = $("#message-box");
  box.textContent = message;
  box.classList.remove("hidden");
  clearTimeout(showMessage._timer);
  showMessage._timer = setTimeout(() => box.classList.add("hidden"), timeout);
}

async function api(path, options = {}) {
  const headers = { ...(options.headers || {}) };
  if (state.token) {
    headers.Authorization = `Bearer ${state.token}`;
  }
  if (options.body && !headers["Content-Type"] && !(options.body instanceof FormData)) {
    headers["Content-Type"] = "application/json";
  }

  const response = await fetch(path, { ...options, headers });
  const contentType = response.headers.get("content-type") || "";
  const payload = contentType.includes("application/json") ? await response.json() : await response.text();

  if (!response.ok) {
    const error = typeof payload === "string" ? payload : payload.error || "Erro inesperado";
    throw new Error(error);
  }
  return payload;
}

function switchAuthTab(mode) {
  $$("[data-auth-tab]").forEach((button) => {
    button.classList.toggle("active", button.dataset.authTab === mode);
  });
  loginForm.classList.toggle("hidden", mode !== "login");
  registerForm.classList.toggle("hidden", mode !== "register");
}

function setAuthenticated(user, token) {
  state.user = user;
  state.token = token || state.token;
  if (state.token) {
    localStorage.setItem("lead_manager_token", state.token);
  }
  $("#auth-panel").classList.add("hidden");
  $("#user-panel").classList.remove("hidden");
  $("#user-greeting").textContent = `${user.name} · ${user.email}`;
}

function clearSession() {
  state.user = null;
  state.token = "";
  state.leads = [];
  state.selectedLeadId = null;
  localStorage.removeItem("lead_manager_token");
  $("#auth-panel").classList.remove("hidden");
  $("#user-panel").classList.add("hidden");
  renderLeads();
  renderDetail();
}

function getFilters(page = state.pagination.page) {
  const formData = new FormData(filtersForm);
  const query = new URLSearchParams();
  formData.forEach((value, key) => {
    if (value) {
      query.set(key, value);
    }
  });
  query.set("page", page);
  query.set("pageSize", "20");
  return query.toString();
}

async function loadMe() {
  if (!state.token) {
    clearSession();
    return;
  }
  try {
    const payload = await api("/api/auth/me");
    setAuthenticated(payload.user);
  } catch {
    clearSession();
  }
}

function renderDashboard(dashboard) {
  $("#stat-total").textContent = dashboard.totals.totalLeads;
  $("#stat-qualified").textContent = dashboard.totals.qualifiedLeads;
  $("#stat-conversion").textContent = `${dashboard.totals.conversionRate}%`;
  $("#stat-latest").textContent = dashboard.capturesByDate.length
    ? dashboard.capturesByDate[dashboard.capturesByDate.length - 1].total
    : "0";

  const maxValue = Math.max(...dashboard.capturesByDate.map((item) => item.total), 1);
  $("#capture-chart").innerHTML = dashboard.capturesByDate.length
    ? dashboard.capturesByDate.map((item) => `
      <div class="bar-row">
        <span>${item.date}</span>
        <div class="bar-track"><div class="bar-value" style="width:${(item.total / maxValue) * 100}%"></div></div>
        <strong>${item.total}</strong>
      </div>
    `).join("")
    : "<p>Nenhuma captura registrada.</p>";

  const statusMap = dashboard.statusCounts || {};
  $("#status-summary").innerHTML = Object.entries(statusMap).map(([label, total]) => `
    <div class="summary-row">
      <span>${label}</span>
      <strong>${total}</strong>
    </div>
  `).join("");
}

function renderLeads() {
  const tbody = $("#leads-table-body");
  if (!state.leads.length) {
    tbody.innerHTML = '<tr><td colspan="6">Nenhum lead encontrado para os filtros atuais.</td></tr>';
  } else {
    tbody.innerHTML = state.leads.map((lead) => `
      <tr>
        <td>
          <strong>${lead.fullName}</strong><br>
          <small>${lead.email}<br>${lead.phone || ""}</small>
        </td>
        <td>${lead.company || "-"}</td>
        <td>${lead.source || "-"}</td>
        <td><span class="badge">${lead.status}</span></td>
        <td>${lead.captureDate || "-"}</td>
        <td>
          <div class="row-actions">
            <button class="secondary" data-action="view" data-id="${lead.id}">Detalhes</button>
            <button class="secondary" data-action="edit" data-id="${lead.id}">Editar</button>
            <button class="secondary" data-action="delete" data-id="${lead.id}">Excluir</button>
          </div>
        </td>
      </tr>
    `).join("");
  }

  $("#table-meta").textContent = `${state.pagination.total} leads encontrados`;
  $("#pagination-label").textContent = `Pagina ${state.pagination.page} de ${Math.max(state.pagination.totalPages, 1)}`;
  $("#previous-page").disabled = state.pagination.page <= 1;
  $("#next-page").disabled = state.pagination.page >= state.pagination.totalPages;
}

function renderDetail(lead = null) {
  const target = $("#lead-detail-content");
  if (!lead) {
    target.innerHTML = "<p>Selecione um lead na tabela para visualizar o historico.</p>";
    interactionForm.classList.add("hidden");
    return;
  }

  const interactions = Array.isArray(lead.interactions) ? lead.interactions : [];
  target.innerHTML = `
    <div class="detail-card">
      <strong>${lead.fullName}</strong>
      <span>${lead.company || "Sem empresa"} · ${lead.jobTitle || "Sem cargo"}</span>
      <span>${lead.email} · ${lead.phone || "Sem telefone"}</span>
      <span>Fonte: ${lead.source || "-"} · Status: ${lead.status}</span>
      <span>Captura: ${lead.captureDate || "-"}</span>
      <span>Observacoes: ${lead.notes || "Nenhuma observacao"}</span>
    </div>
    <div class="detail-card">
      <strong>Historico de interacoes</strong>
      ${interactions.length ? interactions.map((item) => `
        <div class="detail-card">
          <strong>${item.type || "nota"}</strong>
          <span>${item.createdAt || ""}</span>
          <span>${item.note || ""}</span>
        </div>
      `).join("") : "<p>Sem interacoes registradas.</p>"}
    </div>
  `;
  interactionForm.classList.remove("hidden");
}

async function loadDashboard() {
  if (!state.user) return;
  const dashboard = await api("/api/dashboard/summary");
  renderDashboard(dashboard);
}

async function loadLeads(page = 1) {
  if (!state.user) return;
  const payload = await api(`/api/leads?${getFilters(page)}`);
  state.leads = payload.items;
  state.pagination = payload.pagination;
  renderLeads();

  if (state.selectedLeadId) {
    const selected = state.leads.find((lead) => lead.id === state.selectedLeadId);
    if (selected) {
      await openLeadDetail(selected.id);
    }
  }
}

async function refreshData(page = state.pagination.page || 1) {
  try {
    await Promise.all([loadDashboard(), loadLeads(page)]);
  } catch (error) {
    showMessage(error.message);
  }
}

function fillLeadForm(lead = null) {
  leadForm.reset();
  leadForm.id.value = lead?.id || "";
  leadForm.fullName.value = lead?.fullName || "";
  leadForm.email.value = lead?.email || "";
  leadForm.phone.value = lead?.phone || "";
  leadForm.company.value = lead?.company || "";
  leadForm.jobTitle.value = lead?.jobTitle || "";
  leadForm.source.value = lead?.source || "";
  leadForm.status.value = lead?.status || "novo";
  leadForm.captureDate.value = lead?.captureDate || new Date().toISOString().slice(0, 10);
  leadForm.notes.value = lead?.notes || "";
  $("#lead-dialog-title").textContent = lead ? "Editar lead" : "Novo lead";
}

async function openLeadDetail(id) {
  const payload = await api(`/api/leads/${id}`);
  state.selectedLeadId = id;
  renderDetail(payload.lead);
}

async function removeLead(id) {
  if (!confirm("Deseja excluir este lead?")) return;
  await api(`/api/leads/${id}`, { method: "DELETE" });
  if (state.selectedLeadId === id) {
    state.selectedLeadId = null;
    renderDetail();
  }
  showMessage("Lead excluido com sucesso.");
  await refreshData();
}

async function exportExcel() {
  const rows = [];
  let page = 1;
  while (true) {
    const payload = await api(`/api/leads?${getFilters(page).replace("pageSize=20", "pageSize=100")}`);
    rows.push(...payload.items);
    if (page >= payload.pagination.totalPages) break;
    page += 1;
  }

  const worksheet = XLSX.utils.json_to_sheet(rows.map((lead) => ({
    id: lead.id,
    nome_completo: lead.fullName,
    email: lead.email,
    telefone: lead.phone,
    empresa: lead.company,
    cargo: lead.jobTitle,
    fonte: lead.source,
    status: lead.status,
    data_captura: lead.captureDate,
    observacoes: lead.notes
  })));
  const workbook = XLSX.utils.book_new();
  XLSX.utils.book_append_sheet(workbook, worksheet, "Leads");
  XLSX.writeFile(workbook, "leads-export.xlsx");
}

document.addEventListener("click", async (event) => {
  const action = event.target.dataset.action;
  const id = event.target.dataset.id;
  if (!action || !id) return;

  try {
    if (action === "view") await openLeadDetail(id);
    if (action === "edit") {
      const lead = state.leads.find((item) => item.id === id);
      fillLeadForm(lead);
      leadDialog.showModal();
    }
    if (action === "delete") await removeLead(id);
  } catch (error) {
    showMessage(error.message);
  }
});

$$("[data-auth-tab]").forEach((button) => {
  button.addEventListener("click", () => switchAuthTab(button.dataset.authTab));
});

loginForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  try {
    const payload = await api("/api/auth/login", {
      method: "POST",
      body: JSON.stringify(Object.fromEntries(new FormData(loginForm)))
    });
    setAuthenticated(payload.user, payload.token);
    showMessage("Login realizado com sucesso.");
    await refreshData(1);
  } catch (error) {
    showMessage(error.message);
  }
});

registerForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  try {
    const payload = await api("/api/auth/register", {
      method: "POST",
      body: JSON.stringify(Object.fromEntries(new FormData(registerForm)))
    });
    setAuthenticated(payload.user, payload.token);
    showMessage("Conta criada com sucesso.");
    await refreshData(1);
  } catch (error) {
    showMessage(error.message);
  }
});

filtersForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  await refreshData(1);
});

$("#reset-filters-button").addEventListener("click", async () => {
  filtersForm.reset();
  await refreshData(1);
});

$("#refresh-button").addEventListener("click", async () => refreshData());
$("#new-lead-button").addEventListener("click", () => {
  fillLeadForm();
  leadDialog.showModal();
});
$("#close-dialog-button").addEventListener("click", () => leadDialog.close());
$("#logout-button").addEventListener("click", () => {
  clearSession();
  showMessage("Sessao encerrada.");
});
$("#close-detail-button").addEventListener("click", () => {
  state.selectedLeadId = null;
  renderDetail();
});

$("#previous-page").addEventListener("click", async () => {
  if (state.pagination.page > 1) {
    await refreshData(state.pagination.page - 1);
  }
});

$("#next-page").addEventListener("click", async () => {
  if (state.pagination.page < state.pagination.totalPages) {
    await refreshData(state.pagination.page + 1);
  }
});

leadForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const data = Object.fromEntries(new FormData(leadForm));
  try {
    if (data.id) {
      await api(`/api/leads/${data.id}`, { method: "PUT", body: JSON.stringify(data) });
      showMessage("Lead atualizado com sucesso.");
    } else {
      await api("/api/leads", { method: "POST", body: JSON.stringify(data) });
      showMessage("Lead criado com sucesso.");
    }
    leadDialog.close();
    await refreshData();
  } catch (error) {
    showMessage(error.message);
  }
});

interactionForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  if (!state.selectedLeadId) return;
  try {
    const data = Object.fromEntries(new FormData(interactionForm));
    await api(`/api/leads/${state.selectedLeadId}/interactions`, {
      method: "POST",
      body: JSON.stringify(data)
    });
    interactionForm.reset();
    showMessage("Interacao registrada.");
    await openLeadDetail(state.selectedLeadId);
    await loadLeads(state.pagination.page);
  } catch (error) {
    showMessage(error.message);
  }
});

$("#export-csv-button").addEventListener("click", () => {
  window.location.href = `/api/leads/export/csv?${getFilters(1).replace("page=1&", "")}`;
});

$("#export-excel-button").addEventListener("click", async () => {
  try {
    await exportExcel();
    showMessage("Planilha Excel gerada.");
  } catch (error) {
    showMessage(error.message);
  }
});

$("#import-csv-input").addEventListener("change", async (event) => {
  const file = event.target.files[0];
  if (!file) return;
  try {
    const content = await file.text();
    const payload = await api("/api/leads/import/csv", {
      method: "POST",
      headers: { "Content-Type": "text/csv" },
      body: content
    });
    showMessage(`Importacao concluida: ${payload.imported} leads.`);
    await refreshData(1);
  } catch (error) {
    showMessage(error.message);
  } finally {
    event.target.value = "";
  }
});

$("#import-excel-input").addEventListener("change", async (event) => {
  const file = event.target.files[0];
  if (!file) return;
  try {
    const buffer = await file.arrayBuffer();
    const workbook = XLSX.read(buffer, { type: "array" });
    const firstSheet = workbook.Sheets[workbook.SheetNames[0]];
    const rows = XLSX.utils.sheet_to_json(firstSheet, { defval: "" });
    const payload = await api("/api/leads/import/json", {
      method: "POST",
      body: JSON.stringify({ rows })
    });
    showMessage(`Importacao Excel concluida: ${payload.imported} leads.`);
    await refreshData(1);
  } catch (error) {
    showMessage(error.message);
  } finally {
    event.target.value = "";
  }
});

(async () => {
  switchAuthTab("login");
  await loadMe();
  if (state.user) {
    await refreshData(1);
  }
})();
