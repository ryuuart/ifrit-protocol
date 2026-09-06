"""The verbs behind scripts/sigil.py, over one shared core.

`tree` is where the build tree is and how to talk to it; `baseline` is
how a ledger reads, merges, writes and judges what it keeps. Every other
module here is one verb, and the dispatcher imports it by name.
"""
