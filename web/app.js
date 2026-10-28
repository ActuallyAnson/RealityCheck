"use strict";
// RealityCheck viewer. Draws only from the domain-agnostic run document produced by `rc_eval export` (recorded runs)
// or streamed by `rc_demo` (live runs): { original, shrunk: { world, static[], ents[], frames[][], signals{}, violations[] },
// report, events[], cost }. A new simulator domain adds an entry to RENDERERS/STATIC and STRIDE; nothing else changes.

const $ = (s, el = document) => el.querySelector(s);
const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html !== undefined) e.innerHTML = html; return e; };
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
const reduceMotion = matchMedia("(prefers-reduced-motion: reduce)").matches;
const DPR = () => Math.min(2, window.devicePixelRatio || 1);

// ---- event bus: the sound layer (audio.js) and the visuals listen to the same events ----
const bus = new EventTarget();
const emit = (name, detail) => bus.dispatchEvent(new CustomEvent(name, { detail }));

const PALETTE = ["#4cc9f0", "#b79cff", "#5ee0a0", "#ffb454", "#ff9ec4", "#8fa8ff", "#c8e06a", "#ff9b6c"];
const STRIDE = { circle: 4, cartpole: 4 };  // numbers per entity in a frame

// ---- per-kind renderers ----
function glowCircle(ctx, cx, cy, r, col, ctrl) {
  const g = ctx.createRadialGradient(cx - r * 0.3, cy - r * 0.3, r * 0.1, cx, cy, r);
  g.addColorStop(0, col + "66"); g.addColorStop(1, col + "14");
  ctx.beginPath(); ctx.arc(cx, cy, r, 0, 7); ctx.fillStyle = g; ctx.fill();
  ctx.lineWidth = ctrl ? 2.6 : 1.6; ctx.strokeStyle = ctrl ? "#f4f8fc" : col; ctx.stroke();
}
const RENDERERS = {
  cartpole(ctx, tf, ent, f) {  // frame: x, theta, v, omega
    const d = tf.d, gy = 0.6, cx = tf.x(f[0]), cy = tf.y(gy), cw = 0.5 * tf.s, ch = 0.28 * tf.s;
    const tipx = cx + ent.r * tf.s * Math.sin(f[1]), tipy = cy - ch - ent.r * tf.s * Math.cos(f[1]);
    ctx.fillStyle = "rgba(76,201,240,.22)"; ctx.strokeStyle = "#4cc9f0"; ctx.lineWidth = 1.6 * d; ctx.beginPath(); ctx.rect(cx - cw / 2, cy - ch, cw, ch); ctx.fill(); ctx.stroke();
    ctx.strokeStyle = "#e9f0f7"; ctx.lineWidth = 3 * d; ctx.lineCap = "round"; ctx.beginPath(); ctx.moveTo(cx, cy - ch); ctx.lineTo(tipx, tipy); ctx.stroke();
    const bad = Math.abs(f[1]) > 0.8; glowCircle(ctx, tipx, tipy, Math.max(5 * d, (0.05 + 0.16 * Math.sqrt(ent.m)) * tf.s * 0.6), bad ? "#ff5c6c" : "#ffb454", false);
  },
  circle(ctx, tf, ent, f, i, trail) {
    const col = PALETTE[i % PALETTE.length];
    if (trail && trail.length > 1) {
      ctx.lineWidth = 2 * tf.d; ctx.lineCap = "round"; ctx.strokeStyle = col;
      for (let k = 1; k < trail.length; k++) { ctx.globalAlpha = 0.04 + 0.42 * (k / trail.length) ** 2; ctx.beginPath(); ctx.moveTo(tf.x(trail[k - 1][0]), tf.y(trail[k - 1][1])); ctx.lineTo(tf.x(trail[k][0]), tf.y(trail[k][1])); ctx.stroke(); }
      ctx.globalAlpha = 1;
    }
    const cx = tf.x(f[0]), cy = tf.y(f[1]), r = Math.max(3 * tf.d, ent.r * tf.s);
    glowCircle(ctx, cx, cy, r, col, ent.ctrl);
    const sp = Math.hypot(f[2], f[3]);
    if (sp > 0.05) {
      const L = Math.min(r + 30 * tf.d, r + sp * tf.s * 0.05), ux = f[2] / sp, uy = -f[3] / sp, ex = cx + ux * L, ey = cy + uy * L;
      ctx.beginPath(); ctx.moveTo(cx + ux * r, cy + uy * r); ctx.lineTo(ex, ey); ctx.lineWidth = 1.6 * tf.d; ctx.strokeStyle = col; ctx.stroke();
      ctx.beginPath(); ctx.moveTo(ex, ey); ctx.lineTo(ex - ux * 6 * tf.d - uy * 3.5 * tf.d, ey - uy * 6 * tf.d + ux * 3.5 * tf.d); ctx.lineTo(ex - ux * 6 * tf.d + uy * 3.5 * tf.d, ey - uy * 6 * tf.d - ux * 3.5 * tf.d); ctx.closePath(); ctx.fillStyle = col; ctx.fill();
    }
  },
};
const STATIC = {
  track_end(ctx, tf, o) {
    const x = tf.x(o.x), y0 = tf.y(0.6), y1 = tf.y(1.4); ctx.strokeStyle = "#ff5c6c"; ctx.lineWidth = 2 * tf.d; ctx.beginPath(); ctx.moveTo(x, y0 + 4 * tf.d); ctx.lineTo(x, y1); ctx.stroke();
    ctx.strokeStyle = "rgba(255,255,255,.18)"; ctx.lineWidth = tf.d; ctx.beginPath(); ctx.moveTo(tf.x(-2.4), y0 + 4 * tf.d); ctx.lineTo(tf.x(2.4), y0 + 4 * tf.d); ctx.stroke();
  },
  rect(ctx, tf, o) {
    const x = tf.x(o.x), y = tf.y(o.y + o.h), w = o.w * tf.s, h = o.h * tf.s;
    ctx.fillStyle = "#1a2330"; ctx.fillRect(x, y, w, h);
    ctx.save(); ctx.beginPath(); ctx.rect(x, y, w, h); ctx.clip(); ctx.strokeStyle = "rgba(255,255,255,.07)"; ctx.lineWidth = 1;
    for (let k = -h; k < w; k += 7 * tf.d) { ctx.beginPath(); ctx.moveTo(x + k, y + h); ctx.lineTo(x + k + h, y); ctx.stroke(); }
    ctx.restore(); ctx.strokeStyle = "#5c6f87"; ctx.lineWidth = 1.2 * tf.d; ctx.strokeRect(x, y, w, h);
  },
  waypoint(ctx, tf, o) {
    const x = tf.x(o.x), y = tf.y(o.y), r = 7 * tf.d; ctx.strokeStyle = "#ffb454"; ctx.lineWidth = 1.5 * tf.d; ctx.setLineDash([3 * tf.d, 3 * tf.d]);
    ctx.beginPath(); ctx.arc(x, y, r, 0, 7); ctx.stroke(); ctx.setLineDash([]); ctx.beginPath(); ctx.moveTo(x - r - 3 * tf.d, y); ctx.lineTo(x + r + 3 * tf.d, y); ctx.moveTo(x, y - r - 3 * tf.d); ctx.lineTo(x, y + r + 3 * tf.d); ctx.stroke();
  },
};

