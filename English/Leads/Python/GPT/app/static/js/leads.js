const leadState = {
    page: 1,
    pageSize: 10,
    totalPages: 1,
};

function buildQueryString(formatOverride) {
    const form = document.getElementById("filtersForm");
    const params = new URLSearchParams();
    const data = new FormData(form);
    data.forEach((value, key) => {
        if (value) params.set(key, value);
    });
    params.set("page", String(leadState.page));
    params.set("page_size", String(leadState.pageSize));
    if (formatOverride) {
        params.set("format", formatOverride);
    }
    return params.toString();
}

function renderLeads(items) {
    const tbody = document.getElementById("leadsTableBody");
    tbody.innerHTML = items.length
        ? items.map((lead) => `
            <tr>
                <td>
                    <strong>${lead.full_name}</strong><br>
                    <span>${lead.email}</span>
                </td>
                <td>${lead.company || "—"}</td>
                <td><span class="status-pill status-${lead.status}">${lead.status}</span></td>
                <td>${lead.source}</td>
                <td>${formatDate(lead.capture_date)}</td>
                <td>
                    <div class="table-actions">
                        <a class="ghost-button" href="/leads/${lead.id}">View</a>
                        <a class="ghost-button" href="/leads/${lead.id}/edit">Edit</a>
                    </div>
                </td>
            </tr>
        `).join("")
        : `<tr><td colspan="6">No leads found for the current filters.</td></tr>`;
}

async function loadLeads() {
    const response = await apiFetch(`/api/leads?${buildQueryString()}`);
    const data = await response.json();

    leadState.totalPages = data.total_pages;
    renderLeads(data.items);
    document.getElementById("pageIndicator").textContent = `Page ${data.page} of ${data.total_pages}`;
    document.getElementById("resultsSummary").textContent = `${data.total} lead(s) found`;
    document.getElementById("prevPageButton").disabled = data.page <= 1;
    document.getElementById("nextPageButton").disabled = data.page >= data.total_pages;
}

document.getElementById("filtersForm")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    leadState.page = 1;
    await loadLeads();
});

document.getElementById("prevPageButton")?.addEventListener("click", async () => {
    if (leadState.page > 1) {
        leadState.page -= 1;
        await loadLeads();
    }
});

document.getElementById("nextPageButton")?.addEventListener("click", async () => {
    if (leadState.page < leadState.totalPages) {
        leadState.page += 1;
        await loadLeads();
    }
});

document.getElementById("importInput")?.addEventListener("change", async (event) => {
    const file = event.target.files[0];
    if (!file) return;

    const formData = new FormData();
    formData.append("upload", file);

    setMessage("importMessage", "Importing leads...");
    const response = await fetch("/api/leads/import", {
        method: "POST",
        headers: { Authorization: `Bearer ${Auth.getToken()}` },
        body: formData,
    });
    const data = await response.json();

    if (!response.ok) {
        setMessage("importMessage", data.detail || "Import failed.", true);
        return;
    }

    const errorSuffix = data.failed ? ` ${data.failed} row(s) failed.` : "";
    setMessage("importMessage", `${data.created} lead(s) imported.${errorSuffix}`);
    event.target.value = "";
    await loadLeads();
});

async function triggerExport(formatName) {
    const response = await fetch(`/api/leads/export?${buildQueryString(formatName)}`, {
        headers: { Authorization: `Bearer ${Auth.getToken()}` },
    });

    if (!response.ok) {
        setMessage("importMessage", "Export failed.", true);
        return;
    }

    const blob = await response.blob();
    const downloadUrl = window.URL.createObjectURL(blob);
    const link = document.createElement("a");
    const disposition = response.headers.get("Content-Disposition") || "";
    const match = disposition.match(/filename="(.+)"/);
    link.href = downloadUrl;
    link.download = match ? match[1] : `leads_export.${formatName}`;
    document.body.appendChild(link);
    link.click();
    link.remove();
    window.URL.revokeObjectURL(downloadUrl);
}

document.getElementById("exportCsvButton")?.addEventListener("click", () => triggerExport("csv"));
document.getElementById("exportXlsxButton")?.addEventListener("click", () => triggerExport("xlsx"));

loadLeads().catch((error) => console.error(error));
