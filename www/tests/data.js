document.documentElement.dataset.webservLoaded = "true";

const banner = document.createElement("p");
banner.className = "client-banner";
banner.textContent = "Client-side script loaded from /tests/data.js";
document.addEventListener("DOMContentLoaded", () => {
  document.body.appendChild(banner);
});
