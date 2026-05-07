document.getElementById("loginForm")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    setMessage("loginMessage", "Signing in...");

    const form = new FormData(event.currentTarget);
    const payload = Object.fromEntries(form.entries());

    try {
        const response = await apiFetch("/api/auth/login", {
            method: "POST",
            body: JSON.stringify(payload),
        });
        const data = await response.json();
        if (!response.ok) {
            throw new Error(data.detail || "Login failed.");
        }
        Auth.setSession(data.access_token, data.user);
        window.location.href = "/dashboard";
    } catch (error) {
        setMessage("loginMessage", error.message, true);
    }
});