// ---- Stage: one canvas bound to one view ----
class Stage {
  constructor(canvas, scrub, ticks, info, veil) { Object.assign(this, { canvas, scrub, ticks, info, veil }); this.view = null; this.step = 0; this.highlight = null; this.small = false; }
  fit() { const v = this.view; if (!v) return; const w = this.canvas.clientWidth || this.canvas.width / DPR() || 600; this.canvas.width = Math.round(w * DPR()); this.canvas.height = Math.round(w * DPR() * v.world.h / v.world.w); }
  set(view) {
    this.view = view; this.step = 0; this.highlight = null;
    if (!view) { this.scrub.max = 0; this.ticks.innerHTML = ""; this.info.textContent = ""; this.draw(); return; }
    this.scrub.max = view.steps; this.scrub.value = 0; this.ticks.innerHTML = "";
    const seen = new Set();
    for (const v of view.violations) { if (seen.has(v.step)) continue; seen.add(v.step); const i = el("i"); i.style.left = (100 * v.step / Math.max(1, view.steps)) + "%"; this.ticks.appendChild(i); }
    const rules = [...new Set(view.violations.map((v) => v.rule))];
    this.info.textContent = `${view.steps} steps · dt ${(+view.dt).toPrecision(3)}${rules.length ? " · " + rules.join(", ") : " · no violation"}`;
    this.fit(); this.draw();
  }
  seek(s) { if (!this.view) return; this.step = Math.max(0, Math.min(this.view.steps, s)); this.scrub.value = Math.round(this.step); this.draw(); }
  draw() {
    const c = this.canvas, ctx = c.getContext("2d"), v = this.view, d = DPR();
    ctx.setTransform(1, 0, 0, 1, 0, 0); ctx.clearRect(0, 0, c.width, c.height);
    if (!v) return;
    const pad = 12 * d, s = Math.min((c.width - 2 * pad) / v.world.w, (c.height - 2 * pad) / v.world.h);
    const ox = v.world.x0 || 0, tf = { s, d, x: (x) => pad + (x - ox) * s, y: (y) => c.height - pad - y * s };
    ctx.fillStyle = "#06080c"; ctx.fillRect(0, 0, c.width, c.height);
    ctx.fillStyle = "rgba(255,255,255,.06)";
    for (let gx = 0; gx <= v.world.w; gx += 1) for (let gy = 0; gy <= v.world.h; gy += 1) ctx.fillRect(tf.x(ox + gx) - 0.7 * d, tf.y(gy) - 0.7 * d, 1.4 * d, 1.4 * d);
    ctx.strokeStyle = "rgba(255,255,255,.16)"; ctx.lineWidth = d; ctx.strokeRect(tf.x(ox), tf.y(v.world.h), v.world.w * s, v.world.h * s);
    for (const o of v.static) (STATIC[o.k] || (() => {}))(ctx, tf, o);
    const si = Math.round(this.step); let off = 0, hitAny = false;
    v.ents.forEach((ent, i) => {
      const n = STRIDE[ent.k] || 0, f = v.frames[si].slice(off, off + n), trail = [];
      for (let k = Math.max(0, si - 70); k <= si; k++) trail.push(v.frames[k].slice(off, off + 2));
      (RENDERERS[ent.k] || (() => {}))(ctx, tf, ent, f, i, trail);
      const hit = v.violations.some((x) => x.step === si && (x.body === i || x.body < 0)) || (this.highlight && this.highlight.body === i && performance.now() < this.highlight.until);
      if (hit && ent.k === "circle") { hitAny = true; const t = (performance.now() % 900) / 900; ctx.beginPath(); ctx.arc(tf.x(f[0]), tf.y(f[1]), ent.r * s + (6 + 14 * t) * d, 0, 7); ctx.strokeStyle = `rgba(255,92,108,${1 - t})`; ctx.lineWidth = 2.6 * d; ctx.stroke(); }
      off += n;
    });
    if (!this.small) {
      ctx.font = `${11 * d}px ui-monospace, Menlo, monospace`; ctx.fillStyle = "rgba(154,167,182,.85)"; ctx.fillText(`step ${si}`, pad + 6 * d, pad + 13 * d);
      if (v.violations.some((x) => x.step <= si)) { ctx.font = `700 ${11 * d}px ui-monospace, Menlo, monospace`; const t = "VIOLATION"; const w = ctx.measureText(t).width; ctx.fillStyle = "rgba(255,92,108,.16)"; ctx.fillRect(c.width - pad - w - 18 * d, pad, w + 14 * d, 20 * d); ctx.fillStyle = "#ff5c6c"; ctx.fillText(t, c.width - pad - w - 11 * d, pad + 14 * d); }
    }
    this.animating = hitAny;
  }
}

