// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {PreparedStartup,readSavedSectors,sectorsOK,envelope,VERSION,STARTUP,MEMORY_BYTES,WASM_MAX} from './startup.js';
import {prepareWasm} from './prepare_worker.js';
let pass=0;
function check(name,fn){fn();console.log('ok:',name);pass++;}
async function test(name,fn){await fn();console.log('ok:',name);pass++;}
const realWasm=WebAssembly,emptyBytes=new Uint8Array([0,97,115,109,1,0,0,0]),module=new WebAssembly.Module(emptyBytes);
const diagnostics={calls:207,frames:26496,ms:601,writes:0,forbidden:0,memoryBytes:MEMORY_BYTES};
const publicationGetters=['web_lit_buttons','web_lit_keys','web_dim_buttons','web_dim_keys','web_dim_level','web_breath_buttons','web_breath_keys','web_mid_keys','web_out_l','web_out_r','web_screen_draws','web_screen','web_nor','web_nor_size','web_flash_writes_count'];
const sector=new Uint8Array(4096).fill(17),encoded=JSON.stringify({'655360':Buffer.from(sector).toString('base64')});
check('saved sectors decode without changing storage',()=>{let writes=0;const s={getItem:()=>encoded,setItem:()=>writes++};assert.deepEqual(readSavedSectors(s,'x').sectors['655360'],sector);assert.equal(writes,0);});
for(const raw of ['', '{', 'null','[]','{"655360":"bad"}',JSON.stringify({'0':Buffer.from(sector).toString('base64')}),JSON.stringify({'655360':Buffer.from(sector).toString('base64'),'0xA0000':Buffer.from(sector).toString('base64')})])
 check('malformed existing flash is never blank fallback',()=>assert.throws(()=>readSavedSectors({getItem:()=>raw},'x')));
check('absent storage and unavailable storage are distinct',()=>{assert.deepEqual(readSavedSectors({getItem:()=>null},'x'),{sectors:{},writable:true});assert.equal(readSavedSectors({getItem:()=>{throw Error();}},'x').writable,false);});
check('sector boundaries and duplicate normalized offsets',()=>{assert(sectorsOK({'655360':sector}));assert(!sectorsOK({'1048576':sector}));assert(!sectorsOK({'655361':sector}));assert(!sectorsOK({'655360':new Uint8Array(1)}));assert(!sectorsOK({'655360':sector,'0xA0000':sector}));});

