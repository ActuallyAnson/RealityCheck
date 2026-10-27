"use strict";
// RealityCheck viewer. Draws only from the domain-agnostic run document produced by `rc_eval export`:
// { original, shrunk: { world, static[], ents[], frames[][], signals{}, violations[] }, report, events[], cost }.
// A new simulator domain adds an entry to RENDERERS and STRIDE for its entity kinds; nothing else changes.

const $ = (s, el = document) => el.querySelector(s);
const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html !== undefined) e.innerHTML = html; return e; };
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
const reduceMotion = matchMedia("(prefers-reduced-motion: reduce)").matches;

// ---- event bus: the sound layer (audio.js) and the visuals listen to the same events ----
const bus = new EventTarget();
const emit = (name, detail) => bus.dispatchEvent(new CustomEvent(name, { detail }));

const PALETTE = ["#5ad1e6", "#b79cff", "#7ee0a0", "#f0b24a", "#ff9ec4", "#9fb4ff", "#c8e06a", "#ffa36c"];
const STRIDE = { circle: 4 };  // numbers per entity in a frame

// ---- per-kind renderers: (ctx, tf, ent, frameSlice, index, trail) ----
const RENDERERS = {
  circle(ctx, tf, ent, f, i, trail) {
    const col = PALETTE[i % PALETTE.length];
    if (trail && trail.length > 1) {
      ctx.lineWidth = 1.5; ctx.strokeStyle = col;
      for (let k = 1; k < trail.length; k++) {
        ctx.globalAlpha = 0.08 + 0.4 * (k / trail.length);
        ctx.beginPath(); ctx.moveTo(tf.x(trail[k - 1][0]), tf.y(trail[k - 1][1])); ctx.lineTo(tf.x(trail[k][0]), tf.y(trail[k][1])); ctx.stroke();
      }
      ctx.globalAlpha = 1;
    }
    const cx = tf.x(f[0]), cy = tf.y(f[1]), r = Math.max(3, ent.r * tf.s);
    ctx.beginPath(); ctx.arc(cx, cy, r, 0, 7); ctx.fillStyle = col + "33"; ctx.fill();
    ctx.lineWidth = ent.ctrl ? 3 : 1.8; ctx.strokeStyle = ent.ctrl ? "#ffffff" : col; ctx.stroke();
    const sp = Math.hypot(f[2], f[3]);
    if (sp > 0.05) {  // velocity arrow, length capped
      const L = Math.min(r + 26, r + sp * tf.s * 0.06), ux = f[2] / sp, uy = -f[3] / sp;
      ctx.beginPath(); ctx.moveTo(cx, cy); ctx.lineTo(cx + ux * L, cy + uy * L); ctx.lineWidth = 1.5; ctx.strokeStyle = col; ctx.stroke();
    }
  },
};
const STATIC = {
  rect(ctx, tf, o) { ctx.fillStyle = "#26303d"; ctx.strokeStyle = "#4a5a6e"; ctx.lineWidth = 1; const x = tf.x(o.x), y = tf.y(o.y + o.h); ctx.fillRect(x, y, o.w * tf.s, o.h * tf.s); ctx.strokeRect(x, y, o.w * tf.s, o.h * tf.s); },
  waypoint(ctx, tf, o) { const x = tf.x(o.x), y = tf.y(o.y); ctx.strokeStyle = "#f0b24a"; ctx.lineWidth = 1.5; ctx.beginPath(); ctx.arc(x, y, 6, 0, 7); ctx.moveTo(x - 9, y); ctx.lineTo(x + 9, y); ctx.moveTo(x, y - 9); ctx.lineTo(x, y + 9); ctx.stroke(); },
};

