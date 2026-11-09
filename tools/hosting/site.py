#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline immutable Pages assembly. No Git/network writes. Unit tests use fake WASM;
CI additionally requires real native/WASM/browser suites before publication."""
import argparse
import datetime as dt
import hashlib
import html
import json
from pathlib import Path
import re
import shutil

SCHEMA = 1
PRODUCT = 'COFFEE GRAINS'
RELEASE = re.compile(r'^[0-9a-f]{40}-[0-9]+-[0-9]+$')
SHA = re.compile(r'^[0-9a-f]{40}$')
RUNTIME = ['index.html', 'worklet.js', 'startup.js', 'prepare_worker.js',
           'editor_transport.js', 'felucca.wasm', 'fonts/DotGothic16-subset.woff', 'fonts/OFL.txt']


def digest(data):
    return hashlib.sha256(data).hexdigest()


def dump(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, sort_keys=True)+'\n')


def package(source, build, out, revision, release, timestamp, tooling_revision="local-uncommitted"):
    if not SHA.fullmatch(revision) or not RELEASE.fullmatch(release) or not release.startswith(revision+'-'):
        raise ValueError('Full source SHA and unique SHA-run-attempt release ID required')
    if tooling_revision != 'local-uncommitted' and not SHA.fullmatch(tooling_revision):
        raise ValueError('Invalid tooling revision')
    dt.datetime.strptime(timestamp, '%Y-%m-%dT%H:%M:%SZ')
    dest = out/'releases'/release
    if dest.exists():
        raise ValueError('Immutable release already exists')
    dest.mkdir(parents=True)
    info = f'{PRODUCT} · {revision} · built {timestamp} UTC · {release}'
    for name in RUNTIME:
        target = dest/'emu'/name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(build/name, target)
    page = dest/'emu/index.html'
    text = page.read_text()
    if '@VERSION@' in text or '@PREVIEW@' in text or 'Local development build' not in text:
        raise ValueError('Build HTML is unstamped or lacks branding/build marker')
    # Namespacing is packaging-only: stable/dev saves cannot poison one another.
    # Do not migrate the old Felucca key automatically; retained builds can still read it.
    old = 'const STORE = "felucca-emu-flash", KNOBS_ON = "felucca-emu-knobs", KNOB_MODE = "felucca-emu-knobmode";'
    if text.count(old) != 1:
        raise ValueError('Review persistence namespace injection for this source revision')
    text = text.replace(old, '''const deployChannel = new URLSearchParams(location.search).get("channel") === "dev" ? "dev" : "stable";
const STORE = `coffee-grains-${deployChannel}-flash`, KNOBS_ON = `coffee-grains-${deployChannel}-knobs`, KNOB_MODE = `coffee-grains-${deployChannel}-knobmode`;
document.getElementById("build-info").prepend(deployChannel === "dev" ? "DEV — experimental · " : "STABLE · ");''')
    text = text.replace('url.searchParams.set("simulator", nonce);', 'url.searchParams.set("simulator", nonce); url.searchParams.set("channel", deployChannel);')
    text = text.replace('Local development build', html.escape(info))
    page.write_text(text)
    editor = (source/'web/editor.html').read_text().replace('Local development build', html.escape(info))
    editor = editor.replace('</body>', '<script type="module">document.getElementById("build-info").prepend(new URLSearchParams(location.search).get("channel") === "dev" ? "DEV — experimental · " : "STABLE · ");</script></body>')
    (dest/'editor').mkdir()
    (dest/'editor/index.html').write_text(editor)
    (dest/'editor.html').write_text(editor)
    for target in [dest/'editor', dest]:
        for name in ['fm1backup.js','fukiai.ttf','FUKIAI-LICENSE.txt']:
            shutil.copyfile(source/'web'/name, target/name)
        # Both editor entry paths import ./emu/editor_transport.js.
        if target.name == 'editor':
            (target/'emu').mkdir()
            shutil.copyfile(source/'web/emu/editor_transport.js', target/'emu/editor_transport.js')
    shutil.copyfile(source/'web/EDITOR_PROTOCOL.md', dest/'EDITOR_PROTOCOL.md')
    shutil.copyfile(source/'LICENSE', dest/'LICENSE')
    shutil.copytree(source/'LICENSES', dest/'LICENSES')
    for name in ['ATTRIBUTION.txt', 'CREDITS.txt']:
        shutil.copyfile(source/'assets/samples-cc0'/name, dest/name)
    (dest/'README.html').write_text('''<!doctype html><html lang="en"><meta charset="utf-8">
<title>COFFEE GRAINS — Browser guide</title><h1>COFFEE GRAINS</h1><p>Granular Synthesizer</p>
<p>Click Switch on, then use the panel or computer/MIDI keyboard. GRAIN, SAMPLE and SLICE run real firmware.</p>
<p>The experimental Worker path is default-off; it requires ?worker_prepare=1 and two deliberate gestures.</p>
<p>The separate-window simulator editor is read-only. Browser-to-simulator sample uploads, erase and backup restore
are unavailable; hardware Web MIDI features do not imply a writable simulator bridge.</p>
<p>Stable and Dev have separate browser-local saves. They are not cloud backups. Back up valuable projects before
changing versions, clearing site data or using Dev. No automatic import from the older Felucca save key occurs.</p>
<p>Five factory IDs currently contain three distinct materials, with historical aliases. FM-1 target validation,
startup deadline guarantees and physical hardware validation remain outstanding.</p>
<p>Felucca by Leo Kuroshita / Hügelton Instruments; browser build after X0X. Existing licenses and credits retained.</p>
<a href="LICENSE">GPL-3.0</a> · <a href="CREDITS.txt">Sample credits</a></html>''')
    dump(dest/'provenance.json', dict(source=revision, tooling_source=tooling_revision, compiler_image='emscripten/emsdk@sha256:af45409f3199d88db4b1b03af0098532c8fb33a375ac257463eeb0a622870d06', factory_audio='Embedded IMA ADPCM in felucca.wasm; five IDs, three distinct materials', generator_inputs={str(p.relative_to(source)):digest(p.read_bytes()) for p in sorted((source/'assets/samples-cc0').rglob('*')) if p.is_file()}))
    files = {str(p.relative_to(dest)): digest(p.read_bytes()) for p in sorted(dest.rglob('*')) if p.is_file()}
    dump(dest/'manifest.json', dict(schema=SCHEMA, product=PRODUCT, source=revision, release=release,
                                   built_at=timestamp, files=files))
    verify(dest)
    return dest


def verify(dest):
    if not (dest/'manifest.json').is_file():
        raise ValueError('Release manifest missing')
    if any(p.is_symlink() for p in dest.rglob('*')):
        raise ValueError('Symlink in immutable release')
    m = json.loads((dest/'manifest.json').read_text())
    if m.get('schema') != SCHEMA or not RELEASE.fullmatch(m.get('release', '')) or not SHA.fullmatch(m.get('source', '')) or dest.name != m['release'] or not m['release'].startswith(m['source']+'-'):
        raise ValueError('Unsupported release manifest')
    actual = {str(p.relative_to(dest)) for p in dest.rglob('*') if p.is_file() and p != dest/'manifest.json'}
    if set(m['files']) != actual:
        raise ValueError('Missing or extra release files')
    for name, expected in m['files'].items():
        p = Path(name)
        if p.is_absolute() or '..' in p.parts or not (dest/p).is_file() or (dest/p).is_symlink() or digest((dest/p).read_bytes()) != expected:
            raise ValueError('Release checksum/path mismatch: '+name)
    for name in RUNTIME:
        if 'emu/'+name not in actual:
            raise ValueError('Missing runtime: '+name)
    if not (dest/'emu/felucca.wasm').read_bytes().startswith(b'\x00asm\x01\x00\x00\x00'):
        raise ValueError('Not WASM v1')
    return m


def channel(site, name, release, promotion=False):
    if name not in ('stable', 'dev') or not RELEASE.fullmatch(release):
        raise ValueError('Invalid channel/release')
    m = verify(site/'releases'/release)
    # STABLE initial promotion must select a build actually published to DEV.
    if promotion:
        approval = site/'approvals'/f'{release}.json'
        if not approval.is_file() or json.loads(approval.read_text()) != dict(release=release, source=m['source']):
            raise ValueError('Build has never been published to DEV')
    query = '?channel='+name
    url = '../releases/'+release+'/emu/index.html'+query
    target = site/name
    target.mkdir(exist_ok=True)
    (target/'index.html').write_text('''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="robots" content="noindex"><meta http-equiv="Cache-Control" content="no-store">
<title>COFFEE GRAINS — '''+name.upper()+'''</title><h1>COFFEE GRAINS</h1><p>Granular Synthesizer</p>
<p>Opening '''+name.upper()+''' build. <a id="launch" href="'''+url+'''">Launch</a></p>
<script>const u=new URL(document.getElementById('launch').href);const q=new URLSearchParams(location.search);
if(q.get('worker_prepare')==='1')u.searchParams.set('worker_prepare','1');location.replace(u.href);</script></html>''')
    dump(target/'build.json', dict(schema=SCHEMA, channel=name, release=release, source=m['source'], built_at=m['built_at']))
    if name == 'dev':
        dump(site/'approvals'/f'{release}.json', dict(release=release, source=m['source']))
    (site/'.nojekyll').write_text('')
    (site/'index.html').write_text('''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="description" content="COFFEE GRAINS Granular Synthesizer"><title>COFFEE GRAINS — Granular Synthesizer</title>
<h1>COFFEE GRAINS</h1><p>Granular Synthesizer</p><p><a href="stable/">STABLE</a> · <a href="dev/">DEV — experimental</a></p>
<p>Each channel requires approval. No hardware flashing. See the selected build's Browser guide.</p></html>''')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='command', required=True)
    a = sub.add_parser('package')
    for name in ('source', 'build', 'out'):
        a.add_argument('--'+name, type=Path, required=True)
    for name in ('revision', 'release', 'timestamp'):
        a.add_argument('--'+name, required=True)
    a.add_argument('--tooling-revision', default='local-uncommitted')
    a = sub.add_parser('verify'); a.add_argument('directory', type=Path)
    a = sub.add_parser('channel')
    a.add_argument('--site', type=Path, required=True);a.add_argument('--name', choices=['dev','stable'], required=True)
    a.add_argument('--release', required=True);a.add_argument('--promotion', action='store_true')
    args = vars(p.parse_args()); command=args.pop('command')
    if command == 'package':package(**args)
    elif command == 'verify':verify(args['directory'])
    else:channel(**args)

if __name__ == '__main__':main()
