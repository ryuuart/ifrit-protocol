"""Conversion seams of the immediate pen and its offscreen graphics.

Pen conversions retain native paint and colour distinctions. The canvas
a pen lends is the canvas seam's own subject, and so is its fragment.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    table.returns("_sigil.draw.Graphics", "extent", "tuple[float, float]")
    table.parameters("_sigil.draw.Graphics.draw", program="_t.DrawCallback")
    table.parameters("_sigil.draw.on", program="_t.DrawCallback")
    table.erased("_sigil.draw.Pen", "lerpColor", "_t.ColorLike", "_t.ColorLike")
    table.erased("_sigil.draw", "lerpColor", "_t.ColorLike", "_t.ColorLike")
    table.erased("_sigil.draw.Pen", "circle point", "_t.PointLike")
    table.erased("_sigil.draw.Pen", "line", "_t.PointLike", "_t.PointLike")
    table.erased("_sigil.draw.Pen", "element", "_t.RectLike")
    table.erased("_sigil.draw.Pen", "inherit", "_t.ColorLike")
    # A silhouette is whatever answers a path over a size; geometry's own
    # generators are not bound, so in Python the author writes the object.
    table.parameters("_sigil.draw.Pen.shape", silhouette="_t.SilhouetteLike")
    table.parameters("_sigil.draw.Pen.clip", shape="_t.DrawCallback")
    table.parameters("_sigil.draw.Pen.image", image="Graphics")
    # Every form the binding dispatches on, in the order it tests them: a
    # paint with or without the fit it is measured in, a material, then the
    # colour forms. All positional-only, because the binding takes no keyword.
    for method in ("background", "fill", "stroke", "color"):
        result = "_sigil.material.Color" if method == "color" else "None"
        declarations = [f"def {method}(self, value: _t.ColorLike, /) -> {result}: ..."]
        if method in ("fill", "stroke"):
            declarations.append(
                f"def {method}(self, paint: _sigil.material.skia.Paint, fit: Constant = ..., /) -> {result}: ..."
            )
            declarations.append(
                f"def {method}(self, material: _sigil.material.Material, /) -> {result}: ..."
            )
        elif method == "background":
            declarations.append(
                f"def {method}(self, paint: _sigil.material.skia.Paint, /) -> {result}: ..."
            )
            declarations.append(
                f"def {method}(self, material: _sigil.material.Material, /) -> {result}: ..."
            )
        declarations.append(
            f"def {method}(self, gray: _t.FloatLike, alpha: _t.FloatLike = ..., /) -> {result}: ..."
        )
        declarations.append(
            f"def {method}(self, red: _t.FloatLike, green: _t.FloatLike, blue: _t.FloatLike, alpha: _t.FloatLike = ..., /) -> {result}: ..."
        )
        table.declares(
            "_sigil.draw.Pen",
            method,
            "".join(f"@typing.overload\n{line}\n" for line in declarations),
        )
