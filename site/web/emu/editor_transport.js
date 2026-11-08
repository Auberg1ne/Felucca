// SPDX-License-Identifier: GPL-3.0-only
// S4A envelope only. Firmware commands and replies remain in editor.c / Link.
export const S4A = "coffee-grains-s4a";
export const RX_MAX = 8, TX_MAX = 664;
const u32 = n => Number.isInteger(n) && n > 0 && n <= 0xffffffff;
export function readonlyFrame(b) {
  if (!Array.isArray(b) || b.length < 6 || b.length > RX_MAX || b[0] !== 240 || b.at(-1) !== 247 ||
      b[1] !== 125 || b[2] !== 70 || b[3] !== 76 || !Array.from(b).slice(1,-1).every(n=>Number.isInteger(n)&&n>=0&&n<128)) return false;
  const n=b.length-6;
  return ([1,25,27].includes(b[4])&&n===0) || ([2,5,31].includes(b[4])&&n===2) || ([10,29].includes(b[4])&&n===1);
}
export class WasmReadonlyEndpoint {
  constructor(ex, post) { this.ex=ex; this.post=post; this.epoch=0; }
  receive(m) {
    if(!m || typeof m!=="object")return;
    const ex=this.ex;
    if(m.type==="editor-open") {
      const rc=u32(m.epoch)?ex.web_editor_open(m.epoch):3;
      if(!rc)this.epoch=m.epoch;
      this.post({type:"editor-opened",epoch:m.epoch,rc});return;
    }
    if(!this.epoch || m.epoch!==this.epoch)return;
    if(m.type==="editor-close") { ex.web_editor_close(this.epoch);this.epoch=0;return; }
    if(!u32(m.ticket))return;
    if(m.type==="editor-cancel") { ex.web_editor_cancel(m.epoch,m.ticket);return; }
    if(m.type!=="editor-request")return;
    let rc=1;
    if(readonlyFrame(m.bytes)) {
      new Uint8Array(ex.memory.buffer,ex.web_editor_input(),RX_MAX).set(m.bytes);
      rc=ex.web_editor_submit(m.epoch,m.ticket,m.bytes.length);
    }
    if(rc)this.post({type:"editor-error",epoch:m.epoch,ticket:m.ticket,error:`S4A denied/busy (${rc})`});
  }
  drain() { // Called after web_render, never from receive/onmessage.
    const ex=this.ex,n=ex.web_editor_length();
    if(!n)return;
    const epoch=ex.web_editor_epoch(),ticket=ex.web_editor_ticket();
    if(n>TX_MAX || epoch!==this.epoch){ex.web_editor_close(epoch);this.epoch=0;return;}
    const bytes=Array.from(new Uint8Array(ex.memory.buffer,ex.web_editor_output(),n));
    ex.web_editor_pop(epoch,ticket);
    this.post({type:"editor-response",epoch,ticket,bytes});
  }
}
export class ReadonlyClient {
  constructor(port,epoch,Link,onClose=()=>{}) {
    this.port=port;this.epoch=epoch;this.ticket=0;this.pending=0;this.closed=false;this.onClose=onClose;
    this.link=new Link(bytes=>{
      if(!readonlyFrame(bytes))throw new Error("S4A read-only command denied");
      if(this.pending || this.ticket===0xffffffff)throw new Error("S4A busy/exhausted");
      this.pending=++this.ticket;
      port.postMessage({type:"editor-request",epoch,ticket:this.ticket,bytes});
    },{onTimeout:()=>this.close("Simulator request timed out"),onUnknown:()=>{}});
    port.onmessage=({data:m})=>{
      if(this.closed)return;
      if(!m || typeof m!=="object"){this.close("Invalid simulator envelope");return;}
      if(m.epoch!==epoch)return;
      if(m.type==="editor-closed"){this.close(m.error,false);return;}
      if(m.ticket!==this.pending || !this.pending)return;
      if(m.type==="editor-error"){this.close(m.error);return;}
      if(m.type!=="editor-response")return;
      if(!Array.isArray(m.bytes)||m.bytes.length>TX_MAX||m.bytes.length<6||m.bytes[0]!==240||m.bytes.at(-1)!==247||m.bytes[1]!==125||m.bytes[2]!==70||m.bytes[3]!==76||!Array.from(m.bytes).slice(1,-1).every(n=>Number.isInteger(n)&&n>=0&&n<128)){this.close("Invalid simulator response");return;}
      this.pending=0;this.link.receive(m.bytes);
    };
    port.onmessageerror=()=>this.close("Simulator channel error");port.start?.();
    port.postMessage({type:"editor-connected",epoch});
  }
  get idle() { return this.link.idle; }
  get lastSent() { return this.link.lastSent; }
  hasQueued(key) { return this.link.hasQueued(key); }
  request(r,opt={}) {
    if(this.closed)return Promise.reject(new Error("closed"));
    if(this.link.q.length>=8)return Promise.reject(new Error("S4A request queue full"));
    return this.link.request(r,{...opt,retries:0,quiet:false});
  }
  close(reason="Simulator disconnected",notify=true) {
    if(this.closed)return;this.closed=true;
    if(notify){if(this.pending)this.port.postMessage({type:"editor-cancel",epoch:this.epoch,ticket:this.pending});this.port.postMessage({type:"editor-close",epoch:this.epoch});}
    this.pending=0;this.link.close();this.port.close();this.onClose(reason);
  }
}
// Only the opener registered by the simulator can deliver the transferred port.
export function connectReadonly(win,nonce,Link,onClose) {
  return new Promise((resolve,reject)=>{
    const origin=win.location.origin,opener=win.opener;
    if(!opener || origin==="null" || !/^[a-f0-9]{32}$/.test(nonce||"")){reject(new Error("Open this editor from the running simulator"));return;}
    const cleanup=()=>{clearTimeout(timer);win.removeEventListener("message",accept);};
    const accept=e=>{
      const m=e.data;
      if(e.origin!==origin||e.source!==opener||m?.protocol!==S4A||m.nonce!==nonce)return;
      if(m.type==="reject"){cleanup();reject(new Error(m.error));return;}
      if(m.type!=="ready"||!u32(m.epoch)||e.ports.length!==1)return;
      cleanup();resolve(new ReadonlyClient(e.ports[0],m.epoch,Link,onClose));
    };
    const timer=setTimeout(()=>{cleanup();reject(new Error("Simulator handshake timed out"));},2000);
    win.addEventListener("message",accept);opener.postMessage({protocol:S4A,type:"hello",nonce},origin);
  });
}
export class SimulatorBroker {
  constructor(win,post,ready,Channel=MessageChannel) {
    this.win=win;this.post=post;this.ready=ready;this.Channel=Channel;this.next=0;this.lease=null;this.target=null;
    this.accept=e=>{
      const m=e.data,t=this.target;
      if(!t||e.origin!==win.location.origin||e.source!==t.child||m?.protocol!==S4A||m.type!=="hello"||m.nonce!==t.nonce)return;
      if(!ready()||this.lease||this.next===0xffffffff){t.child.postMessage({protocol:S4A,type:"reject",nonce:t.nonce,error:"Simulator unavailable or already controlled"},win.location.origin);return;}
      const epoch=++this.next;
      this.lease={epoch,port:null,pending:0,last:0};
      this.timer=setTimeout(()=>this.end("Simulator open timed out"),1000);
      post({type:"editor-open",epoch});
    };
    win.addEventListener("message",this.accept);
    this.watch=setInterval(()=>{
      const l=this.lease;
      if(l&&(!ready()||this.target?.child.closed||(l.pending&&Date.now()>l.deadline)))
        this.end("Simulator stopped, editor closed or request expired");
    },100);
  }
  setTarget(child,nonce) {
    if(!child||this.lease||(this.target&&!this.target.child.closed))return false;
    this.target={child,nonce};return true;
  }
  receive(m) {
    const l=this.lease;if(!m || typeof m!=="object" || !l||m.epoch!==l.epoch)return;
    if(m.type==="editor-opened") {
      if(l.port)return;
      clearTimeout(this.timer);
      if(m.rc||!this.ready()){this.end("Simulator refused connection");return;}
      const {port1,port2}=new this.Channel();l.port=port1;
      port1.onmessage=({data:q})=>{
        if(this.lease!==l)return;
        if(!q || typeof q!=="object"){this.end("Invalid editor envelope");return;}
        if(q.epoch!==l.epoch)return;
        if(q.type==="editor-connected"){l.confirmed=true;clearTimeout(this.timer);return;}
        if(q.type==="editor-close"){this.end("Editor disconnected");return;}
        if(!l.confirmed){this.end("Editor handshake incomplete");return;}
        if(q.type==="editor-cancel"&&q.ticket===l.pending){this.post(q);l.pending=0;return;}
        if(q.type!=="editor-request")return;
        if(!this.ready()||!u32(q.ticket)||q.ticket<=l.last||l.pending||!readonlyFrame(q.bytes)){this.end("S4A invalid, stale or busy request");return;}
        l.pending=l.last=q.ticket;l.deadline=Date.now()+1000;this.post(q);
      };
      port1.onmessageerror=()=>this.end("Editor channel error");port1.start?.();
      this.timer=setTimeout(()=>this.end("Editor handshake not acknowledged"),1000);
      this.target.child.postMessage({protocol:S4A,type:"ready",nonce:this.target.nonce,epoch:l.epoch},this.win.location.origin,[port2]);return;
    }
    if(m.ticket!==l.pending||!l.pending)return;
    if(m.type==="editor-error"){this.end(m.error);return;}
    if(m.type==="editor-response"){l.pending=0;l.port?.postMessage(m);}
  }
  end(error="Simulator disconnected") {
    clearTimeout(this.timer);const l=this.lease;if(!l)return;this.lease=null;
    this.post({type:"editor-close",epoch:l.epoch});
    if(l.port){l.port.postMessage({type:"editor-closed",epoch:l.epoch,error});l.port.close();}
    else this.target?.child.postMessage({protocol:S4A,type:"reject",nonce:this.target.nonce,error},this.win.location.origin);
  }
  dispose(){this.end();clearInterval(this.watch);this.win.removeEventListener("message",this.accept);}
}

