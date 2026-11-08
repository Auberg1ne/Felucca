// SPDX-License-Identifier: GPL-3.0-only
import {STARTUP,VERSION,WASM_MAX,MEMORY_BYTES,attemptOK,keysOnly,envelope} from './startup.js';
// No saved sectors, project data, callbacks, editor ports or persistence APIs.
// Scalar/pointer readers used by worklet output/publication. Call only after
// web_boot and render warm-up initialize the disposable LED/framebuffer state.
// Deliberately exclude web_anim_levels: it writes its static display cache.
const PUBLICATION_GETTERS = Object.freeze([
  'web_lit_buttons','web_lit_keys','web_dim_buttons','web_dim_keys','web_dim_level',
  'web_breath_buttons','web_breath_keys','web_mid_keys','web_out_l','web_out_r',
  'web_screen_draws','web_screen','web_nor','web_nor_size','web_flash_writes_count'
]);
export async function prepareWasm(bytes, wasm=WebAssembly) {
  if(!(bytes instanceof ArrayBuffer)||!bytes.byteLength||bytes.byteLength>WASM_MAX)throw Error('Invalid WASM size');
  const module=await wasm.compile(bytes);
  if(wasm.Module.imports(module).length)throw Error('Preparation requires import-free WASM');
  let instance=await wasm.instantiate(module,{}),ex=instance.exports;
  if(ex.memory.buffer.byteLength!==MEMORY_BYTES||(typeof SharedArrayBuffer!=='undefined'&&ex.memory.buffer instanceof SharedArrayBuffer))throw Error('Unexpected preparation memory');
  ex._initialize();ex.web_nor_erase();ex.web_master(700);ex.web_boot();
  if(ex.web_now_ms()!==0||ex.web_playing()!==0)throw Error('Unexpected disposable boot state');
  const before=ex.web_flash_writes_count(),forbidden=ex.web_editor_forbidden_count();
  for(let i=0;i<207;i++)ex.web_render(128);
  for(const name of PUBLICATION_GETTERS)ex[name]();
  const diagnostics={calls:207,frames:26496,ms:ex.web_now_ms(),writes:ex.web_flash_writes_count(),forbidden:ex.web_editor_forbidden_count(),memoryBytes:ex.memory.buffer.byteLength};
  if(before||forbidden||diagnostics.writes||diagnostics.forbidden||diagnostics.ms<600||diagnostics.ms>610)throw Error('Disposable preparation invariant failed');
  instance=null;ex=null;return {module,diagnostics};
}
// Keep the module alive until the owning page terminates this Worker.
let used=false,retainedModule=null;
if(typeof WorkerGlobalScope!=='undefined'&&globalThis instanceof WorkerGlobalScope){
  globalThis.onmessage=async({data:m})=>{
    const attempt=m?.attempt;
    try{
      if(used||m?.protocol!==STARTUP||m.version!==VERSION||!attemptOK(attempt)||m.type!=='prepare'||
         !keysOnly(m,['protocol','version','type','attempt','bytes']))throw Error('Invalid/duplicate preparation request');
      used=true;const {module,diagnostics}=await prepareWasm(m.bytes);retainedModule=module;
      postMessage(envelope('prepared',attempt,{module,diagnostics}));
    }catch(e){postMessage(envelope('error',attempt,{error:String(e?.message||e).slice(0,256)}));}
  };
}
