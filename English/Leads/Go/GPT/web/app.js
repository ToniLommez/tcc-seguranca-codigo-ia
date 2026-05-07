const state = {
  token: localStorage.getItem("lead_manager_token") || "",
  user: null,
  route: window.location.hash || "#/dashboard",
  leads: null,
  leadDetail: null,
  dashboard: null,
  notice: null,
  filters: {
    page: 1,
    pageSize: 10,
    sortBy: "captureDate",
    sortDir: "desc",
    status: "",
    source: "",
    q: "",
    dateFrom: "",
    dateTo: ""
  }
};

const app = document.getElementById("app");

window.addEventListener("hashchange", () => {
  state.route = window.location.hash || "#/dashboard";
  hydrateForRoute()
    .catch(showError)
    .finally(render);
});

document.addEventListener("DOMContentLoaded", bootstrap);

async function bootstrap() {
  if (state.token) {
    try {
      state.user = await api("/api/auth/me");
    } catch (error) {
      clearSession();
    }
  }
  render();
  if (state.user) {
    await hydrateForRoute();
    render();
  }
}

function render() {
  app.innerHTML = state.user ? renderShell() : renderAuth();
  bindCommonActions();
}

function renderAuth() {
  return `
    <div class="auth-layout">
      <section class="auth-hero">
        <div>
          <span class="eyebrow">Lead Ops Engine</span>
          <h1 class="hero-title">Turn daily lead flow into a system your team can actually steer.</h1>
          <p class="hero-copy">Register your company account, secure access with JWT authentication, and manage every lead across one desktop-first workspace built for fast client operations.</p>
          <div class="hero-metrics">
            <div class="metric-card">
              <span>Lead Lifecycle</span>
              <strong>New to Won</strong>
            </div>
            <div class="metric-card">
              <span>Import / Export</span>
              <strong>CSV + XLSX</strong>
            </div>
            <div class="metric-card">
              <span>Shared Database</span>
              <strong>Firebase</strong>
            </div>
          </div>
        </div>
        <p class="hero-copy">The backend stores account-specific leads and interaction history, while the dashboard keeps new, contacted, qualified, and lost pipelines visible at a glance.</p>
      </section>

      <section class="auth-panel-wrap">
        <div class="card auth-card">
          ${renderNotice()}
          <div id="auth-login">
            <h2 class="card-title">Welcome back</h2>
            <p class="card-subtitle">Sign in to access your lead pipeline, dashboard, and import/export tools.</p>
            <form id="login-form" class="form-grid">
              <div class="field">
                <label for="login-email">Email</label>
                <input id="login-email" name="email" type="email" required>
              </div>
              <div class="field">
                <label for="login-password">Password</label>
                <input id="login-password" name="password" type="password" required>
              </div>
              <button class="primary" type="submit">Log In</button>
            </form>
            <button class="link-button" data-toggle-auth="register">Need an account? Register here</button>
          </div>

          <div id="auth-register" class="hidden">
            <h2 class="card-title">Create your workspace</h2>
            <p class="card-subtitle">Register a manager or company account and start collecting leads right away.</p>
            <form id="register-form" class="form-grid">
              <div class="field">
                <label for="register-name">Name</label>
                <input id="register-name" name="name" type="text" required>
              </div>
              <div class="field">
                <label for="register-email">Email</label>
                <input id="register-email" name="email" type="email" required>
              </div>
              <div class="field">
                <label for="register-password">Password</label>
                <input id="register-password" name="password" type="password" minlength="6" required>
              </div>
              <button class="primary" type="submit">Register</button>
            </form>
            <button class="link-button" data-toggle-auth="login">Already have an account? Log in</button>
          </div>
        </div>
      </section>
    </div>
  `;
}

function renderShell() {
  return `
    <div class="app-shell">
      <aside class="sidebar">
        <div class="brand">
          <span>Workspace</span>
          <strong>Lead Manager</strong>
        </div>
        <nav class="nav">
          <button data-route="#/dashboard" class="${isActiveRoute("#/dashboard") ? "active" : ""}">Dashboard</button>
          <button data-route="#/leads" class="${isActiveRoute("#/leads") ? "active" : ""}">Leads</button>
          <button data-route="#/leads/new" class="${isActiveRoute("#/leads/new") ? "active" : ""}">New Lead</button>
        </nav>
        <div class="sidebar-footer">
          <div>${escapeHTML(state.user.name)}</div>
          <div>${escapeHTML(state.user.email)}</div>
          <div class="button-row" style="margin-top:16px;">
            <button class="secondary" id="logout-button" type="button">Log Out</button>
          </div>
        </div>
      </aside>

      <main class="main">
        ${renderNotice()}
        ${renderCurrentView()}
      </main>
    </div>
  `;
}

