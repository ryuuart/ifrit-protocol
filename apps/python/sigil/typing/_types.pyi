from collections.abc import Buffer, Callable, Iterable, Sequence
from typing import Literal, Protocol, SupportsFloat, SupportsIndex, TypeAlias

import _sigil.compose
import _sigil.compose.connect
import _sigil.compose.layouts
import _sigil.data
import _sigil.draw
import _sigil.material
import _sigil.material.skia
import _sigil.motion
import _sigil.skia
import _sigil.weave

__all__ = [
    "AddingOperator",
    "AlignLike",
    "ArrangingOperator",
    "AttributeLike",
    "AttributeValue",
    "BankMaker",
    "CellInput",
    "CellValue",
    "ChildLike",
    "ColorLike",
    "DecorationLike",
    "DimensionLike",
    "DirectionLike",
    "DrawCallback",
    "EaseLike",
    "ElementInkLike",
    "FillLike",
    "FloatLike",
    "GradientStops",
    "ImageDecoder",
    "IntRectLike",
    "JsonInput",
    "JsonValue",
    "JustifyLike",
    "KeyedShapeFunction",
    "MotionCallback",
    "NodeLike",
    "OperatorLike",
    "PaintProgram",
    "PenProgram",
    "PointBatch",
    "PointLike",
    "RadianceFunction",
    "RampStops",
    "RectLike",
    "SampleLike",
    "ScalarFunction",
    "ScalarLike",
    "ScopeProgram",
    "ShapeFunction",
    "ShapeLike",
    "SilhouetteLike",
    "SizeLike",
    "SurfacePaintLike",
    "TextureProducer",
    "TickCallback",
    "TileProgram",
    "UniformValue",
    "Vec2",
    "Vec2Like",
    "Vec3",
    "Vec3Like",
    "Vec4",
    "Vec4Like",
]

FloatLike: TypeAlias = SupportsFloat | SupportsIndex
ColorLike: TypeAlias = (
    _sigil.material.Color
    | str
    | tuple[FloatLike, FloatLike, FloatLike]
    | tuple[FloatLike, FloatLike, FloatLike, FloatLike]
    | list[FloatLike]
)
"""A FLAT COLOUR VALUE. The colour class, a CSS string, or three or four
unit channels. A slot that takes it promises only to read a colour: it
resolves nothing from the tree and follows no binding, so what it is
given is what it paints. Colours read back out of the libraries are the
colour class, so a colour taken off one value passes into the next."""
PointLike: TypeAlias = _sigil.skia.Point | Sequence[FloatLike]
RectLike: TypeAlias = _sigil.skia.Rect | Sequence[FloatLike]
IntRectLike: TypeAlias = Sequence[SupportsIndex]
"""A RECTANGLE COUNTED IN WHOLE TEXELS — left, top, right, bottom. A
region of an image addresses texels rather than measuring a distance
across them, so it is written as four integers and read back as four."""
SizeLike: TypeAlias = _sigil.skia.Size | Sequence[FloatLike]
ScalarLike: TypeAlias = (
    FloatLike
    | _sigil.motion.Animatable
    | _sigil.motion.Transitioned
    | _sigil.motion.Output
    | _sigil.motion.Bound
)
DimensionLike: TypeAlias = (
    FloatLike
    | str
    | _sigil.compose.Dimension
    | _sigil.weave.Length
    | _sigil.compose.VarRef
)
FillLike: TypeAlias = (
    ColorLike
    | _sigil.compose.Fill
    | _sigil.compose.VarRef
    | _sigil.material.skia.Paint
    | None
)
"""A FLAT MARK, which may be a reference the tree resolves. Everything a
colour is, plus a Fill, a custom-property reference, and None for no
mark at all. A slot that takes it stores one comparable fill, so a paint
is accepted only where it collapses to one: a solid or a static shader
passes and a live or geometry-dependent paint raises, naming the verb
that does take it."""
SurfacePaintLike: TypeAlias = (
    FillLike
    | _sigil.compose.SurfacePaint
    | _sigil.material.Material
    | _sigil.motion.FillOutput
    | _sigil.motion.FillTransitioned
    | _sigil.motion.ColorTransitioned
)
"""ANYTHING THAT CAN COLOUR A SURFACE, and the widest of the three.
Everything a flat mark is, plus a paint of any tier, a recipe instance,
a bound fill and a fill transition. A slot that takes it resolves
against the frame it paints at, so a gradient measured on the node and a
material that reads the clock both belong in it."""
ElementInkLike: TypeAlias = ColorLike | _sigil.compose.VarRef
"""THE INK AN ELEMENT SETS for itself and everything under it. A colour
or a custom-property reference, and deliberately nothing animatable: a
bound ink would make every inheriting node volatile, so an ink that has
to move is set on a fill that names it."""
AlignLike: TypeAlias = (
    _sigil.compose.Align
    | Literal["auto", "start", "center", "end", "stretch", "baseline"]
)
JustifyLike: TypeAlias = (
    _sigil.compose.Justify
    | Literal["start", "center", "end", "space_between", "space_around", "space_evenly"]
)
ShapeFunction: TypeAlias = Callable[[float, float], _sigil.skia.Path]
ShapeLike: TypeAlias = _sigil.compose.Shape | _sigil.skia.Path | ShapeFunction
NodeLike: TypeAlias = (
    _sigil.compose.Element
    | _sigil.compose.Text
    | _sigil.compose.Image
    | _sigil.compose.Band
)
"""ANY NODE OF THE ELEMENT TREE. The element, and the three typed leaves
a factory hands back. A leaf is a class of its own rather than a subclass
of the element, so that the verbs it shares with the element can hand
back the leaf's own type; a slot that holds a node therefore names all
four, and reads whichever arrives as the element it describes."""
ChildLike: TypeAlias = NodeLike | str | None | Iterable[ChildLike]
"""WHAT A CONTAINER TAKES AS ONE CHILD. A node, words that become a
text element, None for nothing at all, or an ordered iterable of those,
nested as deeply as the author nests it. A mapping or a set is not a
child: the order children are laid out in has to be the order they were
written in."""

