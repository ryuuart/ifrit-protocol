"""Conversion seams of colours, palettes, ramps, patterns and paints.

Material recipes keep value uniforms distinct from animated effects.
"""

from __future__ import annotations

from .table import Table

PAINT = "_sigil.material.skia.Paint"


def register(table: Table) -> None:
    table.erased("_sigil.material.skia.Effect", "glow", "_t.ColorLike")
    table.parameters("_sigil.material.skia.Effect.blur", sigmaMap="Paint")
    table.erased("_sigil.material.skia.Effect", "uniform", "_t.UniformValue")
    table.parameters("_sigil.material.skia.Effect.slot", paint="Paint")
    table.erased(PAINT, "solid", "_t.ColorLike")
    table.erased(PAINT, "uniform", "_t.UniformValue")
    table.erased(PAINT, "sksl", "str | _sigil.skia.RuntimeEffect")
    table.parameters(PAINT + ".sksl", uniforms="dict[str, _t.UniformValue]")
    table.erased(PAINT, "conical linear linearUnit", "_t.PointLike", "_t.PointLike")
    table.erased(PAINT, "glowUnit radial radialUnit sweep", "_t.PointLike")
    for name in (
        "conical",
        "linear",
        "linearUnit",
        "glowUnit",
        "radial",
        "radialUnit",
        "sweep",
    ):
        table.parameters(PAINT + "." + name, stops="_t.GradientStops")
    table.erased("_sigil.material.pattern", "checker", "_t.ColorLike", "_t.ColorLike")
    table.erased(
        "_sigil.material.pattern", "gridLines halftone stripes", "_t.ColorLike"
    )
    table.erased("_sigil.material.Color", "__init__", "_t.ColorLike")
    table.returns(
        "_sigil.material.Color", "__iter__", "collections.abc.Iterator[float]"
    )
    table.returns(
        "_sigil.material.Palette", "__iter__", "collections.abc.Iterator[Color]"
    )
    table.erased("_sigil.material.RampStop", "__init__ color", "_t.ColorLike")
    table.parameters(
        "_sigil.material.Palette.__init__",
        entries="collections.abc.Iterable[_t.ColorLike]",
    )
    table.parameters(
        "_sigil.material.Palette.entries",
        value="collections.abc.Iterable[_t.ColorLike]",
    )
    table.parameters(
        "_sigil.material.palette", pixels="collections.abc.Iterable[_t.ColorLike]"
    )
    table.erased(
        "_sigil.material",
        "toOklab toOklch toLab luminance withAlpha scale lighten rotateHue harmony closestEntry",
        "_t.ColorLike",
    )
    table.erased(
        "_sigil.material",
        "mixLinear lerpOklab mixToward deltaE",
        "_t.ColorLike",
        "_t.ColorLike",
    )
    table.erased("_sigil.material.Dither", "at", "_t.ColorLike")
    table.erased("_sigil.material.field", "halftoneRamp", "_t.ColorLike")