function renderCurrentView() {
  if (state.route.startsWith("#/leads/new")) {
    return renderLeadForm();
  }
  if (/^#\/leads\/[^/]+\/edit$/.test(state.route)) {
    return renderLeadForm(state.leadDetail?.lead);
  }
  if (/^#\/leads\/[^/]+$/.test(state.route)) {
    return renderLeadDetail();
  }
  if (state.route.startsWith("#/leads")) {
    return renderLeads();
  }
  return renderDashboard();
}

function renderDashboard() {
  const summary = state.dashboard;
  const statusCounts = summary?.statusCounts || { new: 0, contacted: 0, qualified: 0, lost: 0 };
  const totalLeads = summary?.totalLeads || 0;
  const trend = summary?.capturesOverTime || [];
  const highestTrend = Math.max(...trend.map(item => item.count), 1);
  const sourceEntries = Object.entries(summary?.sourceCounts || {});

  return `
    <section class="page-header">
      <div>
        <h1>Dashboard</h1>
        <p>Keep the whole lead pipeline visible, including volume by stage and capture pace over the last two weeks.</p>
      </div>
      <div class="toolbar">
        <button class="secondary" id="refresh-dashboard" type="button">Refresh Metrics</button>
      </div>
    </section>

    <section class="grid-cards">
      <article class="card stat-card">
        <span class="label">Total Leads</span>
        <strong class="value">${totalLeads}</strong>
      </article>
      <article class="card stat-card">
        <span class="label">New</span>
        <strong class="value">${statusCounts.new || 0}</strong>
      </article>
      <article class="card stat-card">
        <span class="label">Contacted</span>
        <strong class="value">${statusCounts.contacted || 0}</strong>
      </article>
      <article class="card stat-card">
        <span class="label">Qualified</span>
        <strong class="value">${statusCounts.qualified || 0}</strong>
      </article>
    </section>

    <section class="layout-2">
      <article class="card panel">
        <h2>Captures Over Time</h2>
        <p class="card-subtitle">Daily volume over the last 14 days.</p>
        <div class="chart-bars">
          ${trend.map(point => `
            <div class="chart-bar">
              <small>${point.count}</small>
              <div class="chart-bar-fill" style="height:${Math.max((point.count / highestTrend) * 180, 10)}px"></div>
              <small>${point.label.slice(5)}</small>
            </div>
          `).join("")}
        </div>
      </article>

      <article class="card panel">
        <h2>Lead Sources</h2>
        <p class="card-subtitle">See which acquisition channels are delivering the most volume.</p>
        <div class="source-list">
          ${sourceEntries.length ? sourceEntries.map(([source, count]) => `
            <div class="source-item">
              <strong>${escapeHTML(source)}</strong>
              <small>${count} leads</small>
            </div>
          `).join("") : `<div class="empty-state">No lead sources yet.</div>`}
        </div>
      </article>
    </section>
  `;
}