AttributeLike: TypeAlias = (
    bool
    | int
    | float
    | str
    | Sequence[str]
    | Sequence[FloatLike]
    | _sigil.skia.Point
)
"""A TYPED FACT'S VALUE, as a node states it. The type is part of the
fact: a fact written as a whole number is not read back as a number, and
a sequence of numbers is a list of numbers whatever its length, so a
fact meant as a point is written as a point. An empty sequence is an
empty list of strings, because an empty one has to pick."""
AttributeValue: TypeAlias = (
    bool | int | float | str | list[str] | list[float] | _sigil.skia.Point | None
)
"""A FACT READ BACK, in the type it was stated in, and None where no
fact of that name stands in one of them. A fact a C++ caller stated in
some other type reads as None for the same reason."""

class ArrangingOperator(Protocol):
    """A VALUE THAT PLACES A NODE'S DIRECT CHILDREN during layout,
    reading each one's measured size and facts and writing where it
    goes. It runs inside the layout's converging rounds, so it is asked
    again whenever what it read has moved, and the equality of the class
    it belongs to is what lets its node prune."""

    def arrange(self, arrangement: _sigil.compose.Arrangement) -> None: ...

class AddingOperator(Protocol):
    """A VALUE THAT BUILDS ELEMENTS FROM A SETTLED TREE, run once layout
    has settled and handed every node in its scope. What it attaches is
    an ordinary element beside the authored children, and it can move
    nothing that was authored."""

    def add(self, scope: _sigil.compose.Scope) -> None: ...

OperatorLike: TypeAlias = (
    _sigil.compose.Operator
    | _sigil.compose.layouts.Radial
    | _sigil.compose.layouts.Grid
    | _sigil.compose.layouts.Diagonal
    | _sigil.compose.layouts.BaselineGrid
    | _sigil.compose.layouts.Jittered
    | _sigil.compose.layouts.Jitter
    | _sigil.compose.layouts.AlongPath
    | _sigil.compose.connect.Between
    | _sigil.compose.connect.ByLane
    | ArrangingOperator
    | AddingOperator
)
"""WHAT AN OPERATOR IS BUILT FROM: one already built, a stock arranging
value, a connecting record, or a Python object that arranges or adds. A
Python object takes part in structural equality only where its own class
states an equality, so a frozen dataclass prunes its node and a plain
class is the escape hatch that never does."""
ScopeProgram: TypeAlias = (
    Callable[[], None]
    | Callable[[_sigil.draw.Pen], None]
    | Callable[[_sigil.draw.Pen, _sigil.compose.Scope], None]
)
"""WHAT ONE PEN DRAWS A SETTLED SCOPE WITH, on the same terms as a pen
program: the program names the parameters it reads, from the first, and
both the pen and the scope are lent for the one call. The scope is there
to be read — attaching to it draws nothing, because the pixels land
after the additions are laid out."""

class SilhouetteLike(Protocol):
    """Anything that answers a path over a size: the pen calls
    ``path((width, height))`` and draws what comes back. The size is the
    pair the pen passes, not a wider size input, because a protocol's
    parameter is contravariant and a wider one would reject an author's
    own two-float signature."""

    def path(self, size: tuple[float, float]) -> _sigil.skia.Path: ...