class FakeWorker {
 static all=[];
 constructor(){this.posts=[];this.terminated=false;FakeWorker.all.push(this);}
 postMessage(m){this.posts.push(m);}
 terminate(){this.terminated=true;}
 emit(m){this.onmessage?.({data:m});}
}
class FakeContext {
 static all=[];
 constructor(){this.state='running';this.destination={};this.closed=false;this.audioWorklet={addModule:async()=>{}};FakeContext.all.push(this);}
 resume(){this.state='running';return Promise.resolve();}
 close(){this.closed=true;return Promise.resolve();}
}
class FakeNode {
 static all=[];
 constructor(context,name,options){this.context=context;this.options=options;this.connects=0;this.posts=[];this.port={postMessage:m=>this.posts.push(m),close:()=>this.closed=true};FakeNode.all.push(this);}
 connect(){this.connects++;}
 disconnect(){this.disconnected=true;}
 emit(m){this.port.onmessage?.({data:m});}
}
const tick=()=>new Promise(r=>setImmediate(r));
const fetcher=async()=>({ok:true,body:new ReadableStream({start(c){c.enqueue(emptyBytes);c.close();}})});
function fixture(extra={}){return new PreparedStartup({loadSaved:()=>({}),WorkerClass:FakeWorker,ContextClass:FakeContext,NodeClass:FakeNode,fetcher,timeouts:{prepare:100,init:100,start:100},...extra});}
async function prepared(s){const p=s.prepare();await tick();const w=s.worker;w.emit(envelope('prepared',s.attempt,{module,diagnostics}));await p;return w;}
async function initializing(s){const p=s.start();await tick();return {p,n:s.node};}
await test('preparation has no AudioContext; explicit start and first-render ack gate readiness',async()=>{
 const before=FakeContext.all.length,s=fixture();await prepared(s);assert.equal(FakeContext.all.length,before);assert.equal(s.state,'READY_TO_START');
 const {p,n}=await initializing(s);assert.equal(n.connects,0);assert.equal(n.posts[0].module,module);assert.equal(s.state,'BOOTING');
 n.emit(envelope('initialized',s.attempt,{rate:44100}));await tick();assert.equal(n.connects,1);assert.equal(s.state,'READY_TO_RENDER');
 n.emit(envelope('started',s.attempt));await p;assert.equal(s.state,'RUNNING');s.cancel();assert(n.closed);assert(s.state==='CANCELLED');
});
await test('stale Worker and worklet attempts cannot activate startup',async()=>{
 const s=fixture(),p=s.prepare();await tick();s.worker.emit(envelope('prepared',s.attempt+1,{module,diagnostics}));assert.equal(s.state,'PREPARING');s.worker.emit(envelope('prepared',s.attempt,{module,diagnostics}));await p;
 const {p:q,n}=await initializing(s);n.emit(envelope('initialized',s.attempt+1,{rate:44100}));assert.equal(n.connects,0);s.cancel();await assert.rejects(q);
});
await test('cancel preparation terminates Worker and rejects pending work',async()=>{const s=fixture(),p=s.prepare();await tick();const w=s.worker,late=w.onmessage;s.cancel();await assert.rejects(p);late({data:envelope('prepared',1,{module,diagnostics})});assert.equal(s.state,'CANCELLED');assert(w.terminated);assert.equal(s.context,null);});
await test('cancel during context registration cannot allocate a late node',async()=>{let release;class Context extends FakeContext {constructor(){super();this.audioWorklet.addModule=()=>new Promise(r=>release=r);}}const s=fixture({ContextClass:Context});await prepared(s);const p=s.start();await tick();const context=s.context;s.cancel();await assert.rejects(p);release();await tick();assert.equal(s.node,null);assert(context.closed);});
await test('Worker protocol failure has no fallback and requires reload',async()=>{const s=fixture(),p=s.prepare();await tick();s.worker.emit({...envelope('prepared',s.attempt,{module,diagnostics}),version:VERSION+1});await assert.rejects(p);assert.equal(s.state,'FAILED');await assert.rejects(s.prepare());assert.equal(s.context,null);});
await test('duplicate Worker readiness fails closed',async()=>{const s=fixture();const w=await prepared(s);w.emit(envelope('prepared',s.attempt,{module,diagnostics}));assert.equal(s.state,'FAILED');assert(w.terminated);});
await test('missing, oversized and corrupt WASM fail before preparation',async()=>{
 for(const f of [async()=>({ok:false,status:404}),async()=>({ok:true,body:new ReadableStream({start(c){c.enqueue(new Uint8Array(WASM_MAX+1));c.close();}})})]){const s=fixture({fetcher:f});await assert.rejects(s.prepare());assert.equal(s.state,'FAILED');assert.equal(s.context,null);}
});
await test('timeout cleans up without retry or storage writes',async()=>{const s=fixture({timeouts:{prepare:5,init:5,start:5}}),p=s.prepare();await tick();const w=s.worker;await assert.rejects(p,/timed out/);assert(w.terminated);assert.equal(s.state,'FAILED');});
await test('invalid saved state fails before context allocation',async()=>{const s=fixture({loadSaved:()=>{throw Error('corrupt saves');}});await prepared(s);const before=FakeContext.all.length;await assert.rejects(s.start(),/corrupt saves/);assert.equal(FakeContext.all.length,before);});
await test('duplicate readiness, wrong rate and premature persistence never reach RUNNING',async()=>{
 for(const message of [envelope('initialized',1,{rate:48000}),{type:'frame',sectors:{}}]){const s=fixture();await prepared(s);const {p,n}=await initializing(s);n.emit(message);await assert.rejects(p);assert.equal(s.state,'FAILED');}
});
await test('Worker crash/message-clone errors cancel pending startup',async()=>{for(const kind of ['onerror','onmessageerror']){const s=fixture(),p=s.prepare();await tick();s.worker[kind]({message:'crash'});await assert.rejects(p);assert.equal(s.state,'FAILED');}});

