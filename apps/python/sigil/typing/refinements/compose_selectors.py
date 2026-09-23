"""The CSS selector grammar, the rule and the sheet a node applies.

Input contracts for the erased signatures of the compose-selectors
package, and nothing else: a fragment is one author's alone.

Most of this package states itself: a selector, a weight and a rule are
classes the signatures name, and the `of` filter of a counted
pseudo-class is an optional selector pybind11 already declares. What a
rule states is refined beside the node's own verbs, since the two are
one binding; what is left is the statements a sheet is built from, which
are rules and other sheets in any iterable.
"""

from __future__ import annotations

from .table import Table

MODULE = "_sigil.compose"
RULE = MODULE + ".Rule"
SHEET = MODULE + ".StyleSheet"
STATEMENT = f"{RULE} | {SHEET}"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # What a rule states is the declaring half of a node's vocabulary,
    # refined once for both beside the node's own verbs.
    # A sheet is built from rules and from other sheets, whose rules
    # stand where they stood, and it hands its rules back one by one.
    table.parameters(
        SHEET + ".__init__",
        statements=f"collections.abc.Iterable[{STATEMENT}]",
    )
    table.returns(SHEET, "__iter__", f"collections.abc.Iterator[{RULE}]")
    # A plain selector converts to a relative one reached anywhere under
    # the element, so every slot that takes a relative selector takes a
    # plain one too.
    relative = f"{MODULE}.RelativeSelector | {MODULE}.ElementSelector"
    table.declares(
        MODULE + ".select",
        "has",
        f"def has(relatives: {relative}) -> {MODULE}.ElementSelector:\n"
        '    """Elements from which some of `relatives` can be reached — CSS '
        "`:has(...)`, weighing as the heaviest of them. A plain selector is "
        "reached anywhere under the element; `child`, `next` and `sibling` "
        "name the other three relations. `has(a | b)` asks for either, "
        "`has(a) & has(b)` for both. A `:has()` inside another matches "
        'nothing, as in CSS."""\n',
    )
    table.declares(
        MODULE + ".RelativeSelector",
        "__or__",
        f"def __or__(self, other: {relative}) -> RelativeSelector: ...",
    )
