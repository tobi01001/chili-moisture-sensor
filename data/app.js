"use strict";
const $ = id => document.getElementById(id);
const form = $("settings");

async function api(url, opts) {
  const r = await fetch(url, opts);
  let body = {};
  try { body = await r.json(); } catch (e) { /* no JSON body */ }
  if (!r.ok) throw new Error(body.error || "HTTP " + r.status);
  return body;
}

function msg(text, isError) {
  const m = $("msg");
  m.textContent = text;
  m.className = isError ? "err" : "";
}

async function action(fn) {
  try { await fn(); } catch (e) { msg(e.message, true); }
}

// ---- live values: polled, so they update by themselves --------------------------------------

function fmtLeft(s) {
  if (s < 0) return "(no limit)";
  return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0");
}

function render(s) {
  $("dev").textContent = "Device: " + s.dev;
  $("fw").textContent = s.fw;
  $("cal").textContent = s.cal;
  $("left").textContent = fmtLeft(s.left);
  if (s.have) {
    $("moist").textContent = s.moisture;
    $("raw").textContent = s.raw;
    $("supply").textContent = s.supply + " mV (" + (s.usb ? "USB" : "battery") + ")";
    $("age").textContent = (s.age / 1000).toFixed(1) + " s";
  }
}

async function pollState() {
  try {
    render(await api("/api/state"));
    $("conn").hidden = true;
  } catch (e) {
    $("conn").hidden = false;
  }
}

setInterval(() => { if (!document.hidden) pollState(); }, 1500);
document.addEventListener("visibilitychange", () => { if (!document.hidden) pollState(); });

// ---- settings --------------------------------------------------------------------------------

async function loadSettings(only) {
  const s = await api("/api/settings");
  for (const k in s) {
    if (only && !only.includes(k)) continue;
    if (form.elements[k]) form.elements[k].value = s[k];
  }
}

form.addEventListener("submit", e => {
  e.preventDefault();
  action(async () => {
    await api("/save", { method: "POST", body: new URLSearchParams(new FormData(form)) });
    msg("Saved. Restarting in measuring mode...");
  });
});

$("reboot").onclick = () => action(async () => {
  await api("/api/reboot", { method: "POST" });
  msg("Restarting in measuring mode...");
});

// ---- calibration -----------------------------------------------------------------------------

async function calibrate(which) {
  await api("/api/cal?p=" + which, { method: "POST" });
  await loadSettings(["dry", "wet"]);
  msg("Saved as " + which.toUpperCase() + ": " + form.elements[which].value + " mV");
}
$("calDry").onclick = () => action(() => calibrate("dry"));
$("calWet").onclick = () => action(() => calibrate("wet"));

$("supplyForm").addEventListener("submit", e => {
  e.preventDefault();
  action(async () => {
    const mv = e.target.elements.mv.value;
    await api("/api/calsupply?mv=" + encodeURIComponent(mv), { method: "POST" });
    msg("Supply voltage calibrated");
    pollState();
  });
});

// ---- settling curve --------------------------------------------------------------------------

const STATE_TEXT = ["", "Waiting for the measuring task...", "Recording...", "Done"];
let curve = null;
let suggested = null;

function analyse(mv, step, tol) {
  const n = mv.length, k = Math.min(10, n);
  let fin = 0;
  for (let i = n - k; i < n; i++) fin += mv[i];
  fin /= k;
  let last = -1;
  for (let i = 0; i < n; i++) if (Math.abs(mv[i] - fin) > tol) last = i;
  return { fin, settled: last < n - 1, ms: (last + 1) * step };
}

function drawChart(mv, step, wanted, suggestMs) {
  const W = 400, H = 170, L = 34, R = 8, T = 8, B = 20;
  const lo = Math.min(...mv), hi = Math.max(...mv);
  const span = Math.max(hi - lo, 10);
  const tMax = Math.max((wanted - 1) * step, 1);
  const x = t => L + (W - L - R) * t / tMax;
  const y = v => T + (H - T - B) * (1 - (v - lo) / span);
  let s = `<text x="2" y="${T + 6}">${hi}</text><text x="2" y="${H - B}">${lo}</text>` +
          `<text x="${L}" y="${H - 6}">0</text><text x="${W - R - 30}" y="${H - 6}">${tMax} ms</text>` +
          `<line x1="${L}" y1="${H - B}" x2="${W - R}" y2="${H - B}" stroke="#bbb"/>`;
  if (suggestMs != null && suggestMs <= tMax)
    s += `<line x1="${x(suggestMs)}" y1="${T}" x2="${x(suggestMs)}" y2="${H - B}" stroke="#c62828" stroke-dasharray="3"/>`;
  const pts = mv.map((v, i) => x(i * step).toFixed(1) + "," + y(v).toFixed(1)).join(" ");
  s += `<polyline points="${pts}" fill="none" stroke="#2e7d32" stroke-width="1.5"/>`;
  $("chart").innerHTML = s;
  $("chart").hidden = false;
}

function showCurve(c) {
  curve = c;
  $("curveState").textContent = STATE_TEXT[c.state] || "";
  if (c.mv.length < 2) return;
  const tol = Math.max(1, parseInt($("tol").value, 10) || 10);
  const a = analyse(c.mv, c.step, tol);
  suggested = null;
  if (c.state === 3) {
    $("curveResult").hidden = false;
    $("csv").textContent = c.mv.map((v, i) => i * c.step + ";" + v).join("\n");
    if (a.settled) {
      suggested = Math.min(5000, Math.max(100, Math.ceil(a.ms / 50) * 50));
      $("suggest").textContent = "Final value about " + a.fin.toFixed(0) + " mV. Output stays within \u00b1" +
        tol + " mV from " + a.ms + " ms on.";
    } else {
      $("suggest").textContent = "Not settled within \u00b1" + tol + " mV during the recording. Record longer or increase the tolerance.";
    }
    $("useSettle").disabled = suggested == null;
  }
  drawChart(c.mv, c.step, c.wanted, c.state === 3 && a.settled ? a.ms : null);
}

let curveTimer = null;
async function pollCurve() {
  clearTimeout(curveTimer);
  try {
    const c = await api("/api/settle");
    showCurve(c);
    $("curveStart").disabled = c.state === 1 || c.state === 2;
    if (c.state === 1 || c.state === 2) curveTimer = setTimeout(pollCurve, 300);
  } catch (e) {
    curveTimer = setTimeout(pollCurve, 1000);
  }
}

$("curveStart").onclick = () => action(async () => {
  $("curveResult").hidden = true;
  await api("/api/settle?ms=" + $("curveDur").value, { method: "POST" });
  $("curveStart").disabled = true;
  pollCurve();
});
$("tol").addEventListener("change", () => { if (curve) showCurve(curve); });
$("useSettle").onclick = () => {
  if (suggested == null) return;
  form.elements.settle.value = suggested;
  msg("Settle time set to " + suggested + " ms in the form below. Save it, then recalibrate dry/wet.");
};

// ---- start -----------------------------------------------------------------------------------

pollState();
action(loadSettings);
pollCurve();
