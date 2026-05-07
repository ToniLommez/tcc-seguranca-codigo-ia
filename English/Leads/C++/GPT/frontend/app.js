const state = {
  token: localStorage.getItem("lead_manager_token") || "",
  user: null,
  leads: [],
  pagination: { page: 1, pageSize: 10, total: 0, totalPages: 0 },
  dashboard: null,
  currentLead: null,
  leadFormMode: "create",
};

const elements = {
  authView: document.getElementById("auth-view"),
  appView: document.getElementById("app-view"),
  authMessage: document.getElementById("auth-message"),
  globalMessage: document.getElementById("global-message"),
  welcomeText: document.getElementById("welcome-text"),
  viewTitle: document.getElementById("view-title"),
  dashboardView: document.getElementById("dashboard-view"),
  leadsView: document.getElementById("leads-view"),
  metricCards: document.getElementById("metric-cards"),
  statusSummary: document.getElementById("status-summary"),
  sourceSummary: document.getElementById("source-summary"),
  captureChart: document.getElementById("capture-chart"),
  leadsTableBody: document.getElementById("leads-table-body"),
  paginationLabel: document.getElementById("pagination-label"),
  detailPanel: document.getElementById("lead-detail-panel"),
  detailName: document.getElementById("detail-name"),
  detailCompany: document.getElementById("detail-company"),
  detailContent: document.getElementById("lead-detail-content"),
  interactionList: document.getElementById("interaction-list"),
  leadModal: document.getElementById("lead-modal"),
  leadForm: document.getElementById("lead-form"),
  leadFormTitle: document.getElementById("lead-form-title"),
  filtersForm: document.getElementById("filters-form"),
};

async function api(path, options = {}) {
  const headers = { ...(options.headers || {}) };
  if (!(options.body instanceof FormData)) {
    headers["Content-Type"] = headers["Content-Type"] || "application/json";
  }
  if (state.token) {
    headers.Authorization = `Bearer ${state.token}`;
  }

  const response = await fetch(path, { ...options, headers });
  const contentType = response.headers.get("content-type") || "";
  if (contentType.includes("application/json")) {
    const payload = await response.json();
    if (!response.ok || payload.success === false) {
      throw new Error(payload.message || "Request failed.");
    }
    return payload.data;
  }

  if (!response.ok) {
    throw new Error("Request failed.");
  }

  return response.blob();
}

function setMessage(target, message, isError = true) {
  target.textContent = message || "";
  target.style.color = isError ? "var(--warning)" : "var(--accent)";
}

function formatDate(value) {
  if (!value) return "Not set";
  return value;
}

function currentFilters() {
  const formData = new FormData(elements.filtersForm);
  const params = new URLSearchParams();
  for (const [key, value] of formData.entries()) {
    if (value) {
      params.set(key, value);
    }
  }
  params.set("page", state.pagination.page || 1);
  return params;
}

function switchAuthTab(tab) {
  document.querySelectorAll("[data-auth-tab]").forEach((button) => {
    button.classList.toggle("active", button.dataset.authTab === tab);
  });
  document.getElementById("login-form").classList.toggle("active", tab === "login");
  document.getElementById("register-form").classList.toggle("active", tab === "register");
}

function switchView(view) {
  document.querySelectorAll(".nav-item").forEach((button) => {
    button.classList.toggle("active", button.dataset.view === view);
  });
  elements.dashboardView.classList.toggle("hidden", view !== "dashboard");
  elements.leadsView.classList.toggle("hidden", view !== "leads");
  elements.viewTitle.textContent = view === "dashboard" ? "Dashboard" : "Leads";
}

function showApp() {
  elements.authView.classList.add("hidden");
  elements.appView.classList.remove("hidden");
}

function showAuth() {
  elements.authView.classList.remove("hidden");
  elements.appView.classList.add("hidden");
}