function renderLeads() {
  const response = state.leads;
  const items = response?.items || [];
  const page = response?.page || 1;
  const totalPages = response?.totalPages || 1;

  return `
    <section class="page-header">
      <div>
        <h1>Leads</h1>
        <p>Search, segment, import, export, and keep lead data moving for every client account you manage.</p>
      </div>
      <div class="toolbar">
        <button class="secondary" id="open-import" type="button">Import CSV/XLSX</button>
        <button class="secondary" id="export-csv" type="button">Export CSV</button>
        <button class="secondary" id="export-xlsx" type="button">Export XLSX</button>
        <button class="primary" data-route="#/leads/new" type="button">Create Lead</button>
      </div>
    </section>

    <article class="card panel">
      <div class="filters">
        <div class="field">
          <label for="filter-q">Search</label>
          <input id="filter-q" type="text" placeholder="Name, email or company" value="${escapeHTML(state.filters.q)}">
        </div>
        <div class="field">
          <label for="filter-status">Status</label>
          <select id="filter-status">
            ${selectOptions(["", "new", "contacted", "qualified", "lost"], state.filters.status)}
          </select>
        </div>
        <div class="field">
          <label for="filter-source">Source</label>
          <input id="filter-source" type="text" placeholder="website, event..." value="${escapeHTML(state.filters.source)}">
        </div>
        <div class="field">
          <label for="filter-from">Capture Date From</label>
          <input id="filter-from" type="date" value="${escapeHTML(state.filters.dateFrom)}">
        </div>
        <div class="field">
          <label for="filter-to">Capture Date To</label>
          <input id="filter-to" type="date" value="${escapeHTML(state.filters.dateTo)}">
        </div>
      </div>

      <div class="toolbar" style="margin-bottom:18px;">
        <button class="primary" id="apply-filters" type="button">Apply Filters</button>
        <button class="secondary" id="reset-filters" type="button">Reset</button>
      </div>

      <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Company</th>
              <th>Email</th>
              <th>Source</th>
              <th>Status</th>
              <th>Capture Date</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            ${items.length ? items.map(lead => `
              <tr>
                <td>${escapeHTML(lead.fullName)}</td>
                <td>${escapeHTML(lead.company)}</td>
                <td>${escapeHTML(lead.email)}</td>
                <td>${escapeHTML(lead.source)}</td>
                <td><span class="badge ${escapeHTML(lead.status)}">${escapeHTML(lead.status)}</span></td>
                <td>${formatDate(lead.captureDate)}</td>
                <td>
                  <div class="button-row">
                    <button class="secondary" data-route="#/leads/${lead.id}" type="button">View</button>
                    <button class="secondary" data-route="#/leads/${lead.id}/edit" type="button">Edit</button>
                    <button class="secondary" data-delete-id="${lead.id}" type="button">Delete</button>
                  </div>
                </td>
              </tr>
            `).join("") : `<tr><td colspan="7"><div class="empty-state">No leads matched your filters.</div></td></tr>`}
          </tbody>
        </table>
      </div>

      <div class="pagination">
        <div>Page ${page} of ${totalPages}</div>
        <div class="button-row">
          <button class="secondary" id="prev-page" type="button" ${page <= 1 ? "disabled" : ""}>Previous</button>
          <button class="secondary" id="next-page" type="button" ${page >= totalPages ? "disabled" : ""}>Next</button>
        </div>
      </div>
    </article>

    <input id="import-file" class="hidden" type="file" accept=".csv,.xlsx">
  `;
}

function renderLeadForm(lead = null) {
  const isEdit = Boolean(lead?.id);
  return `
    <section class="page-header">
      <div>
        <h1>${isEdit ? "Edit Lead" : "Create Lead"}</h1>
        <p>${isEdit ? "Update the lead record and keep the timeline current." : "Create a new lead with source, ownership context, and follow-up-ready data."}</p>
      </div>
      <div class="toolbar">
        <button class="secondary" data-route="#/leads" type="button">Back to Leads</button>
      </div>
    </section>

    <article class="card panel">
      <form id="lead-form" class="form-grid">
        <div class="grid-2">
          <div class="field">
            <label for="lead-full-name">Full Name</label>
            <input id="lead-full-name" name="fullName" type="text" value="${escapeHTML(lead?.fullName || "")}" required>
          </div>
          <div class="field">
            <label for="lead-email">Email</label>
            <input id="lead-email" name="email" type="email" value="${escapeHTML(lead?.email || "")}" required>
          </div>
        </div>

        <div class="grid-2">
          <div class="field">
            <label for="lead-phone">Phone</label>
            <input id="lead-phone" name="phone" type="text" value="${escapeHTML(lead?.phone || "")}">
          </div>
          <div class="field">
            <label for="lead-company">Company</label>
            <input id="lead-company" name="company" type="text" value="${escapeHTML(lead?.company || "")}" required>
          </div>
        </div>

        <div class="grid-2">
          <div class="field">
            <label for="lead-position">Position</label>
            <input id="lead-position" name="position" type="text" value="${escapeHTML(lead?.position || "")}">
          </div>
          <div class="field">
            <label for="lead-source">Source</label>
            <input id="lead-source" name="source" type="text" value="${escapeHTML(lead?.source || "")}" placeholder="referral, website, event..." required>
          </div>
        </div>

        <div class="grid-2">
          <div class="field">
            <label for="lead-status">Status</label>
            <select id="lead-status" name="status">
              ${selectOptions(["new", "contacted", "qualified", "lost"], lead?.status || "new")}
            </select>
          </div>
          <div class="field">
            <label for="lead-capture-date">Capture Date</label>
            <input id="lead-capture-date" name="captureDate" type="date" value="${lead?.captureDate ? lead.captureDate.slice(0, 10) : todayDate()}">
          </div>
        </div>

        <div class="button-row">
          <button class="primary" type="submit">${isEdit ? "Save Changes" : "Create Lead"}</button>
          <button class="secondary" data-route="#/leads" type="button">Cancel</button>
        </div>
      </form>
    </article>
  `;
}