// ---- application state ----
const S = { index: [], doc: null, playing: false, speed: 1, mode: "recorded", live: null, sse: null, replay: { i: 0, timer: null, running: false }, sig: null, last: 0, status: null, cost: 0 };
const stOrig = new Stage($("#cOrig"), $("#sOrig"), $("#tOrig"), $("#origInfo"), $("#veilOrig"));
const stShr = new Stage($("#cShr"), $("#sShr"), $("#tShr"), $("#shrInfo"), $("#veilShr"));
const stages = [stOrig, stShr];
let plotStage = stShr;
const money = (x) => "$" + x.toFixed(4);
const HINT = '<div class="empty">Press <b>Replay run</b> (R) to watch the agent hunt, or <b>Skip to result</b>.</div>';
const EV_HINT = '<div class="empty">The report appears when the agent submits a failure that survives validation.</div>';

// ---- pipeline stepper ----
const PIPE = ["hunt", "violation", "shrink", "explain", "verify"];
function pipe(name, state) { const li = $(`#pipe li[data-p="${name}"]`); if (!li) return; li.classList.remove("active", "done", "alert"); if (state) li.classList.add(state); }
function pipeReset() { PIPE.forEach((p) => pipe(p, "")); }
function pipeAll(done) { PIPE.forEach((p) => pipe(p, done[p] || "")); }

// ---- icons ----
const ICO = {
  think: '<svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M9 18h6M10 21h4M12 3a6 6 0 00-3.5 10.9c.6.5 1 1.2 1 2.1h5c0-.9.4-1.6 1-2.1A6 6 0 0012 3z"/></svg>',
  call: '<svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12h14M13 6l6 6-6 6"/></svg>',
  ok: '<svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.5l4.5 4.5L19 7.5"/></svg>',
  bad: '<svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round"><path d="M6 6l12 12M18 6L6 18"/></svg>',
  alert: '<svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 4l9 16H3zM12 10v4M12 17.2v.1"/></svg>',
  eye: '<svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M2 12s3.6-7 10-7 10 7 10 7-3.6 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/></svg>',
};

// ---- data loading ----
async function loadIndex() {
  const idx = await (await fetch("data/index.json")).json();
  S.index = idx;
  const sel = $("#runSel"); sel.innerHTML = "";
  const DOMAINS = { rigid2d: "Rigid bodies", orbit: "Orbits", cartpole: "Cart-pole" }, groups = {};
  for (const r of idx) {
    const dom = r.domain || ({ o: "orbit", c: "cartpole" }[(r.bug || r.id)[0]] || "rigid2d");
    if (!groups[dom]) { groups[dom] = el("optgroup"); groups[dom].label = DOMAINS[dom] || dom; sel.appendChild(groups[dom]); }
    const o = el("option"); o.value = r.id; o.textContent = `${r.id}${r.status === "failure_reported" ? "" : " (" + r.status.replace(/_/g, " ") + ")"}`; groups[dom].appendChild(o);
  }
  const want = new URLSearchParams(location.search).get("run");
  const first = idx.find((r) => r.id === want) || idx.find((r) => r.status === "failure_reported") || idx[0];
  sel.value = first.id;
  await loadRun(first.id);
}
function resetRunUI() {
  stopReplay(); S.playing = false; setPlayIcon(false);
  $("#feed").innerHTML = HINT; $("#evidence").innerHTML = EV_HINT; $("#statusChip").textContent = ""; $("#statusChip").className = "chip";
  pipeReset(); meter(0); solo(false);
}
async function loadRun(id) {
  S.doc = await (await fetch(`data/${id}.json`)).json();
  resetRunUI();
  stOrig.set(S.doc.original || null); stShr.set(S.doc.shrunk || null);
  veil(stOrig, true, "Waiting for the agent to find a failure…"); veil(stShr, true, "The minimal reproduction appears here.");
  plotStage = stShr.view ? stShr : stOrig; buildSignals();
  $("#modelName").textContent = S.doc.cost.calls ? `${S.doc.cost.calls} model calls` : "";
  resize();
}
function veil(stage, on, msg) { if (msg) stage.veil.textContent = msg; stage.veil.classList.toggle("gone", !on); }
function solo(on) { $("#stages").classList.toggle("solo", on); requestAnimationFrame(resize); }
function meter(usd) { S.cost = usd; $("#meterText").textContent = money(usd); $("#meterFill").style.width = Math.min(100, 100 * usd / (S.status ? S.status.run_budget_usd : 0.25)) + "%"; }
function setPlayIcon(p) { $("#icoPlay").hidden = p; $("#icoPause").hidden = !p; $("#playBtn").setAttribute("aria-label", p ? "Pause" : "Play"); }
function setPlaying(p) { S.playing = p; setPlayIcon(p); }

