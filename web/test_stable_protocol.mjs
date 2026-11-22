// SPDX-License-Identifier: GPL-3.0-only
import fs from 'node:fs';import vm from 'node:vm';import assert from 'node:assert/strict';import{spawn,execFileSync}from'node:child_process';import readline from'node:readline';
const html=fs.readFileSync(new URL('editor.html',import.meta.url),'utf8');
const old=execFileSync('git',['show','fd7f6f036487c53af3d31df3f209b63d6d2d9e8d:web/editor.html'],{encoding:'utf8'});
const make=s=>vm.runInNewContext(s.slice(s.indexOf('/*PROTO-BEGIN*/'),s.indexOf('/*PROTO-END*/'))+';({frame,unframe,CMD,parse,req,bank,readLibraryFile,remapParams,assertWinLibraryTransfer,libraryFile,auditionPatch'+(s===html?',stableCapability,negotiateStable,stableScalarRequest,stableScalarReply,parameterProvenance,legacyCoverage':'')+'})',{setTimeout,clearTimeout,console});
const E=make(html),O=make(old);let checks=0;const check=(x,m)=>{assert(x,m);checks++;};
function gateway(path){const child=spawn(path,[],{stdio:['pipe','pipe','inherit']}),q=[];const lines=readline.createInterface({input:child.stdout});lines.on('line',s=>q.shift()?.resolve(s));child.on('exit',code=>{for(const p of q.splice(0))p.reject(Error('gateway exit '+code));});return {async rq(r){const line=await new Promise((resolve,reject)=>{q.push({resolve,reject});child.stdin.write('F '+Array.from(E.frame(...r)).join(' ')+'\n');});const w=line.split(' ').slice(1).map(Number);return w.length?Array.from(E.unframe(w).a):[];},async close(){child.stdin.end();await new Promise(r=>child.once('exit',r));lines.close();}};}
const newer=gateway(process.argv[2]),older=gateway(process.argv[3]);
try{
 const ni=await newer.rq(E.req.info()),oi=await older.rq(O.req.info());
 for(const [editor,info]of[[O,oi],[E,oi],[O,ni],[E,ni]]){const p=editor.parse[editor.CMD.INFO](info);check(p.pcount===118&&p.pe0===110,'all four combinations retain legacy118');}
 let sent=0;check(await E.negotiateStable(E.parse[E.CMD.INFO](oi),()=>{sent++;throw Error('old firmware');},33)===null && !sent,'new client never probes old firmware without capability');
 const info=E.parse[E.CMD.INFO](ni);check(info.stable.count===118 && info.stable.flags===7,'no C or bulk advertisement');
 await newer.rq(E.req.upStore(0,"LEGACY"));await older.rq(O.req.upStore(0,"LEGACY"));
 for(const r of [E.req.upGet(0),E.req.dump(),E.req.trackDump(0),E.req.get(0,109),E.req.desc(0,109)])check(JSON.stringify(await newer.rq(r))===JSON.stringify(await older.rq(r)),'legacy frames byte-identical');
 let session=await E.negotiateStable(info,r=>newer.rq(r),33);check(session.negotiated,'negotiated actual firmware');
 for(let track=0;track<4;track++)for(const value of[0,1,64,127]){
  const r=await newer.rq(E.stableScalarRequest(session,track,109,value));check(E.stableScalarReply(r,session,track,109).value===value,'targeted WIN scalar');
 }
 const get=await newer.rq(E.stableScalarRequest(session,0,109));check(E.stableScalarReply(get,session,0,109).value===127,'stable read');
 assert.throws(()=>E.stableScalarRequest(session,0,118,55));checks++;
 await assert.rejects(()=>E.negotiateStable({stable:{...info.stable,count:119}},()=>{throw Error('must not send');},33),/Unsupported/);checks++;
 const unknown=await newer.rq([82,[0,2,1,33,0,118,0]]);check(unknown[1]===3,'unknown negotiated version refused');
 const stale=await newer.rq(E.stableScalarRequest(session,0,109,0));check(stale[1]===2,'failed negotiation revokes stale lease');
 session=await E.negotiateStable(info,r=>newer.rq(r),34);
 const bulk=await newer.rq([82,[4,1,1,34,0,0,119,0,...Array(238).fill(0)]]);check(bulk[1]===1,'stable119 bulk explicitly unsupported, not reinterpreted');
 const historical=Array.from({length:119},(_,i)=>i),mapped=E.remapParams(historical,null,Array(118).fill(''),110);
 check(mapped[110]===111&&mapped[117]===118,'legacy119 tail-eight mapping');
 check(E.parameterProvenance(null).kind==='legacy-tail-eight','unmarked always historical');
 check(E.parameterProvenance({layout:'stable-id',version:1,count:119}).kind==='stable-id','explicit marker distinguished from length');
 assert.throws(()=>E.parameterProvenance({layout:'stable-id',version:2,count:119}));checks++;
 const ctx={pe0:110,keys:Array(118).fill(''),engines:Array(14).fill('')};ctx.engines[8]='GRAIN';
 assert.throws(()=>E.readLibraryFile({format:'felucca-patch',parameterLayout:{layout:'stable-id',version:1,count:119},engine:8,p:historical},ctx),/not supported/);checks++;
 const marked={engine:8,p:historical,parameterLayout:{layout:'stable-id',version:1,count:119}};
 assert.throws(()=>E.req.upPut(0,marked),/unavailable/);checks++;
 let writes=0;await assert.rejects(()=>E.bank.put(()=>{writes++;},0,marked,info),/unavailable/);check(!writes,'marked library refusal before any wire request');
 for(const version of[0,2,127]){assert.throws(()=>E.readLibraryFile({format:'felucca-patch',version,engine:8,p:historical},ctx),/version/);checks++;}
 const uploadRequests=[];await E.bank.put(async r=>{uploadRequests.push(r);return [0,0];},0,{engine:8,name:'HIST',p:historical},info);
 const bytes=uploadRequests[0][1],offset=7;check(bytes.length===offset+2*118+32,'legacy119 upload encoded as118 values');
 check(bytes[offset+2*110]+128*bytes[offset+2*110+1]-8192===111 && bytes[offset+2*117]+128*bytes[offset+2*117+1]-8192===118,'upload preserves historical E0/E7 mapping');
 for(const c of[0,1,55,100]){
  const required=c?[...Array(119).keys()]:[...Array(118).keys()];check(E.legacyCoverage(required)===(c===0),'future-C abstract coverage refusal; no live C');
 }
 for(const win of[0,1,127]){const p={engine:8,p:Array(118).fill(0)};p.p[109]=win;assert.throws(()=>E.libraryFile('preset',[p],ctx),/versioned backup/);checks++;}
 const before=E.stableScalarReply(await newer.rq(E.stableScalarRequest(session,0,109)),session,0,109).value;
 const bad=await newer.rq([82,[2,1,1,34,0,0,109,0,0,65]]);check(bad[1]===4,'out-of-range rejected');check(E.stableScalarReply(await newer.rq(E.stableScalarRequest(session,0,109)),session,0,109).value===before,'atomic refusal');
 const start=html.indexOf('async function stableSet('),end=html.indexOf('/* engine / preset browser */',start);
 const uiSet=vm.runInNewContext(html.slice(start,end)+';stableSet',{negotiateStable:E.negotiateStable,stableScalarRequest:E.stableScalarRequest,stableScalarReply:E.stableScalarReply,assertSession:d=>{if(d.closed)throw Error('closed');},sessionRequest:(_d,r)=>newer.rq(r)});
 const device={info};for(let track=0;track<4;track++)check((await uiSet(device,track,109,[0,64,127,1][track])).value===[0,64,127,1][track],'actual browser stableSet negotiates before writing');
 console.log(`Stable browser/firmware: ${checks} checks passed; all four compatibility combinations, marked/unmarked119, WIN and refusal policy`);
}finally{await Promise.all([newer.close(),older.close()]);}
