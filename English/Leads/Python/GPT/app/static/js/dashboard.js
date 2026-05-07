async function loadDashboard() {
    const response = await apiFetch("/api/dashboard/summary");
    const data = await response.json();

    const metricsGrid = document.getElementById("metricsGrid");
    metricsGrid.innerHTML = `
        <article class="metric-card">
            <span>Total leads</span>
            <strong>${data.totals.all_leads}</strong>
        </article>
        <article class="metric-card">
            <span>New leads this week</span>
            <strong>${data.totals.recent_leads}</strong>
        </article>
        <article class="metric-card">
            <span>Qualified rate</span>
            <strong>${data.totals.qualified_rate}%</strong>
        </article>
    `;

    new Chart(document.getElementById("capturesChart"), {
        type: "line",
        data: {
            labels: data.captures_over_time.map((item) => item.date),
            datasets: [{
                label: "Captures",
                data: data.captures_over_time.map((item) => item.count),
                borderColor: "#0c7c86",
                backgroundColor: "rgba(12, 124, 134, 0.18)",
                fill: true,
                tension: 0.35,
            }],
        },
        options: {
            plugins: { legend: { display: false } },
        },
    });

    new Chart(document.getElementById("statusChart"), {
        type: "doughnut",
        data: {
            labels: Object.keys(data.by_status),
            datasets: [{
                data: Object.values(data.by_status),
                backgroundColor: ["#0c7c86", "#f6c36f", "#2c7c55", "#b44949"],
            }],
        },
    });

    document.getElementById("topSources").innerHTML = data.top_sources.length
        ? data.top_sources
            .map((item) => `<div class="source-row"><span>${item.source}</span><strong>${item.count}</strong></div>`)
            .join("")
        : "<p>No sources captured yet.</p>";
}

loadDashboard().catch((error) => console.error(error));