// ---- Stage: one canvas bound to one view ----
class Stage {
  constructor(canvas, scrub, ticks, info, veil) { Object.assign(this, { canvas, scrub, ticks, info, veil }); this.view = null; this.step = 0; this.flash = null; this.highlight = null; this.small = false; }
  set(view) {
    this.view = view; this.step = 0; this.highlight = null;
    if (!view) { this.scrub.max = 0; this.ticks.innerHTML = ""; this.info.textContent = ""; this.draw(); return; }
    this.scrub.max = view.steps; this.scrub.value = 0;
    this.ticks.innerHTML = "";
    const seen = new Set();
    for (const v of view.violations) { if (seen.has(v.step)) continue; seen.add(v.step); const i = el("i"); i.style.left = (100 * v.step / Math.max(1, view.steps)) + "%"; this.ticks.appendChild(i); }
    const rules = [...new Set(view.violations.map((v) => v.rule))];
    this.info.textContent = `${view.steps} steps, dt ${(+view.dt).toPrecision(3)}${rules.length ? " | " + rules.join(", ") : " | no violation"}`;
    const w = view.world, W = this.canvas.clientWidth || 600;
    this.canvas.width = Math.round(W * devicePixelRatio); this.canvas.height = Math.round(W * devicePixelRatio * w.h / w.w);
    this.draw();
  }
  seek(s) { if (!this.view) return; this.step = Math.max(0, Math.min(this.view.steps, s)); this.scrub.value = Math.round(this.step); this.draw(); }
  draw() {
    const c = this.canvas, ctx = c.getContext("2d"), v = this.view;
    ctx.setTransform(1, 0, 0, 1, 0, 0); ctx.clearRect(0, 0, c.width, c.height);
    if (!v) return;
    const pad = 10 * devicePixelRatio, s = Math.min((c.width - 2 * pad) / v.world.w, (c.height - 2 * pad) / v.world.h);
    const tf = { s, x: (x) => pad + x * s, y: (y) => c.height - pad - y * s };
    ctx.fillStyle = "#0a0e13"; ctx.fillRect(0, 0, c.width, c.height);
    ctx.strokeStyle = "#1a2330"; ctx.lineWidth = 1; ctx.strokeRect(tf.x(0), tf.y(v.world.h), v.world.w * s, v.world.h * s);
    for (let gx = 0; gx <= v.world.w; gx += 2) { ctx.beginPath(); ctx.moveTo(tf.x(gx), tf.y(0)); ctx.lineTo(tf.x(gx), tf.y(v.world.h)); ctx.globalAlpha = 0.35; ctx.stroke(); ctx.globalAlpha = 1; }
    for (const o of v.static) (STATIC[o.k] || (() => {}))(ctx, tf, o);
    const si = Math.round(this.step);
    let off = 0;
    v.ents.forEach((ent, i) => {
      const n = STRIDE[ent.k] || 0, f = v.frames[si].slice(off, off + n);
      const trail = []; for (let k = Math.max(0, si - 70); k <= si; k++) trail.push(v.frames[k].slice(off, off + 2));
      (RENDERERS[ent.k] || (() => {}))(ctx, tf, ent, f, i, trail);
      // a violation on this entity at this step, or a citation highlight, draws a ring
      const hit = v.violations.some((x) => x.step === si && (x.body === i || x.body < 0)) || (this.highlight && this.highlight.body === i && performance.now() < this.highlight.until);
      if (hit && ent.k === "circle") { const t = (performance.now() % 900) / 900; ctx.beginPath(); ctx.arc(tf.x(f[0]), tf.y(f[1]), ent.r * s + 6 + 10 * t, 0, 7); ctx.strokeStyle = `rgba(255,93,93,${1 - t})`; ctx.lineWidth = 2.5; ctx.stroke(); }
      off += n;
    });
    if (!this.small) { ctx.fillStyle = "#7f8c9b"; ctx.font = `${11 * devicePixelRatio}px ui-monospace, Menlo, monospace`; ctx.fillText(`step ${si}`, pad + 4, pad + 12 * devicePixelRatio); }
    if (v.violations.some((x) => x.step <= si)) { ctx.fillStyle = "#ff5d5d"; ctx.font = `700 ${11 * devicePixelRatio}px ui-monospace, Menlo, monospace`; ctx.fillText("VIOLATION", c.width - pad - 80 * devicePixelRatio, pad + 12 * devicePixelRatio); }
  }
}

