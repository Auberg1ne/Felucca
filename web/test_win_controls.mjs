// SPDX-License-Identifier: GPL-3.0-only
// Actual editor DOM handlers + real native firmware SysEx, not mock parameter/DSP state.
import fs from 'node:fs';import vm from 'node:vm';import assert from 'node:assert/strict';import{spawn}from'node:child_process';import readline from'node:readline';
const html=fs.readFileSync(new URL('editor.html',import.meta.url),'utf8');
const proto=html.slice(html.indexOf('/*PROTO-BEGIN*/'),html.indexOf('/*PROTO-END*/'));
function take(name){const start=html.search(new RegExp(`(?:async )?function ${name}\\(`));assert(start>=0,name);let i=html.indexOf('{',start)+1,depth=1;while(depth){depth+=(html[i]==='{')-(html[i]==='}');i++;}return html.slice(start,i);}
let child,lines;const waiting=[];let error;
function start(){error=null;child=spawn(process.argv[2],[],{stdio:['pipe','pipe','inherit']});lines=readline.createInterface({input:child.stdout});lines.on('line',line=>{const x=waiting.shift();if(!x)throw Error('unsolicited firmware reply');x.resolve(line);});child.on('exit',code=>{error=Error('firmware exited '+code);for(const x of waiting.splice(0))x.reject(error);});}
async function restart(){if(child.exitCode===null){const done=new Promise(r=>child.once('exit',r));child.stdin.end();await done;}lines.close();start();}
start();
function command(text){if(error)throw error;return new Promise((resolve,reject)=>{waiting.push({resolve,reject});child.stdin.write(text+'\n');});}
let requests=0;const commands=[];
async function transport(bytes,opt){requests++;const wire=E.frame(...bytes);commands.push({bytes:Array.from(wire),opt});const line=await command('F '+Array.from(wire).join(' '));return Array.from(line.split(' ').slice(1),Number);}
const context={setTimeout,clearTimeout,console,performance,transport};
const functions=['paramRow','showValue','touched','userSet','sendSet','rereadEngineDesc','selectTrack','afterSoundChange','refreshAll'];
const E=vm.runInNewContext(proto+`
let dev,dragging=null,needReload=false;const ctl=new Map(),sets=new Map(),ops=[];const document={activeElement:null};const RICH_SELECT=false,WAVE_IC={},G_SKIP=new Set();
class Element{constructor(tag,attrs={}){this.tagName=tag.toUpperCase();this.value=String(attrs.value??'');Object.assign(this,attrs);this.events={};this.children=[];this.classList={add(){},remove(){}};}append(...xs){this.children.push(...xs);}addEventListener(n,f){this.events[n]=f;}emit(n){return this.events[n]?.({preventDefault(){},shiftKey:false});}}
const el=(tag,attrs,...children)=>{const e=new Element(tag,attrs);e.append(...children);return e;};const visible=d=>d&&d.max>d.min&&d.label!=='-';const key=(s,id)=>s+':'+id;const modDst=(d,x)=>x;const mixFromDump=()=>{};
const rq=async(bytes,opt)=>unframe(await transport(bytes,opt)).a;
const assertSession=d=>{if(d!==dev)throw Error('closed');};
const getDescOrNull=async(s,id,d=dev)=>parse[CMD.DESC](await rq(req.desc(s,id)));
const getDump=async(d=dev)=>parse[CMD.DUMP](await rq(req.dump()),d.info);
const renderSteps=()=>{},updateBrowser=()=>{},loadSteps=async()=>{},readMixer=async()=>{},sayK=()=>{};const hasTracks=()=>!!dev?.info.ntrk;
const op=fn=>{const p=fn();ops.push(p);return p;};
function buildGroups(){ctl.clear();dev.built=dev.dump.engine;paramRow(0,109);}
`+functions.map(take).join('\n')+`
;({frame,req,parse,CMD,F,winDescriptor,winControlAvailable,winSlot,fmtValue,assertWinLibraryTransfer,libraryFile,auditionPatch,
 async init(){const info=parse[CMD.INFO](await rq(req.info()));dev={info,sel:0,pdesc:Array(info.pcount),gdesc:[],link:{hasQueued:()=>false,cur:null}};dev.dump=await getDump();dev.pdesc[108]=await getDescOrNull(0,108);await rereadEngineDesc();},
 async select(i){selectTrack(i);await Promise.all(ops.splice(0));},async engine(e){await rq(req.set(1,20,e));await afterSoundChange();},
 async edit(value){const c=ctl.get('0:109');if(!c)throw Error('no WIN control');c.input.value=String(value);c.input.emit('input');await Promise.resolve();await new Promise(r=>setTimeout(r,0));},
 async dump(t){return parse[CMD.TRACK_DUMP](await rq(req.trackDump(t)),dev.info);},
 installDescriptor(d){dev.pdesc[109]=d;buildGroups();},tryScalar(v){userSet(0,109,v,false);},info:()=>dev.info,desc:()=>dev.pdesc[109],control:()=>ctl.get('0:109'),state:()=>dev,
 async refresh(){await afterSoundChange();}
})`,context);
let checks=0;const check=(v,msg)=>{assert(v,msg);checks++;};
try{
 await E.init();check(E.winControlAvailable(E.info(),8,E.desc()),'real current WIN descriptor');
 check(E.control()?.input.type==='range','continuous range, not discrete selector');
 check(E.control().input.min===0 && E.control().input.max===127,'full numeric range');
 for(const d of [{...E.desc(),label:'G-F RSV2',max:0,def:0},{...E.desc(),def:0},{...E.desc(),fmt:E.F.ENUM},null])check(!E.winControlAvailable(E.info(),8,d),'reserved/wrong descriptor stays hidden');
 check(!E.winControlAvailable(E.info(),4,E.desc()),'non-GRAIN hidden');
 const canonical=E.desc(),beforeReserved=commands.length;E.installDescriptor({...canonical,label:'G-F RSV2',min:0,max:0,def:0});check(!E.control(),'old reserved row absent');E.tryScalar(0);check(commands.length===beforeReserved,'stale event cannot edit reserved109');await E.refresh();check(!!E.control(),'actual descriptor refresh restores control');
 for(let track=0;track<4;track++){
  const before=commands.length;await E.select(track);
  if(track)check(commands.slice(before).some(x=>x.bytes[4]===E.CMD.DESC&&x.bytes[6]===109),'descriptor109 refreshed after track selection');
  await E.edit(0);const d=await E.dump(track);check(d.p[109]===0,'same value must dispatch independently on each track');
  check(E.control().out.textContent==='0 GAUSS','anchor name and exact number');
 }
 for(let track=0;track<4;track++)check((await E.dump(track)).p[109]===0,'all four independently zero');
 for(let track=0;track<4;track++){
  await E.select(track);await E.edit([1,64,96,127][track]);check((await E.dump(track)).p[109]===[1,64,96,127][track],'independent intermediate/anchor values');
 }
 for(let track=0;track<4;track++)check((await E.dump(track)).p[109]===[1,64,96,127][track],'other tracks retained');
 check(commands.some(x=>x.bytes[4]===E.CMD.TRACK_PARAM&&x.bytes[6]===109),'existing targeted scalar command');
 await E.select(0);await E.engine(4);check(!E.control() && !E.winDescriptor(E.desc()),'actual reserved metadata on engine change hides control');
 await E.engine(8);check(!!E.control(),'engine switch refresh restores correct WIN');
 await E.edit(0);await E.engine(8);check((await E.dump(0)).p[109]===64,'factory reset restores Hann');await E.edit(0);check((await E.dump(0)).p[109]===0,'same-track edit must resend after factory reset');
 const ctx={pe0:110,keys:Array(118).fill('G-F RSV2'),engines:Array(14).fill('')};ctx.engines[8]='GRAIN';
 for(const v of[0,1,63,65,126,127]){const p={engine:8,p:Array(118).fill(0)};p.p[109]=v;assert.throws(()=>E.libraryFile('preset',[p],ctx),/versioned backup/);checks++;}
 const sounds=[],audioProof=[];
 for(const win of[0,64,127,64]){
  await restart();check(true,'fresh native DSP process');await E.init();
  for(let track=0;track<4;track++){await E.select(track);await E.edit(win);check((await E.dump(track)).p[109]===win,'browser scalar reached actual firmware');}
  const row=(await command('A '+win)).split(' ');check(row[0]==='A'&&+row[2]>32&&+row[3]>1&&+row[4]>100&&+row[5]>0,'meaningful final PCM and spawn latches');sounds.push(row[1]);audioProof.push({WIN:win,hash:row[1],peak:+row[2],RMS:+row[3],nonzero:+row[4],latchedGrains:+row[5]});
 }
 check(new Set(sounds.slice(0,3)).size===3,'browser WIN edits audibly change production PCM');check(sounds[1]===sounds[3],'fresh Hann repeat is byte-deterministic');
 console.log(JSON.stringify(audioProof));
 console.log(`WIN browser controls: ${checks} checks passed; actual DOM handlers/native firmware, four tracks, descriptor refresh, provenance and nonzero DSP`);
}finally{child.stdin.end();lines.close();}
