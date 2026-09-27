"""Conversion seams of colours, palettes, ramps, patterns and paints.

Material recipes keep value uniforms distinct from animated effects.
"""

from __future__ import annotations

from .table import Table

PAINT = "_sigil.material.skia.Paint"


def register(table: Table) -> None:
    table.erased("_sigil.material.skia.Filter", "glow dropShadow shadow stroke", "_t.ColorLike")
    table.erased("_sigil.material.Material", "__init__", "_t.ColorLike | _sigil.material.skia.Paint")
    table.erased("_sigil.material.Material", "bind", "_t.ScalarLike")
    table.erased(
        "_sigil.material", "from_", "_sigil.material.Material | _sigil.material.skia.Paint | _t.ColorLike"
    )
    table.parameters("_sigil.material.sbsar.describe", hub="_sigil.io.Hub")
    table.parameters(
        "_sigil.material.studio",
        direction="_t.ScalarLike",
        elevation="_t.ScalarLike",
        color="_t.ColorLike",
        intensity="_t.ScalarLike",
    )
    table.parameters("_sigil.material.environment", rotation="_t.ScalarLike")
    # Where a point or a spot stands is three numbers, read back as a vector.
    table.declares(
        "_sigil.material.Light",
        "position",
        """@property
def position(self) -> _t.Vec3: ...
""",
    )
    table.parameters(
        "_sigil.material.substance",
        hub="_sigil.io.Hub",
        seed="int",
        outputs="collections.abc.Iterable[str | _sigil.material.sbsar.OutputRequest]",
        engine="_sigil.material.sbsar.Engine | None",
        kwargs="float | int | bool",
    )
    table.parameters(
        "_sigil.material.shader",
        hub="_sigil.io.Hub",
        parameters="collections.abc.Mapping[str, _t.UniformValue] | tuple[_t.UniformValue, ...] | None",
        textures="collections.abc.Mapping[str, _sigil.media.Image | _sigil.skia.Image] | None",
    )
    table.parameters("_sigil.material.skia.Filter.blur", sigmaMap="Paint")
    table.erased("_sigil.material.skia.Filter", "set", "_t.UniformValue")
    table.erased("_sigil.material.skia.Filter", "bind", "_t.ScalarLike")
    table.erased("_sigil.material.skia.Filter", "program", "str | _sigil.skia.RuntimeEffect")
    table.parameters("_sigil.material.skia.Filter.program", parameters="dict[str, float]")
    table.parameters("_sigil.material.skia.Filter.slot", paint="Paint")
    table.erased(PAINT, "solid", "_t.ColorLike")
    table.erased(PAINT, "set", "_t.UniformValue")
    table.erased(PAINT, "bind", "_t.ScalarLike")
    table.erased(PAINT, "sksl", "str | _sigil.skia.RuntimeEffect")
    table.parameters(PAINT + ".sksl", uniforms="dict[str, _t.UniformValue]")
    table.erased(
        PAINT, "linearGradient", "_t.PointLike", "_t.PointLike", "_t.GradientStops"
    )
    table.erased(
        PAINT, "radialGradient conicGradient", "_t.PointLike", "_t.GradientStops"
    )
    table.attribute(
        "_sigil.material.skia.GradientOptions.focus", "_t.PointLike | None"
    )
    table.accessor(
        "_sigil.material.skia.ShadowOptions", "offset", "tuple[float, float]", "_t.PointLike"
    )
    table.parameters("_sigil.material.skia.ShadowOptions.__init__", offset="_t.PointLike")
    table.parameters(
        "_sigil.material.skia.GradientOptions.__init__", focus="_t.PointLike | None"
    )
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
    table.erased("_sigil.material.ColorStop", "__init__ color", "_t.ColorLike")
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
