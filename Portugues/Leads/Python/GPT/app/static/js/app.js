const PAGE = window.APP_CONFIG?.pageName || "";
const LEAD_ID = window.APP_CONFIG?.leadId || "";

const statusLabels = {
    novo: "Novo",
    em_contato: "Em contato",
    qualificado: "Qualificado",
    perdido: "Perdido",
};

async function apiFetch(url, options = {}) {
    const config = {
        credentials: "same-origin",
        headers: {
            Accept: "application/json",
            ...(options.body instanceof FormData ? {} : { "Content-Type": "application/json" }),
            ...(options.headers || {}),
        },
        ...options,
    };

    const response = await fetch(url, config);
    const isJson = response.headers.get("content-type")?.includes("application/json");
    const payload = isJson ? await response.json() : null;

    if (!response.ok) {
        const message = payload?.detail || "Não foi possível concluir a operação.";
        if (response.status === 401 && PAGE !== "auth") {
            window.location.href = "/";
            return null;
        }
        throw new Error(message);
    }

    return payload;
}

function showToast(message, isError = false) {
    const current = document.querySelector(".toast");
    if (current) current.remove();

    const toast = document.createElement("div");
    toast.className = "toast";
    if (isError) {
        toast.style.background = "rgba(180, 35, 24, 0.94)";
    }
    toast.textContent = message;
    document.body.appendChild(toast);
    window.setTimeout(() => toast.remove(), 3400);
}

function formatStatus(status) {
    return statusLabels[status] || status;
}

function formatDate(value) {
    if (!value) return "-";
    return new Date(`${value}T00:00:00`).toLocaleDateString("pt-BR");
}

function setText(id, value) {
    const element = document.getElementById(id);
    if (element) element.textContent = value;
}

async function ensureAuthenticated() {
    return apiFetch("/api/auth/me");
}

async function initAuthPage() {
    try {
        const user = await apiFetch("/api/auth/me");
        if (user) {
            window.location.href = "/dashboard";
            return;
        }
    } catch (error) {
        // Usuário não autenticado, segue no login.
    }

    const loginForm = document.getElementById("loginForm");
    const registerForm = document.getElementById("registerForm");

    loginForm?.addEventListener("submit", async (event) => {
        event.preventDefault();
        const formData = new FormData(loginForm);
        try {
            await apiFetch("/api/auth/login", {
                method: "POST",
                body: JSON.stringify(Object.fromEntries(formData.entries())),
            });
            window.location.href = "/dashboard";
        } catch (error) {
            showToast(error.message, true);
        }
    });

    registerForm?.addEventListener("submit", async (event) => {
        event.preventDefault();
        const formData = new FormData(registerForm);
        try {
            await apiFetch("/api/auth/register", {
                method: "POST",
                body: JSON.stringify(Object.fromEntries(formData.entries())),
            });
            window.location.href = "/dashboard";
        } catch (error) {
            showToast(error.message, true);
        }
    });
}

function renderCaptureBars(items) {
    const container = document.getElementById("capturesChart");
    if (!container) return;
    container.innerHTML = "";

    const max = Math.max(...items.map((item) => item.total), 1);
    items.forEach((item) => {
        const row = document.createElement("div");
        row.className = "bar-row";
        row.innerHTML = `
            <span>${new Date(`${item.date}T00:00:00`).toLocaleDateString("pt-BR", { day: "2-digit", month: "2-digit" })}</span>
            <div class="bar"><div class="bar-fill" style="width:${(item.total / max) * 100}%"></div></div>
            <strong>${item.total}</strong>
        `;
        container.appendChild(row);
    });
}

function renderSources(items) {
    const container = document.getElementById("sourcesList");
    if (!container) return;

    if (!items.length) {
        container.innerHTML = '<div class="empty-state">Nenhuma fonte registrada ainda.</div>';
        return;
    }

    container.innerHTML = items
        .map(
            (item) => `
                <article class="source-item">
                    <strong>${item.source}</strong>
                    <p>${item.total} lead(s)</p>
                </article>
            `
        )
        .join("");
}

async function initDashboardPage() {
    const user = await ensureAuthenticated();
    setText("welcomeMessage", `Olá, ${user.name}. Aqui está a visão geral da sua operação.`);

    const summary = await apiFetch("/api/dashboard/summary");
    setText("metricTotal", String(summary.total_leads));
    setText("metricNovo", String(summary.status_summary.novo || 0));
    setText("metricContato", String(summary.status_summary.em_contato || 0));
    setText("metricQualificado", String(summary.status_summary.qualificado || 0));
    setText("metricPerdido", String(summary.status_summary.perdido || 0));
    renderCaptureBars(summary.captures_last_14_days || []);
    renderSources(summary.top_sources || []);
}