// ---- application state ----
const S = { index: [], doc: null, playing: false, speed: 1, revealed: { orig: false, shr: false, ev: false }, replay: { i: 0, timer: null, running: false }, sig: null, last: 0 };
const stOrig = new Stage($("#cOrig"), $("#sOrig"), $("#tOrig"), $("#origInfo"), $("#veilOrig"));
const stShr = new Stage($("#cShr"), $("#sShr"), $("#tShr"), $("#shrInfo"), $("#veilShr"));
const stages = [stOrig, stShr];
let plotStage = stShr;

function money(x) { return "$" + x.toFixed(4); }

async function loadIndex() {
  const idx = await (await fetch("data/index.json")).json();
  S.index = idx;
  const sel = $("#runSel"); sel.innerHTML = "";
  for (const r of idx) { const o = el("option"); o.value = r.id; o.textContent = `${r.id}${r.status === "failure_reported" ? "" : " (" + r.status.replace(/_/g, " ") + ")"}`; sel.appendChild(o); }
  const want = new URLSearchParams(location.search).get("run");
  const first = idx.find((r) => r.id === want) || idx.find((r) => r.status === "failure_reported") || idx[0];
  sel.value = first.id;
  await loadRun(first.id);
}

async function loadRun(id) {
  stopReplay(); S.playing = false; $("#playBtn").textContent = "Play";
  S.doc = await (await fetch(`data/${id}.json`)).json();
  S.revealed = { orig: false, shr: false, ev: false };
  $("#feed").innerHTML = FEED_HINT; $("#evidence").innerHTML = '<p class="dim">The report appears when the agent submits a failure that survives validation.</p>';
  $("#statusChip").textContent = ""; $("#statusChip").className = "chip"; solo(false);
  const c = S.doc.cost; $("#meterText").textContent = money(0); $("#meterFill").style.width = "0";
  stOrig.set(S.doc.original || null); stShr.set(S.doc.shrunk || null);
  veil(stOrig, true); veil(stShr, true);
  plotStage = stShr.view ? stShr : stOrig; buildSignals();
  $("#modelName").textContent = c.calls ? `${c.calls} model calls` : "";
  resize();
}

function solo(on) { $("#stages").classList.toggle("solo", on); requestAnimationFrame(resize); }
const FEED_HINT = '<p class="dim">Press <b>Replay agent run</b> (R) to watch the agent hunt, or <b>Skip</b> to see the finished result.</p>';
function veil(stage, on, msg) { if (msg) stage.veil.textContent = msg; stage.veil.classList.toggle("gone", !on); }
function showAll() { stopReplay(); solo(false); S.revealed = { orig: true, shr: true, ev: true }; veil(stOrig, !stOrig.view, "No failing scenario in this run."); veil(stShr, !stShr.view, "The agent did not produce a validated minimal case."); $("#feed").innerHTML = ""; (S.doc.events || []).forEach((e) => addCard(e, true)); renderEvidence(); meter(S.doc.cost.usd); jumpToViolation(stShr.view ? stShr : stOrig, false); }

function meter(usd) { $("#meterText").textContent = money(usd); $("#meterFill").style.width = Math.min(100, 100 * usd / 0.25) + "%"; }