await test('current Worker protocol and malformed replies fail after readiness; stale replies do not',async()=>{
 for(const bad of [{version:VERSION+1},{protocol:'wrong'},{type:'unexpected'},{diagnostics:{}}]){
  const s=fixture(),w=await prepared(s);w.emit({...envelope('prepared',s.attempt+1,{module,diagnostics}),version:99});assert.equal(s.state,'READY_TO_START');
  w.emit({...envelope('prepared',s.attempt,{module,diagnostics}),...bad});assert.equal(s.state,'FAILED');assert(w.terminated);await assert.rejects(s.start());assert.equal(s.context,null);
 }
});
await test('current protocol mismatch after RUNNING tears down authority, stale versions do not',async()=>{
 for(const source of ['worker','worklet']){
  const s=fixture(),w=await prepared(s),{p,n}=await initializing(s);n.emit(envelope('initialized',s.attempt,{rate:44100}));await tick();n.emit(envelope('started',s.attempt));await p;
  const emit=m=>source==='worker'?w.emit(m):n.emit(m);
  emit({...envelope('started',s.attempt+1),version:99});assert.equal(s.state,'RUNNING');
  emit({...envelope('started',s.attempt),protocol:'wrong'});assert.equal(s.state,'FAILED');assert(n.closed);assert(w.terminated);await assert.rejects(s.resume());
 }
});

