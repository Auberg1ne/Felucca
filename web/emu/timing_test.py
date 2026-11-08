#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Opt-in matched Chromium timing experiment, never a production runtime import.

Requires existing Playwright/Chromium, explicit WASM and validated saved-NOR files.
No builds, downloads, hardware, additional audio nodes or automatic retry. Native
CDP tracing is used in BOTH controls; only the observed copy gets an AW wrapper.
"""
import argparse
from bisect import bisect_left, bisect_right
import asyncio
import base64
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import tempfile
import time
import traceback

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
AUTHOR = 'AudioWorkletProcessor::Process (author script execution)'
CATEGORIES = ['disabled-by-default-audio-worklet', 'webaudio', 'audio', 'v8.wasm',
              'disabled-by-default-v8.compile', 'disabled-by-default-v8.gc',
              'blink.user_timing', 'devtools.timeline']
FIXTURE_SHA = '3ed4aff350319e6b89dcc89aa079080b895b6b0a322f21f8231fc9422f051a01'
SECTORS_SHA = 'fdd572362472732be0c82bf4afac7c0d14328d953d17d5f16f057849b9be68b8'
# Same passive Window probes in every arm. No firmware references/parameters are
# exported. Public WebAudio objects stay in this test closure, not on window.
PROBE = r"""
(() => {
 const contexts=[],nodes=[],workers=[],state={connections:0,storageWrites:0,frames:0,requests:0,replies:0,workerKeys:[],diagnostics:null};
 const mark=n=>performance.mark('CG.'+n);
 for(const [name,list] of [['AudioContext',contexts],['AudioWorkletNode',nodes],['Worker',workers]]){
  const Native=window[name];if(!Native)continue;
  window[name]=new Proxy(Native,{construct(target,args){
   const object=Reflect.construct(target,args);list.push(object);
   if(name==='AudioWorkletNode'){
    const connect=object.connect.bind(object);object.connect=(...a)=>{state.connections++;return connect(...a);};
   }
   if(name==='Worker'||name==='AudioWorkletNode'){
    const port=name==='Worker'?object:object.port;
    port.addEventListener('message',({data:m})=>{
     if(name==='Worker'&&m?.type==='prepared'){state.diagnostics=m.diagnostics;mark('prepared');}
     if(name==='AudioWorkletNode'&&m?.type==='started')mark('started-reply');
     if(m?.type==='frame')state.frames++;
     if(m?.type==='editor-response')state.replies++;
    });
    const post=port.postMessage.bind(port);port.postMessage=(m,...rest)=>{
     if(name==='Worker'&&m?.type==='prepare'){state.workerKeys=Object.keys(m).sort();mark('worker-request');}
     if(m?.type==='start')mark('start-sent');
     if(m?.type==='editor-request')state.requests++;
     return post(m,...rest);
    };
   }
   return object;
  }});
 }
 const set=Storage.prototype.setItem;
 Storage.prototype.setItem=function(...a){state.storageWrites++;return set.apply(this,a);};
 document.addEventListener('click',e=>{
  if(e.target.id==='go'&&e.target.textContent.includes('Prepare simulator'))mark('prepare-gesture');
  if(e.target.id==='go'&&e.target.textContent==='Start audio')mark('start-gesture');
 },true);
 let ticket=0;
 window.__cgTiming={sample:()=>({...state,contexts:contexts.length,nodes:nodes.length,workers:workers.length,
  rate:contexts[0]?.sampleRate,audioTime:contexts[0]?.currentTime,audioState:contexts[0]?.state}),
  metrics:()=>new Promise((resolve,reject)=>{
   const id=++ticket,port=nodes[0].port,timer=setTimeout(()=>{port.removeEventListener('message',receive);reject(Error('metrics timeout'));},2000);
   function receive({data:m}){if(m?.type==='cg-timing-metrics'&&m.ticket===id){clearTimeout(timer);port.removeEventListener('message',receive);resolve(m.stats);}}
   port.addEventListener('message',receive);port.postMessage({type:'cg-timing-read',ticket:id});
  })};
})();
"""


def sha(data):
    return hashlib.sha256(data).hexdigest()


def validate_inputs(args):
    wasm = args.wasm.read_bytes()
    assert sha(wasm) == args.sha256, 'WASM SHA-256 mismatch'
    assert sha(args.fixture_nor.read_bytes()) == FIXTURE_SHA, 'Validated NOR fixture changed'
    raw = args.fixture_sectors.read_text()
    assert sha(raw.encode()) == SECTORS_SHA, 'Validated sector fixture changed'
    # Actual production decoder and geometry; no second storage-format parser.
    code = """import fs from 'node:fs';import crypto from 'node:crypto';import {pathToFileURL} from 'node:url';