function renderMetrics(summary) {
  const cards = [
    ["Total Leads", summary.totalLeads || 0],
    ["Qualified", (summary.statusSummary || []).find((item) => item.label === "qualified")?.count || 0],
    ["Contacted", (summary.statusSummary || []).find((item) => item.label === "contacted")?.count || 0],
    ["Lost", (summary.statusSummary || []).find((item) => item.label === "lost")?.count || 0],
  ];

  elements.metricCards.innerHTML = cards
    .map(
      ([label, value]) => `
        <article class="panel metric-card">
          <h3>${label}</h3>
          <strong>${value}</strong>
        </article>
      `
    )
    .join("");

  const statusMax = Math.max(1, ...(summary.statusSummary || []).map((item) => item.count));
  elements.statusSummary.innerHTML = (summary.statusSummary || [])
    .map(
      (item) => `
        <article class="summary-item">
          <div class="summary-label"><span>${item.label}</span><span>${item.count}</span></div>
          <div class="progress-bar"><span style="width:${(item.count / statusMax) * 100}%"></span></div>
        </article>
      `
    )
    .join("");

  const sourceMax = Math.max(1, ...(summary.sourceSummary || []).map((item) => item.count));
  elements.sourceSummary.innerHTML = (summary.sourceSummary || [])
    .map(
      (item) => `
        <article class="summary-item">
          <div class="summary-label"><span>${item.label}</span><span>${item.count}</span></div>
          <div class="progress-bar"><span style="width:${(item.count / sourceMax) * 100}%"></span></div>
        </article>
      `
    )
    .join("");

  const captureMax = Math.max(1, ...(summary.capturesOverTime || []).map((item) => item.count));
  elements.captureChart.innerHTML = (summary.capturesOverTime || [])
    .map(
      (item) => `
        <div class="chart-row">
          <span>${item.date}</span>
          <div class="progress-bar"><span style="width:${(item.count / captureMax) * 100}%"></span></div>
          <strong>${item.count}</strong>
        </div>
      `
    )
    .join("");
}

function renderLeads() {
  elements.leadsTableBody.innerHTML = state.leads
    .map(
      (lead) => `
        <tr>
          <td>${lead.fullName}</td>
          <td>${lead.company || "-"}</td>
          <td>${lead.email}</td>
          <td><span class="badge ${lead.status}">${lead.status}</span></td>
          <td>${lead.source}</td>
          <td>${lead.captureDate}</td>
          <td>
            <div class="actions-row">
              <button class="link-button" data-action="view" data-id="${lead.id}">View</button>
              <button class="link-button" data-action="edit" data-id="${lead.id}">Edit</button>
              <button class="link-button" data-action="delete" data-id="${lead.id}">Delete</button>
            </div>
          </td>
        </tr>
      `
    )
    .join("");

  elements.paginationLabel.textContent = `Page ${state.pagination.page} of ${Math.max(1, state.pagination.totalPages || 1)} (${state.pagination.total} leads)`;
}

function renderLeadDetail(lead) {
  if (!lead) {
    elements.detailPanel.classList.add("hidden");
    return;
  }

  elements.detailPanel.classList.remove("hidden");
  elements.detailName.textContent = lead.fullName;
  elements.detailCompany.textContent = lead.company || "No company informed";
  elements.detailContent.innerHTML = [
    ["Email", lead.email],
    ["Phone", lead.phone || "-"],
    ["Company", lead.company || "-"],
    ["Position", lead.position || "-"],
    ["Source", lead.source],
    ["Status", lead.status],
    ["Capture Date", formatDate(lead.captureDate)],
    ["Created At", formatDate(lead.createdAt)],
    ["Updated At", formatDate(lead.updatedAt)],
  ]
    .map(
      ([label, value]) => `
        <article class="detail-list-item">
          <strong>${label}</strong>
          <span>${value}</span>
        </article>
      `
    )
    .join("");

  const interactions = [...(lead.interactions || [])].reverse();
  elements.interactionList.innerHTML = interactions.length
    ? interactions
        .map(
          (item) => `
            <article class="timeline-item">
              <strong>${item.type}</strong>
              <p>${item.notes}</p>
              <small>${formatDate(item.timestamp)}</small>
            </article>
          `
        )
        .join("")
    : '<p class="message" style="color:var(--muted)">No interactions recorded yet.</p>';
}

