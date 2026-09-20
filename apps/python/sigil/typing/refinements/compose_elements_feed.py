"""Feed rings, options and the streaming column.

Input contracts for the erased signatures of the
compose-elements/feed package, and nothing else: a fragment is one
author's alone.

The records state themselves: their fields are read and written through
typed properties, a ring's rows are a list of the one row class, and a
row's value is an object because a ring holds whatever it was given.
What is left is the row function, which the binding takes as a bare
callable, and the two iterators, which carry no element type.
"""

from __future__ import annotations

from .table import Table

FEED = "_sigil.compose.feed"
ELEMENT = "_sigil.compose.Element"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # A row function is handed the value its ring holds: a text ring
    # hands it a copy of the text row, and a ring of Python's own values
    # hands back whatever was appended, so that overload is generic in the
    # parameter the author's function declares.
    table.declares(
        FEED,
        "feed",
        f"""@typing.overload
def feed(ring: TextRing, options: TextOptions) -> {ELEMENT}:
    \"\"\"The text feed: the newest rows, each set in the style it names.\"\"\"
@typing.overload
def feed(ring: TextRing) -> {ELEMENT}:
    \"\"\"The text feed under the options a native scope provides, or the default ones.\"\"\"
@typing.overload
def feed(ring: TextRing, options: Options, row: collections.abc.Callable[[TextRow], {ELEMENT}]) -> {ELEMENT}:
    \"\"\"The newest rows of a text ring, each built by the row function.\"\"\"
@typing.overload
def feed[Value](ring: Ring, options: Options, row: collections.abc.Callable[[Value], {ELEMENT}]) -> {ELEMENT}:
    \"\"\"The newest rows of a ring, each built by the row function from the value appended.\"\"\"
""",
    )
    for ring in ("TextRing", "Ring"):
        table.returns(f"{FEED}.{ring}", "__iter__", "collections.abc.Iterator[Row]")
    # One row class serves every ring, so the value under a sequence id is
    # an object, and the ring of Python's own values takes any object. Both
    # are stated as that rather than left as an unknown. The value reads
    # and writes as one type, so it is declared as a field, and the
    # record's keyword takes its type from that field.
    table.attribute(f"{FEED}.Row.value", "builtins.object")
    table.erased(f"{FEED}.Ring", "append", "builtins.object")