const [module,file]=process.argv.slice(1),{readSavedSectors}=await import(pathToFileURL(module));
const decoded=readSavedSectors({getItem:()=>fs.readFileSync(file,'utf8')},'fixture');
const nor=new Uint8Array(1048576).fill(255);for(const [off,bytes] of Object.entries(decoded.sectors))nor.set(bytes,+off);
console.log(JSON.stringify({count:Object.keys(decoded.sectors).length,writable:decoded.writable,sha256:crypto.createHash('sha256').update(nor).digest('hex')}));"""
    checked = json.loads(subprocess.check_output(['node', '--input-type=module', '-e', code,
                         str(HERE / 'startup.js'), str(args.fixture_sectors)], text=True))
    assert checked == {'count': 26, 'writable': True, 'sha256': FIXTURE_SHA}, checked
    return wasm, raw, checked


def instrument_stages(source):
    """Patch only a served test copy; fail on drift, never rewrite app sources.

    Console TimeStamp uses native trace/ThreadTicks clocks, not Date.now. At most
    8192 callbacks emit up to eight static-label instants each; no callback-level
    messages, string construction, stage objects or firmware hooks are added.
    """
    def once(old, new):
        nonlocal source
        if source.count(old) != 1:
            raise ValueError('Stage anchor drift: ' + old.strip())
        source = source.replace(old, new, 1)
    def mark(name, indent='      '):
        return indent + "if (cgStage) console.timeStamp('CG.stage." + name + "');\n"
    once('    process(inputs, outputs) {\n',
         '    process(inputs, outputs) {\n' +
         "      const cgStage = !!this.cgTiming && this.cgTiming.callbacks < 8192 && typeof console.timeStamp === 'function';\n" + mark('entry'))
    once('      if (!ex || !this.renderEnabled || this.loadFailed) return true;',
         '      if (!ex || !this.renderEnabled || this.loadFailed) {\n' + mark('exit', '        ') + '        return true;\n      }')
    once('      ex.web_render(n);\n', mark('render.begin') + '      ex.web_render(n);\n' + mark('render.end'))
    once('      out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));',
         mark('buffers.begin') + '      out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));')
    once('      if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));\n',
         '      if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));\n' + mark('buffers.end'))
    once('        this.publish();\n', mark('publish.begin', '        ') + '        this.publish();\n' + mark('publish.end', '        '))
    once('      return true;\n    }\n  }\n  registerProcessor',
         mark('exit') + '      return true;\n    }\n  }\n  registerProcessor')
    return source


def cpu_quota():
    """Read-only shared-cgroup counters; not a per-AudioWorklet scheduler trace."""
    try:
        root = Path('/sys/fs/cgroup')
        stat = {k: int(v) for k, v in (line.split() for line in (root/'cpu.stat').read_text().splitlines())}
        return {'host_monotonic_us': time.monotonic_ns() / 1000,
                'cpu_max': (root/'cpu.max').read_text().strip(), 'stat': stat}
    except (OSError, ValueError) as error:
        return {'available': False, 'error': str(error)}


async def retrieve(cdp, complete, file):
    await cdp.send('Tracing.end')
    deadline = time.monotonic() + 20
    while not complete and time.monotonic() < deadline:
        await asyncio.sleep(.05)
    assert complete, 'Missing tracingComplete'
    total = 0
    with file.open('wb') as out:
        while True:
            block = await cdp.send('IO.read', {'handle': complete['stream'], 'size': 1048576})
            data = base64.b64decode(block['data']) if block.get('base64Encoded') else block['data'].encode()
            total += len(data)
            assert total <= 128 * 1024**2, 'Trace retrieval exceeds bound'
            out.write(data)
            if block.get('eof'):
                break
    await cdp.send('IO.close', {'handle': complete['stream']})
    assert not complete.get('dataLossOccurred'), complete
    return total


async def capture(pw, args, root, origin, restored, observed, repeat):
    label = f"{'restored' if restored else 'clean'}-{'on' if observed else 'off'}-{repeat}"
    folder = root / label
    folder.mkdir()
    d = {'run': label, 'restored': restored, 'observer': observed, 'repeat': repeat,
         'console': [], 'exceptions': [], 'audio_contexts': [], 'samples': [],
         'trace_complete': {}, 'buffer_usage': [], 'error': None, 'stages': args.stages}
    if args.stages:
        d['quota_begin'] = cpu_quota()
    browser = page = cdp = None
    tracing = False
    print('RUN BEGIN', label, flush=True)
    try:
        assert shutil.disk_usage(root).free > 15 * 1024**3, 'Less than 15 GiB free'
        browser = await pw.chromium.launch_persistent_context(
            str(folder / 'profile'), executable_path=str(args.chromium), headless=True,
            chromium_sandbox=True, ignore_default_args=True, timeout=20000,
            args=['--headless', '--remote-debugging-pipe', '--no-first-run', '--no-default-browser-check',
                  '--host-resolver-rules=MAP * ~NOTFOUND, EXCLUDE 127.0.0.1'])
        d['browser_version'] = browser.browser.version
        async def local(route):
            if route.request.url.startswith(origin + '/') or route.request.url.startswith(('about:', 'data:', 'blob:')):
                await route.continue_()
            else:
                d.setdefault('external_blocked', []).append(route.request.url)
                await route.abort()
        await browser.route('**/*', local)
        initial = PROBE
        if restored:
            initial += "\nif(location.pathname.endsWith('/web/emu/index.html')){localStorage.setItem('felucca-emu-flash'," + json.dumps(args.saved_raw) + ");}"
            initial += "\n// Exclude only the deliberate fixture provisioning write.\n"
        # Provision before the app loads, then the campaign's separate count is
        # sampled relative to this initial value (never reset during rendering).
        await browser.add_init_script(initial)
        page = await browser.new_page()
        page.set_default_timeout(20000)
        page.on('console', lambda m: d['console'].append({'type': m.type, 'text': m.text}) if m.type in ['error', 'warning'] else None)
        page.on('pageerror', lambda e: d['exceptions'].append(str(e)))
        pc = await browser.new_cdp_session(page)
        await pc.send('WebAudio.enable')
        pc.on('WebAudio.contextCreated', lambda e: d['audio_contexts'].append(e))
        pc.on('WebAudio.contextChanged', lambda e: d['audio_contexts'].append(e))
        arm = 'on' if observed else 'off'
        await page.goto(origin + f'/{arm}/web/emu/index.html?worker_prepare=1', wait_until='load')
        d['initial'] = await page.evaluate('__cgTiming.sample()')
        assert d['initial']['contexts'] == 0
        cdp = await browser.browser.new_browser_cdp_session()
        d['advertised_categories'] = (await cdp.send('Tracing.getCategories'))['categories']
        cdp.on('Tracing.tracingComplete', lambda e: d['trace_complete'].update(e))
        cdp.on('Tracing.bufferUsage', lambda e: d['buffer_usage'].append(e))
        d['trace_config'] = {'recordMode': 'recordUntilFull', 'traceBufferSizeInKb': 32768,
            'enableSampling': False, 'enableSystrace': False, 'includedCategories': CATEGORIES, 'excludedCategories': ['*']}
        await cdp.send('Tracing.start', {'traceConfig': d['trace_config'], 'transferMode': 'ReturnAsStream',
            'streamFormat': 'json', 'streamCompression': 'none', 'bufferUsageReportingInterval': 1000})
        tracing = True
        if args.stages:
            before = time.monotonic_ns() / 1000
            await page.evaluate("performance.mark('CG.host-sync')")
            d['host_clock_bounds_us'] = [before, time.monotonic_ns() / 1000]
        await page.locator('#go').click()
        await page.wait_for_function("document.querySelector('#go').textContent==='Start audio'")
        d['prepared'] = await page.evaluate('__cgTiming.sample()')
        assert d['prepared']['contexts'] == 0 and d['prepared']['workers'] == 1
        assert d['prepared']['workerKeys'] == ['attempt', 'bytes', 'protocol', 'type', 'version']
        assert d['prepared']['diagnostics']['calls'] == 207 and d['prepared']['diagnostics']['writes'] == 0
        await page.locator('#go').click()
        await page.wait_for_function("document.querySelector('#start').hidden && !document.querySelector('#open-editor').disabled")
        async def mark(name):
            await page.evaluate('(n)=>performance.mark(n)', 'CG.' + name)
        async def sample():
            x = await page.evaluate('__cgTiming.sample()')
            if args.stages:
                x['quota'] = cpu_quota()
            d['samples'].append(x)
            assert x['audioState'] == 'running' and x['contexts'] == 1 and x['nodes'] == 1 and x['connections'] == 1, x
            assert not d['exceptions'], d['exceptions']
            assert not any(e.get('percentFull', e.get('value', 0)) > .85 for e in d['buffer_usage']), 'Buffer approaching capacity'
            return x
        async def interval(name, seconds):
            start = await sample()
            await mark(name + '.begin')
            until = time.monotonic() + seconds
            while time.monotonic() < until:
                await asyncio.sleep(.1)
                await sample()
            await mark(name + '.end')
            d.setdefault('interval_audio', {})[name] = {'start': start, 'end': await sample()}
        await page.wait_for_timeout(1200)
        await interval('idle', 3)
        if restored:
            async with page.expect_popup() as popup:
                await page.locator('#open-editor').click()
            editor = await popup.value
            editor.on('pageerror', lambda e: d['exceptions'].append(str(e)))
            await editor.wait_for_function("document.querySelector('#status').textContent.includes('Simulator read-only')")
            d['editor'] = await editor.evaluate("({engine:Number(document.querySelector('#engine').value),source:Number(document.querySelector('select[title=SRC]').value),disabled:document.querySelector('select[title=SRC]').disabled})")
            assert d['editor'] == {'engine': 8, 'source': 12, 'disabled': True}, d['editor']
            await editor.close()
            await page.bring_to_front()
            await mark('grain.begin')
            for key in ['z', 'c', 'b']:
                await page.keyboard.down(key)
            start = await sample()
            until = time.monotonic() + 5
            while time.monotonic() < until:
                await asyncio.sleep(.1)
                await sample()
            for key in ['z', 'c', 'b']:
                await page.keyboard.up(key)
            await mark('grain.end')
            d['interval_audio']['grain'] = {'start': start, 'end': await sample()}
            await interval('release', 2)
        await mark('capture-end')
        if observed:
            d['metrics'] = await page.evaluate('__cgTiming.metrics()')
            assert d['metrics']['nonFinite'] == 0
            if restored:
                assert d['metrics']['peak'] > .00001
        d['final'] = await sample()
        assert d['final']['storageWrites'] == d['initial']['storageWrites'], 'Unexpected persistence write'
        stored = await page.evaluate("localStorage.getItem('felucca-emu-flash')")
        assert stored == (args.saved_raw if restored else None), 'Saved data changed'
        d['saved_data_sha256'] = sha(stored.encode()) if stored is not None else None
        # Normal page exit, after the measured interval, retains stream-close
        # diagnostics without altering the startup or adding a stop API.
        await mark('shutdown')
        await page.close()
        page = None
        await asyncio.sleep(.2)
        d['trace_bytes'] = await retrieve(cdp, d['trace_complete'], folder / 'trace.json')
        tracing = False
    except Exception:
        d['error'] = traceback.format_exc()
        raise
    finally:
        if tracing:
            try:
                d['trace_bytes'] = await retrieve(cdp, d['trace_complete'], folder / 'trace.json')
            except Exception:
                d['trace_error'] = traceback.format_exc()
        if args.stages:
            d['quota_end'] = cpu_quota()
        if browser:
            await browser.close()
        if (folder / 'profile').exists():
            shutil.rmtree(folder / 'profile')
        (folder / 'diagnostics.json').write_text(json.dumps(d, indent=2))
        print('RUN END', label, 'FAILED' if d['error'] else 'CAPTURED', flush=True)
    return folder


def percentile(values, p):
    x = sorted(values)
    at = (len(x) - 1) * p / 100
    lo = int(at)
    return x[lo] + (x[min(lo + 1, len(x) - 1)] - x[lo]) * (at - lo)


def distribution(values, budget):
    if not values:
        return {'count': 0}
    return {'count': len(values), 'mean_us': statistics.mean(values),
        **{f'p{p}_us': percentile(values, p) for p in [50, 95, 99]}, 'max_us': max(values),
        'over_budget': sum(v > budget for v in values)}


def analyze(folder):
    d = json.loads((folder / 'diagnostics.json').read_text())
    assert not d['error'], d['error']
    trace = json.loads((folder / 'trace.json').read_text())
    events = trace['traceEvents']
    def mark(n):
        x = [e for e in events if e.get('name') == 'CG.' + n and e.get('ph') == 'I']
        assert len(x) == 1, (n, len(x))
        return x[0]['ts']
    authors = sorted([e for e in events if e.get('name') == AUTHOR and e.get('ph') == 'X'], key=lambda e: e['ts'])
    assert authors and len({(e['pid'], e['tid']) for e in authors}) == 1, 'No unique native callback stream'
    # Verify the chosen native envelope actually contains the JS process call
    # and is itself enclosed by the wider native processor span.
    pair = (authors[0]['pid'], authors[0]['tid'])
    def enclosed_count(candidates, inner):
        candidates = sorted(candidates, key=lambda x: x['ts'])
        at = count = 0
        for event in inner:
            while at < len(candidates) and candidates[at]['ts'] + candidates[at]['dur'] < event['ts'] + event['dur']:
                at += 1
            if at < len(candidates) and candidates[at]['ts'] <= event['ts']:
                count += 1
        return count
    outer_spans = [e for e in events if e.get('name') == 'AudioWorkletProcessor::Process' and e.get('ph') == 'X' and (e['pid'],e['tid']) == pair]
    process_calls = sorted([e for e in events if e.get('name') == 'FunctionCall' and e.get('ph') == 'X' and
        (e['pid'],e['tid']) == pair and e.get('args',{}).get('data',{}).get('functionName') == 'process'], key=lambda e:e['ts'])
    assert enclosed_count(outer_spans, authors) == len(authors), 'Incomplete native containment'
    assert len(process_calls) == len(authors) and enclosed_count(authors, process_calls) == len(authors), 'Incomplete JS process attribution'
    contexts = [e['context'] for e in d['audio_contexts'] if 'context' in e]
    assert contexts and {c['sampleRate'] for c in contexts} == {44100}
    assert {c['renderQuantumSize'] for c in contexts} == {128}
    budget = 128 / 44100 * 1e6
    bounds = {'all': (authors[0]['ts'], mark('capture-end')),
        'startup': (authors[0]['ts'], mark('idle.begin')), 'idle': (mark('idle.begin'), mark('idle.end'))}
    if d['restored']:
        bounds.update(editor=(mark('idle.end'), mark('grain.begin')),
            grain=(mark('grain.begin'), mark('grain.end')), release=(mark('release.begin'), mark('release.end')))
    phases = {}
    for name, (a, b) in bounds.items():
        spans = [e for e in authors if a <= e['ts'] and e['ts'] + e['dur'] <= b]
        outer = [e for e in events if e.get('name') == 'AudioWorkletProcessor::Process' and e.get('ph') == 'X' and a <= e['ts'] and e['ts'] + e['dur'] <= b]
        phases[name] = {**distribution([e['dur'] for e in spans], budget), 'seconds': (b-a)/1e6,
            'native_wrapper': distribution([e['dur'] for e in outer], budget),
            'thread_cpu': distribution([e['tdur'] for e in spans if 'tdur' in e], budget), 'durations_us': [e['dur'] for e in spans]}
        if name in d.get('interval_audio', {}):
            aclock = d['interval_audio'][name]
            phases[name]['audio_clock_progress_s'] = aclock['end']['audioTime'] - aclock['start']['audioTime']
    signals = [e for e in events if 'OnError' in e.get('name', '') or 'Glitch!' in e.get('name', '') or 'Read timed out' in e.get('name', '')]
    for e in signals:
        e['relative_to_start_gesture_ms'] = (e['ts'] - mark('start-gesture')) / 1000
        e['phase'] = 'shutdown' if e['ts'] > mark('capture-end') else 'startup' if e['ts'] < mark('idle.begin') else 'measurement'
    lazy = [e for e in events if e.get('name') == 'wasm.CompileLazy' and e.get('ph') == 'X']
    gc = [e for e in events if e.get('ph') == 'X' and ('GC' in e.get('name', '') or 'Garbage' in e.get('name', ''))]
    overruns = []
    for e in authors:
        if e['dur'] <= budget or e['ts'] + e['dur'] > mark('capture-end'):
            continue
        overlap = lambda x: (x.get('pid'), x.get('tid')) == (e['pid'], e['tid']) and x['ts'] < e['ts'] + e['dur'] and x['ts'] + x.get('dur', 0) > e['ts']
        body = [x for x in events if x.get('name') == 'FunctionCall' and x.get('ph') == 'X' and overlap(x)]
        overruns.append({**e, 'phase': next((k for k, (a,b) in bounds.items() if k != 'all' and a <= e['ts'] < b), 'gap'),
            'lazy_overlaps': [x for x in lazy if overlap(x)], 'gc_overlaps': [x for x in gc if overlap(x)],
            'js_process_calls': body})
    first_completion = None
    if d['observer']:
        stamps = [e for e in events if e.get('name') == 'TimeStamp' and e.get('args', {}).get('data', {}).get('message') == 'CG.first-active']
        assert len(stamps) == 1
        stamp = stamps[0]
        first = [e for e in authors if e['pid'] == stamp['pid'] and e['tid'] == stamp['tid'] and e['ts'] <= stamp['ts'] <= e['ts'] + e['dur']]
        assert len(first) == 1
        first_completion = (first[0]['ts'] + first[0]['dur'] - mark('start-gesture')) / 1000
    closes = [e for e in events if 'missed' in json.dumps(e.get('args', {})).lower() or 'CloseStream' in e.get('name', '')]
    result = {'run': d['run'], 'restored': d['restored'], 'observer': d['observer'], 'repeat': d['repeat'],
        'budget_us': budget, 'quantum': 128, 'rate': 44100, 'callback_buffer_size': contexts[-1].get('callbackBufferSize'),
        'prepare_ms': (mark('prepared') - mark('prepare-gesture')) / 1000,
        'worker_request_ms': (mark('prepared') - mark('worker-request')) / 1000,
        'first_active_completed_ms': first_completion,
        'first_active_note': 'Measured by native first-active marker' if d['observer'] else 'UNVERIFIED: no first-active marker in production control',
        'started_reply_ms': (mark('started-reply') - mark('start-gesture')) / 1000,
        'phases': phases, 'overruns': overruns, 'pipeline_signals': signals, 'close_statistics': closes,
        'clock_domain': trace.get('metadata', {}).get('clock-domain'), 'trace_complete': d['trace_complete'],
        'trace_sha256': sha((folder/'trace.json').read_bytes()), 'context_errors': d['exceptions'],
        'console': d['console'], 'max_buffer_percent': 100 * max((e.get('percentFull', e.get('value',0)) for e in d['buffer_usage']), default=0),
        'metrics': d.get('metrics'), 'editor': d.get('editor'),
        'native_author_count': len(authors), 'js_process_count': len(process_calls), 'native_containment_verified': True,
        'stream_parameters': [e for e in events if e.get('name') == 'OutputStream' and 'params' in e.get('args',{})],
        'controller_errors': [e for e in events if e.get('name') == 'OnControllerError'],
        'broker_disconnects': [e for e in events if e.get('name') == 'AudioOutputStreamBroker' and 'disconnect reason' in e.get('args',{})]}
    if d.get('stages'):
        result['stage_attribution'] = stage_attribution(events, authors, bounds, d, budget)
    (folder / 'analysis.json').write_text(json.dumps(result, indent=2))
    return result


def stage_attribution(events, authors, bounds, diagnostics, budget):
    """Native TimeStamp wall/ThreadTicks clocks; no estimated per-stage CPU."""
    pair = (authors[0]['pid'], authors[0]['tid'])
    markers = sorted([e for e in events if e.get('name') == 'TimeStamp' and
        e.get('args', {}).get('data', {}).get('message', '').startswith('CG.stage.') and
        (e.get('pid'), e.get('tid')) == pair], key=lambda e: e['ts'])
    times = [e['ts'] for e in markers]
    records = []
    for callback in authors:
        found = markers[bisect_left(times, callback['ts']):bisect_right(times, callback['ts'] + callback['dur'])]
        if not found:
            continue  # Explicit fixed cap; coverage is reported below.
        by_name = {e['args']['data']['message'][9:]: e for e in found}
        assert len(by_name) == len(found), 'Duplicate stage marker within callback'
        assert 'entry' in by_name and 'exit' in by_name, 'Incomplete callback stage envelope'
        record = {'ts': callback['ts'], 'callback_us': callback['dur'], 'callback_cpu_us': callback.get('tdur')}
        for label, first, last in [('base', 'entry', 'exit'), ('render', 'render.begin', 'render.end'),
                                    ('buffers', 'buffers.begin', 'buffers.end'), ('publish', 'publish.begin', 'publish.end')]:
            assert (first in by_name) == (last in by_name), 'Incomplete ' + label
            if first not in by_name:
                continue
            begin, end = by_name[first], by_name[last]
            assert end['ts'] >= begin['ts']
            record[label + '_us'] = end['ts'] - begin['ts']
            if 'tts' in begin and 'tts' in end:
                record[label + '_cpu_us'] = end['tts'] - begin['tts']
        record['outside_base_us'] = callback['dur'] - record['base_us']
        record['other_base_us'] = record['base_us'] - sum(record.get(k + '_us', 0) for k in ['render','buffers','publish'])
        if 'base_cpu_us' in record and 'tdur' in callback:
            record['outside_base_cpu_us'] = callback['tdur'] - record['base_cpu_us']
            record['other_base_cpu_us'] = record['base_cpu_us'] - sum(record.get(k + '_cpu_us',0) for k in ['render','buffers','publish'])
        records.append(record)
    assert records, 'No stage markers emitted; attribution unavailable'
    phases = {}
    labels = ['base','render','buffers','publish','outside_base','other_base']
    for name,(a,b) in bounds.items():
        selected = [r for r in records if a <= r['ts'] and r['ts'] + r['callback_us'] <= b]
        phases[name] = {'callbacks': len(selected), 'stages': {}}
        for label in labels:
            wall = [r[label+'_us'] for r in selected if label+'_us' in r]
            cpu = [r[label+'_cpu_us'] for r in selected if label+'_cpu_us' in r]
            phases[name]['stages'][label] = {'elapsed': distribution(wall,budget), 'thread_cpu': distribution(cpu,budget),
                # Keep raw ThreadTicks evidence, but do not silently trust clocks
                # that imply CPU time greater than elapsed time (5 us tolerance).
                'cpu_clock_quality': {'negative': sum(r[label+'_cpu_us'] < 0 for r in selected if label+'_cpu_us' in r),
                    'exceeds_elapsed_by_over_5us': sum(r[label+'_cpu_us'] > r[label+'_us'] + 5 for r in selected if label+'_cpu_us' in r)}}
    sync = [e for e in events if e.get('name') == 'CG.host-sync' and e.get('ph') == 'I']
    clocks = diagnostics.get('host_clock_bounds_us')
    aligned = bool(len(sync)==1 and clocks and clocks[0] <= sync[0]['ts'] <= clocks[1])
    before, after = diagnostics.get('quota_begin',{}), diagnostics.get('quota_end',{})
    quota = {'clock_aligned': aligned, 'clock_validation_bounds_us': clocks, 'native_clock_sync': sync,
             'before': before, 'after': after, 'samples': [x.get('quota') for x in diagnostics['samples']]}
    if 'stat' in before and 'stat' in after:
        quota['deltas'] = {k:after['stat'][k]-before['stat'].get(k,0) for k in after['stat']}
    return {'cap':8192,'covered_callbacks':len(records),'native_callbacks':len(authors),
            'phases':phases,'overruns':[r for r in records if r['callback_us']>budget and r['ts']<bounds['all'][1]],'quota':quota}


def summarize(root):
    results = [analyze(p.parent) for p in sorted(root.glob('*/diagnostics.json'))]
    groups = {}
    for result in results:
        key = ('restored' if result['restored'] else 'clean') + ('-on' if result['observer'] else '-off')
        groups.setdefault(key, []).append(result)
    aggregate = {}
    for key, runs in groups.items():
        aggregate[key] = {'runs': len(runs), 'prepare_ms': [r['prepare_ms'] for r in runs],
            'first_active_completed_ms': [r['first_active_completed_ms'] for r in runs], 'phases': {}}
        for phase in runs[0]['phases']:
            values = [v for r in runs for v in r['phases'][phase]['durations_us']]
            aggregate[key]['phases'][phase] = {**distribution(values, runs[0]['budget_us']),
                'runs_with_overrun': sum(r['phases'][phase]['over_budget'] > 0 for r in runs),
                'per_run_mean_us': [r['phases'][phase]['mean_us'] for r in runs],
                'per_run_max_us': [r['phases'][phase]['max_us'] for r in runs]}
    (root / 'summary.json').write_text(json.dumps({'runs': results, 'aggregate': aggregate}, indent=2))
    print(json.dumps(aggregate, indent=2), flush=True)


async def campaign(args, root):
    from playwright.async_api import async_playwright
    spec = importlib.util.spec_from_file_location('startup_smoke', HERE/'browser_startup_test.py')
    smoke = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(smoke)
    wasm, args.saved_raw, fixture = validate_inputs(args)
    original, hashes = smoke.snapshot(REPO, root, wasm)
    snapshots = root / 'sites'
    for arm in ['off', 'on']:
        shutil.copytree(original, snapshots / arm)
    target = snapshots/'on/web/emu/worklet.js'
    prefix = (HERE/'timing_observer.js').read_bytes()
    source = target.read_text()
    if args.stages:
        source = instrument_stages(source)
    target.write_bytes(prefix + b'\n' + source.encode())
    (root/'inputs.json').write_text(json.dumps({'wasm_sha256': sha(wasm), 'fixture': fixture,
        'source_hashes': hashes, 'observer_sha256': sha(prefix), 'harness_sha256': sha(Path(__file__).read_bytes()),
        'fixture_inputs': {'nor': str(args.fixture_nor.resolve()), 'sectors': str(args.fixture_sectors.resolve()), 'sectors_sha256': SECTORS_SHA}, 'repeats': args.repeats, 'stages': args.stages, 'stage_callback_cap': 8192 if args.stages else None,
        'method': ('Bounded observed clean/restored stage attribution' if args.stages else '2x2 NOR/observer matched controls') + '; fresh browser/profile each run'}, indent=2))
    server, thread = smoke.serve(snapshots)
    origin = f'http://127.0.0.1:{server.server_port}'
    try:
        async with async_playwright() as pw:
            for n in range(args.repeats):
                # Counterbalance order, not independent callback-level randomization.
                order = [(False,False),(False,True),(True,False),(True,True)]
                if n % 2:
                    order = [(False,True),(False,False),(True,True),(True,False)]
                if n % 3 == 2:
                    order = order[2:] + order[:2]
                if args.stages:
                    order = [(False, True), (True, True)]
                    if n % 2:
                        order.reverse()
                for restored, observed in order:
                    await capture(pw, args, root, origin, restored, observed, n+1)
    finally:
        server.shutdown(); server.server_close(); thread.join(3)
        shutil.rmtree(original)
        shutil.rmtree(snapshots)
    summarize(root)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wasm', type=Path)
    parser.add_argument('--sha256')
    parser.add_argument('--fixture-nor', type=Path)
    parser.add_argument('--fixture-sectors', type=Path)
    parser.add_argument('--chromium', type=Path, default=Path('/usr/bin/chromium'))
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--stages', action='store_true', help='Bounded served-copy stage markers and cgroup snapshots; observed clean/restored arms only')
    parser.add_argument('--analyze', type=Path, help='Reanalyze an existing campaign; no browser launch')
    args = parser.parse_args()
    if args.analyze:
        summarize(args.analyze); return 0
    if not all([args.wasm, args.sha256, args.fixture_nor, args.fixture_sectors]):
        parser.error('--wasm, --sha256, --fixture-nor and --fixture-sectors are required')
    if not 2 <= args.repeats <= 10:
        parser.error('--repeats must be between 2 and 10')
    root = Path(tempfile.mkdtemp(prefix='coffee-grains-matched-timing-'))
    print('RESULTS', root, flush=True)
    # Driver stderr includes unfiltered browser/audio backend messages, not CDP
    # sampling. Redirect stdout+stderr to a log to preserve PID/error evidence.
    os.environ['DEBUG'] = 'pw:browser'
    try:
        asyncio.run(campaign(args, root))
    except Exception:
        (root/'failure.txt').write_text(traceback.format_exc())
        raise
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
