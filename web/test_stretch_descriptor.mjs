// SPDX-License-Identifier: GPL-3.0-only
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const html=readFileSync(new URL('editor.html',import.meta.url),'utf8');
const source=html.slice(html.indexOf('async function rereadEngineDesc('),html.indexOf('async function loadSteps('));
let calls=[],builds=0,current,checks=0;
const eq=(...args)=>{checks++;assert.equal(...args);};
const deep=(...args)=>{checks++;assert.deepEqual(...args);};
const refresh=vm.runInNewContext(source+';rereadEngineDesc',{
 assertSession(d){eq(d,current);},
 async getDescOrNull(scope,id,d){calls.push(id);return id===108 && d.info.pe0===110?{label:d.dump.engine===8?'STRETCH':'G-F RSV1',min:0,max:d.dump.engine===8?127:0,def:0}:d.pdesc[id];},
 buildGroups(){builds++;current.built=current.dump.engine;}
});
current={info:{pcount:118,pe0:110},dump:{engine:8},built:0,pdesc:Array.from({length:118},(_,id)=>({label:id===108?'G-F RSV1':`P${id}`,min:0,max:0}))};
const original=JSON.stringify(current.pdesc.slice(0,108));
await refresh(current);deep(calls,[108,110,111,112,113,114,115,116,117]);eq(current.pdesc[108].max,127);eq(builds,1);
eq(JSON.stringify(current.pdesc.slice(0,108)),original);
calls=[];await refresh(current);eq(builds,1);eq(calls.length,9); // no unnecessary rebuild
current.dump.engine=0;calls=[];await refresh(current);eq(current.pdesc[108].label,'G-F RSV1');eq(current.pdesc[108].max,0);eq(builds,2);
current.dump.engine=8;await refresh(current);eq(current.pdesc[108].max,127);eq(builds,3);
// Legacy layouts retain exactly their original eight engine descriptor requests.
current={info:{pcount:111,pe0:103},dump:{engine:0},built:0,pdesc:Array.from({length:111},(_,id)=>({label:`P${id}`,min:0,max:127}))};
calls=[];await refresh(current);deep(calls,[103,104,105,106,107,108,109,110]);eq(builds,3);
// Stable library keys must not turn an engine-aware display label into a file migration.
const librarySource=html.slice(html.indexOf('const stableParamKey ='),html.indexOf('/* one pattern step'));
const L=vm.runInNewContext(librarySource+';({paramKeys,sameKeys,remapParams})');
const desc=Array.from({length:118},(_,i)=>({label:i===108?'STRETCH':`P${i}`}));
const modern=L.paramKeys(desc,110,118);desc[108].label='G-F RSV1';const legacy=L.paramKeys(desc,110,118);
eq(modern[108],'G-F RSV1');eq(L.sameKeys(modern,legacy),true);
const transitional=Array.from(modern);transitional[108]='STRETCH';eq(L.sameKeys(transitional,legacy),true);
const values=Array.from({length:118},(_,i)=>i===108?127:i);
deep(Array.from(L.remapParams(values,transitional,legacy)),values);
const extended=Array.from(legacy);extended.push('FUTURE');const remapped=L.remapParams(values,transitional,extended);
eq(remapped[108],127);eq(remapped[118],null);
// Exercise the actual adoption path: an equivalent legacy key must not rewrite records.
let writes=0;const engines=Array.from({length:14},(_,i)=>i===8?'GRAIN':`E${i}`);
const saved={engine:8,engineName:'GRAIN',p:values.slice(),opaqueFuture:{keep:73}};
const adoptSource=html.slice(html.indexOf('async function libAdopt(d)'),html.indexOf('const engineOk'));
const adopt=vm.runInNewContext(adoptSource+';libAdopt',{
 libMeta:{keys:transitional,engines,pe0:110},lib:[saved],sameKeys:L.sameKeys,
 fromPerc:()=>null,libWrite:async()=>{writes++;},store:{setMeta:async()=>{}},renderLib:()=>{}
});
await adopt({keys:legacy,info:{engines,pe0:110}});
eq(writes,0);eq(saved.p[108],127);eq(saved.opaqueFuture.keep,73);
console.log(`STRETCH editor refresh: ${checks} assertions passed (engine switches, stable common metadata, legacy layout)`);