// ---- feed cards ----
function describe(e) {
  if (e.type === "assistant_text") return { cls: "think", ico: ICO.think, html: `<div class="txt">${esc(e.text)}</div>` };
  if (e.type === "tool_call") return { cls: "call", ico: ICO.call, html: `<div class="row"><span class="tool t-${esc(e.tool)}">${esc(e.tool)}</span><span class="brief">${esc(e.brief || "")}</span></div>${e.args ? `<details><summary>arguments</summary><pre>${esc(e.args)}</pre></details>` : ""}` };
  if (e.type === "tool_result") {
    const cls = e.violated ? "res viol" : e.ok === false ? "res bad" : "res ok";
    return { cls, ico: e.violated ? ICO.alert : e.ok === false ? ICO.bad : ICO.ok, html: `<div class="row"><span class="tool t-${esc(e.tool)}">${esc(e.tool)}</span><span class="brief">${esc(e.brief || (e.ok === false ? "error" : "ok"))}</span></div>${e.summary ? `<details><summary>raw result</summary><pre>${esc(e.summary)}</pre></details>` : ""}` };
  }
  if (e.type === "report_accepted") return { cls: "good", ico: ICO.ok, html: `<div class="row"><span class="tool t-submit_report">report accepted</span><span class="brief">every cited value matches the recorded state</span></div>` };
  if (e.type === "report_rejected") return { cls: "res bad", ico: ICO.bad, html: `<div class="row"><span class="tool t-submit_report">report rejected</span><span class="brief">${esc((e.message || e.error || "citations did not match").slice(0, 180))}</span></div>` };
  if (e.type === "vision") return { cls: "good", ico: ICO.eye, html: `<div class="row"><span class="tool t-render_frames">vision cross-check</span><span class="brief">${e.agrees === false ? "disagrees" : "agrees"}</span></div><div class="brief" style="margin-top:4px">${esc((e.description || "").slice(0, 280))}</div>` };
  return { cls: "res", ico: ICO.call, html: `<div class="row"><span class="tool">${esc(e.type)}</span><span class="brief">${esc(e.message || e.text || "")}</span></div>` };
}
function addCard(e) {
  const f = $("#feed"); if (f.querySelector(".empty")) f.innerHTML = "";
  const d = describe(e), c = el("div", "card " + d.cls, `<div class="ico">${d.ico}</div><div class="body">${d.html}</div>`);
  f.appendChild(c); f.scrollTop = f.scrollHeight;
}

// ---- one handler for recorded replay and live streaming ----
function showOriginal(view) {
  if (S._origShown) return;
  if (view) stOrig.set(view);
  if (!stOrig.view) return;
  S._origShown = true; veil(stOrig, false); pipe("hunt", "done"); pipe("violation", "alert"); emit("violation"); jumpToViolation(stOrig, true);
  plotStage = stOrig; buildSignals();
}
function showShrunk(view) {
  if (view) stShr.set(view);
  if (!stShr.view || S._shrShown) return;
  S._shrShown = true; solo(false); veil(stShr, false); pipe("shrink", "done"); emit("shrink-done"); stShr.seek(0); setPlaying(true);
}
// Returns the pause (ms) to leave after the event when replaying a recording.
function onEvent(e, view) {
  addCard(e); emit("event", e);
  let delay = e.type === "assistant_text" ? 1100 : e.type === "tool_call" ? 650 : 800;
  if (e.type === "tool_call" && e.tool === "shrink") { pipe("shrink", "active"); emit("shrink-start"); delay = 1500; }
  if (e.type === "tool_call" && e.tool === "submit_report") pipe("explain", "active");
  if (e.type === "tool_result" && e.tool === "run_scenario" && e.violated === true && !S._origShown) {
    if (S.mode === "recorded") showOriginal(S.doc.original); delay = 2600;  // live: the canvas view message follows
  }
  if (e.type === "tool_result" && e.tool === "shrink" && e.ok !== false && S.mode === "recorded") { showShrunk(S.doc.shrunk); delay = 2000; }
  if (e.type === "report_accepted") { pipe("explain", "done"); emit("accepted"); delay = 1500; if (S.mode === "recorded") renderEvidence(); }
  if (e.type === "report_rejected") { pipe("explain", "alert"); emit("rejected"); }
  if (e.type === "vision") pipe("verify", "done");
  return delay;
}

// ---- recorded replay ----
function stopReplay() { clearTimeout(S.replay.timer); S.replay.running = false; }
function startReplay() {
  stopReplay(); closeLive(); const d = S.doc; if (!d) return;
  setMode("recorded");
  S._origShown = false; S._shrShown = false; resetRunUI();
  stOrig.set(d.original || null); stShr.set(d.shrunk || null);
  veil(stOrig, true, "Waiting for the agent to find a failure…"); veil(stShr, true, "The minimal reproduction appears here.");
  $("#feed").innerHTML = ""; pipe("hunt", "active"); solo(true); emit("start");
  S.replay = { i: 0, timer: null, running: true };
  const step = () => {
    if (!S.replay.running) return;
    const evs = d.events || [], e = evs[S.replay.i];
    if (!e) { finishReplay(); return; }
    S.replay.i++;
    const delay = onEvent(e);
    meter(d.cost.usd * Math.min(1, S.replay.i / Math.max(1, evs.length)));
    S.replay.timer = setTimeout(step, delay / S.speed);
  };
  step();
}
function finishReplay() {
  S.replay.running = false; solo(false);
  if (S.doc.status !== "failure_reported") { pipe("explain", "alert"); }
  veil(stShr, !stShr.view, "The agent did not produce a validated minimal case."); renderEvidence(); meter(S.doc.cost.usd); emit("done");
}
function showAll() {
  stopReplay(); closeLive(); const d = S.doc; if (!d) return; setMode("recorded"); S._origShown = true; S._shrShown = true; solo(false);
  stOrig.set(d.original || null); stShr.set(d.shrunk || null);
  veil(stOrig, !stOrig.view, "No failing scenario in this run."); veil(stShr, !stShr.view, "The agent did not produce a validated minimal case.");
  $("#feed").innerHTML = ""; (d.events || []).forEach((e) => addCard(e));
  const done = {}; if (d.original) { done.hunt = "done"; done.violation = "alert"; } if (d.shrunk) done.shrink = "done"; if (d.status === "failure_reported") { done.explain = "done"; if (d.report.vision_done) done.verify = "done"; } else done.explain = "alert";
  pipeAll(done); renderEvidence(); meter(d.cost.usd); plotStage = stShr.view ? stShr : stOrig; buildSignals(); jumpToViolation(plotStage, false);
}

