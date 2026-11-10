// SPDX-License-Identifier: GPL-3.0-only
// Development-only startup protocol. Separate from the S4A editor channel.
export const STARTUP = 'coffee-grains-wasm-prepare', VERSION = 1;
export const WASM_MAX = 8 * 1024 * 1024, MEMORY_BYTES = 33554432;
export const STORE_AREAS = [[0x97000, 0xE0000], [0xE5000, 0xE8000], [0xFC000, 0x100000]];
export const attemptOK = n => Number.isInteger(n) && n > 0 && n <= 0xffffffff;
export const envelope = (type, attempt, extra = {}) => ({protocol:STARTUP, version:VERSION, type, attempt, ...extra});
export const matches = (m, attempt) => m && m.protocol === STARTUP && m.version === VERSION && m.attempt === attempt;
export const keysOnly = (m, keys) => m && Object.keys(m).length === keys.length && keys.every(k => Object.hasOwn(m,k));
export function sectorsOK(sectors) {
  if (!sectors || typeof sectors !== 'object' || Array.isArray(sectors)) return false;
  const entries = Object.entries(sectors), seen = new Set();
  return entries.length <= 80 && entries.every(([key,b]) => {
    const off = Number(key);
    if (!Number.isInteger(off) || off % 4096 || seen.has(off) || !(b instanceof Uint8Array) || b.length !== 4096 ||
        !STORE_AREAS.some(([a,e]) => off >= a && off + 4096 <= e)) return false;
    seen.add(off); return true;
  });
}
// Access failure is RAM-only; corrupt existing data is an error, never a blank save.
export function readSavedSectors(storage, key) {
  let raw;
  try { if (!storage) return {sectors:{}, writable:false}; raw = storage.getItem(key); }
  catch { return {sectors:{}, writable:false}; }
  if (raw === null) return {sectors:{}, writable:true};
  if (typeof raw !== 'string' || raw.length > 512 * 1024) throw Error('Saved flash is invalid; original data retained');
  try {
    const encoded = JSON.parse(raw), sectors = Object.create(null);
    if (!encoded || typeof encoded !== 'object' || Array.isArray(encoded) || Object.keys(encoded).length > 80) throw Error();
    for (const [off,b64] of Object.entries(encoded)) {
      if (typeof b64 !== 'string' || b64.length !== 5464) throw Error();
      sectors[off] = Uint8Array.from(atob(b64), c => c.charCodeAt(0));
    }
    if (!sectorsOK(sectors)) throw Error();
    return {sectors, writable:true};
  } catch { throw Error('Saved flash could not be decoded; reload after repairing it. Original data retained'); }
}
export function diagnosticsOK(d) {
  return keysOnly(d,['calls','frames','ms','writes','forbidden','memoryBytes']) && d.calls === 207 && d.frames === 26496 &&
    Number.isInteger(d.ms) && d.ms >= 600 && d.ms <= 610 && d.writes === 0 && d.forbidden === 0 && d.memoryBytes === MEMORY_BYTES;
}
async function fetchBytes(fetcher, signal) {
  const response = await fetcher(new URL('./felucca.wasm', import.meta.url), {signal});
  if (!response.ok) throw Error(`felucca.wasm: ${response.status}`);
  const reader = response.body.getReader(), chunks = []; let length = 0;
  try {
    for (;;) {
      const {done,value} = await reader.read(); if (done) break;
      length += value.byteLength;
      if (length > WASM_MAX) throw Error('WASM exceeds preparation limit');
      chunks.push(value);
    }
  } catch (e) { await reader.cancel().catch(()=>{}); throw e; }
  if (!length) throw Error('Empty WASM');
  const bytes = new Uint8Array(length); let off=0;
  for (const chunk of chunks) { bytes.set(chunk,off); off+=chunk.length; }
  return bytes.buffer;
}
export class PreparedStartup {
  constructor({onState=()=>{},onFrame=()=>{},onContextState=()=>{},loadSaved,master=()=>700,
    WorkerClass=globalThis.Worker,ContextClass=globalThis.AudioContext,NodeClass=globalThis.AudioWorkletNode,
    fetcher=globalThis.fetch,timeouts={prepare:15000,init:5000,start:2000}}) {
    Object.assign(this,{onState,onFrame,onContextState,loadSaved,master,WorkerClass,ContextClass,NodeClass,fetcher,timeouts});
    this.state='IDLE';this.attempt=0;this.worker=null;this.context=null;this.node=null;this.module=null;
  }
  setState(state) { this.state=state;this.onState(state,this.error); }
  current(id) { return this.attempt===id && !['FAILED','CANCELLED'].includes(this.state); }
  check(id) { if (!this.current(id)) throw Error('Startup attempt cancelled'); }
  async bounded(promise,ms,label) {
    let timer;
    try { return await Promise.race([promise,this.cancelled,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error(`${label} timed out; reload required`)),ms);})]); }
    finally { clearTimeout(timer); }
  }
  cleanup() {
    this.abort?.abort();this.worker?.terminate();this.worker=null;this.module=null;
    if(this.node){try{this.node.port.postMessage(envelope("cancel",this.attempt));}catch{} this.node.port.onmessage=null;this.node.port.onmessageerror=null;this.node.onprocessorerror=null;
      try{this.node.disconnect();}catch{} this.node.port.close?.();this.node=null;}
    if(this.context){this.context.onstatechange=null;try{Promise.resolve(this.context.close()).catch(()=>{});}catch{} this.context=null;}
    this.initialFrame=null;
  }
  fail(error) {
    if(['FAILED','CANCELLED'].includes(this.state))return;
    this.error=error instanceof Error?error:Error(String(error));this.setState('FAILED');this.rejectCancel?.(this.error);this.cleanup();
  }
  cancel() {
    if(['FAILED','CANCELLED'].includes(this.state))return;
    this.setState('CANCELLED');this.rejectCancel?.(Error('Startup cancelled; reload required'));this.cleanup();
  }
  async prepare() {
    if(this.state!=='IDLE')throw Error('Preparation already attempted; reload required');
    if(this.attempt===0xffffffff)throw Error('Startup attempt exhausted');
    const id=++this.attempt;this.abort=new AbortController();
    this.cancelled=new Promise((_,reject)=>this.rejectCancel=reject);this.cancelled.catch(()=>{});
    this.setState('PREPARING');
    try {
      this.worker=new this.WorkerClass(new URL('./prepare_worker.js',import.meta.url),{type:'module'});
      let received=false;
      const ready=new Promise(resolve=>{
        this.worker.onmessage=({data:m})=>{
          if(!this.current(id)||m?.attempt!==id)return;
          if(!matches(m,id)){this.fail(Error('Preparation protocol mismatch'));return;}
          if(m.type==='error'){this.fail(Error(typeof m.error==='string'?m.error.slice(0,256):'Worker failed'));return;}
          if(received||this.state!=='PREPARING'||!keysOnly(m,['protocol','version','type','attempt','module','diagnostics'])||
             m.type!=='prepared'||!(m.module instanceof WebAssembly.Module)||!diagnosticsOK(m.diagnostics)) { this.fail(Error('Invalid/duplicate Worker readiness'));return; }
          received=true;resolve(m.module);
        };
        this.worker.onerror=e=>this.fail(Error(e.message||'Preparation Worker failed'));
        this.worker.onmessageerror=()=>this.fail(Error('Preparation message could not be cloned'));
      });ready.catch(()=>{});
      const module=await this.bounded((async()=>{
        const bytes=await fetchBytes(this.fetcher,this.abort.signal);this.check(id);
        this.worker.postMessage(envelope('prepare',id,{bytes}),[bytes]);return await ready;
      })(),this.timeouts.prepare,'Worker preparation');
      this.check(id);this.module=module;this.setState('READY_TO_START');
    }catch(e){this.fail(e);throw e;}
  }
  // Must be called directly by the second user gesture: context/resume precede awaits.
  async start() {
    if(this.state!=='READY_TO_START')throw Error('Worker preparation is not ready');
    const id=this.attempt;this.setState('BOOTING');
    try {
      const sectors=this.loadSaved();if(!sectorsOK(sectors))throw Error('Invalid saved sectors');
      const master=this.master();if(!Number.isInteger(master)||master<0||master>1023)throw Error('Invalid master value');
      this.context=new this.ContextClass({sampleRate:44100,latencyHint:'interactive'});
      const context=this.context;
      const activation=context.state==='suspended'?context.resume():Promise.resolve();
      await this.bounded(activation,this.timeouts.start,'Audio activation');this.check(id);
      if(context.state!=='running')throw Error('Audio requires an explicit start gesture; reload required');
      context.onstatechange=()=>{if(this.current(id))this.onContextState(context.state);};
      await this.bounded(context.audioWorklet.addModule(new URL('./worklet.js',import.meta.url)),this.timeouts.init,'Worklet registration');this.check(id);
      this.node=new this.NodeClass(context,'felucca',{numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[2],processorOptions:{startupMode:'worker-v1',attempt:id}});
      const node=this.node;node.onprocessorerror=()=>this.fail(Error('Audio processor failed; reload required'));
      let initialized,started,receivedStarted=false,startSent=false;
      const ready=new Promise(resolve=>initialized=resolve), running=new Promise(resolve=>started=resolve);
      node.port.onmessageerror=()=>this.fail(Error('Worklet message could not be cloned'));
      node.port.onmessage=({data:m})=>{
        if(!this.current(id))return;
        if(m?.protocol===STARTUP || m?.attempt!==undefined){
          if(m.attempt!==id)return;
          if(!matches(m,id)){this.fail(Error('Worklet startup version mismatch'));return;}
          if(m.type==='error'){this.fail(Error(typeof m.error==='string'?m.error.slice(0,256):'Worklet failed'));return;}
          if(m.type==='initialized'&&this.state==='BOOTING'&&m.rate===44100&&keysOnly(m,['protocol','version','type','attempt','rate'])){this.setState('READY_TO_RENDER');initialized();return;}
          if(m.type==='started'&&startSent&&!receivedStarted&&this.state==='READY_TO_RENDER'&&keysOnly(m,['protocol','version','type','attempt'])){receivedStarted=true;started();return;}
          this.fail(Error('Invalid/duplicate worklet startup reply'));return;
        }
        if(m?.type?.startsWith('editor-')){if(this.state==='RUNNING')this.onFrame(m);return;}
        if(m?.type==='frame'){
          if(this.state==='RUNNING')this.onFrame(m);
          else if(m.sectors)this.fail(Error('Unexpected startup persistence'));
          else this.initialFrame=m;
        }
      };
      // Connect before initialization: Chromium can defer a disconnected worklet's
      // message handling. The worklet remains render-gated until an explicit
      // validated 'start' message after the initialization acknowledgement.
      node.connect(context.destination);
      node.port.postMessage(envelope('initialize',id,{module:this.module,sectors,master}));
      await this.bounded(ready,this.timeouts.init,'Authoritative initialization');this.check(id);
      if(context.state!=='running')throw Error('Audio stopped during initialization; reload required');
      startSent=true;node.port.postMessage(envelope('start',id));
      await this.bounded(running,this.timeouts.start,'First render');this.check(id);this.setState('RUNNING');
      if(this.initialFrame){this.onFrame(this.initialFrame);this.initialFrame=null;}
    }catch(e){this.fail(e);throw e;}
  }
  // Browser interruption recovery only; never used to create a preparation window.
  async resume() {
    if(this.state!=='RUNNING'||!this.context)throw Error('No running simulator');
    try { await this.bounded(this.context.resume(),this.timeouts.start,'Audio resume');this.onContextState(this.context.state); }
    catch(e){this.fail(e);throw e;}
  }
}