// ---- feed ----
function addCard(e, silent) {
  const f = $("#feed"); let c;
  if (e.type === "assistant_text") { c = el("div", "card think", esc(e.text)); }
  else if (e.type === "tool_call") { c = el("div", "card call", `<span class="tool">${esc(e.tool)}</span><code>${esc(e.args || "")}</code>`); }
  else if (e.type === "tool_result") {
    const viol = e.tool === "run_scenario" && e.violated === true;
    c = el("div", "card res " + (viol ? "viol" : e.ok === false ? "bad" : "ok"), `<span class="tool">${esc(e.tool)} ${viol ? "&middot; rule violated" : e.ok === false ? "&middot; error" : "&middot; ok"}</span><code>${esc(e.summary || "")}</code>`);
  } else if (e.type === "report_accepted") { c = el("div", "card good", `<span class="tool">report accepted</span><code>citations match the recorded states</code>`); }
  else if (e.type === "vision") { c = el("div", "card good", `<span class="tool">vision cross-check</span><code>${esc((e.description || e.text || "").slice(0, 300))}</code>`); }
  else { c = el("div", "card res", `<span class="tool">${esc(e.type)}</span><code>${esc(e.message || e.text || "")}</code>`); }
  f.appendChild(c); f.scrollTop = f.scrollHeight;
}

// ---- replay choreography (the demo story): hunt, violation, shrink, report ----
function stopReplay() { clearTimeout(S.replay.timer); S.replay.running = false; }
function startReplay() {
  stopReplay(); const d = S.doc; if (!d) return;
  S.revealed = { orig: false, shr: false, ev: false }; veil(stOrig, true, "Waiting for the agent to find a failure…"); veil(stShr, true, "The minimal reproduction appears here.");
  $("#feed").innerHTML = ""; $("#evidence").innerHTML = '<p class="dim">The report appears when the agent submits a failure that survives validation.</p>'; $("#statusChip").textContent = ""; $("#statusChip").className = "chip"; meter(0);
  S.replay = { i: 0, timer: null, running: true }; S.playing = false; emit("start"); solo(true);
  step();
  function step() {
    if (!S.replay.running) return;
    const evs = d.events || [], e = evs[S.replay.i];
    if (!e) { finishReplay(); return; }
    addCard(e); emit("event", e); S.replay.i++;
    let delay = e.type === "assistant_text" ? 1100 : e.type === "tool_call" ? 650 : 800;
    if (e.type === "tool_call" && e.tool === "shrink") { emit("shrink-start"); delay = 1500; }
    if (e.type === "tool_result" && e.tool === "run_scenario" && e.violated === true && !S.revealed.orig) {
      S.revealed.orig = true; veil(stOrig, false); emit("violation"); jumpToViolation(stOrig, true); delay = 2600;
    }
    if (e.type === "tool_result" && e.tool === "shrink" && e.ok !== false && stShr.view && !S.revealed.shr) { S.revealed.shr = true; solo(false); veil(stShr, false); stShr.seek(0); emit("shrink-done"); playFrom(stShr, 0); delay = 2000; }
    if (e.type === "report_accepted" && !S.revealed.ev) { S.revealed.ev = true; renderEvidence(); emit("accepted"); delay = 1500; }
    if (e.type === "report_rejected") emit("rejected");
    meter(d.cost.usd * Math.min(1, (S.replay.i) / Math.max(1, evs.length)));
    S.replay.timer = setTimeout(step, delay / S.speed);
  }
}
function finishReplay() { S.replay.running = false; solo(false); if (!S.revealed.ev) { S.revealed.ev = true; renderEvidence(); } veil(stShr, !stShr.view, "The agent did not produce a validated minimal case."); meter(S.doc.cost.usd); emit("done"); }