// ---- live mode ----
function setMode(m) {
  S.mode = m; $("#modeRec").classList.toggle("on", m === "recorded"); $("#modeLive").classList.toggle("on", m === "live");
  $("#recCtl").hidden = m !== "recorded"; $("#liveCtl").hidden = m !== "live"; $("#liveInfo").hidden = m !== "live";
  const b = $("#modeBadge"); b.textContent = m === "live" ? "LIVE" : "RECORDED"; b.className = "badge " + (m === "live" ? "live" : "rec");
  b.title = m === "live" ? "A real agent run against the Meta Model API, streamed as it happens." : "Replaying a saved run. Every number comes from files in the repo.";
}
async function refreshStatus() {
  try { S.status = await (await fetch("/api/status")).json(); } catch (e) { S.status = null; }
  const st = S.status, ok = !!(st && st.key_set);
  $("#modeLive").disabled = !st; $("#modeLive").title = !st ? "Live mode needs the rc_demo server." : ok ? "" : `Set ${st.key_env} before starting rc_demo to enable live runs.`;
  $("#startBtn").disabled = !ok;
  $("#liveInfo").innerHTML = st ? `${ok ? "API key detected" : `<b>${esc(st.key_env)} is not set</b> — live runs are disabled; recorded mode works.`} · model <b>${esc(st.model)}</b> · cap $${st.run_budget_usd.toFixed(2)} per run · lifetime spend $${st.ledger_usd.toFixed(2)} of $${st.ledger_cap_usd.toFixed(0)}` : "rc_demo server not reachable.";
}
function closeLive() { if (S.sse) { S.sse.close(); S.sse = null; } }
async function startLive() {
  closeLive(); stopReplay(); setMode("live"); S._origShown = false; S._shrShown = false; S.doc = null;
  resetRunUI(); $("#feed").innerHTML = ""; stOrig.set(null); stShr.set(null);
  veil(stOrig, true, "Waiting for the agent to find a failure…"); veil(stShr, true, "The minimal reproduction appears here."); $("#modelName").textContent = "";
  $("#startBtn").disabled = true;
  let r; try { r = await fetch("/api/live/start", { method: "POST", headers: { "Content-Type": "application/json", "X-RC": "1" }, body: JSON.stringify({ bug: $("#bugSel").value }) }); } catch (e) { return liveError("rc_demo is not reachable"); }
  const j = await r.json(); if (!r.ok) return liveError(j.error || "could not start");
  pipe("hunt", "active"); solo(true); emit("start"); $("#modelName").textContent = j.run;
  let lo = $("#runSel option[value='__live']"); if (!lo) { lo = el("option"); lo.value = "__live"; $("#runSel").prepend(lo); } lo.textContent = "live: " + j.run; $("#runSel").value = "__live";
  S.sse = new EventSource(`/api/live/stream?run=${encodeURIComponent(j.run)}`);
  S.sse.onmessage = (m) => {
    const msg = JSON.parse(m.data);
    if (msg.kind === "event") onEvent(msg.event);
    else if (msg.kind === "view") { if (msg.which === "original") showOriginal(msg.view); else showShrunk(msg.view); }
    else if (msg.kind === "cost") { meter(msg.usd); $("#modelName").textContent = `${msg.calls} model calls`; }
    else if (msg.kind === "final") { S.doc = msg.doc; solo(false); if (S.doc.status !== "failure_reported") pipe("explain", "alert"); veil(stShr, !stShr.view && !S.doc.shrunk, "The agent did not produce a validated minimal case."); renderEvidence(); meter(S.doc.cost.usd); emit("done"); }
    else if (msg.kind === "error") liveError(msg.message);
    else if (msg.kind === "end") { closeLive(); $("#startBtn").disabled = !(S.status && S.status.key_set); refreshStatus(); }
  };
  S.sse.onerror = () => { closeLive(); $("#startBtn").disabled = !(S.status && S.status.key_set); };
}
function liveError(text) { addCard({ type: "report_rejected", message: text }); pipe("hunt", "alert"); $("#startBtn").disabled = !(S.status && S.status.key_set); closeLive(); }

// ---- playback ----
function jumpToViolation(stage, autoplay) { const v = stage.view; if (!v || !v.violations.length) { stage.seek(0); return; } stage.seek(Math.max(0, v.violations[0].step - 18)); if (autoplay && !reduceMotion) setPlaying(true); plotStage = stage; buildSignals(); }
function tick(now) {
  const dt = Math.min(0.1, (now - S.last) / 1000); S.last = now;
  if (S.playing) {
    let any = false;
    for (const st of stages) if (st.view && st.veil.classList.contains("gone") && st.step < st.view.steps) { st.seek(Math.min(st.view.steps, st.step + dt * 60 * S.speed)); any = true; }
    if (!any) setPlaying(false);
  } else for (const st of stages) if (st.view && (st.highlight || st.animating)) st.draw();
  drawPlot(); readout(); requestAnimationFrame(tick);
}
function readout() { const st = plotStage; if (!st || !st.view) { $("#stepRead").textContent = ""; return; } const si = Math.round(st.step), v = st.view.violations.filter((x) => x.step === si); $("#stepRead").textContent = `step ${si} / ${st.view.steps}` + (v.length ? " · " + v.map((x) => x.rule).join(", ") : ""); }

