const Auth = {
    tokenKey: "lead_manager_token",
    userKey: "lead_manager_user",

    getToken() {
        return localStorage.getItem(this.tokenKey);
    },

    setSession(token, user) {
        localStorage.setItem(this.tokenKey, token);
        localStorage.setItem(this.userKey, JSON.stringify(user));
    },

    clearSession() {
        localStorage.removeItem(this.tokenKey);
        localStorage.removeItem(this.userKey);
    },

    getUser() {
        const raw = localStorage.getItem(this.userKey);
        return raw ? JSON.parse(raw) : null;
    },

    ensureAuthenticated() {
        const publicPages = ["/login", "/register"];
        if (!publicPages.includes(window.location.pathname) && !this.getToken()) {
            window.location.href = "/login";
        }
        if (publicPages.includes(window.location.pathname) && this.getToken()) {
            window.location.href = "/dashboard";
        }
    },
};

async function apiFetch(path, options = {}) {
    const headers = {
        "Content-Type": "application/json",
        ...(options.headers || {}),
    };
    const token = Auth.getToken();
    if (token) {
        headers.Authorization = `Bearer ${token}`;
    }

    const response = await fetch(path, { ...options, headers });
    if (response.status === 401) {
        Auth.clearSession();
        window.location.href = "/login";
        throw new Error("Authentication expired.");
    }
    return response;
}

function setMessage(elementId, message, isError = false) {
    const element = document.getElementById(elementId);
    if (!element) return;
    element.textContent = message;
    element.style.color = isError ? "#b44949" : "#5f7276";
}

function formatDate(value) {
    if (!value) return "—";
    if (typeof value === "string" && !value.includes("T")) {
        const [year, month, day] = value.split("-");
        return `${day}/${month}/${year}`;
    }
    return new Date(value).toLocaleDateString();
}

function formatDateTime(value) {
    if (!value) return "—";
    return new Date(value).toLocaleString();
}

function populateCurrentUser() {
    const user = Auth.getUser();
    const chip = document.getElementById("currentUserName");
    if (chip && user) {
        chip.textContent = user.name;
    }
}

function wireLogout() {
    const button = document.getElementById("logoutButton");
    if (!button) return;
    button.addEventListener("click", () => {
        Auth.clearSession();
        window.location.href = "/login";
    });
}

Auth.ensureAuthenticated();
populateCurrentUser();
wireLogout();