// ---- playback ----
function playFrom(stage, s) { stage.seek(s); S.playing = true; $("#playBtn").textContent = "Pause"; }
function jumpToViolation(stage, autoplay) { const v = stage.view; if (!v || !v.violations.length) { stage.seek(0); return; } stage.seek(Math.max(0, v.violations[0].step - 18)); if (autoplay && !reduceMotion) { S.playing = true; $("#playBtn").textContent = "Pause"; } plotStage = stage; buildSignals(); }
function tick(now) {
  const dt = Math.min(0.1, (now - S.last) / 1000); S.last = now;
  if (S.playing) {
    let any = false;
    for (const st of stages) if (st.view && (st === stOrig ? S.revealed.orig || !S.replay.running : S.revealed.shr || !S.replay.running)) { if (st.step < st.view.steps) { st.seek(Math.min(st.view.steps, st.step + dt * 60 * S.speed)); any = true; } }
    if (!any) { S.playing = false; $("#playBtn").textContent = "Play"; }
  } else { for (const st of stages) if (st.view && (st.highlight || st.view.violations.some((x) => x.step === Math.round(st.step)))) st.draw(); }
  drawPlot(); readout(); requestAnimationFrame(tick);
}
function readout() { const st = plotStage; if (!st || !st.view) { $("#stepRead").textContent = ""; return; } const si = Math.round(st.step), v = st.view.violations.filter((x) => x.step === si); $("#stepRead").textContent = `step ${si}/${st.view.steps}` + (v.length ? "  " + v.map((x) => x.rule).join(", ") : ""); }

// ---- signal plot ----
function buildSignals() { const sel = $("#sigSel"); sel.innerHTML = ""; const v = plotStage && plotStage.view; if (!v) return; for (const k of Object.keys(v.signals)) { const o = el("option"); o.value = k; o.textContent = k; sel.appendChild(o); } S.sig = sel.value; }
function drawPlot() {
  const cv = $("#plot"), W = cv.clientWidth, H = cv.clientHeight; if (!W) return;
  if (cv.width !== Math.round(W * devicePixelRatio)) { cv.width = Math.round(W * devicePixelRatio); cv.height = Math.round(H * devicePixelRatio); }
  const ctx = cv.getContext("2d"); ctx.setTransform(devicePixelRatio, 0, 0, devicePixelRatio, 0, 0); ctx.clearRect(0, 0, W, H);
  const v = plotStage && plotStage.view; if (!v || !S.sig || !v.signals[S.sig]) return;
  const ys = v.signals[S.sig], n = ys.length, lim = v.signal_limits && v.signal_limits[S.sig];
  let lo = Math.min(...ys), hi = Math.max(...ys); if (lim !== undefined) { lo = Math.min(lo, 0); hi = Math.max(hi, lim * 1.05); } if (hi === lo) hi = lo + 1;
  const m = 26, X = (i) => m + (W - m - 8) * i / Math.max(1, n - 1), Y = (y) => H - 18 - (H - 30) * (y - lo) / (hi - lo);
  ctx.strokeStyle = "#1c2531"; ctx.fillStyle = "#7f8c9b"; ctx.font = "10px ui-monospace, Menlo, monospace"; ctx.lineWidth = 1;
  for (let g = 0; g <= 3; g++) { const y = lo + (hi - lo) * g / 3; ctx.beginPath(); ctx.moveTo(m, Y(y)); ctx.lineTo(W - 8, Y(y)); ctx.stroke(); ctx.fillText(Math.abs(y) >= 100 ? y.toFixed(0) : y.toFixed(1), 2, Y(y) + 3); }
  if (lim !== undefined) { ctx.strokeStyle = "#f0b24a"; ctx.setLineDash([5, 4]); ctx.beginPath(); ctx.moveTo(m, Y(lim)); ctx.lineTo(W - 8, Y(lim)); ctx.stroke(); ctx.setLineDash([]); }
  for (const x of v.violations) { ctx.strokeStyle = "rgba(255,93,93,.65)"; ctx.beginPath(); ctx.moveTo(X(x.step), 4); ctx.lineTo(X(x.step), H - 18); ctx.stroke(); }
  ctx.strokeStyle = "#5ad1e6"; ctx.lineWidth = 1.6; ctx.beginPath(); const stepEvery = Math.max(1, Math.floor(n / (W * 1.5)));
  for (let i = 0; i < n; i += stepEvery) { i === 0 ? ctx.moveTo(X(i), Y(ys[i])) : ctx.lineTo(X(i), Y(ys[i])); } ctx.stroke();
  const cur = Math.round(plotStage.step); ctx.strokeStyle = "#d8dfe8"; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(X(cur), 4); ctx.lineTo(X(cur), H - 18); ctx.stroke();
  ctx.fillStyle = "#d8dfe8"; ctx.beginPath(); ctx.arc(X(cur), Y(ys[cur]), 3, 0, 7); ctx.fill();
}
$("#plot").addEventListener("click", (e) => { const v = plotStage && plotStage.view; if (!v) return; const r = e.target.getBoundingClientRect(); const i = Math.round((e.clientX - r.left - 26) / (r.width - 34) * v.steps); S.playing = false; $("#playBtn").textContent = "Play"; plotStage.seek(i); });