// ---- signal plot ----
function buildSignals() {
  const sel = $("#sigSel"), v = plotStage && plotStage.view, prev = S.sig; sel.innerHTML = ""; if (!v) return;
  for (const k of Object.keys(v.signals)) { const o = el("option"); o.value = k; o.textContent = k; sel.appendChild(o); }
  if (prev && v.signals[prev]) sel.value = prev; S.sig = sel.value;
  $("#plotLegend").innerHTML = `<span><i></i>${esc(S.sig || "")}</span>${v.signal_limits && v.signal_limits[S.sig] !== undefined ? '<span><i class="lim"></i>declared limit</span>' : ""}<span><i class="vio"></i>violation</span>`;
}
function drawPlot() {
  const cv = $("#plot"), W = cv.clientWidth, H = cv.clientHeight, d = DPR(); if (!W) return;
  if (cv.width !== Math.round(W * d)) { cv.width = Math.round(W * d); cv.height = Math.round(H * d); }
  const ctx = cv.getContext("2d"); ctx.setTransform(d, 0, 0, d, 0, 0); ctx.clearRect(0, 0, W, H);
  const v = plotStage && plotStage.view; if (!v || !S.sig || !v.signals[S.sig]) return;
  const ys = v.signals[S.sig], n = ys.length, lim = v.signal_limits && v.signal_limits[S.sig];
  let lo = Math.min(...ys), hi = Math.max(...ys); if (lim !== undefined) { lo = Math.min(lo, 0); hi = Math.max(hi, lim * 1.05); } if (hi === lo) { hi = lo + 1; lo -= 1; }
  const m = 44, X = (i) => m + (W - m - 12) * i / Math.max(1, n - 1), Y = (y) => H - 22 - (H - 40) * (y - lo) / (hi - lo);
  ctx.strokeStyle = "rgba(255,255,255,.06)"; ctx.fillStyle = "#667385"; ctx.font = "10px ui-monospace, Menlo, monospace"; ctx.lineWidth = 1;
  for (let g = 0; g <= 3; g++) { const y = lo + (hi - lo) * g / 3; ctx.beginPath(); ctx.moveTo(m, Y(y)); ctx.lineTo(W - 12, Y(y)); ctx.stroke(); ctx.fillText(Math.abs(y) >= 100 ? y.toFixed(0) : y.toFixed(1), 6, Y(y) + 3); }
  if (lim !== undefined) { ctx.strokeStyle = "#ffb454"; ctx.setLineDash([5, 4]); ctx.beginPath(); ctx.moveTo(m, Y(lim)); ctx.lineTo(W - 12, Y(lim)); ctx.stroke(); ctx.setLineDash([]); }
  const cur = Math.round(plotStage.step);
  const stepEvery = Math.max(1, Math.floor(n / (W * 1.5)));
  ctx.beginPath(); for (let i = 0; i < n; i += stepEvery) { i === 0 ? ctx.moveTo(X(i), Y(ys[i])) : ctx.lineTo(X(i), Y(ys[i])); } ctx.lineTo(X(n - 1), Y(ys[n - 1]));
  const grad = ctx.createLinearGradient(0, 0, 0, H); grad.addColorStop(0, "rgba(76,201,240,.16)"); grad.addColorStop(1, "rgba(76,201,240,0)");
  ctx.save(); ctx.lineTo(X(n - 1), H - 22); ctx.lineTo(X(0), H - 22); ctx.closePath(); ctx.fillStyle = grad; ctx.fill(); ctx.restore();
  ctx.strokeStyle = "#4cc9f0"; ctx.lineWidth = 1.7; ctx.stroke();
  for (const x of v.violations) { ctx.strokeStyle = "rgba(255,92,108,.75)"; ctx.lineWidth = 1.2; ctx.beginPath(); ctx.moveTo(X(x.step), 6); ctx.lineTo(X(x.step), H - 22); ctx.stroke(); }
  ctx.strokeStyle = "rgba(230,235,242,.8)"; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(X(cur), 6); ctx.lineTo(X(cur), H - 22); ctx.stroke();
  ctx.fillStyle = "#e6ebf2"; ctx.beginPath(); ctx.arc(X(cur), Y(ys[Math.min(cur, n - 1)]), 3.4, 0, 7); ctx.fill();
  ctx.fillStyle = "#667385"; ctx.fillText("0", m, H - 8); ctx.fillText(String(v.steps), W - 12 - 20, H - 8); ctx.fillText("step", W / 2 - 10, H - 8);
}
$("#plot").addEventListener("click", (e) => { const v = plotStage && plotStage.view; if (!v) return; const r = e.target.getBoundingClientRect(); const i = Math.round((e.clientX - r.left - 44) / (r.width - 56) * v.steps); setPlaying(false); plotStage.seek(i); });

