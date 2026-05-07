const detailLeadId = window.PAGE_DATA.lead_id;

function renderSnapshot(lead) {
    document.getElementById("leadNameHeading").textContent = lead.full_name;
    document.getElementById("editLeadLink").href = `/leads/${lead.id}/edit`;
    document.getElementById("leadSnapshot").innerHTML = `
        <dt>Email</dt><dd>${lead.email}</dd>
        <dt>Phone</dt><dd>${lead.phone || "—"}</dd>
        <dt>Company</dt><dd>${lead.company || "—"}</dd>
        <dt>Position</dt><dd>${lead.position || "—"}</dd>
        <dt>Source</dt><dd>${lead.source}</dd>
        <dt>Status</dt><dd><span class="status-pill status-${lead.status}">${lead.status}</span></dd>
        <dt>Capture date</dt><dd>${formatDate(lead.capture_date)}</dd>
        <dt>Interactions</dt><dd>${lead.interaction_count}</dd>
    `;
}

function renderTimeline(interactions) {
    const timeline = document.getElementById("interactionTimeline");
    timeline.innerHTML = interactions.length
        ? interactions.map((item) => `
            <article class="timeline-item">
                <header>
                    <strong>${item.summary}</strong>
                    <span>${formatDateTime(item.happened_at)}</span>
                </header>
                <p><strong>${item.interaction_type}</strong></p>
                <p>${item.notes || "No additional notes."}</p>
            </article>
        `).join("")
        : "<p>No interactions logged yet.</p>";
}

async function loadLeadDetail() {
    const response = await apiFetch(`/api/leads/${detailLeadId}`);
    const data = await response.json();
    renderSnapshot(data.lead);
    renderTimeline(data.interactions);
}

document.getElementById("interactionForm")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    const payload = Object.fromEntries(form.entries());
    payload.happened_at = new Date(payload.happened_at).toISOString();

    setMessage("interactionMessage", "Saving interaction...");
    const response = await apiFetch(`/api/leads/${detailLeadId}/interactions`, {
        method: "POST",
        body: JSON.stringify(payload),
    });
    const data = await response.json();
    if (!response.ok) {
        setMessage("interactionMessage", data.detail || "Unable to save interaction.", true);
        return;
    }

    event.currentTarget.reset();
    setMessage("interactionMessage", "Interaction saved.");
    await loadLeadDetail();
});

document.getElementById("deleteLeadButton")?.addEventListener("click", async () => {
    const confirmed = window.confirm("Delete this lead and its interaction history?");
    if (!confirmed) return;

    const response = await apiFetch(`/api/leads/${detailLeadId}`, { method: "DELETE" });
    if (response.ok) {
        window.location.href = "/leads";
    }
});

document.querySelector('input[name="happened_at"]')?.setAttribute(
    "value",
    new Date().toISOString().slice(0, 16),
);

loadLeadDetail().catch((error) => console.error(error));