function renderLeadDetail() {
  const detail = state.leadDetail;
  if (!detail?.lead) {
    return `<article class="card panel"><div class="empty-state">Loading lead details...</div></article>`;
  }

  return `
    <section class="page-header">
      <div>
        <h1>${escapeHTML(detail.lead.fullName)}</h1>
        <p>Complete lead context plus the interaction timeline for your team.</p>
      </div>
      <div class="toolbar">
        <button class="secondary" data-route="#/leads" type="button">Back</button>
        <button class="primary" data-route="#/leads/${detail.lead.id}/edit" type="button">Edit Lead</button>
      </div>
    </section>

    <section class="detail-grid">
      <article class="card panel">
        <h2>Lead Profile</h2>
        <div class="detail-list">
          ${detailField("Email", detail.lead.email)}
          ${detailField("Phone", detail.lead.phone || "Not provided")}
          ${detailField("Company", detail.lead.company)}
          ${detailField("Position", detail.lead.position || "Not provided")}
          ${detailField("Source", detail.lead.source)}
          ${detailField("Status", `<span class="badge ${escapeHTML(detail.lead.status)}">${escapeHTML(detail.lead.status)}</span>`)}
          ${detailField("Capture Date", formatDate(detail.lead.captureDate))}
        </div>
      </article>

      <article class="card panel">
        <h2>Interaction History</h2>
        <form id="interaction-form" class="form-grid" style="margin-bottom:22px;">
          <div class="field">
            <label for="interaction-type">Interaction Type</label>
            <input id="interaction-type" name="type" type="text" placeholder="Call, meeting, email..." required>
          </div>
          <div class="field">
            <label for="interaction-note">Note</label>
            <textarea id="interaction-note" name="note" required></textarea>
          </div>
          <button class="primary" type="submit">Add Interaction</button>
        </form>

        <div class="timeline">
          ${detail.interactions?.length ? detail.interactions.map(item => `
            <div class="timeline-item">
              <strong>${escapeHTML(item.type)}</strong>
              <div>${escapeHTML(item.note)}</div>
              <small>${formatDateTime(item.createdAt)}</small>
            </div>
          `).join("") : `<div class="empty-state">No interactions yet.</div>`}
        </div>
      </article>
    </section>
  `;
}

function detailField(label, value) {
  return `
    <div class="detail-item">
      <span>${label}</span>
      <div>${value}</div>
    </div>
  `;
}

function renderNotice() {
  if (!state.notice) {
    return "";
  }
  return `<div class="notice ${state.notice.type}">${escapeHTML(state.notice.message)}</div>`;
}

function bindCommonActions() {
  document.querySelectorAll("[data-route]").forEach(button => {
    button.addEventListener("click", async event => {
      const nextRoute = event.currentTarget.getAttribute("data-route");
      window.location.hash = nextRoute;
      await hydrateForRoute();
      render();
    });
  });

  if (!state.user) {
    bindAuthActions();
    return;
  }

  document.getElementById("logout-button")?.addEventListener("click", () => {
    clearSession();
    state.notice = { type: "success", message: "You have been logged out." };
    render();
  });

  bindDashboardActions();
  bindLeadActions();
  bindLeadFormActions();
  bindDetailActions();
}

