#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline credential routing checks; never contact GitHub."""
import base64
import importlib.util
import os
import re
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch
spec=importlib.util.spec_from_file_location('state',Path(__file__).with_name('deployment_state.py'))
state=importlib.util.module_from_spec(spec);spec.loader.exec_module(state)
class Authorization(unittest.TestCase):
    def test_missing_writer_even_with_spoofed_job(self):
        with patch.dict(os.environ,{'GH_TOKEN':'reader','GITHUB_JOB':'publish','GITHUB_ACTIONS':'true'},clear=True),patch.object(subprocess,'run') as run:
            with self.assertRaisesRegex(ValueError,'Independent'):state.GitStore.__new__(state.GitStore).cas(None,{},'test')
            run.assert_not_called()
    def test_reader_cannot_be_writer(self):
        with patch.dict(os.environ,{'GH_TOKEN':'reader','STATE_WRITER_TOKEN':'reader'},clear=True):
            with self.assertRaisesRegex(ValueError,'Independent'):state.GitStore.__new__(state.GitStore).cas(None,{},'test')
    def route(self,reject=False):
        calls=[]
        def fake(args,**kw):
            calls.append((args,kw['env'].copy()))
            return subprocess.CompletedProcess(args,1 if reject and 'push' in args else 0,b'a'*40+b'\n',b'')
        with patch.dict(os.environ,{'GITHUB_REPOSITORY':'Auberg1ne/Felucca','GH_TOKEN':'reader','STATE_WRITER_TOKEN':'app'},clear=True):
            store=state.GitStore()
            with patch.object(store,'read',return_value=(None,{})),patch.object(subprocess,'run',side_effect=fake):
                if reject:
                    with self.assertRaisesRegex(ValueError,'reconcile'):store.cas(None,{},'test')
                else:store.cas(None,{},'test')
        for args,env in calls:
            self.assertNotIn('STATE_WRITER_TOKEN',env)
            value=env['GIT_CONFIG_VALUE_0']
            expected='app' if 'push' in args else 'reader'
            self.assertEqual(value,'AUTHORIZATION: basic '+base64.b64encode(('x-access-token:'+expected).encode()).decode())
        self.assertEqual(sum('push' in args for args,_ in calls),1)
    def test_token_routing(self):self.route()
    def test_rejected_write_never_retries(self):self.route(True)
    def test_missing_secret_fails_shell_gate(self):
        for value,expected in [('',1),('placeholder',0)]:
            result=subprocess.run(['bash','-c','test -n "$APP_KEY"'],env={'APP_KEY':value},capture_output=True)
            self.assertEqual(result.returncode,expected)
            self.assertEqual(result.stdout+result.stderr,b'')
    def test_guard_precedes_publication_and_writer_is_step_scoped(self):
        text=(Path(__file__).resolve().parents[2]/'.github/workflows/coffee-grains-pages.yml').read_text()
        self.assertLess(text.index('deployment_state.py begin'),text.index('uses: actions/deploy-pages@'))
        self.assertLess(text.index('uses: actions/deploy-pages@'),text.index('deployment_state.py finalize'))
        self.assertIn("github.ref == 'refs/heads/main'",text)
        self.assertNotIn('set -x',text)
        self.assertNotIn('skip-token-revoke:',text)
        for pin in re.findall(r'uses: ([^\s]+)',text):
            self.assertRegex(pin,r'^[^@]+@[0-9a-f]{40}$')
        self.assertEqual(text.count('uses: actions/create-github-app-token@67018539274d69449ef7c02e8e71183d1719ab42'),2)
    def test_stale_expected_head_cannot_push(self):
        with patch.dict(os.environ,{'GH_TOKEN':'reader','STATE_WRITER_TOKEN':'app'},clear=True):
            store=state.GitStore.__new__(state.GitStore)
            with patch.object(store,'read',return_value=('new',{})),patch.object(subprocess,'run') as run:
                with self.assertRaisesRegex(ValueError,'conflict'):store.cas('old',{},'test')
                run.assert_not_called()
    def test_permissions_and_placement(self):
        text=(Path(__file__).resolve().parents[2]/'.github/workflows/coffee-grains-pages.yml').read_text()
        self.assertNotIn('      contents: write',text)
        self.assertEqual(text.count('permission-contents: write'),2)
        self.assertEqual(text.count('STATE_WRITER_TOKEN: ${{ steps.state-writer.outputs.token }}'),3)
        self.assertNotIn('APP_PRIVATE_KEY',text.split('  publish:')[0])
        for job in ('publish','reconcile'):
            section=text.split('  '+job+':')[1].split('\n  reconcile:')[0]
            self.assertIn('github-pages',section)
            self.assertIn("app-id: '5678740'",section)
            self.assertIn('owner: Auberg1ne',section)
            self.assertIn('repositories: Felucca',section)
            self.assertNotIn('path: source',section)
            self.assertIn('run: test -n "$APP_KEY"',section)
if __name__=='__main__':unittest.main()