// ---- evidence ----
function frameValue(view, step, body, field) {
  if (!view || step < 0 || step >= view.frames.length) return null;
  if (field === "energy") return view.signals.energy ? view.signals.energy[step] : null;
  if (body < 0) return null;
  let off = 0; for (let i = 0; i < body; i++) off += STRIDE[view.ents[i].k] || 0;
  const f = view.frames[step].slice(off, off + 4); if (f.length < 4) return null;
  return { x: f[0], y: f[1], vx: f[2], vy: f[3], speed: Math.hypot(f[2], f[3]) }[field] ?? null;
}
function renderEvidence() {
  const d = S.doc, ev = $("#evidence"); ev.innerHTML = ""; const r = d.report || {}, view = d.shrunk;
  const chip = $("#statusChip");
  if (d.status === "failure_reported") { chip.textContent = "REPORT VALIDATED"; chip.className = "chip good"; }
  else if (d.status === "failure_not_reported") { chip.textContent = "FAILURE FOUND, NO VALID REPORT"; chip.className = "chip warn"; }
  else { chip.textContent = "NO FAILURE FOUND"; chip.className = "chip bad"; }
  if (d.status !== "failure_reported" || !view) { ev.appendChild(el("div", "sec", `<h3>Outcome</h3>The agent stopped (${esc(r.stop_reason || "n/a")}) without a failure that passed validation. Misses are reported, not hidden.`)); return; }
  const v0 = view.violations[0];
  ev.appendChild(el("div", "sec", `<h3>Violated rule</h3><div class="rule">${esc(r.rule)}</div><div class="dim small">${esc(v0 ? v0.detail : "")}</div>`));
  const ex = el("div", "sec"); ex.innerHTML = "<h3>Explanation (model)</h3>" + esc(r.explanation).replace(/\b(steps?)\s+(\d+)/gi, (m, a, n) => `${a} <a class="step" data-step="${n}">${n}</a>`); ev.appendChild(ex);
  ex.querySelectorAll("a.step").forEach((a) => a.addEventListener("click", () => seekShrunk(+a.dataset.step, -1)));
  const cs = el("div", "sec"); cs.innerHTML = "<h3>Cited states, checked against the recorded run</h3>"; let okN = 0, tot = 0;
  for (const c of r.citations || []) {
    const hasV = c.value !== undefined && c.value !== null && c.field; let mark = "", cls = "";
    if (hasV) { const real = frameValue(view, c.step, c.body, c.field); tot++; if (real !== null) { const ok = Math.abs(real - c.value) <= Math.max(0.02, 0.01 * Math.abs(real)); okN += ok; cls = ok ? "ok" : "bad"; mark = `${ok ? "✓" : "✗"} ${c.field}=${(+real).toFixed(3)}`; } }
    const row = el("div", "cite", `<span class="st">step ${c.step}</span><span class="cl">${esc(c.claim || "")}${c.body >= 0 ? ` <span class="dim">(body ${c.body})</span>` : ""}</span><span class="vv ${cls}">${mark}</span>`);
    row.addEventListener("click", () => seekShrunk(c.step, c.body)); cs.appendChild(row);
  }
  if (tot) cs.appendChild(el("div", "dim small", `${okN} of ${tot} cited values match the simulation state (tolerance max(0.02, 1%)).`)); ev.appendChild(cs);
  if (r.vision_done) ev.appendChild(el("div", "sec", `<h3>Vision cross-check <span class="chip ${r.vision_agrees ? "good" : "warn"}">${r.vision_agrees ? "agrees" : "disagrees"}</span></h3>${esc(r.vision_note || "")}`));
  const strip = el("div", "sec", "<h3>Frame strip around the violation</h3>"), grid = el("div", "strip"); strip.appendChild(grid); ev.appendChild(strip);
  const vs = v0 ? v0.step : 0, picks = [-4, -2, -1, 0, 1, 3].map((o) => Math.max(0, Math.min(view.steps, vs + o)));
  for (const s of picks) { const w = el("div"), cv = el("canvas"); cv.style.width = "100%"; w.appendChild(cv); w.appendChild(el("div", "lab", "step " + s)); grid.appendChild(w); const mini = new Stage(cv, { max: 0, value: 0 }, { innerHTML: "" }, { textContent: "" }, null); mini.small = true; mini.view = view; cv.width = 300 * devicePixelRatio; cv.height = Math.round(300 * devicePixelRatio * view.world.h / view.world.w); mini.step = s; mini.draw(); w.addEventListener("click", () => seekShrunk(s, -1)); }
  const cost = d.cost; ev.appendChild(el("div", "sec", `<h3>Run cost</h3><span class="dim small">${cost.calls} calls &middot; ${cost.tokens_in.toLocaleString()} tokens in &middot; ${cost.tokens_out.toLocaleString()} out &middot; ${money(cost.usd)} &middot; ${(cost.latency_ms / 1000).toFixed(0)} s API time</span>`));
}
function seekShrunk(step, body) { if (!stShr.view) return; S.playing = false; $("#playBtn").textContent = "Play"; plotStage = stShr; buildSignals(); stShr.seek(step); stShr.highlight = body >= 0 ? { body, until: performance.now() + 2500 } : null; emit("cite"); }

