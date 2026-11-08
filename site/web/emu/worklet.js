// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Felucca in an AudioWorklet: felucca.wasm is the whole FM-1 firmware against a simulated FM-1
// (web/emu/felucca_web.c). Each render quantum asks it for 128 frames, which runs the device's clock forward;
// between quanta the page's input goes in and, about 30 times a second (4 when the page is hidden), the screen,
// the lights and any changed flash go out. After X0X's web/emu/worklet.js (charlesvestal/fm1-x0x, GPL-3.0).

// The flash the page keeps: storage.c's areas (projects, user presets and their FM6 patches, the user sample
// slots 0x97000..0xDFFFF; the autosave 0xE5000..0xE6FFF (1.2) and FM6's voice bank 0xE7000 (1.4.1); the settings and
// the second FM6 copy 0xFC000..0xFFFFF), only the sectors that hold something (an erased sector is all 0xFF). {offset: bytes}
import { WasmReadonlyEndpoint } from "./editor_transport.js";
import {STARTUP, MEMORY_BYTES, STORE_AREAS, attemptOK, matches, keysOnly, envelope, sectorsOK} from "./startup.js";
export {STORE_AREAS} from "./startup.js";

export function storeSectors(nor) {
  const out = {};
  for (const [a, b] of STORE_AREAS)
    for (let off = a; off < b; off += 4096) {
      const s = nor.subarray(off, off + 4096);
      if (s.some((v) => v !== 0xFF)) out[off] = s.slice();
    }
  return out;
}
export function restoreSectors(nor, sectors) {
  for (const [k, v] of Object.entries(sectors || {})) {
    const off = +k, b = v instanceof Uint8Array ? v : new Uint8Array(v);
    if (STORE_AREAS.some(([a, e]) => off >= a && off + 4096 <= e && off % 4096 === 0) && b.length === 4096)
      nor.set(b, off);
  }
}

export const HOLD_MS = 24;

// The panel's knobs from a MIDI controller (the page's Web MIDI in): CC 20..26 turn SELECT, ALGORITHM, PRESETS,
// KNOB 1..4 (the encoder roles), a detent a step; CC 27 sets MASTER (a pot on the FM-1: absolute in every mode). The
// firmware's CC map (midi_control.c) has none of them, so they stay here. mode, as the controller sends a turn:
//   abs  absolute (the default): by how far the value moves, so it takes over from anywhere without a jump; last:
//        the value each knob CC sent before (the first one only takes its place)
//   bin  relative, 64 +- n (65 = +1, 63 = -1)
//   twos relative, two's complement (1 = +1, 127 = -1)
//   sign relative, sign bit (1 = +1, 65 = -1)
// -> {role, n} | {master: 0..1023} | null (not one of them: on to the firmware)
export const KNOB_CC = 20, MASTER_CC = 27, KNOB_MODES = ["abs", "bin", "twos", "sign"];
export function midiKnob(last, cc, value, mode = "abs") {
  if (cc === MASTER_CC) return { master: Math.round(value * 1023 / 127) };
  const role = cc - KNOB_CC;
  if (role < 0 || role >= 7) return null;
  if (mode === "bin") return { role, n: value - 64 };
  if (mode === "twos") return { role, n: value < 64 ? value : value - 128 };
  if (mode === "sign") return { role, n: value < 64 ? value : 64 - value };
  const was = last[role];
  last[role] = value;
  return { role, n: was === undefined ? 0 : value - was };
}

