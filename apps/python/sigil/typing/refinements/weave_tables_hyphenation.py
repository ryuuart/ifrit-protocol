"""Hyphenator subclassing, HyphenationOptions.patterns and the line tables.

Input contracts for the erased signatures of the
weave/tables-hyphenation package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table

OPTIONS = "_sigil.weave.HyphenationOptions"
HYPHENATOR = "_sigil.weave.Hyphenator"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # The table a paragraph asks where a word may break is held shared, and
    # an option that holds none leaves only the soft hyphens typed in.
    table.attribute(OPTIONS + ".patterns", HYPHENATOR + " | None")
    table.parameters(OPTIONS + ".__init__", patterns=HYPHENATOR + " | None")
