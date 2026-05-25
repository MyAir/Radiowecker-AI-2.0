// Radiowecker Web UI — vanilla JS (no framework).
"use strict";

const $  = (s, r=document) => r.querySelector(s);
const $$ = (s, r=document) => Array.from(r.querySelectorAll(s));

// ----- API helpers -----
async function api(path, opts={}) {
  const r = await fetch(path, { headers: { "Content-Type": "application/json" }, ...opts });
  const txt = await r.text();
  let json; try { json = txt ? JSON.parse(txt) : {}; } catch { json = { raw: txt }; }
  if (!r.ok) throw new Error(json.error || ("HTTP " + r.status));
  return json;
}
const apiGet  = (p)         => api(p);
const apiPost = (p, body)   => api(p, { method: "POST",   body: JSON.stringify(body || {}) });
const apiPut  = (p, body)   => api(p, { method: "PUT",    body: JSON.stringify(body || {}) });
const apiDel  = (p)         => api(p, { method: "DELETE" });

// ----- Tabs -----
$$(".tab").forEach(t => t.addEventListener("click", () => {
  $$(".tab").forEach(x => x.classList.remove("active"));
  $$(".tab-pane").forEach(x => x.classList.remove("active"));
  t.classList.add("active");
  $("#tab-" + t.dataset.tab).classList.add("active");
  if (t.dataset.tab === "system") refreshSystem();
}));

// ----- Status header (polled) -----
async function refreshStatus() {
  try {
    const s = await apiGet("/api/status");
    $("#st-time").textContent = s.timeISO ? s.timeISO.substring(11, 19) : "--:--:--";
    $("#st-next").textContent = "Nächster: " + (s.alarms?.nextLabel || "---");
    $("#st-ip").textContent   = s.ip || "---";
    $("#st-wifi").textContent = s.ssid ? (s.ssid + " " + s.quality + "%") : "kein WLAN";
    // Reflect master switch state without stomping mid-toggle.
    const cb = $("#masterEnabled");
    if (cb && document.activeElement !== cb) cb.checked = !!s.alarms?.masterEnabled;
  } catch (e) { /* offline → next tick */ }
}

// ----- Weekday helpers -----
const WDAY_NAMES = ["So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"];
function wdaysToStr(mask) {
  if (mask === 0) return "einmalig";
  if (mask === 0x7F) return "täglich";
  if (mask === 0x3E) return "Mo–Fr";
  if (mask === 0x41) return "Sa+So";
  const out = [];
  for (let i = 0; i < 7; i++) if (mask & (1 << i)) out.push(WDAY_NAMES[i]);
  return out.join(", ");
}

// ----- Alarms -----
let _stationsCache = [];

