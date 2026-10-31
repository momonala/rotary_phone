const toast = document.querySelector(".toast");
const TOAST_HOLD_MS = 2200;
const TOAST_NEXT_MS = 900;
const POLL_MS = 1500;
const toastQueue = [];
let toastTimer = null;
let lastEventId = null;

function showNextToast() {
  const item = toastQueue.shift();
  if (!item) {
    toast.classList.remove("is-open");
    toastTimer = null;
    return;
  }
  toast.textContent = item.message;
  toast.classList.toggle("error", item.isError);
  toast.classList.add("is-open");
  toastTimer = setTimeout(showNextToast, toastQueue.length ? TOAST_NEXT_MS : TOAST_HOLD_MS);
}

function showToast(message, isError) {
  toastQueue.push({ message, isError });
  if (toastTimer === null) {
    showNextToast();
  } else {
    clearTimeout(toastTimer);
    toastTimer = setTimeout(showNextToast, TOAST_NEXT_MS);
  }
}

async function pollPhoneEvents() {
  if (document.hidden) return;
  try {
    const query = lastEventId === null ? "" : `?after=${lastEventId}`;
    const response = await fetch(`/api/events${query}`);
    if (!response.ok) return;
    const data = await response.json();
    if (lastEventId !== null && data.last_id < lastEventId) {
      lastEventId = 0;
      return;
    }
    if (lastEventId !== null) data.events.forEach((event) => showToast(event.text, false));
    lastEventId = data.last_id;
  } catch {
  }
}

async function runAction(button) {
  button.disabled = true;
  try {
    const response = await fetch(`/api/actions/${button.dataset.action}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: "{}",
    });
    const result = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(result.error || `Request failed (${response.status})`);
    showToast(`Sent: ${result.label}`, false);
    navigator.vibrate?.(20);
  } catch (error) {
    showToast(error.message || "Network error", true);
  } finally {
    button.disabled = false;
  }
}

document.querySelectorAll("button[data-action]").forEach((button) => {
  button.addEventListener("click", () => runAction(button));
});

pollPhoneEvents();
setInterval(pollPhoneEvents, POLL_MS);