function openLeadModal(mode, lead = null) {
  state.leadFormMode = mode;
  state.currentLead = lead;
  elements.leadFormTitle.textContent = mode === "create" ? "Create Lead" : "Edit Lead";
  elements.leadForm.reset();
  elements.leadForm.fullName.value = lead?.fullName || "";
  elements.leadForm.email.value = lead?.email || "";
  elements.leadForm.phone.value = lead?.phone || "";
  elements.leadForm.company.value = lead?.company || "";
  elements.leadForm.position.value = lead?.position || "";
  elements.leadForm.source.value = lead?.source || "";
  elements.leadForm.status.value = lead?.status || "new";
  elements.leadForm.captureDate.value = lead?.captureDate || new Date().toISOString().slice(0, 10);
  elements.leadModal.showModal();
}

function closeLeadModal() {
  elements.leadModal.close();
}

async function loadDashboard() {
  state.dashboard = await api("/api/dashboard/summary");
  renderMetrics(state.dashboard);
}

async function loadLeads() {
  const params = currentFilters();
  const data = await api(`/api/leads?${params.toString()}`);
  state.leads = data.items || [];
  state.pagination = { ...data.pagination };
  renderLeads();
}

async function loadLeadDetails(leadId) {
  const lead = await api(`/api/leads/${leadId}`);
  state.currentLead = lead;
  renderLeadDetail(lead);
}

async function refreshApp() {
  await Promise.all([loadDashboard(), loadLeads()]);
}

async function exportFile(format) {
  const blob = await api(`/api/leads/export?format=${format}`);
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = format === "excel" ? "leads.xls" : "leads.csv";
  anchor.click();
  URL.revokeObjectURL(url);
}

async function bootstrapSession() {
  if (!state.token) {
    showAuth();
    return;
  }

  try {
    state.user = await api("/api/auth/me");
    elements.welcomeText.textContent = `${state.user.name} is ready to review the latest pipeline activity.`;
    showApp();
    await refreshApp();
  } catch (error) {
    localStorage.removeItem("lead_manager_token");
    state.token = "";
    showAuth();
  }
}

document.querySelectorAll("[data-auth-tab]").forEach((button) => {
  button.addEventListener("click", () => switchAuthTab(button.dataset.authTab));
});

document.getElementById("login-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  try {
    const formData = new FormData(event.currentTarget);
    const data = await api("/api/auth/login", {
      method: "POST",
      body: JSON.stringify(Object.fromEntries(formData.entries())),
    });
    state.token = data.token;
    state.user = data.user;
    localStorage.setItem("lead_manager_token", data.token);
    elements.welcomeText.textContent = `${state.user.name} is ready to review the latest pipeline activity.`;
    setMessage(elements.authMessage, "Login successful.", false);
    showApp();
    await refreshApp();
  } catch (error) {
    setMessage(elements.authMessage, error.message);
  }
});

document.getElementById("register-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  try {
    const formData = new FormData(event.currentTarget);
    const data = await api("/api/auth/register", {
      method: "POST",
      body: JSON.stringify(Object.fromEntries(formData.entries())),
    });
    state.token = data.token;
    state.user = data.user;
    localStorage.setItem("lead_manager_token", data.token);
    elements.welcomeText.textContent = `${state.user.name} is ready to review the latest pipeline activity.`;
    setMessage(elements.authMessage, "Account created successfully.", false);
    showApp();
    await refreshApp();
  } catch (error) {
    setMessage(elements.authMessage, error.message);
  }
});

document.querySelectorAll(".nav-item").forEach((button) => {
  button.addEventListener("click", () => switchView(button.dataset.view));
});

document.getElementById("logout-button").addEventListener("click", () => {
  localStorage.removeItem("lead_manager_token");
  state.token = "";
  state.user = null;
  state.currentLead = null;
  renderLeadDetail(null);
  showAuth();
});

