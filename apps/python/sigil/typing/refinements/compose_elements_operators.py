"""The facts a node states, the operators that read them, and the scope.

Input contracts for the erased signatures of the
compose-elements/operators package, and nothing else: a fragment is one
author's alone.

Three readings are erased here and nowhere else in the package. A FACT'S
VALUE is read by the type Python spells it as, so both the writing and
the reading name the set rather than one type. AN OPERATOR is read from
an operator, a stock arranging value, a connecting record or a Python
object that arranges or adds, which is one alias the whole family takes.
The LENT VALUES — the arrangement's children and the scope's nodes —
come back as ordinary lists of the bound records, which the bindings
build by hand and pybind11 therefore cannot spell.
"""

from __future__ import annotations

from .table import Table

MODULE = "_sigil.compose"
ATTRIBUTES = MODULE + ".Attributes"
OPERATOR = MODULE + ".Operator"
ARRANGEMENT = MODULE + ".Arrangement"
CHILD = ARRANGEMENT + ".Child"
SCOPE = MODULE + ".Scope"
NODE = SCOPE + ".Node"

# Every kind of node states these three, and each leaf is a class of its
# own rather than a subclass of the element, so the row is written once
# per class the vocabulary is bound on.
NODES = (MODULE + ".Element", MODULE + ".Text", MODULE + ".Image", MODULE + ".Band")


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.parameters(ATTRIBUTES + ".set", value="_t.AttributeLike")
    table.returns(ATTRIBUTES, "get", "_t.AttributeValue")
    table.parameters(OPERATOR + ".__init__", value="_t.OperatorLike")
    for node in NODES:
        table.parameters(node + ".attribute", value="_t.AttributeLike")
        table.parameters(
            node + ".operators",
            operators="collections.abc.Sequence[_t.OperatorLike]",
        )
    table.parameters(MODULE + ".drawWith", program="_t.ScopeProgram")

    # A rectangle and a point are read through the shared conversion
    # wherever an operator writes one; the rect property's setter is the
    # one erased argument under its name.
    table.erased(CHILD, "rect", "_t.RectLike")
    table.parameters(CHILD + ".place", rect="_t.RectLike")
    table.parameters(CHILD + ".centreAt", centre="_t.PointLike")
    table.returns(CHILD, "attribute", "_t.AttributeValue")
    table.returns(ARRANGEMENT, "children", "list[" + CHILD + "]")

    table.returns(NODE, "attribute", "_t.AttributeValue")
    table.parameters(NODE + ".toLocal", value="_t.PointLike | _sigil.skia.Path")
    table.returns(NODE, "toLocal", "_sigil.skia.Point | _sigil.skia.Path")
    table.parameters(NODE + ".attach", element="_t.NodeLike")
    table.returns(SCOPE, "nodes", "list[" + NODE + "]")
    table.returns(SCOPE, "find", NODE + " | None")
    table.returns(SCOPE, "having withClass", "list[" + NODE + "]")
    table.parameters(SCOPE + ".attach", element="_t.NodeLike")
