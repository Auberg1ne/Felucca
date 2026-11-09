#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Packaged-site test under /Felucca/, using one real authoritative node per page.
No browser sandbox bypass; no firmware mutation commands or synthetic editor traffic.
Native storage/engine tests cover sample upload and DSP separately."""
import argparse
import asyncio
import http.server
import json
import mimetypes
from pathlib import Path
import shutil
import tempfile
import threading

OBSERVE = '''window.__hosting={contexts:[],nodes:[]};
for(const [name,list] of [['AudioContext','contexts'],['AudioWorkletNode','nodes']]) {
const Native=window[name];window[name]=new Proxy(Native,{construct(T,args){const obj=Reflect.construct(T,args);window.__hosting[list].push(obj);return obj;}});
}'''

async def run(args):
    from playwright.async_api import async_playwright
    with tempfile.TemporaryDirectory(prefix='coffee-grains-pages-test-') as tmp:
        root=Path(tmp);dest=root/'Felucca';shutil.copytree(args.site,dest)
        releases=sorted((dest/'releases').iterdir());assert len(releases)==1,'Browser build gate requires exactly one candidate'
        release=releases[0].name
        import importlib.util
        spec=importlib.util.spec_from_file_location('builder',Path(__file__).with_name('site.py'))
        builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
        builder.channel(dest,'dev',release);builder.channel(dest,'stable',release,True)
        class Handler(http.server.SimpleHTTPRequestHandler):
            def __init__(self,*a,**kw):super().__init__(*a,directory=str(root),**kw)
            def guess_type(self,p):
                if p.endswith('.wasm'):return 'application/wasm'
                if p.endswith(('.js','.mjs')):return 'text/javascript'
                return mimetypes.guess_type(p)[0] or 'application/octet-stream'
            def log_message(self,*a):pass
        server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        origin=f'http://127.0.0.1:{server.server_port}';errors=[];checks=[]
        try:
            async with async_playwright() as pw:
                browser=await pw.chromium.launch(executable_path=str(args.chromium),headless=True,chromium_sandbox=True)
                try:
                    context=await browser.new_context()
                    async def network(route):
                        if route.request.url.startswith(origin+'/') or route.request.url.startswith(('data:','blob:')):await route.continue_()
                        else:await route.abort()
                    await context.route('**/*',network)
                    await context.add_init_script(OBSERVE)
                    def watch(page):
                        page.on('pageerror',lambda e:errors.append(str(e)))
                        page.on('console',lambda m:errors.append(m.text) if m.type=='error' else None)
                        page.on('response',lambda r:errors.append(f'HTTP {r.status} {r.url}') if r.status>=400 else None)
                    context.on('page',watch)
                    page=await context.new_page()
                    await page.goto(origin+'/Felucca/stable/');await page.wait_for_url('**/emu/index.html?channel=stable')
                    assert (await page.locator('#build-info').inner_text()).startswith('STABLE · ');assert await page.title()=='COFFEE GRAINS — Granular Synthesizer';checks.append('repository-subpath channel and branding')
                    assert await page.evaluate('__hosting.contexts.length')==0;checks.append('no AudioContext before gesture')
                    await page.get_by_role('button',name='Switch on',exact=True).click()
                    await page.wait_for_function('document.getElementById("start").hidden',timeout=30000)
                    assert await page.evaluate('__hosting.contexts.length===1 && __hosting.nodes.length===1 && __hosting.contexts[0].state==="running"')
                    checks.append('default real WASM/AudioWorklet startup, one authority')
                    await page.keyboard.press('z');await page.wait_for_timeout(300);await page.keyboard.up('z');checks.append('normal keyboard event path')
                    async with page.expect_popup() as popup:
                        await page.locator('#open-editor').click()
                    editor=await popup.value;await editor.wait_for_load_state()
                    # Existing transport regression verifies actual response fields; here assert deployed path/DOM.
                    await editor.wait_for_function('document.getElementById("engine")?.options.length>1',timeout=30000)
                    assert await editor.locator('#engine').is_disabled();checks.append('packaged separate-window real read-only editor')
                    assert 'COFFEE GRAINS' in await editor.title()
                    await editor.close()
                    await page.evaluate('localStorage.setItem("coffee-grains-stable-knobs","1");localStorage.setItem("felucca-emu-flash","untouched sentinel")')
                    await page.goto(origin+'/Felucca/dev/');await page.wait_for_url('**/emu/index.html?channel=dev')
                    assert (await page.locator('#build-info').inner_text()).startswith('DEV — experimental · ');assert await page.evaluate('localStorage.getItem("coffee-grains-stable-knobs")==="1" && localStorage.getItem("coffee-grains-dev-knobs")===null && localStorage.getItem("felucca-emu-flash")==="untouched sentinel"')
                    checks.append('stable/dev persistence isolation and old key retained')
                    assert await page.evaluate('__hosting.contexts.length')==0
                    assert not errors,errors
                    checks.append('no page/console errors or missing resources')
                    print(json.dumps({'passed':len(checks),'checks':checks,'errors':errors,'public_https':'UNVERIFIED','audio_deadlines':'UNVERIFIED'},indent=2))
                finally:await browser.close()
        finally:server.shutdown();server.server_close();thread.join()

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--site',type=Path,required=True);p.add_argument('--chromium',type=Path,default=Path('/usr/bin/chromium'))
    asyncio.run(run(p.parse_args()))