// Load the real worklet class with an AudioWorklet host test adapter, not a second receiver implementation.
let Processor;
globalThis.AudioWorkletProcessor=class {constructor(){this.port={messages:[],postMessage(m){this.messages.push(m);}};}};
globalThis.registerProcessor=(name,p)=>Processor=p;globalThis.currentTime=0;
await import('./worklet.js?startup-test');
check('unknown startup modes and invalid attempts are rejected at construction',()=>{assert.throws(()=>new Processor({processorOptions:{startupMode:''}}));assert.throws(()=>new Processor({processorOptions:{startupMode:'worker-v1',attempt:0}}));});
let calls=0,instances=[];
function exportsFixture(){const memory={buffer:new ArrayBuffer(MEMORY_BYTES)};let renders=0,writes=0;return {...Object.fromEntries(publicationGetters.map(n=>[n,()=>0])),memory,_initialize(){},web_nor:()=>0x100000,web_nor_size:()=>0x100000,web_nor_erase(){new Uint8Array(memory.buffer,0x100000,0x100000).fill(255);},web_master(){},web_boot(){},web_now_ms:()=>0,web_playing:()=>0,web_sample_rate:()=>44100,web_flash_writes_count:()=>writes,web_editor_forbidden_count:()=>0,web_render(){renders++;},web_out_l:()=>0,web_out_r:()=>512,web_editor_length:()=>0,get renders(){return renders;}};}
const fakeWasm={...WebAssembly,Module:WebAssembly.Module,instantiate:async input=>{calls++;const instance={exports:exportsFixture()};instances.push(instance);return input instanceof realWasm.Module?instance:{instance};}};
globalThis.WebAssembly=fakeWasm;
const options={processorOptions:{startupMode:'worker-v1',attempt:7}};
const init=()=>envelope('initialize',7,{module,sectors:{'655360':sector},master:700});
const output=()=>[ [new Float32Array(128),new Float32Array(128)] ];
await test('actual receiver distinguishes byte/module return shapes and restores sectors before boot',async()=>{
 const legacy=new Processor();await legacy.onMessage({type:'load',wasm:emptyBytes.buffer,sectors:{}});assert(legacy.ex);assert.equal(legacy.port.messages[0].type,'ready');
 const n=new Processor(options),before=calls;await n.onMessage(init());assert.equal(calls,before+1);assert.equal(n.ex.memory.buffer.byteLength,MEMORY_BYTES);assert.deepEqual(new Uint8Array(n.ex.memory.buffer,n.ex.web_nor()+655360,4096),sector);
 n.process([],output());assert.equal(n.ex.renders,0);await n.onMessage(envelope('start',7));n.process([],output());assert.equal(n.ex.renders,1);assert(n.port.messages.some(m=>m.type==='started'));
});
await test('module mode rejects raw bytes, stale/invalid envelopes and duplicate initialization',async()=>{
 const n=new Processor(options),before=calls;await n.onMessage({...init(),attempt:8});assert.equal(calls,before);await n.onMessage(init());await n.onMessage(init());assert.equal(calls,before+1);assert(n.loadFailed);assert.equal(n.ex,null);
 const raw=new Processor(options);await raw.onMessage({type:'load',wasm:emptyBytes.buffer});assert(raw.loadFailed);
 const wrong=new Processor(options);await wrong.onMessage({...init(),version:2});assert(wrong.loadFailed);
});
await test('cancellation during instantiation prevents late boot/publication',async()=>{
 const save=fakeWasm.instantiate;let release;fakeWasm.instantiate=()=>new Promise(r=>release=r);const n=new Processor(options),p=n.onMessage(init());await n.onMessage(envelope('cancel',7));release({exports:exportsFixture()});await p;assert.equal(n.ex,null);assert(!n.port.messages.some(m=>m.type==='initialized'));fakeWasm.instantiate=save;
});
await test('overlapping initialization allocates only one authority and fails closed',async()=>{
 const save=fakeWasm.instantiate;let release,count=0;fakeWasm.instantiate=()=>{count++;return new Promise(r=>release=r);};const n=new Processor(options),p=n.onMessage(init());await n.onMessage(init());assert.equal(count,1);release({exports:exportsFixture()});await p;assert(n.loadFailed);assert.equal(n.ex,null);fakeWasm.instantiate=save;
});
await test('mutating/editor messages cannot render or open an endpoint before start',async()=>{const n=new Processor(options);await n.onMessage(init());await n.onMessage({type:'editor-open',epoch:1});assert.equal(n.editor.epoch,0);n.process([],output());assert.equal(n.ex.renders,0);});
await test('bad sector payload is denied before allocation',async()=>{const n=new Processor(options),before=calls;await n.onMessage({...init(),sectors:{'0':sector}});assert.equal(calls,before);assert(n.loadFailed);});
globalThis.WebAssembly=realWasm;
const artifact=process.argv[2];
if(artifact){
 const bytes=fs.readFileSync(artifact);
 await test('real verified WASM Worker preparation uses private memory, 207 blocks and zero storage counters',async()=>{const result=await prepareWasm(bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength));assert.equal(result.diagnostics.calls,207);assert.equal(result.diagnostics.ms,601);assert.equal(result.diagnostics.writes,0);assert(result.module instanceof realWasm.Module);});
 await test('publication preparation calls exactly the audited getters after warm-up without changing private memory',async()=>{
  let privateInstance,rendered=0,getterStarted=false,before,getterOrder=[],contextReads=0;
  const savedContext=Object.getOwnPropertyDescriptor(globalThis,'AudioContext');
  Object.defineProperty(globalThis,'AudioContext',{configurable:true,get(){contextReads++;throw Error('Preparation must not access AudioContext');}});
  const api={Module:realWasm.Module,compile:b=>realWasm.compile(b),instantiate:async mod=>{
   privateInstance=await realWasm.instantiate(mod,{});const ex=privateInstance.exports,wrapped={...ex};
   wrapped.web_render=n=>{rendered++;return ex.web_render(n);};
   for(const name of publicationGetters)wrapped[name]=()=>{
    // Counter checks before warm-up are not preparation calls.
    if(rendered===207){if(!getterStarted){getterStarted=true;before=new Uint8Array(ex.memory.buffer).slice();}getterOrder.push(name);}
    return ex[name]();
   };
   // The excluded cache-writing getter must never be called by preparation.
   wrapped.web_anim_levels=()=>{throw Error('Mutable getter reached');};
   return {exports:wrapped};
  }};
  let result;
  try{result=await prepareWasm(bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength),api);}
  finally{if(savedContext)Object.defineProperty(globalThis,'AudioContext',savedContext);else delete globalThis.AudioContext;}
  assert.equal(rendered,207);assert.equal(contextReads,0);
  // The final flash-write diagnostic rereads its counter after the exact list.
  assert.deepEqual(getterOrder,[...publicationGetters,'web_flash_writes_count']);
  assert.deepEqual(new Uint8Array(privateInstance.exports.memory.buffer),before);
  assert.equal(result.diagnostics.forbidden,0);assert.equal(result.diagnostics.writes,0);
  assert.deepEqual(Object.keys(result).sort(),['diagnostics','module']);
 });
 await test('prepared module yields independent fresh authority with unchanged boot state and NOR',async()=>{
  let disposable;const compiled=await realWasm.compile(bytes);
  const ref=await realWasm.instantiate(compiled,{});ref.exports._initialize();ref.exports.web_nor_erase();ref.exports.web_master(700);ref.exports.web_boot();
  const refBoot=new Uint8Array(ref.exports.memory.buffer).slice();
  const result=await prepareWasm(bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength),{
   Module:realWasm.Module,compile:async()=>compiled,instantiate:async mod=>{disposable=await realWasm.instantiate(mod,{});return disposable;}
  });
  const fresh=await realWasm.instantiate(result.module,{}),ex=fresh.exports;
  ex._initialize();ex.web_nor_erase();ex.web_master(700);ex.web_boot();
  assert.notEqual(ex.memory.buffer,disposable.exports.memory.buffer);assert.notEqual(ex.memory.buffer,ref.exports.memory.buffer);
  assert.deepEqual(new Uint8Array(ex.memory.buffer),refBoot);
  assert.equal(ex.web_now_ms(),0);assert.equal(ex.web_playing(),0);assert.equal(ex.web_flash_writes_count(),0);assert.equal(ex.web_editor_forbidden_count(),0);
 });
 await test('real module and byte receivers both boot without advancing firmware or writing NOR',async()=>{const mod=await WebAssembly.compile(bytes);const n=new Processor(options);await n.onMessage({...init(),module:mod,sectors:{}});assert(n.ex);assert.equal(n.ex.web_now_ms(),0);assert.equal(n.ex.web_playing(),0);assert.equal(n.ex.web_flash_writes_count(),0);assert.equal(n.ex.web_editor_forbidden_count(),0);const legacy=new Processor();await legacy.onMessage({type:'load',wasm:bytes,sectors:{}});assert(legacy.ex);assert.equal(legacy.ex.web_now_ms(),0);});
}