// ---- evidence ----
function frameValue(view, step, body, field) {
  if (!view || step < 0 || step >= view.frames.length) return null;
  if (view.signals && view.signals[field] && (field === "energy" || body < 0 || view.domain === "cartpole")) return view.signals[field][step];
  if (view.domain === "cartpole") { const f = view.frames[step]; return { x: f[0], theta: f[1], v: f[2], omega: f[3] }[field] ?? null; }
  if (body < 0) return null;
  let off = 0; for (let i = 0; i < body; i++) off += STRIDE[view.ents[i].k] || 0;
  const f = view.frames[step].slice(off, off + 4); if (f.length < 4) return null;
  return { x: f[0], y: f[1], vx: f[2], vy: f[3], speed: Math.hypot(f[2], f[3]) }[field] ?? null;
}
function renderEvidence() {
  const d = S.doc, ev = $("#evidence"); ev.innerHTML = ""; if (!d) return; const r = d.report || {}, view = d.shrunk, chip = $("#statusChip");
  if (d.status === "failure_reported") { chip.textContent = "REPORT VALIDATED"; chip.className = "chip good"; }
  else if (d.status === "failure_not_reported") { chip.textContent = "NO VALID REPORT"; chip.className = "chip warn"; }
  else { chip.textContent = "NO FAILURE FOUND"; chip.className = "chip bad"; }
  if (d.status !== "failure_reported" || !view) { ev.appendChild(el("div", "sec", `<h3>Outcome</h3><p style="margin:0">The agent stopped (<code>${esc(r.stop_reason || "n/a")}</code>) without a failure that passed validation. Misses are reported, not hidden.</p>`)); return; }
  const v0 = view.violations[0], ev0 = d.original ? d.original.violations[0] : null;
  const hero = el("div", "sec verdict", `<h3>Violated rule</h3><div class="rule">${esc(r.rule)}</div><div class="sub">${esc(v0 ? v0.detail : "")}</div>
    <div class="metrics"><div class="metric"><b>${v0 ? v0.step : "-"}</b><span>first step</span></div><div class="metric"><b>${d.original && d.original.size !== undefined ? d.original.size : "-"}→${view.size !== undefined ? view.size : "-"}</b><span>scenario size</span></div><div class="metric"><b>${money(d.cost.usd)}</b><span>${d.cost.calls} calls</span></div></div>`);
  ev.appendChild(hero);
  const ex = el("div", "sec"); ex.innerHTML = "<h3>Explanation (model)</h3><div class='expl'>" + esc(r.explanation).replace(/\b(steps?)\s+(\d+)/gi, (m, a, n) => `${a} <a class="step" data-step="${n}">${n}</a>`) + "</div>"; ev.appendChild(ex);
  ex.querySelectorAll("a.step").forEach((a) => a.addEventListener("click", () => seekShrunk(+a.dataset.step, -1)));
  const cs = el("div", "sec"); cs.innerHTML = "<h3>Cited states, checked against the recorded run</h3>"; let okN = 0, tot = 0;
  for (const c of r.citations || []) {
    const hasV = c.value !== undefined && c.value !== null && c.field; let mark = "", cls = "";
    if (hasV) { const real = frameValue(view, c.step, c.body, c.field); tot++; if (real !== null) { const ok = Math.abs(real - c.value) <= Math.max(0.02, 0.01 * Math.abs(real)); okN += ok; cls = ok ? "ok" : "bad"; mark = `${ok ? "✓" : "✗"} ${c.field} ${(+real).toFixed(3)}`; } }
    const row = el("div", "cite", `<span class="st">step ${c.step}</span><span class="cl">${esc(c.claim || "")}${c.body >= 0 ? ` <span class="dim">(body ${c.body})</span>` : ""}</span><span class="vv ${cls}">${mark}</span>`);
    row.addEventListener("click", () => seekShrunk(c.step, c.body)); cs.appendChild(row);
  }
  if (tot) cs.appendChild(el("div", "dim small", `${okN} of ${tot} cited values match the simulation state (tolerance max(0.02, 1%)).`)); ev.appendChild(cs);
  if (r.vision_done) ev.appendChild(el("div", "sec", `<h3>Vision cross-check <span class="chip ${r.vision_agrees ? "good" : "warn"}">${r.vision_agrees ? "agrees" : "disagrees"}</span></h3>${esc(r.vision_note || "")}`));
  const strip = el("div", "sec", "<h3>Frame strip around the violation</h3>"), grid = el("div", "strip"); strip.appendChild(grid); ev.appendChild(strip);
  const vs = v0 ? v0.step : 0, picks = [-4, -2, -1, 0, 1, 3].map((o) => Math.max(0, Math.min(view.steps, vs + o)));
  for (const s of picks) { const w = el("div"), cv = el("canvas"); w.appendChild(cv); w.appendChild(el("div", "lab", "step " + s)); grid.appendChild(w); const mini = new Stage(cv, { max: 0, value: 0 }, { innerHTML: "" }, { textContent: "" }, null); mini.small = true; mini.view = view; cv.width = Math.round(300 * DPR()); cv.height = Math.round(300 * DPR() * view.world.h / view.world.w); mini.step = s; mini.draw(); w.addEventListener("click", () => seekShrunk(s, -1)); }
  const cost = d.cost; ev.appendChild(el("div", "sec", `<h3>Run cost</h3><span class="dim small">${cost.calls} calls · ${cost.tokens_in.toLocaleString()} tokens in · ${cost.tokens_out.toLocaleString()} out · ${money(cost.usd)} · ${(cost.latency_ms / 1000).toFixed(0)} s API time</span>`));
}
function seekShrunk(step, body) { if (!stShr.view) return; setPlaying(false); plotStage = stShr; buildSignals(); stShr.seek(step); stShr.highlight = body >= 0 ? { body, until: performance.now() + 2500 } : null; emit("cite"); }

// ---- scoreboard ----
async function loadScoreboard() {
  const sb = await (await fetch("data/scoreboard.json")).json();
  $("#scoreTitle").textContent = sb.title; $("#scoreNote").textContent = `Model ${sb.model}. "Valid and attributable" means the report passed citation checks and the shrunk scenario does not also break the unmodified simulator.`;
  const t = $("#scoreTable"); t.innerHTML = ""; t.appendChild(el("div", "srow head", "<span>bug</span><span>split</span><span>agent: valid, attributable</span><span>random baseline: attributable</span><span>explained</span>"));
  sb.rows.forEach((r, k) => {
    const held = r.split === "heldout", a = r.agent_valid_attributable / r.runs, b = (r.baseline_attributable || 0) / r.runs, dl = (k * 0.04).toFixed(2);
    t.appendChild(el("div", "srow", `<b>${r.bug}</b><span class="tagsplit ${held ? "held" : ""}">${r.split}</span><div class="bar agent"><i style="--w:${a * 100}%;--d:${dl}s"></i><span>${r.agent_valid_attributable}/${r.runs}</span></div><div class="bar base"><i style="--w:${b * 100}%;--d:${dl}s"></i><span>${r.baseline_attributable || 0}/${r.runs}</span></div><span class="${r.agent_explained / r.runs < 0.5 ? "miss" : ""}">${r.agent_explained}/${r.runs}</span>`));
  });
  $("#scoreFoot").textContent = `Control runs (no bug): the agent reported violations in ${sb.control_detected} of ${sb.control_runs} runs on simulator v1; this exposed a simulator weakness, fixed in v2. Total API cost for this table: ${money(sb.total_cost_usd)}. "Explained" is a keyword check against the hidden ground truth and is deliberately crude.`;
  emit("scoreboard");
}

