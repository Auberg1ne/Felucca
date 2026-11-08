// SPDX-License-Identifier: GPL-3.0-only
import fs from "node:fs";
import vm from "node:vm";
import { spawn, spawnSync } from "node:child_process";
import os from "node:os";
import path from "node:path";
import { MessageChannel } from "node:worker_threads";
import { S4A, readonlyFrame, WasmReadonlyEndpoint, ReadonlyClient, connectReadonly, SimulatorBroker, readonlySnapshot, loadReadonly } from "./editor_transport.js";
let pass=0,fail=0;
const ok=(v,s)=>{console.log(`${v?"ok":"FAIL"}: ${s}`);v?pass++:fail++;};
const sleep=n=>new Promise(r=>setTimeout(r,n));
const html=fs.readFileSync(new URL("../editor.html",import.meta.url),"utf8");
const proto=html.slice(html.indexOf("/*PROTO-BEGIN*/"),html.indexOf("/*PROTO-END*/"));
const E=vm.runInNewContext(proto+";({Link,frame,parse,req,CMD,makeMockDevice})",{setTimeout,clearTimeout,setInterval,clearInterval,console});
for(let cmd=0;cmd<128;cmd++)for(let n=0;n<=2;n++) {
  const expected=([1,25,27].includes(cmd)&&!n)||([2,5,31].includes(cmd)&&n===2)||([10,29].includes(cmd)&&n===1);
  if(readonlyFrame(E.frame(cmd,Array(n).fill(0)))!==!!expected)throw new Error(`policy ${cmd}/${n}`);
}
ok(true,"exact envelope policy across every command ID and argument length");
for(const b of [[],[240,125,70,76,25],[240,125,70,76,25,0,247],[240,124,70,76,25,247],[240,125,70,76,31,0,128,247],Object.assign(Array(8),{0:240,1:125,2:70,3:76,4:31,7:247}),E.frame(3,[0,0])])ok(!readonlyFrame(b),"malformed/foreign/oversized or mutation frame denied");
// Fixed-memory ABI test double only: no firmware command implementation here.
function model() {
  const mem=new ArrayBuffer(2048);let epoch=0,last=0,pending=null,len=0,ticket=0,calls=0,lastEpoch=0;
  const ex={memory:{buffer:mem},web_editor_input:()=>0,web_editor_output:()=>32,
    web_editor_open:e=>{if(epoch||e<=lastEpoch)return 3;epoch=lastEpoch=e;last=0;return 0;},
    web_editor_close:e=>{if(epoch===e){epoch=0;pending=null;len=0;}},
    web_editor_submit:(e,t,n)=>{if(e!==epoch||t<=last)return 3;if(pending||len)return 2;last=t;ticket=t;pending=Array.from(new Uint8Array(mem,0,n));return 0;},
    web_editor_cancel:(e,t)=>{if(e===epoch&&t===ticket){pending=null;len=0;}},
    web_editor_pop:(e,t)=>ex.web_editor_cancel(e,t),web_editor_length:()=>len,web_editor_epoch:()=>epoch,web_editor_ticket:()=>ticket,
    render:()=>{if(pending){calls++;new Uint8Array(mem,32,7).set(E.frame(25,[0]));len=7;pending=null;}},
    state:()=>({epoch,pending,len,calls})};return ex;
}
function windows() {
  const origin="https://test.invalid",listeners=new Map();
  const host={location:{origin},addEventListener:(k,f)=>listeners.set("h"+k,f),removeEventListener:(k)=>listeners.delete("h"+k)};
  const child={location:{origin},opener:host,closed:false,addEventListener:(k,f)=>listeners.set("c"+k,f),removeEventListener:k=>listeners.delete("c"+k)};
  host.postMessage=(data,o)=>queueMicrotask(()=>listeners.get("hmessage")?.({data,origin:o,source:child,ports:[]}));
  child.postMessage=(data,o,ports=[])=>queueMicrotask(()=>listeners.get("cmessage")?.({data,origin:o,source:host,ports}));
  return {host,child,listeners};
}
{
  const {host,child,listeners}=windows(),ex=model();let ready=true,endpoint;
  const broker=new SimulatorBroker(host,m=>endpoint.receive(m),()=>ready,MessageChannel);
  endpoint=new WasmReadonlyEndpoint(ex,m=>broker.receive(m));
  const nonce="a".repeat(32);broker.setTarget(child,nonce);
  for(const e of [{origin:"https://evil.invalid",source:child,nonce},{origin:host.location.origin,source:{},nonce},{origin:host.location.origin,source:child,nonce:"b".repeat(32)}]) {
    listeners.get("hmessage")({...e,data:{protocol:S4A,type:"hello",nonce:e.nonce}});
  }
  ok(!broker.lease,"wrong origin/source/nonce cannot acquire lease");
  endpoint.receive(null);broker.receive(null);
  ok(!ex.state().pending,"malformed envelopes do not dispatch or throw");
  const client=await connectReadonly(child,nonce,E.Link);
  ok(ex.state().epoch===1&&!broker.setTarget({},nonce),"explicit same-origin handshake and single-controller lease");
  const query=client.request(E.req.ping());await sleep(10);
  ok(ex.state().pending&&ex.state().calls===0,"message callback only queues; no firmware dispatch");
  broker.receive({type:"editor-response",epoch:99,ticket:1,bytes:E.frame(25,[0])});
  broker.receive({type:"editor-response",epoch:1,ticket:99,bytes:E.frame(25,[0])});
  ok(client.pending===1,"stale epoch and ticket responses rejected");
  ex.render();endpoint.drain();new Uint8Array(ex.memory.buffer,32,7).fill(255);
  ok((await query)[0]===0,"complete response copied before WASM buffer reuse");
  const inbox=Array.from(new Uint8Array(ex.memory.buffer,0,8));
  endpoint.receive({type:"editor-request",epoch:1,ticket:99,bytes:Array(665).fill(0)});
  ok(Array.from(new Uint8Array(ex.memory.buffer,0,8)).join()===inbox.join()&&!ex.state().pending,"oversized frames rejected before copying to fixed WASM input");
  const count=ex.state().calls;
  await client.request([15,[]]).then(()=>ok(false,"denied command"),()=>ok(true,"SMP_INFO denied by client without dispatch"));
  ok(ex.state().calls===count,"denial does not run firmware");
  const timed=client.request(E.req.ping(),{timeout:20,quiet:true,retries:3});
  await timed.then(()=>ok(false,"timeout"),()=>ok(true,"even quiet timeout closes Link and cancels queued request without retry"));await sleep(10);
  ok(!ex.state().pending&&!ex.state().epoch&&!broker.lease,"timeout releases C inbox and lease without reset");
  const next=await connectReadonly(child,nonce,E.Link);
  ok(ex.state().epoch===2,"reconnect receives a new epoch");
  const requests=Array.from({length:11},()=>next.request(E.req.ping(),{timeout:1000}));
  const settled=Promise.allSettled(requests);
  ok(next.link.q.length===8,"Link admits one active request and at most eight queued requests");
  next.close();const results=await settled;await sleep(10);
  ok(results.filter(r=>r.reason?.message==="S4A request queue full").length===2,"excess requests rejected before transport submission");
  ok(!broker.lease&&next.closed,"bounded Link queue and disconnect reject pending work");
  const abandoned=await connectReadonly(child,nonce,E.Link);
  const lost=abandoned.request(E.req.ping(),{timeout:1000});const outcome=lost.catch(e=>e);
  await sleep(10);broker.lease.deadline=Date.now()-1;await sleep(120);
  ok((await outcome)?.message==="closed" && abandoned.closed&&!ex.state().pending&&!broker.lease,"broker expiry cancels an abandoned undispatched request without client timer");
  const suspended=await connectReadonly(child,nonce,E.Link);
  ready=false;broker.end("suspended");await sleep(10);
  ok(suspended.closed&&!ex.state().epoch,"suspension invalidates connection and C session");
  broker.dispose();
}
{
  const {host,child,listeners}=windows(),ex=model();let endpoint;
  const broker=new SimulatorBroker(host,m=>endpoint.receive(m),()=>true,MessageChannel);
  endpoint=new WasmReadonlyEndpoint(ex,m=>broker.receive(m));
  child.postMessage=(m,o,ports=[])=>ports.forEach(p=>p.close()); // ready message never reaches the editor
  const nonce="d".repeat(32);broker.setTarget(child,nonce);
  listeners.get("hmessage")({origin:host.location.origin,source:child,data:{protocol:S4A,type:"hello",nonce}});
  await sleep(1100);
  ok(!broker.lease&&!ex.state().epoch,"lost transferred-port handshake expires and releases the C session");
  broker.dispose();
}
async function nativeDevice(binary,engine) {
  const child=spawn(binary,["--wire",String(engine)],{stdio:["pipe","pipe","pipe"]});let pending=Buffer.alloc(0),stderr="";
  child.stderr.on("data",b=>stderr+=b);
  const link=new E.Link(bytes=>child.stdin.write(Buffer.from([bytes.length,...bytes])),{timeout:1000});
  child.stdout.on("data",b=>{
    pending=Buffer.concat([pending,b]);
    while(pending.length>=3) {
      const n=pending[1]|pending[2]<<8;if(pending.length<3+n)return;
      const rc=pending[0],frame=Array.from(pending.subarray(3,3+n));pending=pending.subarray(3+n);
      if(rc)link.finish(null,new Error(`C denied ${rc}`));else link.receive(frame);
    }
  });
  child.on("error",e=>link.finish(null,e));child.on("exit",()=>{if(!link.closed)link.close();});
  const seen=[];const request=(r,o)=>{seen.push(r);return link.request(r,{...o,retries:0});};
  const close=async()=>{link.close();child.stdin.end();const code=await new Promise(r=>child.once("close",r));if(code!==0||stderr)throw new Error(`native exit ${code}: ${stderr}`);};
  return {request,close,seen};
}
// Exercise the actual simulator page's lifecycle wiring, not a second broker implementation.
{
  const page=fs.readFileSync(new URL("./index.html",import.meta.url),"utf8");
  const {host,child,listeners}=windows(),ex=model(),button={disabled:false,textContent:"Open editor"};
  let endpoint,opens=0;
  host.open=()=>{opens++;return child;};
  button.addEventListener=(type,fn)=>button[type]=fn;
  const wiring=page.slice(page.indexOf("const send ="),page.indexOf("\nfunction build(name)"))
    .replace('await import("./editor_transport.js")','bridgeModule');
  const setup=vm.runInNewContext(`(async()=>{
    let editorContext={state:"running"},editorReloadRequired=false;
    let simulatorReady=true,authorityReady=true,workerStartup=null;
    const node={port:{postMessage:m=>endpoint.receive(m)}};
    ${wiring}
    return {broker:editorBroker,updateEditorButton};
  })`,{window:host,document:{getElementById:()=>button},bridgeModule:{SimulatorBroker},endpoint:{receive:m=>endpoint.receive(m)}});
  const {broker,updateEditorButton}=await setup();
  endpoint=new WasmReadonlyEndpoint(ex,m=>broker.receive(m));
  const nonce="e".repeat(32);broker.setTarget(child,nonce);
  const client=await connectReadonly(child,nonce,E.Link);
  const abandoned=client.request(E.req.ping(),{timeout:1000}).catch(e=>e);
  await sleep(10);
  listeners.get("hpagehide")({persisted:true});await sleep(10);
  ok((await abandoned)?.message==="closed" && !ex.state().pending&&!ex.state().epoch&&!broker.lease,
    "actual pagehide wiring cancels pending work and disposes the old lease");
  listeners.get("hpageshow")({persisted:true});updateEditorButton();button.click();
  broker.receive({type:"editor-response",epoch:1,ticket:1,bytes:E.frame(25,[0])});
  host.postMessage({protocol:S4A,type:"hello",nonce},host.location.origin);await sleep(10);
  ok(button.disabled&&button.textContent.includes("Reload simulator page")&&button.textContent.includes("previous session discarded"),
    "cached page restoration visibly requires reload, even with running audio");
  ok(!broker.lease&&!ex.state().pending&&!ex.state().epoch&&!listeners.has("hmessage")&&opens===0,
    "cached page cannot reopen leases or accept old replies/handshakes without reload");
  broker.dispose();
}
function uiFunction(name) {
  const start=html.indexOf(`function ${name}(`),end=html.indexOf("\n}",start);
  if(start<0||end<0)throw new Error(`missing UI function ${name}`);
  return html.slice(start,end+2);
}
// DOM regression (Chromium or jsdom), using successful real-firmware load snapshots below.
// Unrelated panel builders are stubbed; selector creation, ordering, selection and
// disabling execute the production editor functions against actual DOM elements.
async function selectorDOM(states) {
  const browser=process.env.S4A_CHROMIUM,domModule=process.env.S4A_DOM_MODULE;
  if(!browser&&!domModule){console.log("UNVERIFIED: selector DOM checks require S4A_CHROMIUM or S4A_DOM_MODULE (jsdom), plus native firmware snapshots");return;}
  const tmp=fs.mkdtempSync(path.join(os.tmpdir(),"s4a-selector-"));
  try {
    const code=`
      let dev;const $=id=>document.getElementById(id);
      ${html.slice(html.indexOf("const el ="),html.indexOf("const sleep =",html.indexOf("const el =")))}
      ${html.slice(html.indexOf("const ENGINE_ORDER ="),html.indexOf("\nfunction devicePresetRows"))}
      const buildGroups=()=>{},renderPanels=()=>{},renderTrackbar=()=>{},aliasOf=(names,i)=>i;
      ${uiFunction("initEngineSelector")}
      ${uiFunction("updateBrowser")}
      ${uiFunction("renderReadonlyView")}
      const results=[];
      for(const state of ${JSON.stringify(states)}) {
        dev={readonly:true,...state};$("engine").disabled=false;
        initEngineSelector();const normalEnabled=!$("engine").disabled;
        $("engine").replaceChildren();renderReadonlyView();
        const options=Array.from($("engine").options);
        results.push({engine:state.dump.engine,pass:normalEnabled&&options.length===state.info.engines.filter(n=>n!=="-").length &&
          options.every(o=>o.textContent===state.info.engines[+o.value]&&o.textContent!=="-") &&
          new Set(options.map(o=>o.value)).size===options.length && $("engine").value===String(state.dump.engine) &&
          $("engine").disabled && $("preset").disabled && $("mutate").disabled && !$("connect").disabled});
      }
      document.getElementById("result").textContent=JSON.stringify(results);
    `;
    const document=`<!doctype html><main><button id="connect"></button><select id="engine"></select><select id="preset"></select><button id="mutate"></button></main><pre id="result"></pre><script>${code}</script>`;
    let json;
    if(domModule) {
      const {JSDOM}=await import(domModule);
      const dom=new JSDOM(document,{runScripts:"dangerously"});
      try {json=dom.window.document.getElementById("result").textContent;} finally {dom.window.close();}
    } else {
      const file=path.join(tmp,"selector.html");fs.writeFileSync(file,document);
      const r=spawnSync(browser,["--headless","--no-sandbox","--disable-dev-shm-usage",`--user-data-dir=${path.join(tmp,"profile")}`,"--dump-dom",`file://${file}`],{encoding:"utf8",timeout:20000,maxBuffer:2*1024*1024});
      if(r.error||r.status!==0)throw new Error(`Chromium DOM check unavailable: ${r.error?.message||r.stderr}`);
      json=r.stdout.match(/<pre id="result">([^<]+)<\/pre>/)?.[1];
    }
    if(!json)throw new Error("DOM result missing");
    const results=JSON.parse(json);if(results.length!==states.length)throw new Error("incomplete DOM results");
    for(const r of results)ok(r.pass,`${domModule?"jsdom":"Chromium"} DOM: successful firmware load for engine ${r.engine} populates disabled engine selector`);
    ok(uiFunction("buildUI").includes("initEngineSelector();"),"normal and read-only editor paths share selector initialization");
  } finally {fs.rmSync(tmp,{recursive:true,force:true});}
}
const binary=process.argv[2];
if(binary&&!binary.startsWith("--")) {
  const states=[];
  for(const engine of [4,8,13]) {
    const d=await nativeDevice(binary,engine);
    try {
      const state=await loadReadonly(d.request,E.parse,E.req,E.CMD),info=state.info;
      states.push(state);
      ok(state.dump.engine===engine&&state.pdesc.length===info.pcount&&state.gdesc.length===info.gcount&&d.seen.every(([c,a])=>readonlyFrame(E.frame(c,a))),`actual read-only loader against firmware engine ${engine}: no excluded requests`);
      const desc=E.parse[E.CMD.DESC](await d.request(E.req.desc(0,info.pe0)));
      const names=Array.from({length:8},(_,k)=>`USR${k+1}`);
      ok(engine===13?desc.names.join()===["BREAK",...names.slice(0,3),"PIANO",...names.slice(3)].join():desc.names.length===13&&desc.names.slice(5).join()===names.join(),`real firmware via Link: engine ${engine} eight-slot source IDs`);
      if(engine!==13){
        const mock=E.makeMockDevice({auto:false});
        try {ok(desc.names.join()===["GUITAR","GUITAR","VOICE","HIT","GUITAR",...names].join()&&
          desc.names.join()===mock.tables.ENG[engine].edit[0].names.join()&&state.pdesc[info.pe0].names.join()===desc.names.join(),
          `S5 real firmware descriptors and mock share all thirteen identities for engine ${engine}`);}
        finally{mock.stop();}
      }
      const snap=await readonlySnapshot(d.request,E.parse,E.req,E.CMD,info);
      ok(snap.dump.engine===engine&&snap.dump.p.length===info.pcount&&snap.dump.g.length===info.gcount,`real firmware via Link: engine ${engine} snapshot without DUMP or selection change`);
      for(const [cmd,args] of [[15,[]],[27,[1]],[31,[0,0,0,0]],[65,[]],[3,[0,0]]])
        await d.request([cmd,args]).then(()=>ok(false,"C mutation denied"),()=>ok(true,`C rejects excluded command/subform ${cmd}`));
    } finally {await d.close();}
  }
  await selectorDOM(states);
} else console.log("UNVERIFIED: native firmware integration needs compiled editor_bridge_test argument");
if(process.argv.includes("--wasm")) {
  const file=process.argv[process.argv.indexOf("--wasm")+1];
  if(!file||!fs.existsSync(file)){console.log("UNVERIFIED: rebuilt WASM unavailable");process.exitCode=77;}
  else {
    const {instance}=await WebAssembly.instantiate(fs.readFileSync(file),{});const ex=instance.exports;ex._initialize();ex.web_nor_erase();ex.web_boot();
    const {host,child}=windows();let endpoint;
    const broker=new SimulatorBroker(host,m=>endpoint.receive(m),()=>true,MessageChannel);
    endpoint=new WasmReadonlyEndpoint(ex,m=>broker.receive(m));const nonce="c".repeat(32);broker.setTarget(child,nonce);
    const client=await connectReadonly(child,nonce,E.Link);
    const timer=setInterval(()=>{ex.web_render(128);endpoint.drain();},1);
    try {
      const state=await loadReadonly((r,o)=>client.request(r,o),E.parse,E.req,E.CMD);
      ok(state.pdesc.length===state.info.pcount,"real Link -> channel -> WASM firmware -> complete responses loads metadata");
      ok(ex.web_editor_forbidden_count()===0,"WASM queries invoke no forbidden adapters");
    } finally {clearInterval(timer);client.close();broker.dispose();}
  }
}
ok(html.includes("async function connectSimulatorReadonly")&&!html.slice(html.indexOf("async function connectSimulatorReadonly"),html.indexOf("/* the whole connect sequence */")).includes("req.dump"),"read-only loader separated from normal DUMP/PROJECT/WATCH initialization");
console.log(`S4A JS: ${pass} passed, ${fail} failed`);if(fail)process.exitCode=1;
