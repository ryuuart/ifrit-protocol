"""The CSS selector grammar, the rule and the sheet a node applies.

Input contracts for the erased signatures of the compose-selectors
package, and nothing else: a fragment is one author's alone.

Most of this package states itself: a selector, a weight and a rule are
classes the signatures name, and the `of` filter of a counted
pseudo-class is an optional selector pybind11 already declares. What is
left is what a rule states — an ink, which is a colour or a custom
property exactly as an element's is, and a custom property's value,
which is a colour or a length — and the statements a sheet is built
from, which are rules and other sheets in any iterable.
"""

from __future__ import annotations

from .table import Table

MODULE = "_sigil.compose"
RULE = MODULE + ".Rule"
SHEET = MODULE + ".StyleSheet"
STATEMENT = f"{RULE} | {SHEET}"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # A rule states the same ink and the same custom properties a node's
    # own verbs state, and reads them the same way.
    table.erased(RULE, "ink", "_t.ElementInkLike")
    table.erased(RULE, "var", "_t.DimensionLike | _t.ColorLike")
    # A sheet is built from rules and from other sheets, whose rules
    # stand where they stood, and it hands its rules back one by one.
    table.parameters(
        SHEET + ".__init__",
        statements=f"collections.abc.Iterable[{STATEMENT}]",
    )
    table.returns(SHEET, "__iter__", f"collections.abc.Iterator[{RULE}]")