// ---- wiring ----
function resize() { for (const st of stages) if (st.view) { st.fit(); st.draw(); } }
addEventListener("resize", resize);
for (const st of stages) st.scrub.addEventListener("input", () => { setPlaying(false); st.seek(+st.scrub.value); plotStage = st; buildSignals(); });
$("#playBtn").onclick = () => { setPlaying(!S.playing); emit("click"); if (S.playing) for (const st of stages) if (st.view && st.step >= st.view.steps) st.seek(0); };
$("#speedSel").onchange = (e) => (S.speed = +e.target.value);
$("#sigSel").onchange = (e) => { S.sig = e.target.value; buildSignals(); };
$("#runSel").onchange = (e) => { if (e.target.value === "__live") return; closeLive(); setMode("recorded"); loadRun(e.target.value).then(showAll); };
$("#replayBtn").onclick = () => { emit("click"); startReplay(); };
$("#skipBtn").onclick = () => { emit("click"); showAll(); };
$("#modeRec").onclick = () => { closeLive(); setMode("recorded"); emit("click"); };
$("#modeLive").onclick = () => { setMode("live"); emit("click"); refreshStatus(); };
$("#startBtn").onclick = () => { emit("click"); startLive(); };
function vio(dir) { const st = plotStage; if (!st || !st.view) return; const xs = [...new Set(st.view.violations.map((v) => v.step))].sort((a, b) => a - b); if (!xs.length) return; const cur = Math.round(st.step); const t = dir > 0 ? xs.find((x) => x > cur) : [...xs].reverse().find((x) => x < cur); setPlaying(false); st.seek(t === undefined ? xs[dir > 0 ? xs.length - 1 : 0] : t); }
$("#nextV").onclick = () => vio(1); $("#prevV").onclick = () => vio(-1);
document.querySelectorAll(".tab").forEach((b) => b.addEventListener("click", () => { document.querySelectorAll(".tab").forEach((x) => { const on = x === b; x.classList.toggle("on", on); x.setAttribute("aria-selected", on); }); const sc = b.dataset.tab === "score"; $("#runView").hidden = sc; $("#pipe").hidden = sc; $("#scoreView").hidden = !sc; emit("click"); if (sc) loadScoreboard(); else resize(); }));
addEventListener("keydown", (e) => {
  if (e.target.tagName === "SELECT" || (e.target.tagName === "INPUT" && e.target.type !== "range")) return;
  const st = plotStage;
  if (e.code === "Space") { e.preventDefault(); $("#playBtn").click(); }
  else if (e.key === "ArrowRight" && st) { setPlaying(false); st.seek(st.step + (e.shiftKey ? 10 : 1)); }
  else if (e.key === "ArrowLeft" && st) { setPlaying(false); st.seek(st.step - (e.shiftKey ? 10 : 1)); }
  else if (e.key === "k" || e.key === "K") vio(1); else if (e.key === "j" || e.key === "J") vio(-1);
  else if (e.key === "r" || e.key === "R") $("#replayBtn").click(); else if (e.key === "m" || e.key === "M") $("#soundBtn").click();
});

// seeded bug choices for live demos (the agent never sees them)
for (const b of ["b01", "b02", "b03", "b05", "b06", "b08", "b10", "b04", "b07", "b09", "o01", "o02", "o03", "c01", "c02", "c03"]) { const o = el("option"); o.value = b; o.textContent = ({ o: "orbit domain: hidden bug ", c: "cart-pole domain: hidden bug " }[b[0]] || "rigid-body domain: hidden bug ") + b; $("#bugSel").appendChild(o); }
setMode("recorded"); refreshStatus();
loadIndex().then(() => {
  const p = new URLSearchParams(location.search), m = p.get("mode");
  if (m === "replay") startReplay(); else if (m === "live") { if (p.get("bug")) $("#bugSel").value = p.get("bug"); refreshStatus().then(startLive); } else showAll();
  if (p.get("tab") === "score") document.querySelector('.tab[data-tab="score"]').click();
  requestAnimationFrame(tick);
}).catch((err) => { $("#feed").innerHTML = `<div class="empty">Could not load data/index.json (${esc(err.message)}). Run <code>./build/rc_eval export</code> first.</div>`; });
window.RC = { bus, emit, S };

(function () {
  let simple = true; try { simple = localStorage.getItem("rc_simple") !== "0"; } catch (e) {}
  const apply = () => { document.body.classList.toggle("simple", simple); const b = $("#detailBtn"); b.setAttribute("aria-pressed", String(!simple)); b.classList.toggle("primary", !simple); try { localStorage.setItem("rc_simple", simple ? "1" : "0"); } catch (e) {} };
  $("#detailBtn").onclick = () => { simple = !simple; apply(); };
  addEventListener("keydown", (e) => { if ((e.key === "d" || e.key === "D") && e.target.tagName !== "SELECT" && e.target.tagName !== "INPUT") $("#detailBtn").click(); });
  apply();
})();