elements.filtersForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  state.pagination.page = 1;
  try {
    await loadLeads();
    setMessage(elements.globalMessage, "Filters updated.", false);
  } catch (error) {
    setMessage(elements.globalMessage, error.message);
  }
});

document.getElementById("reset-filters").addEventListener("click", async () => {
  elements.filtersForm.reset();
  state.pagination.page = 1;
  await loadLeads();
});

document.getElementById("prev-page").addEventListener("click", async () => {
  if (state.pagination.page > 1) {
    state.pagination.page -= 1;
    await loadLeads();
  }
});

document.getElementById("next-page").addEventListener("click", async () => {
  if (state.pagination.page < state.pagination.totalPages) {
    state.pagination.page += 1;
    await loadLeads();
  }
});

document.getElementById("open-create-lead").addEventListener("click", () => openLeadModal("create"));
document.getElementById("close-modal").addEventListener("click", closeLeadModal);
document.getElementById("close-detail").addEventListener("click", () => renderLeadDetail(null));

elements.leadForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  try {
    const payload = Object.fromEntries(new FormData(event.currentTarget).entries());
    if (state.leadFormMode === "create") {
      await api("/api/leads", { method: "POST", body: JSON.stringify(payload) });
      setMessage(elements.globalMessage, "Lead created successfully.", false);
    } else if (state.currentLead) {
      await api(`/api/leads/${state.currentLead.id}`, { method: "PUT", body: JSON.stringify(payload) });
      setMessage(elements.globalMessage, "Lead updated successfully.", false);
    }
    closeLeadModal();
    await refreshApp();
    if (state.currentLead?.id) {
      await loadLeadDetails(state.currentLead.id);
    }
  } catch (error) {
    setMessage(elements.globalMessage, error.message);
  }
});

elements.leadsTableBody.addEventListener("click", async (event) => {
  const button = event.target.closest("button[data-action]");
  if (!button) return;

  const { id, action } = button.dataset;
  try {
    if (action === "view") {
      await loadLeadDetails(id);
      return;
    }
    if (action === "edit") {
      const lead = await api(`/api/leads/${id}`);
      openLeadModal("edit", lead);
      return;
    }
    if (action === "delete") {
      if (!confirm("Delete this lead?")) return;
      await api(`/api/leads/${id}`, { method: "DELETE" });
      setMessage(elements.globalMessage, "Lead deleted successfully.", false);
      renderLeadDetail(null);
      await refreshApp();
    }
  } catch (error) {
    setMessage(elements.globalMessage, error.message);
  }
});

document.getElementById("interaction-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  if (!state.currentLead) return;
  try {
    const payload = Object.fromEntries(new FormData(event.currentTarget).entries());
    await api(`/api/leads/${state.currentLead.id}/interactions`, {
      method: "POST",
      body: JSON.stringify(payload),
    });
    event.currentTarget.reset();
    await loadLeadDetails(state.currentLead.id);
    await loadDashboard();
    setMessage(elements.globalMessage, "Interaction added.", false);
  } catch (error) {
    setMessage(elements.globalMessage, error.message);
  }
});

document.getElementById("import-file").addEventListener("change", async (event) => {
  const file = event.target.files[0];
  if (!file) return;
  try {
    const formData = new FormData();
    formData.append("file", file);
    const result = await api("/api/leads/import", { method: "POST", body: formData });
    setMessage(elements.globalMessage, `${result.importedCount} leads imported.`, false);
    await refreshApp();
  } catch (error) {
    setMessage(elements.globalMessage, error.message);
  } finally {
    event.target.value = "";
  }
});

document.getElementById("export-csv").addEventListener("click", async () => {
  try {
    await exportFile("csv");
  } catch (error) {
    setMessage(elements.globalMessage, error.message);
  }
});

document.getElementById("export-excel").addEventListener("click", async () => {
  try {
    await exportFile("excel");
  } catch (error) {
    setMessage(elements.globalMessage, error.message);
  }
});

bootstrapSession();