function leadFiltersToQuery(form, page = 1) {
    const params = new URLSearchParams();
    const formData = new FormData(form);

    for (const [key, rawValue] of formData.entries()) {
        const value = String(rawValue).trim();
        if (!value) continue;
        params.set(key, value);
    }
    params.set("page", String(page));
    return params;
}

function leadActionButtons(lead) {
    return `
        <div class="header-actions">
            <a class="button ghost" href="/leads/${lead.id}">Detalhes</a>
            <a class="button ghost" href="/leads/${lead.id}/edit">Editar</a>
        </div>
    `;
}

function renderLeadsTable(items) {
    const tbody = document.getElementById("leadsTableBody");
    if (!tbody) return;

    if (!items.length) {
        tbody.innerHTML = `
            <tr>
                <td colspan="6">
                    <div class="empty-state">Nenhum lead encontrado com os filtros atuais.</div>
                </td>
            </tr>
        `;
        return;
    }

    tbody.innerHTML = items
        .map(
            (lead) => `
                <tr>
                    <td>
                        <strong>${lead.full_name}</strong>
                        <div>${lead.email}</div>
                    </td>
                    <td>
                        <strong>${lead.company}</strong>
                        <div>${lead.role}</div>
                    </td>
                    <td>${lead.source}</td>
                    <td><span class="status-pill ${lead.status}">${formatStatus(lead.status)}</span></td>
                    <td>${formatDate(lead.captured_at)}</td>
                    <td>${leadActionButtons(lead)}</td>
                </tr>
            `
        )
        .join("");
}

async function initLeadsPage() {
    await ensureAuthenticated();
    const filtersForm = document.getElementById("filtersForm");
    const summaryElement = document.getElementById("tableSummary");
    const paginationLabel = document.getElementById("paginationLabel");
    const prevPageButton = document.getElementById("prevPageButton");
    const nextPageButton = document.getElementById("nextPageButton");
    const importButton = document.getElementById("importButton");
    const importInput = document.getElementById("importInput");
    const exportCsvButton = document.getElementById("exportCsvButton");
    const exportXlsxButton = document.getElementById("exportXlsxButton");
    const clearFiltersButton = document.getElementById("clearFiltersButton");

    let currentPage = 1;
    let totalPages = 1;

    async function loadLeads(page = 1) {
        currentPage = page;
        const query = leadFiltersToQuery(filtersForm, page);
        const response = await apiFetch(`/api/leads?${query.toString()}`);
        if (!response) return;

        renderLeadsTable(response.items || []);
        summaryElement.textContent = `${response.total} lead(s) encontrados`;
        paginationLabel.textContent = `Página ${response.page} de ${response.total_pages}`;
        totalPages = response.total_pages || 1;
        prevPageButton.disabled = response.page <= 1;
        nextPageButton.disabled = response.page >= totalPages;
    }

    filtersForm?.addEventListener("submit", async (event) => {
        event.preventDefault();
        try {
            await loadLeads(1);
        } catch (error) {
            showToast(error.message, true);
        }
    });

    clearFiltersButton?.addEventListener("click", async () => {
        filtersForm.reset();
        try {
            await loadLeads(1);
        } catch (error) {
            showToast(error.message, true);
        }
    });

    prevPageButton?.addEventListener("click", async () => {
        if (currentPage <= 1) return;
        try {
            await loadLeads(currentPage - 1);
        } catch (error) {
            showToast(error.message, true);
        }
    });

    nextPageButton?.addEventListener("click", async () => {
        if (currentPage >= totalPages) return;
        try {
            await loadLeads(currentPage + 1);
        } catch (error) {
            showToast(error.message, true);
        }
    });

    importButton?.addEventListener("click", () => importInput?.click());
    importInput?.addEventListener("change", async () => {
        const file = importInput.files?.[0];
        if (!file) return;
        const formData = new FormData();
        formData.append("file", file);

        try {
            const result = await apiFetch("/api/leads/import", {
                method: "POST",
                body: formData,
                headers: {},
            });
            showToast(`Importação concluída. ${result.imported} lead(s) importados.`);
            importInput.value = "";
            await loadLeads(1);
        } catch (error) {
            showToast(error.message, true);
        }
    });

    function exportFile(fileFormat) {
        const query = leadFiltersToQuery(filtersForm, 1);
        query.delete("page");
        query.set("file_format", fileFormat);
        window.location.href = `/api/leads/export?${query.toString()}`;
    }

    exportCsvButton?.addEventListener("click", () => exportFile("csv"));
    exportXlsxButton?.addEventListener("click", () => exportFile("xlsx"));

    await loadLeads(1);
}

function fillLeadForm(form, lead) {
    form.full_name.value = lead.full_name;
    form.email.value = lead.email;
    form.phone.value = lead.phone;
    form.company.value = lead.company;
    form.role.value = lead.role;
    form.source.value = lead.source;
    form.status.value = lead.status;
    form.captured_at.value = lead.captured_at;
}