// ---- scoreboard ----
async function loadScoreboard() {
  const sb = await (await fetch("data/scoreboard.json")).json();
  $("#scoreTitle").textContent = sb.title; $("#scoreNote").textContent = `Model ${sb.model}. "Valid and attributable" means the report passed citation checks and the shrunk scenario does not also break the unmodified simulator.`;
  const t = $("#scoreTable"); t.innerHTML = ""; t.appendChild(el("div", "srow head", "<span>bug</span><span>split</span><span>agent: valid, attributable</span><span>random baseline: attributable</span><span>explained</span>"));
  for (const r of sb.rows) {
    const held = r.split === "heldout";
    const a = r.agent_valid_attributable / r.runs, b = (r.baseline_attributable || 0) / r.runs;
    const row = el("div", "srow", `<b>${r.bug}</b><span class="tagsplit ${held ? "held" : ""}">${r.split}</span><div class="bar agent"><i style="--w:${a * 100}%;--d:${(sb.rows.indexOf(r) * 0.04).toFixed(2)}s"></i><span>${r.agent_valid_attributable}/${r.runs}</span></div><div class="bar base"><i style="--w:${b * 100}%;--d:${(sb.rows.indexOf(r) * 0.04).toFixed(2)}s"></i><span>${r.baseline_attributable || 0}/${r.runs}</span></div><span class="${r.agent_explained / r.runs < 0.5 ? "miss" : ""}">${r.agent_explained}/${r.runs}</span>`);
    t.appendChild(row);
  }
  $("#scoreFoot").textContent = `Control runs (no bug): the agent reported violations in ${sb.control_detected} of ${sb.control_runs} runs on simulator v1; this exposed a simulator weakness, fixed in v2. Total API cost for this table: ${money(sb.total_cost_usd)}. "Explained" is a keyword check against the hidden ground truth and is deliberately crude.`;
  emit("scoreboard");
}

