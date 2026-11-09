#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest
spec = importlib.util.spec_from_file_location('site_builder', Path(__file__).with_name('site.py'))
site = importlib.util.module_from_spec(spec); spec.loader.exec_module(site)

class Assembly(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name); self.src=self.root/'src'; self.build=self.root/'build'; self.out=self.root/'site'
        repo=Path(__file__).resolve().parents[2]
        for name in ['web/editor.html','web/fukiai.ttf','web/FUKIAI-LICENSE.txt','web/fm1backup.js','web/emu/editor_transport.js','web/EDITOR_PROTOCOL.md','LICENSE','assets/samples-cc0/ATTRIBUTION.txt','assets/samples-cc0/CREDITS.txt']:
            p=self.src/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(repo/name,p)
        shutil.copytree(repo/'LICENSES',self.src/'LICENSES')
        for name in site.RUNTIME:
            p=self.build/name;p.parent.mkdir(parents=True,exist_ok=True)
            if name=='index.html':p.write_text((repo/'web/emu/index.html').read_text().replace('@VERSION@','test').replace('@PREVIEW@',' (preview)'))
            elif name=='felucca.wasm':p.write_bytes(b'\x00asm\x01\x00\x00\x00')
            else:p.write_text('fixture')
        self.sha='a'*40; self.release=self.sha+'-1-1'
        self.dest=site.package(self.src,self.build,self.out,self.sha,self.release,'2026-11-09T00:00:00Z')
    def test_complete_manifest(self):self.assertEqual(site.verify(self.dest)['source'],self.sha)
    def test_corruption(self):
        (self.dest/'emu/felucca.wasm').write_bytes(b'broken')
        with self.assertRaises(ValueError):site.verify(self.dest)
    def test_no_missing_files(self):
        (self.dest/'emu/worklet.js').unlink()
        with self.assertRaises(ValueError):site.verify(self.dest)
    def test_no_immutable_overwrite(self):
        with self.assertRaises(ValueError):site.package(self.src,self.build,self.out,self.sha,self.release,'2026-11-09T00:00:00Z')
    def test_promote_requires_dev(self):
        with self.assertRaises(ValueError):site.channel(self.out,'stable',self.release,True)
        site.channel(self.out,'dev',self.release)
        site.channel(self.out,'stable',self.release,True)
        self.assertIn('channel=stable',(self.out/'stable/index.html').read_text())
    def test_dev_failure_preserves_stable(self):
        site.channel(self.out,'dev',self.release);site.channel(self.out,'stable',self.release,True)
        before=(self.out/'stable/index.html').read_bytes()
        with self.assertRaises(ValueError):site.channel(self.out,'dev','b'*40+'-2-1')
        self.assertEqual(before,(self.out/'stable/index.html').read_bytes())
    def test_two_channels_and_rollback(self):
        site.channel(self.out,'dev',self.release);site.channel(self.out,'stable',self.release,True)
        second=self.sha+'-2-1'; site.package(self.src,self.build,self.out,self.sha,second,'2026-11-09T01:00:00Z')
        stable=(self.out/'stable/index.html').read_bytes()
        site.channel(self.out,'dev',second)
        self.assertEqual(stable,(self.out/'stable/index.html').read_bytes())
        site.channel(self.out,'dev',self.release,True)
        self.assertTrue((self.out/'releases'/second).is_dir())
    def test_bad_release(self):
        for release in ['../bad','main',self.release+'/../x']:
            with self.assertRaises(ValueError):site.channel(self.out,'dev',release)
    def test_path_safety(self):
        import json
        p=self.dest/'manifest.json';m=json.loads(p.read_text());m['files']['../escape']='0'*64;p.write_text(json.dumps(m))
        with self.assertRaises(ValueError):site.verify(self.dest)
    def test_persistence_namespaced_without_migration(self):
        text=(self.dest/'emu/index.html').read_text()
        self.assertIn('coffee-grains-${deployChannel}-flash',text)
        self.assertNotIn('const STORE = "felucca-emu-flash"',text)
        self.assertNotIn('getItem("felucca-emu-flash")',text)
    def test_editor_both_routes(self):
        self.assertEqual((self.dest/'editor/index.html').read_bytes(),(self.dest/'editor.html').read_bytes())
        for p in ['editor/fm1backup.js','editor/fukiai.ttf','editor/FUKIAI-LICENSE.txt','fukiai.ttf','fm1backup.js','editor/emu/editor_transport.js','emu/editor_transport.js']:
            self.assertTrue((self.dest/p).is_file())
    def test_brand_and_source_info(self):
        for f in ['emu/index.html','editor.html','editor/index.html']:
            s=(self.dest/f).read_text();self.assertIn('COFFEE GRAINS',s);self.assertIn('Granular Synthesizer',s);self.assertIn(self.sha,s)

if __name__=='__main__':unittest.main(verbosity=2)