if (typeof registerProcessor === "function") {
  const clock = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();
  class Felucca extends AudioWorkletProcessor {
    constructor(options = {}) {
      super();
      const config=options.processorOptions || {};
      if(config.startupMode!==undefined && config.startupMode!=="worker-v1")throw Error("Unknown startup mode");
      this.workerMode=config.startupMode==="worker-v1";
      this.attempt=config.attempt;
      if(this.workerMode && !attemptOK(this.attempt))throw Error("Invalid startup attempt");
      this.loading=false;this.loadFailed=false;this.renderEnabled=!this.workerMode;this.started=false;
      this.ex = null;
      this.sentDraws = -1;
      this.sentWrites = -1;
      this.sentLeds = "";
      this.lastFrame = 0;
      this.period = 1 / 30;
      this.busy = 0;
      this.frames = 0;
      this.queue = [];                                // button / key changes, each held >= HOLD_MS of device time
      this.nextOk = 0;
      this.port.onmessage = (e) => this.onMessage(e.data);
    }

    startupError(message) {
      this.loadFailed=true;this.renderEnabled=false;this.ex=null;
      this.port.postMessage(this.workerMode ? envelope('error',this.attempt,{error:String(message).slice(0,256)}) : {type:'error',message:String(message).slice(0,256)});
    }
    async onMessage(m) {
      if(!m || typeof m!=='object')return;
      if(this.workerMode && m.protocol===STARTUP) {
        if(m.attempt!==this.attempt)return; // stale startup generation
        if(!matches(m,this.attempt)){this.startupError('Startup version mismatch');return;}
        if(m.type==='cancel' && keysOnly(m,['protocol','version','type','attempt'])) {
          this.loadFailed=true;this.renderEnabled=false;this.ex=null;return;
        }
        if(m.type==='start') {
          if(!keysOnly(m,['protocol','version','type','attempt']) || !this.ex || this.loadFailed || this.renderEnabled) this.startupError('Invalid/duplicate render start');
          else this.renderEnabled=true;
          return;
        }
        if(m.type!=='initialize'){this.startupError('Unknown startup message');return;}
      }
      const initialize=this.workerMode ? m.protocol===STARTUP && m.type==='initialize' : m.type==='load';
      if(this.workerMode && m.type==='load'){this.startupError('Worker startup requires compiled module');return;}
      if(initialize) {
        if(this.workerMode && m.attempt!==this.attempt)return;
        if(this.loading || this.loadFailed){this.startupError('Duplicate initialization');return;}
        this.loading=true; // claim before await: never allocate overlapping authorities
        try {
          if(this.workerMode && (!matches(m,this.attempt)||!keysOnly(m,['protocol','version','type','attempt','module','sectors','master']) ||
             !(m.module instanceof WebAssembly.Module)||!sectorsOK(m.sectors)||!Number.isInteger(m.master)||m.master<0||m.master>1023))throw Error('Invalid authoritative initialization');
          const result=await WebAssembly.instantiate(this.workerMode ? m.module : m.wasm, {});
          if(this.loadFailed)return;
          const instance=this.workerMode ? result : result.instance, ex=instance.exports;
          if(this.workerMode && (ex.memory.buffer.byteLength!==MEMORY_BYTES || (typeof SharedArrayBuffer!=='undefined' && ex.memory.buffer instanceof SharedArrayBuffer)))throw Error('Invalid authoritative memory');
          ex._initialize();
          ex.web_nor_erase();
          restoreSectors(new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()), m.sectors);
          ex.web_master(m.master ?? 700);
          ex.web_boot();
          if(this.workerMode && (ex.web_now_ms()!==0 || ex.web_playing()!==0 || ex.web_sample_rate()!==44100 || ex.web_flash_writes_count()!==0 || ex.web_editor_forbidden_count()!==0))throw Error('Authoritative boot invariant failed');
          this.ex = ex;
          this.editor = new WasmReadonlyEndpoint(ex, m => this.port.postMessage(m));
          this.sentWrites = ex.web_flash_writes_count();
          this.port.postMessage(this.workerMode ? envelope('initialized',this.attempt,{rate:ex.web_sample_rate()}) : {type:'ready',rate:ex.web_sample_rate()});
        } catch(err) { this.startupError(err?.message||err); }
        return;
      }
      const ex = this.ex;
      if (!ex) return;
      if(this.workerMode && !this.started)return;
      if (m.type?.startsWith("editor-")) { this.editor.receive(m); return; }
      if (m.type === "buttons" || m.type === "keys") this.queue.push(m);
      else if (m.type === "enc") ex.web_enc(m.role, m.n | 0);
      else if (m.type === "master") ex.web_master(m.value | 0);
      else if (m.type === "midi") ex.web_midi(m.data[0] | 0, m.data[1] | 0, m.data[2] | 0);
      else if (m.type === "visible") this.period = m.on ? 1 / 30 : 1 / 4;
    }

    publish() {
      const ex = this.ex, mem = ex.memory.buffer;
      const msg = { type: "frame" };
      const leds = [ex.web_lit_buttons(), ex.web_lit_keys(), ex.web_dim_buttons(), ex.web_dim_keys(), ex.web_dim_level(),
                    ex.web_breath_buttons(), ex.web_breath_keys(), ex.web_mid_keys ? ex.web_mid_keys() : 0];
      const an = ex.web_anim_levels ? ex.web_anim_levels() : 0;   // the power-on sweep: each LED's level
      const anim = an ? Array.from(new Uint8Array(mem, an, 41)) : null;
      const key = leds.join(",") + (anim ? ";" + anim.join(",") : "");
      if (key !== this.sentLeds) {
        this.sentLeds = key;
        msg.leds = leds;
        msg.anim = anim;
      }
      if (this.frames >= 44100) {                     // the share of real time spent running the device
        msg.load = this.busy / (this.frames / 44.1);
        this.busy = this.frames = 0;
      }
      const transfer = [];
      const draws = ex.web_screen_draws();
      if (draws !== this.sentDraws) {
        this.sentDraws = draws;
        msg.fb = new Uint16Array(mem, ex.web_screen(), 240 * 240).slice();
        transfer.push(msg.fb.buffer);
      }
      const writes = ex.web_flash_writes_count();
      if (writes !== this.sentWrites) {               // a save is done within one main-loop pass: whole here
        this.sentWrites = writes;
        msg.sectors = storeSectors(new Uint8Array(mem, ex.web_nor(), ex.web_nor_size()));
      }
      if (msg.leds || msg.fb || msg.sectors || msg.load !== undefined) this.port.postMessage(msg, transfer);
    }

    // A click's down and up can arrive within one render quantum, but the FM-1's contacts never close for less than
    // its debounce, and the UI reads them once a frame: each change of the buttons or the keys stays HOLD_MS of the
    // device's time before the next one goes in (a tap: down for a frame and more, then up).
    input() {
      const ex = this.ex, now = ex.web_now_ms();
      while (this.queue.length && now - this.nextOk >= 0) {
        const m = this.queue.shift();
        if (m.type === "buttons") ex.web_buttons(m.mask >>> 0);
        else ex.web_keys(m.mask >>> 0);
        this.nextOk = now + HOLD_MS;
      }
    }

    process(inputs, outputs) {
      const ex = this.ex;
      if (!ex || !this.renderEnabled || this.loadFailed) return true;
      if (this.queue.length) this.input();
      const out = outputs[0], n = out[0].length;
      const t0 = clock();
      ex.web_render(n);
      if(this.workerMode && !this.started){this.started=true;this.port.postMessage(envelope("started",this.attempt));}
      this.editor.drain();
      this.busy += clock() - t0;
      this.frames += n;
      out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
      if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
      if (currentTime - this.lastFrame >= this.period) {
        this.lastFrame = currentTime;
        this.publish();
      }
      return true;
    }
  }
  registerProcessor("felucca", Felucca);
}