DecorationLike: TypeAlias = (
    _sigil.compose.Decoration | _sigil.compose.PathFormat | _sigil.compose.Shadow
)
PointBatch: TypeAlias = Buffer | Iterable[PointLike]
DrawCallback: TypeAlias = Callable[[_sigil.draw.Pen], None]
PaintProgram: TypeAlias = (
    Callable[[], None]
    | Callable[[_sigil.draw.Canvas], None]
    | Callable[[_sigil.draw.Canvas, _sigil.compose.PaintContext], None]
)
"""WHAT A CUSTOM LEAF PAINTS WITH. A program names the parameters it
reads, from the first: nothing, the canvas, or the canvas and the paint
context of the node. Both are lent for the one call and refuse every
reading once it has returned, so neither is kept past it."""
PenProgram: TypeAlias = (
    Callable[[], None]
    | Callable[[_sigil.draw.Pen], None]
    | Callable[[_sigil.draw.Pen, _sigil.compose.PaintContext], None]
)
"""WHAT A PEN LEAF AND A GRAPHICS LEAF RUN EACH FRAME, on the same terms
as a paint program with the node's pen in place of the canvas."""
KeyedShapeFunction: TypeAlias = ShapeFunction | Callable[[], _sigil.skia.Path]
"""THE OUTLINE A KEYED SHAPE GENERATES, over the box's width and height,
or over nothing when the path is the same whatever the box is. The key
beside it is the identity: everything the function reads belongs in the
key, because a node whose key is unchanged replays the picture it
recorded."""
ScalarFunction: TypeAlias = Callable[[float], float]
EaseLike: TypeAlias = _sigil.motion.Easing | _sigil.motion.Curve | ScalarFunction | None
GradientStops: TypeAlias = Iterable[tuple[FloatLike, ColorLike]]
RampStops: TypeAlias = Sequence[_sigil.material.RampStop]
"""THE STOPS A RAMP IS SAMPLED FROM, in the order they climb. A sequence
rather than any iterable, because the stops are read more than once and
a generator would be spent on the first reading."""
UniformValue: TypeAlias = ScalarLike | _sigil.material.Color | str | Sequence[FloatLike]
"""A NAMED UNIFORM on an SkSL paint or effect. A number, a live scalar
that makes the value re-read every frame, a colour written as the colour
class or a CSS string, or a flat array matched against the declared
uniform's total float count. A paint and an effect take the same set, so
one uniform is written the same way on either seam."""
CellValue: TypeAlias = None | bool | int | float | str | _sigil.data.Instant
CellInput: TypeAlias = CellValue | _sigil.data.Flag
JsonValue: TypeAlias = (
    None | bool | int | float | str | list[JsonValue] | dict[str, JsonValue]
)
JsonInput: TypeAlias = (
    None
    | bool
    | int
    | float
    | str
    | _sigil.data.Json
    | list[JsonInput]
    | tuple[JsonInput, ...]
    | dict[str, JsonInput]
)
Vec2: TypeAlias = tuple[float, float]
Vec3: TypeAlias = tuple[float, float, float]
Vec4: TypeAlias = tuple[float, float, float, float]
Vec2Like: TypeAlias = Sequence[FloatLike]
Vec3Like: TypeAlias = Sequence[FloatLike]
Vec4Like: TypeAlias = Sequence[FloatLike]
SampleLike: TypeAlias = (
    _sigil.draw.brush.Sample
    | PointLike
    | tuple[PointLike, FloatLike]
    | tuple[FloatLike, FloatLike, FloatLike]
)
DirectionLike: TypeAlias = (
    _sigil.draw.brush.Direction
    | _sigil.draw.brush.Curl
    | _sigil.draw.brush.Vortex
    | _sigil.draw.brush.Wave
    | Callable[[_sigil.skia.Point, float], float]
    | None
)
TickCallback: TypeAlias = (
    Callable[[], bool | None]
    | Callable[[float], bool | None]
    | Callable[[float, float], bool | None]
)
MotionCallback: TypeAlias = Callable[[], None] | Callable[[float], None]
"""WHAT A MOTION REPORTS TO when it starts, on each frame it writes, and
when it finishes. A report that names a parameter is handed the value
the motion has just written into its output; one that names none is
simply called."""
TileProgram: TypeAlias = Callable[[_sigil.draw.Canvas, Vec2, int], None]
"""THE DRAWING ONE TILE IS BAKED FROM: the canvas of the bake, the
tile's size in pixels, and the seed the bake was asked for. The same
seed draws the same tile, which is what makes regeneration a choice.
The canvas is lent for the call and refuses every verb once the bake
has returned."""
TextureProducer: TypeAlias = Callable[[], _sigil.skia.Image | None]
"""HOW A NAMED TEXTURE BAKES ITS IMAGE, called on first use and kept.
The key beside it is the identity, so a producer under a key another
texture already baked is never called. None is a producer that had
nothing to give, and leaves the texture empty rather than raising."""
RadianceFunction: TypeAlias = Callable[[float, float], Vec3Like]
"""THE LIGHT ARRIVING ALONG ONE DIRECTION, as linear red, green and
blue. It is called once per texel of a panorama with the
equirectangular coordinates that texel stands for, so this is the seam
a procedural sky is written against."""
ImageDecoder: TypeAlias = Callable[[str], _sigil.skia.Image | None]
"""HOW A TEXTURE SET READS ONE FILE. The path is handed over as the
folder listing spelled it, and None is a file this decoder cannot
read, which leaves that role empty instead of failing the set."""
BankMaker: TypeAlias = Callable[[int], _sigil.material.Material]
"""HOW A BANK MINTS THE INSTANCE A BUCKET IS MISSING. The bucket number
is what varies the piece — a seed the recipe reads, a jitter on a tone
— and the material comes back whole, so a blend of several recipes is
banked exactly as one recipe is."""
