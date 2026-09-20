---
kind: type
library: SigilDraw
name: Pen
qualified: sigil::draw::Pen
group: The pen
status: stable
---

p5's surface, verbatim where C++ allows it: the same verbs, the same
argument orders and counts, the same defaults. A sketch pasted from p5
differs by `pen.` in front of each verb and nothing else it needs to
think about; what is this library's own is an ADDED overload on the same
verb, never a renamed one.

## Description

The pen holds the style, the transform, the seeded streams and what it
keeps between frames as ONE VALUE, so two pens draw side by side and
nothing is global. The defaults are p5's: a white fill, a black
one-pixel stroke, `rectMode(CORNER)`, `ellipseMode(CENTER)`,
`angleMode(RADIANS)`, `colorMode(RGB)` over 255. The added overloads are
a material as a fill, a silhouette as a shape, a `weave::Type` as the
font, and a retained guest.

Drawing happens between `Pen::begin` and `Pen::end`, which whoever holds
the canvas calls around a frame. The style survives from frame to frame
as it does in p5 — a `Pen::noStroke` in setup holds — and the transform
starts over at the canvas the frame was begun on.

### The frame, and the door out of p5's vocabulary

`Pen::begin` starts a frame: the transform the canvas carries at that
moment is what `Pen::resetMatrix` returns to, and the frame's values are
read into the variables the verbs expose.

`Pen::canvas` is THE CANVAS ITSELF, carrying the pen's current transform
— every `Pen::translate`, `Pen::rotate`, `Pen::scale` and open
`Pen::push` this frame is already on it, so a rect drawn there lands
where `Pen::rect` would put it. It is null between frames.

It is the DOOR OUT of p5's vocabulary: another library's drawing takes an
`SkCanvas&` and this is the one to hand it, alongside `Pen::fillPaint`
and `Pen::strokePaint` for the style the pen stands at and
`Pen::contentScale` for the device pixels one canvas unit covers.
Whatever is drawn through it lands in the same place in the same order as
the pen's own verbs, since there is only one canvas.

Leave it as it was found: the pen's transform and clip carry into the
rest of the frame, so an unbalanced save there is an unbalanced transform
for every verb after it.

`Pen::contentScale` is how many device pixels one canvas unit covered
when the frame began — one on a plate at the declared size, two on a
doubled screen. It is what a hairline, a dash period or a bake resolution
computed outside the pen has to be scaled by.

### What a host seeds, and what a program then owns

`Pen::inherit` is THE INK AND THE FONT THE PEN BEGINS IN, which a host
hands over after `Pen::begin` each frame: they seed the style for what
the PROGRAM has not set. The fill and the stroke take the ink until a
`Pen::fill` or a `Pen::stroke` is called, and the text type takes the
font until a `Pen::textFont`, a `Pen::textSize` or a `Pen::textStyle` is
— after which that verb's choice holds from frame to frame, as p5's
does, and the seed stops reaching it. Nothing else in the style is
touched, so a `Pen::noFill` or a `Pen::noStroke` still means what it
says.

A PEN NOBODY CALLS IT ON KEEPS p5's OWN DEFAULTS: a white fill, a black
stroke, text at twelve pixels. `Pen::inheritedInk` and the font beside it
answer what the last seed carried — black and `weave::initialType` on a
pen that was never told one — so whatever else is seeded from this pen
reads the same values there rather than keeping its own copy of them.

### Fitting a material to what it paints

`Pen::fill` taking a paint and a fit is THE SAME MATERIAL, FITTED TO WHAT
IT PAINTS. `SHAPE` measures the material against the BOUNDS OF EACH SHAPE
the pen draws — the box's top-left is the material's origin and the box
is its unit square — so a unit gradient, a unit glow and anything else
reading `uResolution` land on the shape. `CANVAS`, the default, measures
against the frame, which is where a pen's coordinates otherwise live.

A compose leaf has this and needs no word for it: a node paints inside
its own laid-out box, so a unit-space fill already has a box to be a unit
of. A pen has one canvas and many shapes, so which one a material is a
unit of has to be said — and it is said on the fill, because it is a fact
about that material and not about the pen.

Every verb that fills a shape wears it — a rect, an ellipse, an arc, a
triangle, a quad, a bezier, a curve, a silhouette, a `Pen::beginShape`
outline, a mesh — and so does a `Pen::line` and a `Pen::point` on the
stroke side. A box with no width or no height has no unit square, so a
horizontal line falls back to the canvas rather than dividing by zero.
Text, images and `Pen::background` are always the canvas: they are not
shapes and have no bounds the pen decides. It is style, so `Pen::push`
saves it and `Pen::pop` puts it back. `Pen::stroke` taking a fit reads it
exactly the same way.

### The dashed stroke

`Pen::strokeDash` is THE PEN'S OWN STROKE VERB: a dashed stroke. p5 has
no word for one and reaches through to its drawing context, so this
stands beside `Pen::strokeWeight`, `Pen::strokeCap` and
`Pen::strokeJoin` rather than renaming any of them.

The intervals are the run of lengths the stroke alternates along, on
first: `{6, 4}` is six drawn and four skipped, `{6}` is six and six since
an odd run repeats itself. The phase starts the run partway in, so an
animated phase is a marching-ants line. It is measured in the pen's own
units, along the path, which means a dashed shape under a `Pen::scale`
dashes at the scaled length.

Every stroked verb wears it — a line, a rect, an ellipse, an arc, a
`Pen::beginShape` outline, a shaped glyph's stroke — except `Pen::point`,
which is a disc and not a stroke. A run with a negative length or no
length at all is no dash. It is style, so `Pen::push` saves it and
`Pen::pop` puts it back.

