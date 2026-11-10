#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline adapter for the integrated H13 predicate; never imported by a workflow.

No HTTP, Git or CLI. Fixture timestamps are not a substitute for a live operation window.
"""
import authorization_canary as canary
from rule_suite_diagnostic import Unavailable

REPOSITORY_ID = canary.CREATION_BYPASS_REPOSITORY_ID
SUITE_ID = canary.CREATION_BYPASS_SUITE
CREATION_RULESET_ID = canary.CREATION_BYPASS_RULESET


def creation_bypass(listing, detail, active_creation_rulesets, *, operation):
    try:
        canary.require(isinstance(listing, list) and 0 < len(listing) < 100 and
                       all(isinstance(r, dict) for r in listing), 'Incomplete offline suite list')
        ids = [r.get('id') for r in listing]
        canary.require(all(type(x) is int and x > 0 for x in ids) and len(set(ids)) == len(ids),
                       'Invalid or duplicate offline suite IDs')
        matches = [r for r in listing if r.get('ref') == canary.REF and
                   r.get('before_sha') == canary.ZERO and r.get('after_sha') == canary.CREATION_BYPASS_AFTER
                   and r.get('actor_name') == canary.CREATION_BYPASS_ACTOR]
        canary.require(len(matches) == 1 and isinstance(detail, dict), 'Missing or ambiguous offline suite')
        row = matches[0]
        canary._verified_app_create_bypass(row, detail, canary.ZERO, canary.CREATION_BYPASS_AFTER,
            canary.CREATION_BYPASS_ACTOR, {'creation':active_creation_rulesets}, operation=operation,
            started_at=row.get('pushed_at'), observed_at=row.get('pushed_at'))
    except canary.Inconclusive:
        raise Unavailable('Integrated creation-bypass predicate rejected offline evidence') from None
    return {'proposal_only':True, 'suite_id':SUITE_ID, 'creation_bypass_evidence':True,
            'failed_creation_ruleset_id':CREATION_RULESET_ID, 'h9_verdict':'NOT_REEVALUATED'}
