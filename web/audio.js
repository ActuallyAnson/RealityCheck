"use strict";
// Synthesized sound effects (WebAudio only: oscillators, filtered noise, envelopes), so there are no audio assets or
// licences. Every sound is triggered from the same event bus as the visuals, so live and recorded runs sound identical.
// Starts muted; browsers require a user gesture before audio can start, so the first click on the sound button
// (or the M key) creates the AudioContext.
(function () {
  let ctx = null, master = null, drone = null, on = false;
  const btn = document.getElementById("soundBtn");
  const SCALE = [0, 3, 5, 7, 10, 12, 15, 17];  // minor pentatonic-ish steps over A3
  const hz = (semi) => 220 * Math.pow(2, semi / 12);
  const hash = (s) => { let h = 0; for (const c of s) h = (h * 31 + c.charCodeAt(0)) >>> 0; return h; };

  function ensure() {
    if (ctx) return true;
    const AC = window.AudioContext || window.webkitAudioContext; if (!AC) return false;
    ctx = new AC(); master = ctx.createGain(); master.gain.value = 0.55;
    const comp = ctx.createDynamicsCompressor(); master.connect(comp); comp.connect(ctx.destination);
    return true;
  }
  function env(g, t, a, d, peak, end) { g.gain.cancelScheduledValues(t); g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(peak, t + a); g.gain.exponentialRampToValueAtTime(end || 0.0001, t + a + d); }
  function osc(type, f, t, a, d, peak, dest, f2) {
    const o = ctx.createOscillator(), g = ctx.createGain(); o.type = type; o.frequency.setValueAtTime(f, t);
    if (f2) o.frequency.exponentialRampToValueAtTime(f2, t + a + d);
    env(g, t, a, d, peak); o.connect(g); g.connect(dest || master); o.start(t); o.stop(t + a + d + 0.05); return o;
  }
  function noise(t, d, peak, lo, hi, dest) {
    const n = Math.floor(ctx.sampleRate * d), buf = ctx.createBuffer(1, n, ctx.sampleRate), ch = buf.getChannelData(0);
    for (let i = 0; i < n; i++) ch[i] = Math.random() * 2 - 1;
    const src = ctx.createBufferSource(); src.buffer = buf; const f = ctx.createBiquadFilter(); f.type = "bandpass"; f.frequency.setValueAtTime(hi, t); f.frequency.exponentialRampToValueAtTime(lo, t + d); f.Q.value = 0.9;
    const g = ctx.createGain(); env(g, t, 0.004, d, peak); src.connect(f); f.connect(g); g.connect(dest || master); src.start(t);
  }

  const S = {
    ping() { const t = ctx.currentTime; osc("sine", 880, t, 0.005, 0.18, 0.05); osc("sine", 1320, t + 0.02, 0.005, 0.14, 0.025); },
    call(tool) { const t = ctx.currentTime, f = hz(SCALE[hash(tool || "") % SCALE.length] + 12); osc("triangle", f, t, 0.003, 0.07, 0.06); },
    result(ok) { const t = ctx.currentTime; if (ok === false) { osc("sine", 110, t, 0.004, 0.2, 0.12, null, 70); return; } osc("sine", 660, t, 0.004, 0.09, 0.045); osc("sine", 990, t + 0.05, 0.004, 0.11, 0.04); },
    violation() {
      const t = ctx.currentTime;
      osc("sine", 70, t, 0.003, 0.9, 0.5, null, 38);                       // sub hit
      noise(t, 0.35, 0.28, 300, 5000);                                      // glitch burst
      osc("sawtooth", hz(0 + 12), t, 0.01, 0.7, 0.07); osc("sawtooth", hz(6 + 12), t, 0.01, 0.7, 0.07);  // tritone stab
      osc("square", 1480, t + 0.04, 0.002, 0.05, 0.04); osc("square", 1245, t + 0.1, 0.002, 0.05, 0.035);
    },
    shrinkStart() {
      const t = ctx.currentTime, o = ctx.createOscillator(), f = ctx.createBiquadFilter(), g = ctx.createGain();
      o.type = "sawtooth"; o.frequency.setValueAtTime(330, t); o.frequency.exponentialRampToValueAtTime(70, t + 1.4);
      f.type = "lowpass"; f.frequency.setValueAtTime(3200, t); f.frequency.exponentialRampToValueAtTime(180, t + 1.4); f.Q.value = 6;
      env(g, t, 0.05, 1.35, 0.09); o.connect(f); f.connect(g); g.connect(master); o.start(t); o.stop(t + 1.5);
      let at = 0, gap = 0.2; while (at < 1.3) { osc("square", 1500 - at * 600, t + at, 0.001, 0.025, 0.03); at += gap; gap = Math.max(0.045, gap * 0.8); }  // ticks speed up
    },
    shrinkDone() { const t = ctx.currentTime; osc("square", 520, t, 0.001, 0.04, 0.05); osc("square", 780, t + 0.06, 0.001, 0.04, 0.05); osc("sine", 1568, t + 0.12, 0.004, 0.4, 0.07); },
    accepted() { const t = ctx.currentTime; [0, 4, 7, 12].forEach((s, i) => { osc("triangle", hz(s + 12), t + i * 0.09, 0.01, 0.9, 0.07); osc("sine", hz(s + 24), t + i * 0.09, 0.01, 0.6, 0.03); }); },
    rejected() { const t = ctx.currentTime; osc("square", 120, t, 0.004, 0.28, 0.1); osc("square", 113, t, 0.004, 0.28, 0.08); },
    click() { const t = ctx.currentTime; osc("sine", 1200, t, 0.001, 0.03, 0.04); },
    cite() { const t = ctx.currentTime; osc("triangle", 1046, t, 0.002, 0.06, 0.05); },
    score() { const t = ctx.currentTime; for (let i = 0; i < 10; i++) osc("triangle", hz(SCALE[i % SCALE.length] + 12 + (i > 7 ? 12 : 0)), t + i * 0.07, 0.002, 0.08, 0.04); },
  };

  function droneStart() {
    if (!ctx || drone) return;
    const t = ctx.currentTime, g = ctx.createGain(), lfo = ctx.createOscillator(), lg = ctx.createGain();
    g.gain.setValueAtTime(0.0001, t); g.gain.linearRampToValueAtTime(0.03, t + 2.5);
    const a = ctx.createOscillator(), b = ctx.createOscillator(); a.type = "sine"; b.type = "sine"; a.frequency.value = 55; b.frequency.value = 82.6;
    lfo.frequency.value = 0.12; lg.gain.value = 0.012; lfo.connect(lg); lg.connect(g.gain);
    a.connect(g); b.connect(g); g.connect(master); a.start(); b.start(); lfo.start(); drone = { g, nodes: [a, b, lfo] };
  }
  function droneStop() { if (!drone) return; const t = ctx.currentTime, d = drone; d.g.gain.cancelScheduledValues(t); d.g.gain.setValueAtTime(d.g.gain.value, t); d.g.gain.linearRampToValueAtTime(0.0001, t + 1.2); setTimeout(() => d.nodes.forEach((n) => { try { n.stop(); } catch (e) {} }), 1400); drone = null; }

  function play(fn, ...args) { if (on && ctx) { try { S[fn](...args); } catch (e) {} } }
  function setOn(v) {
    on = v && ensure();
    if (on && ctx.state === "suspended") ctx.resume();
    btn.setAttribute("aria-pressed", String(on)); const t = document.getElementById("soundTxt"); if (t) t.textContent = on ? "sound on" : "sound off";
    if (!on) droneStop(); else play("click");
    try { localStorage.setItem("rc_sound", on ? "1" : "0"); } catch (e) {}
  }
  btn.addEventListener("click", () => setOn(!on));

  const bus = window.RC ? window.RC.bus : null;
  function wire(b) {
    b.addEventListener("event", (ev) => { const e = ev.detail; if (e.type === "assistant_text") play("ping"); else if (e.type === "tool_call") play("call", e.tool); else if (e.type === "tool_result") play("result", e.ok); });
    b.addEventListener("violation", () => play("violation"));
    b.addEventListener("shrink-start", () => play("shrinkStart"));
    b.addEventListener("shrink-done", () => play("shrinkDone"));
    b.addEventListener("accepted", () => play("accepted"));
    b.addEventListener("rejected", () => play("rejected"));
    b.addEventListener("cite", () => play("cite"));
    b.addEventListener("click", () => play("click"));
    b.addEventListener("scoreboard", () => play("score"));
    b.addEventListener("start", () => { if (on) droneStart(); });
    b.addEventListener("done", () => droneStop());
  }
  // app.js loads after this file; attach once its bus exists.
  const wait = setInterval(() => { if (window.RC) { clearInterval(wait); wire(window.RC.bus); } }, 30);
})();