async function refreshAlarms() {
  const data = await apiGet("/api/alarms");
  $("#masterEnabled").checked = !!data.masterEnabled;
  const tb = $("#alarms-body");
  tb.innerHTML = "";
  for (const a of data.alarms) {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td><input type="checkbox" ${a.enabled ? "checked" : ""}></td>
      <td><b>${String(a.hour).padStart(2,"0")}:${String(a.minute).padStart(2,"0")}</b></td>
      <td>${escapeHtml(a.title || "")}</td>
      <td>${wdaysToStr(a.weekdays)}</td>
      <td>${a.soundType === "sd" ? "SD: " + escapeHtml(a.soundPath || "") : "Stream"}</td>
      <td>${a.volume}</td>
      <td><button data-act="edit">Bearbeiten</button>
          <button data-act="del" class="danger">Löschen</button></td>`;
    tr.querySelector('input[type=checkbox]').addEventListener("change", async e => {
      await apiPut(`/api/alarms/${a.id}`, { enabled: e.target.checked });
    });
    tr.querySelector('[data-act=edit]').addEventListener("click", () => openAlarmModal(a));
    tr.querySelector('[data-act=del]').addEventListener("click", async () => {
      if (!confirm(`Wecker "${a.title}" löschen?`)) return;
      await apiDel(`/api/alarms/${a.id}`);
      await refreshAlarms();
    });
    tb.appendChild(tr);
  }
}

$("#masterEnabled").addEventListener("change", async e => {
  await apiPost("/api/alarms/master", { enabled: e.target.checked });
});
$("#btn-skip").addEventListener("click",     () => apiPost("/api/alarms/skip-next"));
$("#btn-unskip").addEventListener("click",   () => apiPost("/api/alarms/unskip"));
$("#btn-add-alarm").addEventListener("click", () => openAlarmModal(null));

function openAlarmModal(a) {
  const f = $("#form-alarm");
  f.reset();
  // Populate station picker from cache.
  const sel = f.elements.streamUrlPick;
  sel.innerHTML = '<option value="">— Eigene URL —</option>' +
    _stationsCache.map(s => `<option value="${escapeAttr(s.url)}">${escapeHtml(s.name)}</option>`).join("");
  if (a) {
    $("#modal-title").textContent = "Wecker bearbeiten";
    f.elements.id.value     = a.id;
    f.elements.title.value  = a.title || "";
    f.elements.hour.value   = a.hour;
    f.elements.minute.value = a.minute;
    f.elements.enabled.checked = !!a.enabled;
    $$('input[name=wd]', f).forEach(cb => cb.checked = (a.weekdays & (1 << +cb.value)) !== 0);
    $$('input[name=soundType]', f).forEach(r => r.checked = (r.value === a.soundType));
    f.elements.streamUrl.value = a.streamUrl || "";
    f.elements.soundPath.value = a.soundPath || "";
    f.elements.volume.value    = a.volume;
    if (a.streamUrl && _stationsCache.some(s => s.url === a.streamUrl)) sel.value = a.streamUrl;
  } else {
    $("#modal-title").textContent = "Neuer Wecker";
    f.elements.id.value = "";
    f.elements.hour.value = 7;
    f.elements.minute.value = 0;
    f.elements.enabled.checked = true;
    f.elements.volume.value = 12;
    f.querySelector('input[name=soundType][value=stream]').checked = true;
  }
  updateSoundTypeVisibility();
  f.elements.volOut.value = f.elements.volume.value;
  $("#modal").classList.remove("hidden");
}

function updateSoundTypeVisibility() {
  const f = $("#form-alarm");
  const st = (f.elements.soundType.value || "stream");
  $("#lbl-stream").classList.toggle("hidden", st !== "stream");
  $("#lbl-sd").classList.toggle("hidden", st !== "sd");
}
$$('input[name=soundType]', $("#form-alarm")).forEach(r =>
  r.addEventListener("change", updateSoundTypeVisibility));
$("#form-alarm").elements.streamUrlPick.addEventListener("change", e => {
  if (e.target.value) $("#form-alarm").elements.streamUrl.value = e.target.value;
});
$("#form-alarm").elements.volume.addEventListener("input", e => {
  $("#form-alarm").elements.volOut.value = e.target.value;
});
$("#btn-cancel").addEventListener("click", () => $("#modal").classList.add("hidden"));
$("#form-alarm").addEventListener("submit", async e => {
  e.preventDefault();
  const f = e.target;
  let mask = 0;
  $$('input[name=wd]', f).forEach(cb => { if (cb.checked) mask |= (1 << +cb.value); });
  const body = {
    title:     f.elements.title.value,
    hour:      +f.elements.hour.value,
    minute:    +f.elements.minute.value,
    weekdays:  mask,
    enabled:   f.elements.enabled.checked,
    soundType: f.elements.soundType.value,
    streamUrl: f.elements.streamUrl.value,
    soundPath: f.elements.soundPath.value,
    volume:    +f.elements.volume.value,
  };
  try {
    if (f.elements.id.value) await apiPut(`/api/alarms/${f.elements.id.value}`, body);
    else                     await apiPost("/api/alarms", body);
    $("#modal").classList.add("hidden");
    await refreshAlarms();
  } catch (err) { alert("Fehler: " + err.message); }
});

// ----- Stations -----
async function refreshStations() {
  const data = await apiGet("/api/stations");
  _stationsCache = data.stations || [];
  const tb = $("#stations-body");
  tb.innerHTML = "";
  _stationsCache.forEach((s, i) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td>${escapeHtml(s.name)}</td>
      <td><small>${escapeHtml(s.url)}</small></td>
      <td>${s.favorite ? "★" : ""}</td>
      <td><button data-act="edit">Bearbeiten</button>
          <button data-act="del" class="danger">Löschen</button></td>`;
    tr.querySelector('[data-act=edit]').addEventListener("click", () => openStationModal(i, s));
    tr.querySelector('[data-act=del]').addEventListener("click", async () => {
      if (!confirm(`Sender "${s.name}" löschen?`)) return;
      await apiDel(`/api/stations/${i}`);
      await refreshStations();
    });
    tb.appendChild(tr);
  });
}
$("#btn-add-station").addEventListener("click", () => openStationModal(-1, null));
$("#btn-cancel-station").addEventListener("click", () => $("#modal-station").classList.add("hidden"));
function openStationModal(index, s) {
  const f = $("#form-station");
  f.reset();
  f.elements.index.value = index;
  if (s) {
    $("#modal-station-title").textContent = "Sender bearbeiten";
    f.elements.name.value = s.name;
    f.elements.url.value  = s.url;
    f.elements.favorite.checked = !!s.favorite;
  } else {
    $("#modal-station-title").textContent = "Neuer Sender";
  }
  $("#modal-station").classList.remove("hidden");
}
$("#form-station").addEventListener("submit", async e => {
  e.preventDefault();
  const f = e.target;
  const body = {
    name:     f.elements.name.value,
    url:      f.elements.url.value,
    favorite: f.elements.favorite.checked,
  };
  try {
    const idx = +f.elements.index.value;
    if (idx >= 0) await apiPut(`/api/stations/${idx}`, body);
    else          await apiPost("/api/stations", body);
    $("#modal-station").classList.add("hidden");
    await refreshStations();
  } catch (err) { alert("Fehler: " + err.message); }
});

