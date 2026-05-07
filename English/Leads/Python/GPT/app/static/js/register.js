document.getElementById("registerForm")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    setMessage("registerMessage", "Creating account...");

    const form = new FormData(event.currentTarget);
    const payload = Object.fromEntries(form.entries());

    try {
        const response = await apiFetch("/api/auth/register", {
            method: "POST",
            body: JSON.stringify(payload),
        });
        const data = await response.json();
        if (!response.ok) {
            throw new Error(data.detail || "Registration failed.");
        }
        Auth.setSession(data.access_token, data.user);
        window.location.href = "/dashboard";
    } catch (error) {
        setMessage("registerMessage", error.message, true);
    }
});
