// SPDX-License-Identifier: GPL-3.0-only
import {readFileSync} from 'node:fs';import vm from 'node:vm';import assert from 'node:assert/strict';
const html=readFileSync(new URL('editor.html',import.meta.url),'utf8');const proto=html.slice(html.indexOf('/*PROTO-BEGIN*/'),html.indexOf('/*PROTO-END*/'));
const E=vm.runInNewContext(proto+';({parse,CMD,req,engineAllowed,deviceEngineOrder,devicePresetRows,auditionPatch,makeMockDevice,Link})',{setTimeout,clearTimeout,setInterval,clearInterval,console});
let checks=0;const ok=x=>{assert(x);checks++};
const m=E.makeMockDevice(),input=[...m.access.inputs.values()][0],output=[...m.access.outputs.values()][0];const link=new E.Link(d=>output.send(d));input.onmidimessage=e=>link.receive(e.data);
const wire=await link.request(E.req.info());const base=E.parse[E.CMD.INFO](wire);ok(base.engineOnly==null);
const info=E.parse[E.CMD.INFO]([...wire,0x47,1,8]);ok(info.engineOnly===8 && info.nengines===base.nengines && info.pcount===base.pcount);ok(JSON.stringify(info.engines)===JSON.stringify(base.engines));ok(JSON.stringify(E.deviceEngineOrder(info))==='[8]');
for(let e=0;e<info.nengines;e++)ok(E.engineAllowed(info,e)===(e===8));
let calls=0;await assert.rejects(E.auditionPatch(async()=>{calls++},info,{engine:4,p:[]}),/incompatible preset/);ok(calls===0);
const rows=E.devicePresetRows(info,{8:['Cloud','Freeze'],4:['Legacy']},{state:{caps:8,filter:0},slots:{slots:[{slot:0,used:1,engine:4,name:'Old'},{slot:1,used:1,engine:8,name:'Grain'}]},favorites:{}});
ok(rows.every(r=>r.engine===8 || (r.engine===info.nengines && r.preset===1)) && rows.length===3);
console.log(`GRAIN-only editor: ${checks} checks passed; optional capability, unchanged IDs, selection and no-write rejection`);

process.exit(0);
