#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Opt-in real-browser smoke test of unchanged simulator/editor files.

Requires Python Playwright and a sandbox-capable system Chromium. Uses an existing
WASM artifact; never builds, installs, weakens the sandbox, or accesses hardware.
Results/snapshots use a temporary directory outside the checkout. Instrumentation
observes native object creation and port traffic; it does not dispatch commands,
render firmware, replace handlers, or measure AudioWorklet deadlines.
"""
import argparse
import asyncio
import hashlib
import http.server
import json
import mimetypes
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import traceback
from urllib.parse import urlsplit

class EnvironmentUnavailable(RuntimeError):
    pass


OBSERVE = r"""
(() => {
  const t=window.__startupSmoke={contexts:[],nodes:[],workers:[],events:[],storageWrites:0};
  const event=(kind,data={})=>{if(t.events.length<4096)t.events.push({kind,...data});};
  for(const [name,list] of [['AudioContext','contexts'],['AudioWorkletNode','nodes'],['Worker','workers']]) {
    const Native=window[name];if(!Native)continue;
    window[name]=new Proxy(Native,{construct(target,args){
      const object=Reflect.construct(target,args);t[list].push(object);event('new-'+name);
      if(name==='AudioWorkletNode') {
        const connect=object.connect.bind(object);
        object.connect=(...args)=>{event('node-connect');return connect(...args);};
      }
      if(name==='AudioContext')object.addEventListener('statechange',()=>event('context-state',{state:object.state}));
      if(name==='Worker'||name==='AudioWorkletNode') {
        const port=name==='Worker'?object:object.port;
        port.addEventListener('message',({data:m})=>event(name+'-reply',{
          type:m?.type,protocol:m?.protocol,attempt:m?.attempt,hasSectors:!!m?.sectors,
          diagnostics:m?.diagnostics,hasModule:m?.module instanceof WebAssembly.Module}));
        const post=port.postMessage.bind(port);
        port.postMessage=(m,...rest)=>{event(name+'-send',{
          type:m?.type,protocol:m?.protocol,attempt:m?.attempt,keys:Object.keys(m||{}),
          hasModule:m?.module instanceof WebAssembly.Module});return post(m,...rest);};
      }
      return object;
    }});
  }
  const set=Storage.prototype.setItem;
  Storage.prototype.setItem=function(...args){t.storageWrites++;return set.apply(this,args);};
})();
"""


def options():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--wasm',type=Path,required=True,help='Existing simulator WASM with S4A exports')
    p.add_argument('--sha256',required=True,help='Expected artifact SHA-256; mismatch stops before launch')
    p.add_argument('--chromium',type=Path,default=Path('/usr/bin/chromium'))
    p.add_argument('--case',choices=['all','default','worker','cancel','corrupt-storage','missing-wasm'],default='all')
    return p.parse_args()


def snapshot(repo,root,wasm):
    dest=root/'snapshot'
    paths=subprocess.check_output(['git','ls-files','--','web'],cwd=repo,text=True).splitlines()
    paths+=['web/emu/startup.js','web/emu/prepare_worker.js']
    hashes={}
    for name in sorted(set(paths)):
        source=repo/name
        if not source.is_file():continue
        target=dest/name;target.parent.mkdir(parents=True,exist_ok=True)
        data=source.read_bytes();target.write_bytes(data);hashes[name]=hashlib.sha256(data).hexdigest()
    target=dest/'web/emu/felucca.wasm';target.write_bytes(wasm)
    hashes['web/emu/felucca.wasm']=hashlib.sha256(wasm).hexdigest()
    return dest,hashes


def serve(directory):
    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self,*args,**kwargs):super().__init__(*args,directory=str(directory),**kwargs)
        def guess_type(self,path):
            if path.endswith(('.js','.mjs')):return 'text/javascript'
            if path.endswith('.wasm'):return 'application/wasm'
            return mimetypes.guess_type(path)[0] or 'application/octet-stream'
        def end_headers(self):
            self.send_header('Cache-Control','no-store');super().end_headers()
        def log_message(self,*args):pass
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    return server,thread


async def run(args,repo,root,report):
    from playwright.async_api import async_playwright
    data=args.wasm.read_bytes()
    assert hashlib.sha256(data).hexdigest()==args.sha256,'WASM SHA-256 mismatch'
    if not args.chromium.is_file():raise EnvironmentUnavailable('System Chromium is missing')
    dest,report['source_sha256']=snapshot(repo,root,data)
    server,thread=serve(dest)
    origin=f'http://127.0.0.1:{server.server_port}'
    report['origin']=origin
    cases=['default','worker','cancel','corrupt-storage','missing-wasm'] if args.case=='all' else [args.case]
    try:
        async with async_playwright() as pw:
            # Explicit executable, fresh profile, enabled sandbox, no fallback.
            try:
                browser=await pw.chromium.launch_persistent_context(
                    str(root/'profile'),executable_path=str(args.chromium),headless=True,
                    chromium_sandbox=True,ignore_default_args=True,timeout=20000,
                    args=['--headless','--remote-debugging-pipe','--no-first-run','--no-default-browser-check',
                          '--host-resolver-rules=MAP * ~NOTFOUND, EXCLUDE 127.0.0.1'])
            except Exception as e:
                raise EnvironmentUnavailable('Chromium could not launch: '+str(e)) from e
            try:
                report['browser_version']=browser.browser.version
                async def local_only(route):
                    url=route.request.url
                    if url.startswith(origin+'/') or url.startswith(('data:','blob:','about:')):
                        await route.continue_()
                    else:
                        report.setdefault('blocked_external',[]).append(url);await route.abort()
                await browser.route('**/*',local_only)
                await browser.add_init_script(OBSERVE)
                for case in cases:
                    item={'case':case,'checks':[],'console':[],'page_errors':[]};report['cases'].append(item)
                    page=await browser.new_page()
                    page.set_default_timeout(20000)
                    page.on('pageerror',lambda e,i=item:i['page_errors'].append(str(e)))
                    page.on('console',lambda m,i=item:i['console'].append({'type':m.type,'text':m.text}) if m.type in ['error','warning'] else None)
                    def check(condition,name):
                        item['checks'].append({'name':name,'pass':bool(condition)})
                        assert condition,name
                    # Each case uses the actual page from a clean test-only saved state.
                    await page.goto(origin+'/web/emu/index.html',wait_until='load')
                    await page.evaluate('localStorage.clear()')
                    if case=='corrupt-storage':
                        await page.evaluate("localStorage.setItem('felucca-emu-flash','{')")
                    if case=='missing-wasm':
                        await page.route('**/felucca.wasm',lambda r:r.fulfill(status=404,body='intentional test failure'))
                    suffix='' if case=='default' else '?worker_prepare=1'
                    await page.goto(origin+'/web/emu/index.html'+suffix,wait_until='load')
                    go=page.locator('#go')
                    try:
                        check(await page.evaluate('__startupSmoke.contexts.length===0'),'No context before a deliberate action')
                        if case=='default':
                            check(await go.inner_text()=='Switch on','Ordinary byte startup is default')
                            await go.click()
                        else:
                            check('Prepare' in await go.inner_text(),'Worker opt-in offers preparation')
                            await go.click()
                            if case=='missing-wasm':
                                await page.wait_for_function("document.querySelector('#go').textContent==='Reload required'")
                                check(await page.evaluate('__startupSmoke.contexts.length===0'),'Fetch failure creates no context and no fallback')
                            else:
                                await page.wait_for_function("document.querySelector('#go').textContent==='Start audio'")
                                check(await page.evaluate('__startupSmoke.contexts.length===0'),'Worker readiness creates no AudioContext')
                                check(await page.locator('#open-editor').is_disabled(),'Editor disabled after preparation')
                                if case=='cancel':
                                    await page.locator('#cancel-worker').click()
                                    check(await go.inner_text()=='Reload required','Cancellation requires reload')
                                    check(await page.evaluate('__startupSmoke.contexts.length===0'),'Cancellation never creates authority')
                                else:
                                    await go.click()
                        if case in ['default','worker']:
                            await page.wait_for_function("document.querySelector('#start').hidden && !document.querySelector('#open-editor').disabled")
                            await page.wait_for_function("__startupSmoke.events.filter(e=>e.kind==='AudioWorkletNode-reply'&&e.type==='frame').length>=2")
                            counts=await page.evaluate('({contexts:__startupSmoke.contexts.length,nodes:__startupSmoke.nodes.length,workers:__startupSmoke.workers.length,state:__startupSmoke.contexts[0].state,time:__startupSmoke.contexts[0].currentTime})')
                            check(counts['contexts']==1 and counts['nodes']==1,'Exactly one authoritative context/node')
                            check(counts['workers']==(case=='worker'),'Expected disposable Worker count')
                            await page.wait_for_function('start=>__startupSmoke.contexts[0].currentTime>start+0.1',arg=counts['time'])
                            check(counts['state']=='running','AudioContext running and clock progresses (no deadline claim)')
                            async with page.expect_popup() as popup:
                                await page.locator('#open-editor').click()
                            editor=await popup.value
                            editor.on('pageerror',lambda e,i=item:i['page_errors'].append('editor: '+str(e)))
                            await editor.wait_for_function("document.querySelector('#status').textContent.includes('Simulator read-only')")
                            check(await editor.locator('#engine').is_disabled(),'Real firmware editor loads with engine changes disabled')
                            check(await editor.locator('#engine option').count()>1,'Real engine descriptors populate selector')
                            check(await editor.locator('#preset').is_disabled(),'Preset mutations disabled')
                            await editor.close()
                            await page.wait_for_function("__startupSmoke.events.some(e=>e.kind==='AudioWorkletNode-send'&&e.type==='editor-close')")
                            check(True,'Closing editor releases controller session')
                        elif case=='corrupt-storage':
                            await page.wait_for_function("document.querySelector('#go').textContent==='Reload required'")
                            check(await page.evaluate('__startupSmoke.contexts.length===0'),'Corrupt save stops before authoritative allocation')
                            check(await page.locator('#persistence-status').get_attribute('data-state')=='read-failed','Corrupt save has visible write-prohibited status')
                            check(await page.evaluate("localStorage.getItem('felucca-emu-flash')==='{'"),'Original corrupt bytes retained')
                        item['events']=await page.evaluate('__startupSmoke.events')
                        if case in ['default','worker']:
                            events=item['events']
                            check(sum(e['kind']=='node-connect' for e in events)==1,'Node connects exactly once')
                            if case=='worker':
                                ready=[e for e in events if e['kind']=='Worker-reply' and e.get('type')=='prepared']
                                check(len(ready)==1 and ready[0]['hasModule'] and ready[0]['diagnostics']['calls']==207 and ready[0]['diagnostics']['writes']==0,'Disposable preparation returns module, 207 blocks and zero writes')
                                init=[e for e in events if e['kind']=='AudioWorkletNode-send' and e.get('type')=='initialize']
                                check(len(init)==1 and init[0]['hasModule'],'Exactly one compiled-module initialization')
                                order=[e.get('type') for e in events if e['kind'].startswith('AudioWorkletNode')]
                                check(order.index('initialized')<order.index('start')<order.index('started')<order.index('editor-open'),'Editor opens after initialization and first-render acknowledgement')
                            else:
                                check(sum(e['kind']=='AudioWorkletNode-send' and e.get('type')=='load' for e in events)==1,'Default path initializes from bytes once')
                        check(await page.evaluate('__startupSmoke.storageWrites===0'),'No browser persistence writes during test')
                        for event in item['events']:
                            if event['kind']=='Worker-send':
                                check(sorted(event['keys'])==['attempt','bytes','protocol','type','version'],'Worker receives only bytes and protocol metadata')
                        check(not item['page_errors'],'No page exceptions')
                        item['result']='PASS'
                    finally:
                        if not page.is_closed():await page.close()
            finally:await browser.close()
    finally:
        server.shutdown();server.server_close();thread.join(3)
        report['server_stopped']=not thread.is_alive()
        for path in [root/'profile',dest]:
            if path.exists():shutil.rmtree(path)


def main():
    args=options();repo=Path(__file__).resolve().parents[2]
    root=Path(tempfile.mkdtemp(prefix='coffee-grains-browser-startup-'))
    report={'result':'FAIL','cases':[],'limitations':['No callback timing or deadline measurements','No physical hardware validation','Observer wrappers affect scheduling; no timing claims']}
    print('Results:',root,flush=True)
    try:
        asyncio.run(run(args,repo,root,report));report['result']='PASS'
    except (ModuleNotFoundError, PermissionError, EnvironmentUnavailable) as e:
        report['result']='UNVERIFIED';report['error']=str(e)
    except Exception:
        report['error']=traceback.format_exc()
    finally:
        # Also clean snapshots when a prerequisite fails before server ownership.
        for path in [root/'profile',root/'snapshot']:
            if path.exists():shutil.rmtree(path)
        (root/'report.json').write_text(json.dumps(report,indent=2))
        total=sum(len(c['checks']) for c in report['cases'])
        passed=sum(bool(x['pass']) for c in report['cases'] for x in c['checks'])
        print(f"{report['result']}: {passed}/{total} checks; {root/'report.json'}",flush=True)
        if report.get('error'):print(report['error'],flush=True)
    return 0 if report['result']=='PASS' else 77 if report['result']=='UNVERIFIED' else 1


if __name__=='__main__':raise SystemExit(main())
