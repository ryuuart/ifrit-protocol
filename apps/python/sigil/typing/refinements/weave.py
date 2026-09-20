"""Conversion seams of text styles, paragraph layout and the weave kit.

Typography values use the same color, point and native scalar conversions.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    table.erased("_sigil.weave.Type", "color", "_t.ColorLike | None")
    table.attribute("_sigil.weave.Type.textTransform", "TextTransform | None")
    table.attribute("_sigil.weave.Type.verticalForm", "VerticalForm | None")
    table.returns("_sigil.weave.Type", "features", "list[FontFeature] | None")
    table.returns("_sigil.weave.Type", "variations", "list[FontVariation]")
    for field in ("size", "track", "wordSpacing"):
        table.declares(
            "_sigil.weave.Type",
            field,
            f"""@property
def {field}(self) -> Length | None: ...
@{field}.setter
def {field}(self, value: Length | float | int | None) -> None: ...
""",
        )
    table.parameters(
        "_sigil.weave.Type.features",
        value="collections.abc.Sequence[FontFeature] | None",
    )
    table.parameters(
        "_sigil.weave.Type.variations", value="collections.abc.Sequence[FontVariation]"
    )
    table.returns("_sigil.weave.StyleSheet", "types", "TypeSheet")
    for operator in ("__or__", "__and__"):
        table.declares(
            "_sigil.weave.Selector",
            operator,
            f"def {operator}(self, other: Selector) -> Selector: ...",
        )
    table.erased("_sigil.weave.Decoration", "color", "_t.ColorLike")
    table.erased("_sigil.weave.PaintLayer", "offset blurred", "_t.PointLike")
    table.erased("_sigil.weave.kit", "dropShadow", "_t.ColorLike", "_t.PointLike")
    table.erased("_sigil.weave.kit", "glow outline", "_t.ColorLike")
    for name in ("BlockFlow", "VerticalBlockFlow", "ExclusionFlow"):
        table.erased(f"_sigil.weave.{name}", "__init__", "_t.RectLike")
    table.erased(
        "_sigil.weave.silhouette", "rectangle circle ellipse coverage", "_t.RectLike"
    )
    table.erased("_sigil.weave.Beside", "base", "_t.RectLike")
    table.erased("_sigil.weave.Exclusion", "offset", "_t.PointLike")
    table.erased("_sigil.weave.LineInterval", "origin direction", "_t.PointLike")
    table.erased("_sigil.weave", "layoutSingleLine", "_t.PointLike")
    table.erased("_sigil.weave", "layoutWarichu", "_t.RectLike")
    for field, item in (
        ("decorations", "Decoration"),
        ("underlays", "PaintLayer"),
        ("overlays", "PaintLayer"),
    ):
        table.returns("_sigil.weave.Type", field, f"list[{item}] | None")
        table.parameters(
            f"_sigil.weave.Type.{field}",
            value=f"collections.abc.Sequence[{item}] | None",
        )