async function initLeadFormPage() {
    await ensureAuthenticated();
    const form = document.getElementById("leadForm");
    const title = document.getElementById("leadFormTitle");
    const isEdit = Boolean(LEAD_ID);

    if (isEdit) {
        title.textContent = "Editar lead";
        const lead = await apiFetch(`/api/leads/${LEAD_ID}`);
        fillLeadForm(form, lead);
    } else {
        form.captured_at.value = new Date().toISOString().slice(0, 10);
    }

    form?.addEventListener("submit", async (event) => {
        event.preventDefault();
        const payload = Object.fromEntries(new FormData(form).entries());

        try {
            const response = await apiFetch(isEdit ? `/api/leads/${LEAD_ID}` : "/api/leads", {
                method: isEdit ? "PUT" : "POST",
                body: JSON.stringify(payload),
            });
            showToast("Lead salvo com sucesso.");
            window.location.href = `/leads/${response.id}`;
        } catch (error) {
            showToast(error.message, true);
        }
    });
}

function renderLeadDetail(lead) {
    setText("leadNameHeading", lead.full_name);
    const editLink = document.getElementById("editLeadLink");
    if (editLink) editLink.href = `/leads/${lead.id}/edit`;

    const detailList = document.getElementById("leadDetailList");
    if (!detailList) return;

    const fields = [
        ["E-mail", lead.email],
        ["Telefone", lead.phone],
        ["Empresa", lead.company],
        ["Cargo", lead.role],
        ["Fonte", lead.source],
        ["Status", formatStatus(lead.status)],
        ["Data de captura", formatDate(lead.captured_at)],
        ["Criado em", new Date(lead.created_at).toLocaleString("pt-BR")],
        ["Atualizado em", new Date(lead.updated_at).toLocaleString("pt-BR")],
    ];

    detailList.innerHTML = fields
        .map(([label, value]) => `<dt>${label}</dt><dd>${value}</dd>`)
        .join("");
}

function renderInteractions(items) {
    const container = document.getElementById("interactionsList");
    if (!container) return;

    if (!items.length) {
        container.innerHTML = '<div class="empty-state">Nenhuma interação registrada para este lead.</div>';
        return;
    }

    container.innerHTML = items
        .map(
            (item) => `
                <article class="timeline-item">
                    <strong>${item.interaction_type}</strong>
                    <p>${item.note}</p>
                    <small>${item.created_by_name} em ${new Date(item.created_at).toLocaleString("pt-BR")}</small>
                </article>
            `
        )
        .join("");
}

async function initLeadDetailPage() {
    await ensureAuthenticated();

    async function loadLead() {
        const lead = await apiFetch(`/api/leads/${LEAD_ID}`);
        if (!lead) return;
        renderLeadDetail(lead);
        renderInteractions(lead.interactions || []);
    }

    const interactionForm = document.getElementById("interactionForm");
    interactionForm?.addEventListener("submit", async (event) => {
        event.preventDefault();
        const payload = Object.fromEntries(new FormData(interactionForm).entries());
        try {
            await apiFetch(`/api/leads/${LEAD_ID}/interactions`, {
                method: "POST",
                body: JSON.stringify(payload),
            });
            interactionForm.reset();
            interactionForm.interaction_type.value = "observacao";
            showToast("Interação registrada com sucesso.");
            await loadLead();
        } catch (error) {
            showToast(error.message, true);
        }
    });

    const deleteButton = document.getElementById("deleteLeadButton");
    deleteButton?.addEventListener("click", async () => {
        const confirmed = window.confirm("Deseja realmente excluir este lead?");
        if (!confirmed) return;

        try {
            await apiFetch(`/api/leads/${LEAD_ID}`, { method: "DELETE" });
            showToast("Lead excluído com sucesso.");
            window.location.href = "/leads";
        } catch (error) {
            showToast(error.message, true);
        }
    });

    await loadLead();
}

async function initGlobalActions() {
    const logoutButton = document.getElementById("logoutButton");
    logoutButton?.addEventListener("click", async () => {
        try {
            await apiFetch("/api/auth/logout", { method: "POST" });
        } catch (error) {
            // Mesmo com erro seguimos limpando a sessão do frontend.
        }
        window.location.href = "/";
    });
}

async function boot() {
    try {
        await initGlobalActions();
        if (PAGE === "auth") await initAuthPage();
        if (PAGE === "dashboard") await initDashboardPage();
        if (PAGE === "leads") await initLeadsPage();
        if (PAGE === "lead-form") await initLeadFormPage();
        if (PAGE === "lead-detail") await initLeadDetailPage();
    } catch (error) {
        showToast(error.message || "Erro inesperado.", true);
    }
}

document.addEventListener("DOMContentLoaded", boot);
