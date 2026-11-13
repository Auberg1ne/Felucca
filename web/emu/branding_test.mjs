/* SPDX-License-Identifier: GPL-3.0-only */
/* Frontend smoke test with a deliberately held/rejected WASM fetch, not an audio test.
 * Run: node web/emu/branding_test.mjs (requires Chromium on PATH or CHROMIUM). */
import {spawn} from 'node:child_process';
import {readFileSync,existsSync,mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {once} from 'node:events';
import {createServer} from 'node:http';
import assert from 'node:assert/strict';
const html=readFileSync(new URL('./index.html',import.meta.url));
const profile=mkdtempSync(join(tmpdir(),'coffee-branding-'));
const server=createServer((req,res)=>{const js=req.url==='/worklet.js';res.setHeader('Content-Type',js?'text/javascript':'text/html');res.end(js?readFileSync(new URL('./worklet.js',import.meta.url)):html);});
server.listen(0,'127.0.0.1');await once(server,'listening');
const chrome=spawn(process.env.CHROMIUM||'chromium',['--headless','--no-sandbox','--disable-gpu','--disable-dev-shm-usage','--remote-allow-origins=*','--remote-debugging-port=0','--user-data-dir='+profile,'about:blank'],{stdio:'ignore'});
let socket,checks=0;
const check=(a,b)=>{checks++;assert.deepEqual(a,b);};
try {
 for(let i=0;i<100&&!existsSync(profile+'/DevToolsActivePort');i++)await new Promise(r=>setTimeout(r,100));
 const [port,path]=readFileSync(profile+'/DevToolsActivePort','utf8').trim().split('\n');
 socket=new WebSocket(`ws://127.0.0.1:${port}${path}`);await once(socket,'open');
 let id=0;const pending=new Map();
 socket.onmessage=({data})=>{const m=JSON.parse(data);if(m.id){const p=pending.get(m.id);pending.delete(m.id);m.error?p.reject(new Error(JSON.stringify(m.error))):p.resolve(m.result);}};
 const send=(method,params={},sessionId)=>new Promise((resolve,reject)=>{const n=++id;pending.set(n,{resolve,reject});socket.send(JSON.stringify({id:n,method,params,...sessionId?{sessionId}:{}}));});
 const {targetId}=await send('Target.createTarget',{url:'about:blank'});
 const {sessionId}=await send('Target.attachToTarget',{targetId,flatten:true});
 const evaluate=async expression=>{const r=await send('Runtime.evaluate',{expression,returnByValue:true},sessionId);if(r.exceptionDetails)throw new Error(JSON.stringify(r.exceptionDetails));return r.result.value;};

 await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}`},sessionId);
 for(let i=0;i<100&&!await evaluate('document.readyState === "complete"');i++)await new Promise(r=>setTimeout(r,50));
 check(await evaluate('document.title'),'COFFEE GRAINS');
 check(await evaluate('document.querySelector("h1").textContent'),'COFFEE GRAINS');
 check(await evaluate('document.getElementById("go").textContent'),'Start COFFEE GRAINS');
 for(const width of [1366,390]){
  await send('Emulation.setDeviceMetricsOverride',{width,height:900,deviceScaleFactor:1,mobile:false},sessionId);
  check(await evaluate('document.documentElement.scrollWidth<=innerWidth'),true);
  check(await evaluate('document.querySelector("canvas").getAttribute("aria-label")'),'COFFEE GRAINS screen');
 }
 await evaluate(`window.AudioContext=class{constructor(){this.sampleRate=44100;this.audioWorklet={addModule:async()=>{}}}async resume(){}};window.fetch=url=>{window.wasmURL=url;return new Promise((resolve,reject)=>{window.failLoad=reject;window.finishLoad=resolve})};`);
 await evaluate('document.getElementById("go").click()');
 check(await evaluate('document.getElementById("go").textContent'),'Starting COFFEE GRAINS…');
 check(await evaluate('document.getElementById("go").disabled'),true);
 check(await evaluate('window.wasmURL'),'felucca.wasm');
 await evaluate('window.failLoad(new Error("test load failure"))');
 for(let i=0;i<100&&await evaluate('document.getElementById("go").disabled');i++)await new Promise(r=>setTimeout(r,20));
 check(await evaluate('document.getElementById("go").textContent'),'Start COFFEE GRAINS');
 check(await evaluate('document.getElementById("status").textContent.includes("Could not start COFFEE GRAINS: test load failure")'),true);
 await evaluate(`window.AudioWorkletNode=class{constructor(ac,name){window.processor=name;this.port={postMessage:m=>{if(m.type==='load')queueMicrotask(()=>this.port.onmessage({data:{type:'ready'}}))}}}connect(){}};document.getElementById('go').click();window.finishLoad({ok:true,arrayBuffer:async()=>new ArrayBuffer(0)});`);
 for(let i=0;i<100&&!await evaluate('document.getElementById("start").hidden');i++)await new Promise(r=>setTimeout(r,20));
 check(await evaluate('document.getElementById("start").hidden'),true);
 check(await evaluate('document.getElementById("go").textContent'),'On');
 check(await evaluate('window.processor'),'felucca');
 const source=html.toString();
 for(const key of ['felucca-emu-flash','felucca-emu-knobs','felucca-emu-knobmode','new AudioWorkletNode(ac, "felucca"','github.com/hugelton/Felucca'])check(source.includes(key),true);
 console.log(`browser branding frontend: ${checks} checks passed (desktop/mobile, loading/error, internal identifiers)`);
} finally {
 socket?.close();chrome.kill('SIGTERM');await Promise.race([once(chrome,'exit'),new Promise(r=>setTimeout(r,3000))]);server.close();rmSync(profile,{recursive:true,force:true});
}
