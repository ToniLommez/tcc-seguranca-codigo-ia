const jsonHeaders = {
  "Content-Type": "application/json"
};

function setMessage(container, message, type = "") {
  if (!container) return;
  container.textContent = message || "";
  container.className = "form-message";
  if (type) container.classList.add(type);
}

async function request(url, options = {}) {
  const response = await fetch(url, {
    credentials: "same-origin",
    ...options
  });

  const contentType = response.headers.get("content-type") || "";
  const data = contentType.includes("application/json") ? await response.json() : null;

  if (!response.ok) {
    const message = data?.error || "Não foi possível concluir a operação.";
    throw new Error(message);
  }

  return data;
}

document.querySelectorAll("[data-auth-form]").forEach((form) => {
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const mode = form.dataset.authForm;
    const message = form.querySelector("[data-form-message]");
    const formData = new FormData(form);

    const payload = Object.fromEntries(formData.entries());
    const endpoint = mode === "register" ? "/api/auth/register" : "/api/auth/login";

    try {
      setMessage(message, "Processando...");
      await request(endpoint, {
        method: "POST",
        headers: jsonHeaders,
        body: JSON.stringify(payload)
      });
      window.location.href = "/app/dashboard";
    } catch (error) {
      setMessage(message, error.message, "error");
    }
  });
});

document.querySelectorAll("[data-logout]").forEach((button) => {
  button.addEventListener("click", async () => {
    await request("/api/auth/logout", { method: "POST" });
    window.location.href = "/login";
  });
});

document.querySelectorAll("[data-lead-form]").forEach((form) => {
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const mode = form.dataset.leadForm;
    const leadId = form.dataset.leadId;
    const message = form.querySelector("[data-form-message]");
    const payload = Object.fromEntries(new FormData(form).entries());
    const url = mode === "edit" ? `/api/leads/${leadId}` : "/api/leads";
    const method = mode === "edit" ? "PUT" : "POST";

    try {
      setMessage(message, "Salvando lead...");
      const data = await request(url, {
        method,
        headers: jsonHeaders,
        body: JSON.stringify(payload)
      });
      setMessage(message, "Lead salvo com sucesso.", "success");
      window.location.href = `/app/leads/${data.id}`;
    } catch (error) {
      setMessage(message, error.message, "error");
    }
  });
});

document.querySelectorAll("[data-delete-lead]").forEach((button) => {
  button.addEventListener("click", async () => {
    const leadId = button.dataset.deleteLead;
    if (!window.confirm("Deseja realmente excluir este lead?")) return;

    try {
      await request(`/api/leads/${leadId}`, { method: "DELETE" });
      window.location.href = "/app/leads";
    } catch (error) {
      window.alert(error.message);
    }
  });
});

document.querySelectorAll("[data-interaction-form]").forEach((form) => {
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const leadId = form.dataset.leadId;
    const message = form.querySelector("[data-form-message]");
    const payload = Object.fromEntries(new FormData(form).entries());

    try {
      setMessage(message, "Registrando interação...");
      await request(`/api/leads/${leadId}/interactions`, {
        method: "POST",
        headers: jsonHeaders,
        body: JSON.stringify(payload)
      });
      window.location.reload();
    } catch (error) {
      setMessage(message, error.message, "error");
    }
  });
});

document.querySelectorAll("[data-delete-interaction]").forEach((button) => {
  button.addEventListener("click", async () => {
    const leadId = button.dataset.leadId;
    const interactionId = button.dataset.deleteInteraction;
    if (!window.confirm("Deseja excluir esta interação?")) return;

    try {
      await request(`/api/leads/${leadId}/interactions/${interactionId}`, { method: "DELETE" });
      window.location.reload();
    } catch (error) {
      window.alert(error.message);
    }
  });
});

document.querySelectorAll("[data-import-form]").forEach((form) => {
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const message = form.closest(".panel")?.querySelector("[data-form-message]");
    const body = new FormData(form);

    try {
      setMessage(message, "Importando arquivo...");
      const result = await request("/api/leads/import", {
        method: "POST",
        body
      });
      const summary = `Importados: ${result.imported} | Atualizados: ${result.updated} | Ignorados: ${result.skipped}`;
      setMessage(message, summary, "success");
      window.setTimeout(() => window.location.reload(), 900);
    } catch (error) {
      setMessage(message, error.message, "error");
    }
  });
});
