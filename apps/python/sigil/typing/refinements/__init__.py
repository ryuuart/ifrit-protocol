"""Explicit Python input contracts for erased native binding signatures.

One fragment beside this file per binding subject, each with a `register`
function that writes into the shared table. Adding a subject is adding a
fragment; no fragment reads or edits another, and this file names none of
them — every module here except the table and the engine is collected.
"""

from __future__ import annotations

import ast
import importlib
import pathlib

from .engine import prune_imports as prune_imports
from .engine import refine as apply_refinements
from .table import Table

SEEN: set[str] = set()
TABLE = Table()

_INFRASTRUCTURE = {"__init__", "engine", "table"}


def _collect() -> None:
    directory = pathlib.Path(__file__).resolve().parent
    for path in sorted(directory.glob("*.py")):
        if path.stem in _INFRASTRUCTURE:
            continue
        fragment = importlib.import_module(f"{__name__}.{path.stem}")
        register = getattr(fragment, "register", None)
        if register is None:
            raise RuntimeError(
                f"{path} declares no register(table); a refinement fragment is "
                "one function that writes into the table it is given."
            )
        register(TABLE)


_collect()


def refine(module: str, tree: ast.Module) -> None:
    """Rewrite one generated module's declarations from the shared table."""
    apply_refinements(TABLE, SEEN, module, tree)


def validate() -> None:
    """Refuse refinements the extension no longer has anything to refine."""
    absent = TABLE.paths() - SEEN
    if absent:
        raise RuntimeError(
            "Refinements no longer present in the extension: "
            + ", ".join(sorted(absent))
        )
