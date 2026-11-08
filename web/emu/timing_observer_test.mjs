// SPDX-License-Identifier: GPL-3.0-only
// Registration/scope host adapters only. Real timing uses the production WASM.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import vm from 'node:vm';
import { spawnSync } from 'node:child_process';
import { pathToFileURL, fileURLToPath } from 'node:url';
const here = path.dirname(fileURLToPath(import.meta.url));
const prefix = fs.readFileSync(path.join(here, 'timing_observer.js'), 'utf8');
let passed = 0;
function check(name, fn) { fn(); passed++; console.log('PASS:', name); }
for (const existing of [false, true]) check(`Window never installs registration (existing=${existing})`, () => {
  const original = () => { throw Error('must not register'); };
  const scope = { window: {}, ...(existing ? { registerProcessor: original } : {}) };
  Object.defineProperty(scope, 'AudioWorkletProcessor', { get() { throw Error('no worklet access in Window'); } });
  vm.runInNewContext(prefix, scope);
  assert.equal(Object.hasOwn(scope, 'registerProcessor'), existing);
  if (existing) assert.equal(scope.registerProcessor, original);
});
check('ordinary Worker does not receive registration', () => {
  const scope = {}; vm.runInNewContext(prefix, scope); assert(!Object.hasOwn(scope, 'registerProcessor'));
});
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'cg-observer-'));
try {
  for (const f of ['worklet.js', 'startup.js', 'editor_transport.js'])
    fs.copyFileSync(path.join(here, f), path.join(tmp, f));
  const file = path.join(tmp, 'worklet.js'); fs.writeFileSync(file, prefix + '\n' + fs.readFileSync(file, 'utf8'));
  globalThis.window = {};
  const module = await import(pathToFileURL(file).href + '?window');
  check('actual module imports in Window without AudioWorklet globals', () => {
    assert.equal(typeof globalThis.registerProcessor, 'undefined'); assert.equal(typeof module.midiKnob, 'function');
  });
  delete globalThis.window;
  let registrations = 0, constructed = 0, Registered, listener, message;
  globalThis.sampleRate = 44100;
  globalThis.AudioWorkletProcessor = class {
    constructor() { constructed++; this.port = { addEventListener(k, f) { listener = f; }, postMessage(m) { message = m; } }; }
  };
  globalThis.registerProcessor = function (name, Class) {
    assert.equal(this, globalThis); assert.equal(name, 'felucca'); registrations++; Registered = Class;
  };
  await import(pathToFileURL(file).href + '?worklet');
  check('one registration, no eager processor construction', () => { assert.equal(registrations, 1); assert.equal(constructed, 0); });
  const instance = new Registered({ processorOptions: { startupMode: 'worker-v1', attempt: 1 } });
  check('one requested instance constructs its production base exactly once', () => {
    assert.equal(constructed, 1); assert.equal(instance.ex, null); assert.equal(typeof instance.port.onmessage, 'function');
  });
  check('metrics cannot export firmware memory or state', () => {
    instance.ex = { memory: 'PRIVATE', project: 'PRIVATE' };
    listener({ data: { type: 'cg-timing-read', ticket: 1, nor: true } });
    assert.deepEqual(Object.keys(message).sort(), ['stats', 'ticket', 'type']);
    assert.deepEqual(Object.keys(message.stats).sort(), ['activeCallbacks', 'callbacks', 'frames', 'maxQuantum', 'minQuantum', 'nonFinite', 'peak', 'rate']);
    assert(!JSON.stringify(message).includes('PRIVATE'));
  });
  check('unrelated messages and invalid tickets are ignored', () => {
    message = null; listener({ data: { type: 'editor-request', ticket: 2 } });
    listener({ data: { type: 'cg-timing-read', ticket: -1 } }); assert.equal(message, null);
  });
  check('no context, node or connection created', () => {
    assert.equal(typeof AudioContext, 'undefined'); assert.equal(typeof AudioWorkletNode, 'undefined');
    assert.equal(registrations, 1); assert.equal(constructed, 1);
  });
  // Execute the actual production process body and the served-copy transform
  // with deterministic output/host adapters. Timing runs never use this adapter.
  const transformed = spawnSync('python3', ['-c',
    "import importlib.util,sys;from pathlib import Path;s=importlib.util.spec_from_file_location('timing',sys.argv[1]);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);print(m.instrument_stages(Path(sys.argv[2]).read_text()),end='')",
    path.join(here, 'timing_test.py'), path.join(here, 'worklet.js')], { encoding: 'utf8' });
  assert.equal(transformed.status, 0, transformed.stderr);
  const stagedFile = path.join(tmp, 'staged.js');
  fs.writeFileSync(stagedFile, prefix + '\n' + transformed.stdout);
  const Original = Registered;
  await import(pathToFileURL(stagedFile).href);
  const Staged = Registered;
  const oldStamp = console.timeStamp, marks = [];
  console.timeStamp = name => marks.push(name);
  globalThis.currentTime = 1;
  function run(Class, active, capped = false) {
    const proc = new Class({ processorOptions: { startupMode: 'worker-v1', attempt: 1 } });
    const calls = [], memory = { buffer: new ArrayBuffer(2048) };
    proc.ex = active ? { memory, web_render(n) { calls.push('render'); new Float32Array(memory.buffer, 0, n).fill(.125); new Float32Array(memory.buffer, 512, n).fill(-.25); }, web_out_l: () => 0, web_out_r: () => 512 } : null;
    proc.editor = { drain() { calls.push('drain'); } };
    proc.publish = () => calls.push('publish'); proc.renderEnabled = true; proc.started = true;
    if (capped) { proc.cgTiming.callbacks = 8192; proc.cgTiming.activeCallbacks = 1; }
    const output = [[new Float32Array(128), new Float32Array(128)]];
    marks.length = 0; const result = proc.process([], output);
    return { calls, output: output[0].map(a => Array.from(a)), result, marks: [...marks], frames: proc.frames };
  }
  try {
    const plain = run(Original, true), staged = run(Staged, true);
    check('stage transform preserves actual process output and dispatch order', () => {
      assert.deepEqual(staged.output, plain.output); assert.deepEqual(staged.calls, plain.calls);
      assert.equal(staged.result, plain.result); assert.equal(staged.frames, plain.frames);
      assert.deepEqual(staged.calls, ['render', 'drain', 'publish']);
    });
    check('active stages bracket render, output buffers and publication', () => {
      assert.deepEqual(staged.marks.filter(n => n.startsWith('CG.stage.')), ['entry','render.begin','render.end','buffers.begin','buffers.end','publish.begin','publish.end','exit'].map(n => 'CG.stage.' + n));
    });
    check('inactive startup remains silent with entry/exit only', () => {
      const inactive = run(Staged, false); assert.deepEqual(inactive.calls, []);
      assert(inactive.output.flat().every(x => x === 0));
      assert.deepEqual(inactive.marks, ['CG.stage.entry','CG.stage.exit']);
    });
    check('marker cap stops instrumentation without stopping rendering', () => {
      const capped = run(Staged, true, true); assert.deepEqual(capped.output, plain.output);
      assert.deepEqual(capped.calls, plain.calls); assert.deepEqual(capped.marks, []);
    });
  } finally { console.timeStamp = oldStamp; delete globalThis.currentTime; }
} finally {
  fs.rmSync(tmp, { recursive: true, force: true });
  for (const k of ['window', 'sampleRate', 'AudioWorkletProcessor', 'registerProcessor']) delete globalThis[k];
}
console.log(`${passed} observer checks passed`);
