#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('source_policy',Path(__file__).with_name('source_policy.py'))
policy=importlib.util.module_from_spec(spec);spec.loader.exec_module(policy)
SOURCE='c2be0a28f0f0ed2304d7294dc731ad55b6dbae28'

class Policy(unittest.TestCase):
    def test_initial_source_exact(self):self.assertEqual(policy.approved(SOURCE),SOURCE)
    def test_no_ref_or_shortened_revision(self):
        for s in ['main','origin/main','feat/coffee-grains-pages-hosting',SOURCE[:7],SOURCE.upper(),SOURCE+'\n',SOURCE+'^{}','--help','']:
            with self.assertRaises(ValueError):policy.approved(s)
    def test_unknown_full_commit_refused(self):
        with self.assertRaises(ValueError):policy.approved('a'*40)
    def test_malformed_policies_fail_closed(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'approved.json'
            for obj in [dict(schema=2,sources=[SOURCE]),dict(schema=1,sources=[]),dict(schema=1,sources=[SOURCE,SOURCE]),dict(schema=1,sources=['main']),dict(schema=1,sources='not a list'),dict(schema=1,sources=[{}]),[],None]:
                p.write_text(json.dumps(obj))
                with self.assertRaises(ValueError):policy.approved(SOURCE,p)
    def mock_checkout(self,dirty='',head=SOURCE,kind='commit',modules=' c46e0086f69bdc26df1cd144dca9e683cc4531dd firmware/engines\n',code=0):
        root=Path('/approved-source')
        def run(command,**kwargs):
            args=command[3:]
            if args==['rev-parse','--show-toplevel']:value=str(root)+'\n'
            elif args==['cat-file','-t',SOURCE]:value=kind+'\n'
            elif args==['rev-parse','HEAD']:value=head+'\n'
            elif args==['status','--porcelain','--untracked-files=normal']:value=dirty
            elif args==['submodule','status','--recursive']:value=modules
            else:raise AssertionError(command)
            return subprocess.CompletedProcess(command,code,value,'')
        return root,patch.object(policy.subprocess,'run',side_effect=run)
    def test_exact_clean_checkout_and_submodules(self):
        root,mock=self.mock_checkout()
        with mock:self.assertEqual(policy.verify_checkout(root,SOURCE,True),root)
    def test_dirty_wrong_head_and_noncommit_refused(self):
        for kwargs in [dict(dirty=' M web/editor.html\n'),dict(head='a'*40),dict(kind='tag')]:
            root,mock=self.mock_checkout(**kwargs)
            with mock,self.assertRaises(ValueError):policy.verify_checkout(root,SOURCE)
    def test_missing_or_changed_submodules_refused(self):
        for m in ['', '-abc firmware/engines\n','+abc firmware/engines\n','Uabc firmware/engines\n']:
            root,mock=self.mock_checkout(modules=m)
            with mock,self.assertRaises(ValueError):policy.verify_checkout(root,SOURCE,True)
    def test_unavailable_git_metadata_refused(self):
        root,mock=self.mock_checkout(code=128)
        with mock,self.assertRaises(ValueError):policy.verify_checkout(root,SOURCE)
    def test_fixture_environment_cannot_fall_back_to_main(self):
        spec=importlib.util.spec_from_file_location('fixture_test',Path(__file__).with_name('site_test.py'))
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        case=module.Assembly('test_complete_manifest')
        try:
            with patch.dict(os.environ,{'COFFEE_GRAINS_TEST_SOURCE':'','COFFEE_GRAINS_TEST_SHA':''}):
                with self.assertRaisesRegex(ValueError,'COFFEE_GRAINS_TEST_SOURCE'):case.setUp()
        finally:case.doCleanups()
    def test_controller_checkout_is_not_a_simulator_fixture(self):
        root=Path(__file__).resolve().parents[2]
        with self.assertRaisesRegex(ValueError,'exact approved commit'):policy.verify_checkout(root,SOURCE)
    def test_workflow_uses_only_approved_source_and_explicit_test_fixtures(self):
        root=Path(__file__).resolve().parents[2]
        s=(root/'.github/workflows/coffee-grains-pages.yml').read_text()
        self.assertIn('COFFEE_GRAINS_TEST_SOURCE: ${{ github.workspace }}/source',s)
        self.assertIn('COFFEE_GRAINS_TEST_SHA: ${{ inputs.revision }}',s)
        self.assertIn('source_policy.py "$REVISION" --checkout source --require-submodules',s)
        approve=s[s.index('  approve:'):s.index('  build:')]
        self.assertIn('source_policy.py "$REVISION"',approve)
        build=s[s.index('  build:'):s.index('  publish:')]
        self.assertLess(build.index('source_policy.py "$REVISION"'),build.index('ref: ${{ inputs.revision }}'))
        fixture=(root/'tools/hosting/site_test.py').read_text()
        self.assertNotIn('repo=Path(__file__).resolve().parents[2]',fixture)
        self.assertIn('source_policy.verify_checkout(source,revision)',fixture)

if __name__=='__main__':unittest.main(verbosity=2)
