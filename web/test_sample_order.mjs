// SPDX-License-Identifier: GPL-3.0-only
// Real editor ordering code, with raw IDs kept in dropdown option values.
import assert from 'node:assert/strict';
import {readFileSync, existsSync} from 'node:fs';
import vm from 'node:vm';
const html=readFileSync(new URL('./editor.html',import.meta.url),'utf8');
const proto=html.slice(html.indexOf('/*PROTO-BEGIN*/'),html.indexOf('/*PROTO-END*/'));
const E=vm.runInNewContext(proto+';({enumShown,aliasOf,F})',{setTimeout,clearTimeout,setInterval,clearInterval,console});
let checks=0;
const same=(a,b)=>{checks++;assert.deepEqual(Array.from(a),b);};
const names=['CELLO','CELLO','FLUTE','GUITAR','CELLO',...Array.from({length:8},(_,i)=>`USR${i+1}`),'SYNTH','VOX'];
const want=[0,2,3,13,14,5,6,7,8,9,10,11,12];
const visible=(d)=>Array.from(E.enumShown(d)).filter(v=>E.aliasOf(d.names,v-d.min)===v-d.min);
for(const label of ['SET','SRC']) {
 const d={label,fmt:E.F.ENUM,min:0,max:14,names:[...names]};
 same(visible(d),want);
 same(E.enumShown(d),[0,1,4,2,3,13,14,5,6,7,8,9,10,11,12]);
 same(visible(d).map(v=>d.names[v]),['CELLO','FLUTE','GUITAR','SYNTH','VOX',...names.slice(5,13)]);
 same(visible(d).reverse(),[...want].reverse());
 same(Array.from(E.enumShown(d)).sort((a,b)=>a-b),Array.from({length:15},(_,i)=>i));
 same(d.names,names); // sorting never mutates descriptor indices or labels
 for(const v of [1,4]) {
  checks++;assert.equal(E.aliasOf(d.names,v),0);
  const all=Array.from(E.enumShown(d));checks++;assert.ok(all.includes(v));
  // Native select arrows skip hidden aliases, retaining their raw option values.
  checks++;assert.equal(all.slice(all.indexOf(v)+1).find(x=>E.aliasOf(d.names,x)===x),2);
  checks++;assert.equal(all.slice(0,all.indexOf(v)).reverse().find(x=>E.aliasOf(d.names,x)===x),0);
 }
 for(let k=0;k<8;k++){checks++;assert.equal(d.names[5+k],`USR${k+1}`);}
}
// Old firmware is still listed correctly; SLICE's distinct source ABI is untouched.
const legacy=['PIANO','PIANO','FLUTE','SAX','PIANO',...names.slice(5,13)];
same(visible({fmt:E.F.ENUM,min:0,max:12,names:legacy}),[0,2,3,5,6,7,8,9,10,11,12]);
const slice=['BREAK','USR1','USR2','USR3','GUITAR',...names.slice(8,13)];
same(E.enumShown({fmt:E.F.ENUM,min:0,max:9,names:slice}),Array.from({length:10},(_,i)=>i));
same(E.enumShown({fmt:E.F.ENUM,min:0,max:2,names:['1/16','1/4','1/8']}),[1,2,0]);
same(E.enumShown({fmt:E.F.INT,min:0,max:2,names:['USR1','USR2','USR3']}),[0,1,2]);
// When supplied by the host suite, exercise descriptors emitted by the actual firmware.
const desc=new URL('../build/host/desc.json',import.meta.url);
if(existsSync(desc)) {
 const fw=JSON.parse(readFileSync(desc,'utf8'));
 for(const engine of ['SAMPLE','GRAIN']) {
  const d=fw.ENG.find(e=>e.name===engine).edit[0];same(visible(d),want);same(d.names,names);
 }
}
console.log(`web sample order: ${checks} checks passed`);