// ----- Config -----
async function refreshConfig() {
  const c = await apiGet("/api/config");
  const f = $("#form-config");
  for (const [k, v] of Object.entries(c)) {
    const el = f.elements[k];
    if (!el) continue;
    if (el.type === "checkbox") el.checked = !!v;
    else el.value = v;
  }
}
$("#form-config").addEventListener("submit", async e => {
  e.preventDefault();
  const f = e.target;
  const body = {};
  for (const el of f.elements) {
    if (!el.name) continue;
    if (el.type === "checkbox") body[el.name] = el.checked;
    else if (el.type === "number" || el.type === "range") body[el.name] = +el.value;
    else body[el.name] = el.value;
  }
  const msg = $("#cfg-msg");
  msg.classList.remove("err");
  try { await apiPut("/api/config", body); msg.textContent = "Gespeichert."; await refreshConfig(); }
  catch (err) { msg.textContent = "Fehler: " + err.message; msg.classList.add("err"); }
  setTimeout(() => msg.textContent = "", 3000);
});

// ----- Weather -----
async function refreshWeather() {
  const w = await apiGet("/api/weather");
  const f = $("#form-weather");
  for (const [k, v] of Object.entries(w)) {
    const el = f.elements[k];
    if (el) el.value = v;
  }
  reverseGeocode(+w.lat, +w.lon);
}

function formatGeo(g) {
  // OWM geocoding entry: { name, local_names, lat, lon, country, state? }
  const parts = [g.name];
  if (g.state) parts.push(g.state);
  if (g.country) parts.push(g.country);
  return parts.join(", ");
}

async function reverseGeocode(lat, lon) {
  const cur = $("#geo-current");
  if (!isFinite(lat) || !isFinite(lon) || (lat === 0 && lon === 0)) {
    cur.textContent = "—"; return;
  }
  cur.textContent = lat.toFixed(4) + ", " + lon.toFixed(4) + " …";
  try {
    const res = await apiGet("/api/geocode/reverse?lat=" + lat + "&lon=" + lon + "&limit=1");
    if (Array.isArray(res) && res.length > 0) {
      cur.textContent = formatGeo(res[0])
        + "  (" + lat.toFixed(4) + ", " + lon.toFixed(4) + ")";
    } else {
      cur.textContent = "Unbekannt (" + lat.toFixed(4) + ", " + lon.toFixed(4) + ")";
    }
  } catch (err) {
    cur.textContent = "Fehler: " + err.message;
  }
}

async function searchLocation() {
  const q = $("#geo-q").value.trim();
  const msg = $("#geo-msg");
  const ul  = $("#geo-results");
  msg.classList.remove("err"); msg.textContent = "";
  ul.innerHTML = "";
  if (!q) return;
  msg.textContent = "Suche …";
  try {
    const res = await apiGet("/api/geocode?q=" + encodeURIComponent(q) + "&limit=5");
    msg.textContent = "";
    if (!Array.isArray(res) || res.length === 0) {
      msg.textContent = "Keine Treffer.";
      return;
    }
    for (const g of res) {
      const li = document.createElement("li");
      li.textContent = formatGeo(g) + "  —  " + (+g.lat).toFixed(4) + ", " + (+g.lon).toFixed(4);
      li.addEventListener("click", () => {
        const f = $("#form-weather");
        f.elements.lat.value = (+g.lat).toFixed(4);
        f.elements.lon.value = (+g.lon).toFixed(4);
        ul.innerHTML = "";
        $("#geo-q").value = "";
        reverseGeocode(+g.lat, +g.lon);
      });
      ul.appendChild(li);
    }
  } catch (err) {
    msg.textContent = "Fehler: " + err.message;
    msg.classList.add("err");
  }
}

