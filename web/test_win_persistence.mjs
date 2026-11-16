// SPDX-License-Identifier: GPL-3.0-only
import fs from 'node:fs';import vm from 'node:vm';import assert from 'node:assert/strict';
const html=fs.readFileSync(new URL('editor.html',import.meta.url),'utf8');
const proto=html.slice(html.indexOf('/*PROTO-BEGIN*/'),html.indexOf('/*PROTO-END*/'));
const E=vm.runInNewContext(proto+';({assertWinLibraryTransfer,libraryFile,auditionPatch,CMD,parse,req})',{setTimeout,clearTimeout,console});
const ctx={pe0:110,keys:Array(118).fill(''),engines:Array(14).fill('')};ctx.keys[109]='WIN';ctx.engines[8]='GRAIN';let checks=0;
for(const v of [0,1,63,65,126,127]){const p={engine:8,p:Array(118).fill(0)};p.p[109]=v;assert.throws(()=>E.libraryFile('preset',[p],ctx),/versioned backup/);checks++;}
const p={engine:8,p:Array(118).fill(0)};p.p[109]=64;assert.doesNotThrow(()=>E.assertWinLibraryTransfer([p],ctx));checks++;
// Exercise actual audition function with its ordinary decoder replaced only at the wire boundary.
const info={pcount:118,pe0:110};const calls=[];E.parse[E.CMD.DUMP]=()=>({p:Array(118).fill(64)});
await E.auditionPatch(async bytes=>{calls.push(Array.from(bytes));return [];},info,{engine:8,p:Array(118).fill(0)});
assert(calls.some(x=>JSON.stringify(x)===JSON.stringify(Array.from(E.req.set(1,20,8)))));checks++;
assert(!calls.some(x=>JSON.stringify(x)===JSON.stringify(Array.from(E.req.set(0,109,0)))));checks++;
assert.match(html,/id === 109 && dev\.info\.pcount === 118 && dev\.info\.pe0 === 110/);checks++;
assert.match(html,/i === 109 && dev\.info\.pcount === 118 && dev\.info\.pe0 === 110/);checks++;
console.log(`WIN browser provenance: ${checks} checks passed; headless metadata, no ambiguous non-Hann export or SET109 bulk audition`);