await test('missing publication getter fails preparation without returning a module',async()=>{
 const ex=exportsFixture();ex.web_now_ms=()=>ex.renders?601:0;delete ex.web_lit_buttons;
 await assert.rejects(prepareWasm(emptyBytes.buffer,{Module:realWasm.Module,compile:async()=>module,instantiate:async()=>({exports:ex})}),/not a function/);
});
await test('getter-time storage or forbidden operations fail preparation invariants',async()=>{
 for(const counter of ['web_flash_writes_count','web_editor_forbidden_count']){
  const ex=exportsFixture();let attempts=0;ex.web_now_ms=()=>ex.renders?601:0;
  ex.web_lit_buttons=()=>{attempts++;};ex[counter]=()=>attempts;
  await assert.rejects(prepareWasm(emptyBytes.buffer,{Module:realWasm.Module,compile:async()=>module,instantiate:async()=>({exports:ex})}),/invariant/);
 }
});

await test('cancel racing a prepared reply never retains the compiled module',async()=>{const s=fixture(),p=s.prepare();await tick();s.worker.emit(envelope('prepared',s.attempt,{module,diagnostics}));s.cancel();await assert.rejects(p);assert.equal(s.module,null);});
await test('unknown startup envelope cannot bypass into ordinary MIDI handling',async()=>{globalThis.WebAssembly=fakeWasm;try{const n=new Processor(options);await n.onMessage(init());await n.onMessage(envelope('start',7));n.process([],output());await n.onMessage(envelope('midi',7,{data:[144,60,100]}));assert(n.loadFailed);}finally{globalThis.WebAssembly=realWasm;}});
await test('activation resume is invoked synchronously by start; no preparation suspension',async()=>{
 let resumed=0;class Context extends FakeContext{constructor(){super();this.state='suspended';}resume(){resumed++;return super.resume();}}
 const s=fixture({ContextClass:Context});await prepared(s);assert.equal(resumed,0);const p=s.start();assert.equal(resumed,1);await tick();s.cancel();await assert.rejects(p);
});
await test('processor failure after readiness tears down without retry',async()=>{const s=fixture();const w=await prepared(s);const {p,n}=await initializing(s);n.onprocessorerror();await assert.rejects(p);assert(w.terminated);assert(n.closed);assert.equal(s.state,'FAILED');});
await test('one-shot Worker entry rejects extra authority data and concurrent duplicate preparation',async()=>{
 const replies=[];globalThis.WorkerGlobalScope=class{static [Symbol.hasInstance](v){return v===globalThis;}};globalThis.postMessage=m=>replies.push(m);
 globalThis.WebAssembly={Module:realWasm.Module,compile:async()=>module,instantiate:async()=>{const ex=exportsFixture();ex.web_now_ms=()=>ex.renders?601:0;return {exports:ex};}};
 try{
  await import('./prepare_worker.js?entry-test');
  await globalThis.onmessage({data:envelope('prepare',4,{bytes:emptyBytes.buffer,sectors:{}})});assert.equal(replies.at(-1).type,'error');
  const first=globalThis.onmessage({data:envelope('prepare',4,{bytes:emptyBytes.buffer})});
  await globalThis.onmessage({data:envelope('prepare',4,{bytes:emptyBytes.buffer})});await first;
  assert.equal(replies.filter(m=>m.type==='prepared').length,1);assert.equal(replies.filter(m=>m.type==='error').length,2);
  const m=replies.find(m=>m.type==='prepared');assert.deepEqual(Object.keys(m).sort(),['protocol','version','type','attempt','module','diagnostics'].sort());assert.equal(m.diagnostics.calls,207);
 }finally{globalThis.WebAssembly=realWasm;delete globalThis.WorkerGlobalScope;delete globalThis.postMessage;delete globalThis.onmessage;}
});
if(process.env.S4A_DOM_MODULE){
 const {JSDOM}=await import(process.env.S4A_DOM_MODULE);
 const page=fs.readFileSync(new URL('./index.html',import.meta.url),'utf8');
 function pageFixture({raw=null,readFails=false,writeFails=false}={}){
  const dom=new JSDOM(page),input={},store={raw,readFails,writeFails,writes:0,getItem(){if(this.readFails)throw Error('read denied');return this.raw;},setItem(k,v){if(this.writeFails)throw Error('quota');this.raw=v;this.writes++;}};
  class Controller extends PreparedStartup{constructor(o){super({...o,WorkerClass:FakeWorker,ContextClass:FakeContext,NodeClass:FakeNode,fetcher,timeouts:{prepare:100,init:100,start:100}});}}
  const extract=(a,b)=>page.slice(page.indexOf(a),page.indexOf(b,page.indexOf(a)));
  const source=extract('const send =','const { SimulatorBroker }')+
   extract('function updateEditorButton()','document.getElementById("open-editor").addEventListener')+
   extract('let flashWritable=true;','// ------------------------------------------------------------------ Web MIDI in')+
   extract('async function midiOn()','// ------------------------------------------------------------------ switching on')+
   extract('async function switchOn()','document.addEventListener("visibilitychange"');
  const api=vm.runInNewContext(`let workerStartup=null,simulatorReady=false,authorityReady=false,node=null,editorContext=null,lastLeds=[],editorReloadRequired=false;const workerOptIn=true,STORE='test',knobsOn=false,knobCC=[];const editorBroker={end(){},receive(){}};function say(t){document.getElementById('status').textContent=t;} ${source};({controller:()=>workerStartup,ready:()=>simulatorReady,send,loadFlash,saveFlash});`,{
   document:dom.window.document,window:{localStorage:store},localStorage:store,readSavedSectors,atob,btoa,
   navigator:{requestMIDIAccess:async()=>({inputs:new Map([['test',input]])})},PreparedStartup:Controller,
   syncButtons(){},syncKeys(){},masterValue:700,drawScreen(){},drawLights(){},console
  });
  const doc=dom.window.document,go=doc.getElementById('go'),editor=doc.getElementById('open-editor'),status=doc.getElementById('persistence-status');
  async function prepare(){go.click();await tick();const c=api.controller();c.worker.emit(envelope('prepared',c.attempt,{module,diagnostics}));await tick();return c;}
  async function start(){go.click();await tick();const c=api.controller(),n=c.node;if(n){n.emit(envelope('initialized',c.attempt,{rate:44100}));await tick();n.emit(envelope('started',c.attempt));await tick();}return n;}
  function visible(){assert(!status.hidden);assert.equal(status.closest('[hidden]'),null);assert.equal(status.getAttribute('role'),'status');assert.equal(status.getAttribute('aria-live'),'polite');}
  return {dom,api,store,input,doc,go,editor,status,prepare,start,visible,close(){api.controller().cancel();dom.window.close();}};
 }
 await test('real page MIDI keeps Note Off on same live authority during interruption, editor stays gated',async()=>{
  const f=pageFixture();try{
   const before=FakeContext.all.length;f.api.send({type:'midi',data:[144,60,100]});await f.prepare();assert.equal(FakeContext.all.length,before);assert(f.editor.disabled);
   f.go.click();await tick();const c=f.api.controller(),n=c.node;
   f.api.send({type:'midi',data:[144,60,100]});assert(!n.posts.some(m=>m.type==='midi'));assert(f.editor.disabled);
   n.emit(envelope('initialized',c.attempt,{rate:44100}));await tick();assert(f.editor.disabled);n.emit(envelope('started',c.attempt));await tick();assert(!f.editor.disabled);
   f.input.onmidimessage({data:[144,60,100]});assert.deepEqual(n.posts.filter(m=>m.type==='midi').map(m=>Array.from(m.data)),[[144,60,100]]);
   c.context.state='suspended';c.context.onstatechange();assert(f.editor.disabled);f.input.onmidimessage({data:[128,60,0]});assert.deepEqual(n.posts.filter(m=>m.type==='midi').map(m=>Array.from(m.data)),[[144,60,100],[128,60,0]]);
   assert.equal(f.go.textContent,'Resume audio');f.go.click();await tick();assert(f.api.ready());assert(!f.editor.disabled);assert.equal(n.connects,1);assert.equal(n.posts.filter(m=>m.type==='midi').length,2);
   c.cancel();const count=n.posts.length;f.input.onmidimessage({data:[144,62,100]});assert.equal(n.posts.length,count);assert(f.editor.disabled);
  }finally{f.close();}
 });
 await test('fatal current-attempt failure blocks actual page MIDI callback to old instance',async()=>{
  const f=pageFixture();try{const c=await f.prepare(),w=c.worker,n=await f.start();w.emit({...envelope('prepared',c.attempt,{module,diagnostics}),version:99});assert.equal(c.state,'FAILED');const count=n.posts.length;f.input.onmidimessage({data:[128,60,0]});assert.equal(n.posts.length,count);assert(f.editor.disabled);}finally{f.close();}
 });
 await test('closed contexts and stale authority identities reject real page MIDI routing',async()=>{
  const f=pageFixture();try{const c=await f.prepare(),n=await f.start();const count=n.posts.length;
   c.node={};f.input.onmidimessage({data:[128,60,0]});assert.equal(n.posts.length,count);c.node=n;
   c.context.state='closed';c.context.onstatechange();f.input.onmidimessage({data:[128,60,0]});assert.equal(n.posts.length,count);assert(f.editor.disabled);
  }finally{f.close();}
 });
 await test('RAM-only status remains visible after overlay hides and writes remain prohibited',async()=>{
  const f=pageFixture({readFails:true});try{await f.prepare();await f.start();assert(f.doc.getElementById('start').hidden);f.visible();assert.equal(f.status.dataset.state,'ram-only');f.store.readFails=false;assert.equal(f.api.saveFlash({}),false);assert.equal(f.store.writes,0);assert.equal(f.status.dataset.state,'ram-only');}finally{f.close();}
 });
 await test('corrupt saved bytes fail before context creation and retain explicit write prohibition',async()=>{
  const f=pageFixture({raw:'{'});try{await f.prepare();const count=FakeContext.all.length;await f.start();assert.equal(FakeContext.all.length,count);assert.equal(f.api.controller().state,'FAILED');f.visible();assert.equal(f.status.dataset.state,'read-failed');assert.equal(f.api.saveFlash({}),false);assert.equal(f.store.raw,'{');assert.equal(f.store.writes,0);}finally{f.close();}
 });
 await test('real publication save failure remains visible, cleared only by later successful publication',async()=>{
  const f=pageFixture({raw:'{}'});try{await f.prepare();const n=await f.start();assert(f.doc.getElementById('start').hidden);f.visible();assert.equal(f.status.dataset.state,'available');f.store.writeFails=true;n.emit({type:'frame',sectors:{'655360':sector}});f.visible();assert.equal(f.status.dataset.state,'save-failed');assert.match(f.status.textContent,/not saved/);assert.equal(f.store.raw,'{}');assert.equal(f.store.writes,0);f.store.writeFails=false;n.emit({type:'frame',sectors:{'655360':sector}});assert.equal(f.status.dataset.state,'available');assert.match(f.status.textContent,/were saved/);assert.equal(f.store.raw,encoded);assert.equal(f.store.writes,1);}finally{f.close();}
 });
}else console.log('UNVERIFIED: startup DOM check requires S4A_DOM_MODULE (no browser used)');

console.log(`${pass} startup checks passed`);
