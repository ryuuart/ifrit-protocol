"""copy.deepcopy on every bound record.

Input contracts for the erased signatures of the
sketch-kit-and-hosting/value-copy-protocol package, and nothing else: a fragment is one
author's alone.

This package adds `__copy__` and `__deepcopy__` to every bound record, and
pybind11 writes both signatures whole: `__copy__` takes nothing, `__deepcopy__`
takes the memo dictionary the `copy` module keeps its cycles in, and each
answers the class it is defined on. `copy.copy` and `copy.deepcopy` are
already declared to answer what they were given, so nothing here states an
input type; what is left is the one class whose name the stub generator
rewrites out from under both signatures.
"""

from __future__ import annotations

from .table import Table

# A RECORD NAMED AFTER A BUILT-IN COLLECTION. The stub generator reads a
# bare `Set`, `Dict`, `List`, `Tuple`, `FrozenSet` or `Type` as the typing
# alias of that name and writes the built-in it stands for, which leaves
# the two copy methods declaring a `set` where they take and answer a
# world set. They are the only methods it reaches, because they are the
# only ones whose first parameter carries the class as an annotation.
WORLD_SET = "_sigil.world.kit.Set"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.declares(WORLD_SET, "__copy__", "def __copy__(self) -> Set: ...\n")
    table.declares(
        WORLD_SET,
        "__deepcopy__",
        "def __deepcopy__(self, memo: dict[builtins.int, builtins.object])"
        " -> Set: ...\n",
    )