$("#btn-geo-search").addEventListener("click", searchLocation);
$("#geo-q").addEventListener("keydown", e => {
  if (e.key === "Enter") { e.preventDefault(); searchLocation(); }
});

$("#form-weather").addEventListener("submit", async e => {
  e.preventDefault();
  const f = e.target;
  const body = {
    WeatherAPIKey: f.elements.WeatherAPIKey.value,
    lat:   +f.elements.lat.value,
    lon:   +f.elements.lon.value,
    units: f.elements.units.value,
    lang:  f.elements.lang.value,
  };
  const msg = $("#weather-msg"); msg.classList.remove("err");
  try {
    await apiPut("/api/weather", body);
    msg.textContent = "Gespeichert.";
    reverseGeocode(body.lat, body.lon);
  }
  catch (err) { msg.textContent = "Fehler: " + err.message; msg.classList.add("err"); }
  setTimeout(() => msg.textContent = "", 3000);
});

// ----- System -----
async function refreshSystem() {
  const [sys, st] = await Promise.all([apiGet("/api/system"), apiGet("/api/status")]);
  const rows = [
    ["Build",       sys.build],
    ["Sketch MD5",  sys.sketchMD5],
    ["Chip",        sys.chipModel + " @ " + sys.cpuFreqMhz + " MHz"],
    ["SDK",         sys.sdkVersion],
    ["Flash",       fmtBytes(sys.flashSize)],
    ["Heap frei",   fmtBytes(sys.heapFree) + " (min " + fmtBytes(sys.heapMin) + ")"],
    ["PSRAM frei",  fmtBytes(sys.psramFree)],
    ["Uptime",      fmtUptime(sys.uptimeMs)],
    ["Hostname",    st.hostname],
    ["IP",          st.ip],
    ["WLAN",        st.ssid + " (" + st.rssi + " dBm, " + st.quality + "%)"],
    ["Audio",       st.audio?.playing
                      ? ("läuft – " + (st.audio.meta || "(keine Metadaten)"))
                      : "gestoppt"],
  ];
  $("#sys-info").innerHTML = rows.map(r => `<tr><td>${r[0]}</td><td>${escapeHtml(String(r[1]))}</td></tr>`).join("");
}
$("#btn-ntp").addEventListener("click", async () => {
  const m = $("#sys-msg"); m.classList.remove("err");
  try { await apiPost("/api/system/ntp-sync"); m.textContent = "NTP-Sync ausgelöst."; }
  catch (e) { m.textContent = "Fehler: " + e.message; m.classList.add("err"); }
  setTimeout(() => m.textContent = "", 3000);
});
$("#btn-reboot").addEventListener("click", async () => {
  if (!confirm("Gerät wirklich neu starten?")) return;
  const m = $("#sys-msg"); m.classList.remove("err");
  try { await apiPost("/api/system/reboot"); m.textContent = "Neustart läuft …"; }
  catch (e) { m.textContent = "Fehler: " + e.message; m.classList.add("err"); }
});

// ----- Utils -----
function fmtBytes(n) {
  if (!n && n !== 0) return "?";
  if (n >= 1048576) return (n / 1048576).toFixed(1) + " MB";
  if (n >= 1024)    return (n / 1024).toFixed(1) + " kB";
  return n + " B";
}
function fmtUptime(ms) {
  const s = Math.floor(ms / 1000);
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  return (d ? d + "d " : "") + String(h).padStart(2,"0") + ":" + String(m).padStart(2,"0");
}
function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, c => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
  }[c]));
}
function escapeAttr(s) { return escapeHtml(s); }

// ----- Boot -----
(async function init() {
  await refreshStatus();
  setInterval(refreshStatus, 5000);
  try {
    await refreshStations();
    await refreshAlarms();
    await refreshConfig();
    await refreshWeather();
  } catch (e) { console.error(e); }
})();