// ---- wiring ----
function resize() {
  for (const st of stages) {
    if (!st.view) continue;
    const w = st.canvas.clientWidth, vw = st.view.world;
    if (!w) continue;
    st.canvas.width = Math.round(w * devicePixelRatio); st.canvas.height = Math.round(w * devicePixelRatio * vw.h / vw.w); st.draw();
  }
}
addEventListener("resize", resize);
for (const st of stages) st.scrub.addEventListener("input", () => { S.playing = false; $("#playBtn").textContent = "Play"; st.seek(+st.scrub.value); plotStage = st; buildSignals(); });
$("#playBtn").onclick = () => { S.playing = !S.playing; $("#playBtn").textContent = S.playing ? "Pause" : "Play"; emit("click"); if (S.playing) for (const st of stages) if (st.view && st.step >= st.view.steps) st.seek(0); };
$("#speedSel").onchange = (e) => (S.speed = +e.target.value);
$("#sigSel").onchange = (e) => (S.sig = e.target.value);
$("#runSel").onchange = (e) => loadRun(e.target.value);
$("#replayBtn").onclick = () => { emit("click"); startReplay(); };
$("#skipBtn").onclick = () => { emit("click"); showAll(); };
function vio(dir) { const st = plotStage; if (!st || !st.view) return; const xs = [...new Set(st.view.violations.map((v) => v.step))].sort((a, b) => a - b); if (!xs.length) return; const cur = Math.round(st.step); const t = dir > 0 ? xs.find((x) => x > cur) : [...xs].reverse().find((x) => x < cur); S.playing = false; $("#playBtn").textContent = "Play"; st.seek(t === undefined ? xs[dir > 0 ? xs.length - 1 : 0] : t); }
$("#nextV").onclick = () => vio(1); $("#prevV").onclick = () => vio(-1);
document.querySelectorAll(".tab").forEach((b) => b.addEventListener("click", () => { document.querySelectorAll(".tab").forEach((x) => x.classList.toggle("on", x === b)); const sc = b.dataset.tab === "score"; $("#runView").hidden = sc; $("#scoreView").hidden = !sc; emit("click"); if (sc) loadScoreboard(); else resize(); }));
addEventListener("keydown", (e) => {
  if (e.target.tagName === "SELECT" || e.target.tagName === "INPUT" && e.target.type !== "range") return;
  const st = plotStage;
  if (e.code === "Space") { e.preventDefault(); $("#playBtn").click(); }
  else if (e.key === "ArrowRight" && st) { S.playing = false; st.seek(st.step + (e.shiftKey ? 10 : 1)); }
  else if (e.key === "ArrowLeft" && st) { S.playing = false; st.seek(st.step - (e.shiftKey ? 10 : 1)); }
  else if (e.key === "k" || e.key === "K") vio(1); else if (e.key === "j" || e.key === "J") vio(-1);
  else if (e.key === "r" || e.key === "R") $("#replayBtn").click(); else if (e.key === "m" || e.key === "M") $("#soundBtn").click();
});
loadIndex().then(() => { const m = new URLSearchParams(location.search).get("mode"); if (m === "skip") showAll(); else if (m === "replay") startReplay(); if (new URLSearchParams(location.search).get("tab") === "score") document.querySelector('.tab[data-tab="score"]').click(); requestAnimationFrame(tick); }).catch((err) => { $("#feed").innerHTML = `<div class="card viol">Could not load data/index.json (${esc(err.message)}). Run <code>./build/rc_eval export</code> first.</div>`; });
window.RC = { bus, emit, S };