function bindAuthActions() {
  document.querySelectorAll("[data-toggle-auth]").forEach(button => {
    button.addEventListener("click", () => {
      document.getElementById("auth-login").classList.toggle("hidden");
      document.getElementById("auth-register").classList.toggle("hidden");
    });
  });

  document.getElementById("login-form")?.addEventListener("submit", async event => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    try {
      const response = await api("/api/auth/login", {
        method: "POST",
        body: JSON.stringify({
          email: form.get("email"),
          password: form.get("password")
        })
      }, false);
      setSession(response.token, response.user);
      state.notice = { type: "success", message: "Login successful." };
      window.location.hash = "#/dashboard";
      await hydrateForRoute();
      render();
    } catch (error) {
      showError(error);
    }
  });

  document.getElementById("register-form")?.addEventListener("submit", async event => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    try {
      const response = await api("/api/auth/register", {
        method: "POST",
        body: JSON.stringify({
          name: form.get("name"),
          email: form.get("email"),
          password: form.get("password")
        })
      }, false);
      setSession(response.token, response.user);
      state.notice = { type: "success", message: "Registration complete." };
      window.location.hash = "#/dashboard";
      await hydrateForRoute();
      render();
    } catch (error) {
      showError(error);
    }
  });
}

function bindDashboardActions() {
  document.getElementById("refresh-dashboard")?.addEventListener("click", async () => {
    await loadDashboard();
    state.notice = { type: "success", message: "Dashboard refreshed." };
    render();
  });
}

function bindLeadActions() {
  document.getElementById("apply-filters")?.addEventListener("click", async () => {
    syncFiltersFromUI();
    state.filters.page = 1;
    await loadLeads();
    render();
  });

  document.getElementById("reset-filters")?.addEventListener("click", async () => {
    state.filters = { page: 1, pageSize: 10, sortBy: "captureDate", sortDir: "desc", status: "", source: "", q: "", dateFrom: "", dateTo: "" };
    await loadLeads();
    render();
  });

  document.getElementById("prev-page")?.addEventListener("click", async () => {
    if (state.filters.page > 1) {
      state.filters.page -= 1;
      await loadLeads();
      render();
    }
  });

  document.getElementById("next-page")?.addEventListener("click", async () => {
    state.filters.page += 1;
    await loadLeads();
    render();
  });

  document.querySelectorAll("[data-delete-id]").forEach(button => {
    button.addEventListener("click", async event => {
      const id = event.currentTarget.getAttribute("data-delete-id");
      if (!window.confirm("Delete this lead?")) {
        return;
      }
      try {
        await api(`/api/leads/${id}`, { method: "DELETE" });
        state.notice = { type: "success", message: "Lead deleted." };
        await Promise.all([loadLeads(), loadDashboard()]);
        render();
      } catch (error) {
        showError(error);
      }
    });
  });

  document.getElementById("open-import")?.addEventListener("click", () => {
    document.getElementById("import-file")?.click();
  });

  document.getElementById("import-file")?.addEventListener("change", async event => {
    const [file] = event.target.files;
    if (!file) {
      return;
    }
    try {
      const formData = new FormData();
      formData.append("file", file);
      const result = await api("/api/leads/import", { method: "POST", body: formData, rawBody: true });
      state.notice = { type: result.failed ? "error" : "success", message: `Imported ${result.imported} leads${result.failed ? ` with ${result.failed} errors.` : "."}` };
      await Promise.all([loadLeads(), loadDashboard()]);
      render();
    } catch (error) {
      showError(error);
    } finally {
      event.target.value = "";
    }
  });

  document.getElementById("export-csv")?.addEventListener("click", () => downloadExport("csv"));
  document.getElementById("export-xlsx")?.addEventListener("click", () => downloadExport("xlsx"));
}

function bindLeadFormActions() {
  document.getElementById("lead-form")?.addEventListener("submit", async event => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    const payload = {
      fullName: form.get("fullName"),
      email: form.get("email"),
      phone: form.get("phone"),
      company: form.get("company"),
      position: form.get("position"),
      source: form.get("source"),
      status: form.get("status"),
      captureDate: form.get("captureDate")
    };

    const editingLead = state.leadDetail?.lead && state.route.endsWith("/edit") ? state.leadDetail.lead : null;
    const endpoint = editingLead ? `/api/leads/${editingLead.id}` : "/api/leads";
    const method = editingLead ? "PUT" : "POST";

    try {
      const lead = await api(endpoint, { method, body: JSON.stringify(payload) });
      state.notice = { type: "success", message: editingLead ? "Lead updated." : "Lead created." };
      window.location.hash = `#/leads/${lead.id}`;
      await hydrateForRoute();
      render();
    } catch (error) {
      showError(error);
    }
  });
}