### Blending

`Pen::blendMode` is HOW WHAT IS DRAWN MEETS WHAT IS ALREADY THERE.
`BLEND` lays the source over the canvas by its alpha and is where a pen
starts; `ADD` adds the two and clamps, which is what light does;
`REPLACE` overwrites, alpha and all; `REMOVE` takes the source's alpha
out of the canvas; and `DARKEST`, `LIGHTEST`, `DIFFERENCE`, `EXCLUSION`,
`MULTIPLY`, `SCREEN`, `OVERLAY`, `HARD_LIGHT`, `SOFT_LIGHT`, `DODGE`,
`BURN` and `SUBTRACT` are the rest of the separable functions.

It reaches every verb that puts pixels down — a fill, a stroke, a glyph,
an image, the triangle mesh a per-corner shape is drawn as, and the
ground a `Pen::background` lays — and it is style, so `Pen::push` saves
it and `Pen::pop` puts it back.

`Pen::noSmooth` is jagged edges AND jagged pixels: antialiasing off on
every shape, and `Pen::image` sampled nearest-neighbour with no mipmap,
so a small source blown up is blocks rather than a blur. `Pen::smooth`
puts both back.

### A corner that carries its own colour

`Pen::vertex` is a corner of the shape being built, WEARING THE FILL THAT
STANDS WHEN IT IS ADDED. Calling `Pen::fill` between two vertex calls
therefore colours the shape corner by corner, and the colour is
interpolated across each triangle of the mesh the kind describes — which
is how a ramp along a streak, a lit facet or a heat gradient is drawn
without one shape per band.

It costs nothing where nothing changes: a shape whose corners all carry
one colour is drawn as a path, filled and stroked exactly as before. A
shape whose corners differ is FILLED AS A TRIANGLE MESH, so the fill must
be a solid colour — a gradient or an effect cannot also be interpolated
per corner — while the stroke, if there is one, still follows the shape's
outline. Only the triangle and quad kinds have a mesh; `POLYGON` is one
path with one fill, as p5 has it.

### The mesh verb

`Pen::vertices` is THE PEN'S OWN MESH VERB: an `SkVertices` built
somewhere else — a triangulated field, a lit strip, a deformed grid, a
marching-squares contour — drawn HERE, with the pen's fill, its blend,
its clip and its transform, so a mesh lands in the same place and the
same order as the pen's own shapes and nothing has to go through
`Pen::canvas` to put one down.

The pen's FILL is what paints it. Where the mesh carries its own corner
colours and the fill is a plain colour, the corners paint it and the
fill's colour stands aside — which is the same rule `Pen::vertex` follows
when the corners disagree. Where the fill is a material, the material
paints the whole mesh and the corner colours are not read; fit it with
`SHAPE` and its unit square is the mesh's own bounds.

A mesh has no outline, so it is not stroked and it adds nothing to a clip
mask being recorded — a `Pen::line` and a `Pen::image` add nothing for
the same reason. Building the mesh is Skia's business: the verb takes one
and asks no questions about how it was made.

`Pen::shape` is THE PEN'S OWN SHAPE VERB: a silhouette — a geometry kit
value, or anything with a path over a size — fitted to the box the rect
mode reads from the four numbers, filled and stroked as a rect is. The
overload taking a path draws it as it stands.

### The clip

`Pen::clip` is p5's CLIP: the callable draws the mask, and everything
drawn after it is confined to what the callable covered.

```cpp
pen.clip([&] { pen.circle(100, 100, 80); });
pen.image(photo, 60, 60);            // a round photo
```

NOTHING THE CALLABLE DRAWS LANDS ON THE CANVAS. Its shape verbs are
recorded into one path instead — a rect, an ellipse, an arc, a triangle,
a quad, a bezier, a curve, a silhouette, a point, and a `Pen::beginShape`
run of any kind — each in the space it was called in, so a
`Pen::translate` inside it moves the mask with it. The verbs that carry
no outline add nothing: a line, an image, a text, a background.

IT LASTS UNTIL THE MATCHING `Pen::pop`, and to the end of the frame when
it was set outside any `Pen::push`, which is p5's own scoping — so a
masked passage is a push, a clip, the drawing, and a pop. A clip inside a
clip keeps only what falls in both.

### A retained guest

`Pen::element` is THE OTHER WAY THROUGH THE DOOR. Something another
library keeps between frames is painted inside a box on this frame: laid
out, reconciled and cached by its own library, with this pen lending it
the canvas, the transform above the box and the clock. The guest is told
apart by the call site, so a loop that paints several passes distinguishes
them by the index. The pen's clock is what the guest's clock is stepped
by, so a guest advances on the frames it is painted and stands still on
the frames it is not.

### The paints, for a guest that draws with them

`Pen::fillPaint` is the fill as an `SkPaint`, resolved against the CANVAS
for this frame.

NULL UNDER `Pen::noFill`, and that is the whole answer: no fill means
there is no fill to hand over, not that the pen has a colourless one. A
caller through the canvas door therefore checks before it dereferences —
`if (const SkPaint* fill = pen.fillPaint())` — exactly as every verb in
the class does. The pen's blend, its antialiasing and its dash live on
these paints, so under no fill there is nowhere for them to be read from
either; take them off `Pen::strokePaint`, or set a fill.

A material fitted to the shape has no shape there — that is the
canvas-framed resolve, since a caller asking for the paint has not named
a box. `Pen::strokePaint` is the stroke on the same rule, and null when
`Pen::noStroke` holds or the weight is zero.

## See also

`Graphics`, `on`, `NoiseField`, `Constant`
