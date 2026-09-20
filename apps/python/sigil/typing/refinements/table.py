"""What one binding subject may say about signatures pybind11 erased.

A refinement fragment receives this table and records the input contracts,
return types and whole declarations the compiled docstrings cannot carry.
Every path is a raw `_sigil` spelling, because the table is applied to the
declarations stubgen produces; the public spelling is decided afterwards by
typing/surface.py.
"""

from __future__ import annotations

# The one colour class Python sees. Skia's colour and SigilMaterial's are the
# same four floats, and the bindings' caster makes this the class every colour
# is read back as.
COLOR = "_sigil.material.Color"
# A recipe instance is one kind of paint, and Python converts one into a paint
# wherever a paint is taken, so every parameter written as the material paint
# accepts a material as well.
MATERIAL = "_sigil.material.Material"
PAINT = "_sigil.material.skia.Paint"


class Table:
    """The refinements every fragment writes into, keyed by raw path."""

    def __init__(self) -> None:
        self.erased_arguments: dict[str, list[str]] = {}
        self.return_types: dict[str, str] = {}
        self.declarations: dict[str, str] = {}
        self.parameter_types: dict[str, dict[str, str]] = {}
        self.attribute_types: dict[str, str] = {}

    def erased(self, prefix: str, names: str, *types: str) -> None:
        """Give the erased arguments of each named member their input types."""
        for name in names.split():
            self.erased_arguments[prefix + "." + name] = list(types)

    def returns(self, prefix: str, names: str, value: str) -> None:
        """State what each named member returns."""
        for name in names.split():
            self.return_types[prefix + "." + name] = value

    def declares(self, prefix: str, name: str, text: str) -> None:
        """Replace one member's generated declaration with this text."""
        self.declarations[prefix + "." + name] = text

    def parameters(self, member: str, /, **types: str) -> None:
        """Name the input types of one member's erased parameters.

        The parameter names are keywords here, so the member path is
        positional: a binding is free to call one of its inputs `member`.
        """
        self.parameter_types.setdefault(member, {}).update(types)

    def attribute(self, path: str, text: str) -> None:
        """State the type of one attribute the bindings expose as a field."""
        self.attribute_types[path] = text

    def paths(self) -> set[str]:
        """Return every raw path this table speaks about."""
        return (
            self.erased_arguments.keys()
            | self.return_types.keys()
            | self.declarations.keys()
            | self.parameter_types.keys()
            | self.attribute_types.keys()
        )