function bindDetailActions() {
  document.getElementById("interaction-form")?.addEventListener("submit", async event => {
    event.preventDefault();
    const leadID = state.leadDetail?.lead?.id;
    if (!leadID) {
      return;
    }

    const form = new FormData(event.currentTarget);
    try {
      await api(`/api/leads/${leadID}/interactions`, {
        method: "POST",
        body: JSON.stringify({
          type: form.get("type"),
          note: form.get("note")
        })
      });
      state.notice = { type: "success", message: "Interaction added." };
      await hydrateForRoute();
      render();
    } catch (error) {
      showError(error);
    }
  });
}

async function hydrateForRoute() {
  if (!state.user) {
    return;
  }

  if (state.route.startsWith("#/leads/new")) {
    state.leadDetail = null;
    return;
  }

  if (/^#\/leads\/[^/]+\/edit$/.test(state.route) || /^#\/leads\/[^/]+$/.test(state.route)) {
    const leadID = state.route.split("/")[2];
    if (leadID) {
      state.leadDetail = await api(`/api/leads/${leadID}`);
      return;
    }
  }

  state.leadDetail = null;
  if (state.route.startsWith("#/leads")) {
    await loadLeads();
    return;
  }
  await loadDashboard();
}

async function loadDashboard() {
  state.dashboard = await api("/api/dashboard");
}

async function loadLeads() {
  const params = new URLSearchParams();
  Object.entries(state.filters).forEach(([key, value]) => {
    if (value !== "" && value !== null && value !== undefined) {
      params.set(key, value);
    }
  });
  state.leads = await api(`/api/leads?${params.toString()}`);
}

function syncFiltersFromUI() {
  state.filters.q = document.getElementById("filter-q")?.value || "";
  state.filters.status = document.getElementById("filter-status")?.value || "";
  state.filters.source = document.getElementById("filter-source")?.value || "";
  state.filters.dateFrom = document.getElementById("filter-from")?.value || "";
  state.filters.dateTo = document.getElementById("filter-to")?.value || "";
}

async function downloadExport(format) {
  try {
    const params = new URLSearchParams({ ...state.filters, format });
    const response = await fetch(`/api/leads/export?${params.toString()}`, {
      headers: { Authorization: `Bearer ${state.token}` }
    });
    if (!response.ok) {
      const error = await response.json().catch(() => ({ error: "Export failed." }));
      throw new Error(error.error || "Export failed.");
    }

    const blob = await response.blob();
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = format === "xlsx" ? "leads.xlsx" : "leads.csv";
    anchor.click();
    URL.revokeObjectURL(url);
    state.notice = { type: "success", message: `Exported leads as ${format.toUpperCase()}.` };
    render();
  } catch (error) {
    showError(error);
  }
}

async function api(url, options = {}, includeAuth = true) {
  const headers = options.rawBody ? {} : { "Content-Type": "application/json" };
  if (includeAuth && state.token) {
    headers.Authorization = `Bearer ${state.token}`;
  }

  const response = await fetch(url, {
    ...options,
    headers: {
      ...headers,
      ...(options.headers || {})
    }
  });

  if (response.status === 204) {
    return null;
  }

  const payload = await response.json().catch(() => ({}));
  if (!response.ok) {
    if (response.status === 401) {
      clearSession();
      render();
    }
    throw new Error(payload.error || "Request failed.");
  }

  return payload;
}

function setSession(token, user) {
  state.token = token;
  state.user = user;
  localStorage.setItem("lead_manager_token", token);
}

function clearSession() {
  state.token = "";
  state.user = null;
  state.dashboard = null;
  state.leads = null;
  state.leadDetail = null;
  localStorage.removeItem("lead_manager_token");
}

function selectOptions(options, selected) {
  return options.map(option => {
    const label = option === "" ? "All" : option.charAt(0).toUpperCase() + option.slice(1);
    return `<option value="${option}" ${option === selected ? "selected" : ""}>${label}</option>`;
  }).join("");
}

function isActiveRoute(target) {
  if (target === "#/leads") {
    return state.route.startsWith("#/leads") && !state.route.startsWith("#/leads/new");
  }
  return state.route === target;
}

function showError(error) {
  state.notice = { type: "error", message: error.message || "Something went wrong." };
  render();
}

function formatDate(value) {
  if (!value) {
    return "N/A";
  }
  return new Date(value).toLocaleDateString();
}

function formatDateTime(value) {
  if (!value) {
    return "N/A";
  }
  return new Date(value).toLocaleString();
}

function todayDate() {
  return new Date().toISOString().slice(0, 10);
}

function escapeHTML(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}
