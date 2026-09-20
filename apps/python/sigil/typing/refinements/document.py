"""Conversion seams of the semantic document components."""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    table.attribute("_sigil.compose.document.__all__", "builtins.list[str]")
    for factory in ("article", "section", "list", "quote"):
        overloads = f"""@typing.overload
def {factory}(children: collections.abc.Iterable[_sigil.compose.Element], /) -> _sigil.compose.Element: ...
@typing.overload
def {factory}(*children: _sigil.compose.Element) -> _sigil.compose.Element: ...
"""
        if factory == "quote":
            overloads += """@typing.overload
def quote(words: str) -> _sigil.compose.Element: ...
"""
        table.declares("_sigil.compose.document", factory, overloads)