// Client orchestration of allowed real queries; no synthetic firmware commands.
export async function readonlySnapshot(request, parse, req, CMD, info) {
  const first=parse[CMD.TRACK](await request(req.track()));
  const dump=parse[CMD.TRACK_DUMP](await request(req.trackDump(first.sel)),info),g=[];
  for(let id=0;id<info.gcount;id++)g.push(parse[CMD.GET](await request(req.get(1,id))).value);
  const last=parse[CMD.TRACK](await request(req.track()));
  if(first.sel!==last.sel || dump.track!==last.sel || dump.engine!==last.tracks[last.sel]?.engine || dump.preset!==last.tracks[last.sel]?.preset)
    throw new Error("Simulator selection changed; reconnect for a fresh snapshot");
  return {sel:last.sel,dump:{engine:dump.engine,preset:dump.preset,p:dump.p,g}};
}
export async function loadReadonly(request,parse,req,CMD) {
  const info=parse[CMD.INFO](await request(req.info()));
  const before=parse[CMD.TRACK](await request(req.track()));
  const pdesc=[],gdesc=[],names=[],titles=[];
  for(let id=0;id<info.pcount;id++)pdesc[id]=parse[CMD.DESC](await request(req.desc(0,id)));
  for(let id=0;id<info.gcount;id++)gdesc[id]=parse[CMD.DESC](await request(req.desc(1,id)));
  for(let e=0;e<info.nengines;e++) {
    const r=parse[CMD.NAMES](await request(req.names(e)));names[e]=r.names;titles[e]=r.titles;
  }
  const snap=await readonlySnapshot(request,parse,req,CMD,info);
  if(before.sel!==snap.sel||before.tracks[before.sel].engine!==snap.dump.engine)throw new Error("Selection changed during descriptors; reconnect");
  return {info,pdesc,gdesc,names,titles,...snap};
}
