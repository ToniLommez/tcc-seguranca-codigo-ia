const formMode = window.PAGE_DATA.mode;
const leadId = window.PAGE_DATA.lead_id;

function toInputDate(dateValue) {
    return String(dateValue).slice(0, 10);
}

async function loadLeadForEdit() {
    if (formMode !== "edit" || !leadId) {
        document.getElementById("formTitle").textContent = "Create lead";
        document.querySelector('input[name="capture_date"]').value = new Date().toISOString().split("T")[0];
        return;
    }

    document.getElementById("formTitle").textContent = "Edit lead";
    const response = await apiFetch(`/api/leads/${leadId}`);
    const data = await response.json();
    const lead = data.lead;

    Object.entries(lead).forEach(([key, value]) => {
        const field = document.querySelector(`[name="${key}"]`);
        if (!field || value === null || value === undefined) return;
        field.value = key === "capture_date" ? toInputDate(value) : value;
    });
}

document.getElementById("leadForm")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    const payload = Object.fromEntries(form.entries());

    const url = formMode === "edit" ? `/api/leads/${leadId}` : "/api/leads";
    const method = formMode === "edit" ? "PUT" : "POST";
    setMessage("leadFormMessage", "Saving lead...");

    try {
        const response = await apiFetch(url, {
            method,
            body: JSON.stringify(payload),
        });
        const data = await response.json();
        if (!response.ok) {
            throw new Error(data.detail || "Unable to save lead.");
        }
        window.location.href = `/leads/${data.id || leadId}`;
    } catch (error) {
        setMessage("leadFormMessage", error.message, true);
    }
});

loadLeadForEdit().catch((error) => console.error(error));
