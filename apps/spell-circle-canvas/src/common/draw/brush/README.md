# SigilDrawBrush — natural media over the pen

The brush chapter of SigilDraw: `README.md` one directory up is the
library's own page, and this is the part of it that is brushes.

A natural-media mark has five independent parts: DEVICE INPUT, an evenly
spaced stream of DABS, a TOOL that says how each dab lands, optionally a
FIELD that bends the centreline, and a polygonal SURFACE that receives a
wash, a hatch or a mass of gestures. `SigilDrawBrush` keeps each as its
own value under `sigil::draw::brush`, so one path can be tried with five
tools and one polygon can receive several interiors. Selection lives in
an explicit `Engine`, never in a process global.

```cpp
#include <sigildraw/brush/Brush.h>

namespace brush = sigil::draw::brush;

brush::Engine brushes;                       // a member of the sketch
brushes.scaleBrushes(2);
brushes.set("HB", {0.10f, 0.18f, 0.28f, 1}, 1.4f);
brushes.wiggle(3);
brushes.line(pen, {30, 220}, {570, 220});

brush::Tool lead = brush::pencil({0.1f, 0.1f, 0.12f, 1}, 2.0f);
brush::line(pen, lead, {30, 260}, {570, 262}, 0.9f, 0.2f);
```

Two conventions run through the whole library. **Angles are the pen's**:
clockwise-positive on the y-down canvas, as `pen.rotate` turns, so
`pen.arc` and `brushes.arc` sweep the same quadrant and a sketch's own
field agrees with the stock ones. A scalar angle passed beside a pen —
`flowLine`, `arc`, `move`, `endStroke`, the scalar `hatch` — is in the
pen's angle mode; an angle inside a value (`Hatch`, `Wash`, `Plot`, a
field's answer) is radians. **The clock is the pen's**: every engine verb
that takes a pen reads its field at `pen.millis()`, so a time-varying
field moves with the frame and stands still on a plate.

### Tools

| word | what it is |
| --- | --- |
| `Tool` | one tool, plain data: tip, colour, width, spacing, opacity, scatter, density, bristles, pressure envelope, blend, rotation, aspect, the jitters, the speed, pressure and tilt responses, `sharpness`, `noise`, `markerTip`, a shape source, a grain source, the dynamics, a custom tip |
| `Tip` | `Dust` (dry particles around the centreline), `Fibres` (parallel hairs, intermittently dry), `Nib` (one pressure-width mark), `Scatter` (particles around each dab), `Image` (the tool's shape source stamped per dab), `Custom` (a callback per dab) |
| `Rotation` | how a shape or custom tip turns: `Fixed`, `Natural` (with the heading), `Random`, `Tilt` (with the stylus azimuth) |
| `Pressure` | the envelope along a stroke: a three-point start/middle/end, or a bell (`gaussianProfile`), or a caller's `curve`; `variation` and the bell's jitters are re-rolled per stroke by `prepareStroke`, which is what makes two strokes with one tool differ |
| `pencil`, `charcoal`, `marker`, `watercolor`, `spray` | stock values; every field stays public |
| `Catalogue` | named tools; `Catalogue::stock()` holds `2B`, `HB`, `2H`, `cpencil`, `pen`, `rotring`, `spray`, `marker`, `marker2`, `charcoal`, `hatch_brush`, `pastel`, `crayon`; `scale` multiplies width, scatter AND spacing |
| `weightedChoice(pen, {{value, weight}…})` | a value in proportion to its weight from the pen's stream; empty answers nothing |

Opacity is the tool's load and the colour's own alpha multiplies it.
Density is how much a dry tip deposits: the probability a dust particle
lands and the share of fibres and scatter particles that deposit, so a
value above one only lets a light pressure keep depositing. A custom tip is called with the pen
translated to the dab, rotated to its angle, scaled to its size and
aspect, the pigment as fill and stroke, and the default rect and ellipse
modes; the transform is restored after every dab, and those four style
words are reset before the next.

### Dabs and deposition

| word | what it is |
| --- | --- |
| `Input` | one device observation: position, pressure, tilt (0 upright, 1 flat), barrel rotation, seconds on the host's clock, tilt direction |
| `Dab` | one deposition event: position, pressure, tilt, barrel, direction, speed, distance, unit progress, tilt direction |
| `Sampler` | live input resampled one spacing apart on `geometry::path::Stride`, which carries the unspent part of an interval across events; speed through a first-order filter of `kSpeedFilterSeconds`; the first dab is held until the first movement gives it a heading, and a stroke that never moves is one dab at direction zero |
| `dabs(input, spacing)` | a whole recorded path resampled, with progress assigned |
| `deposit(pen, tool, dabs, options)` | THE EXECUTOR SEAM: a stored path, a live stylus and generated geometry all reach it. Dust, nib and scatter dabs go down as one sprite batch per stroke; fibres, shape and custom tips and the SUBTRACT blend draw through the pen's verbs dab by dab. `markerTip` pools pigment at the ends the options name; a standing grain puts the whole run of dabs in one layer and takes its coverage out of that. A fibre stroke under a blend that is not a fixed function of source and destination — MULTIPLY and the rest past Skia's coefficient modes — lays its hairs in a layer of its own bounded by the marks, at plain source-over, and composites once under the tool's blend, so the hairs meet each other rather than the canvas one at a time |
| `spacingOf(tool)` | how far apart the tool lays its dabs, in canvas units. Every resampling in the library asks this rather than reading `spacing`, because a tool with a shape states its spacing against the stamp |
| `paint(pen, tool, stroke)` | rolls the tool's randomness once, then deposits along a stroke. The dabs carry no speed: a stored path has no clock, so `speedSize` and `speedOpacity` act on live input only |
| `line`, `spline`, `flowLine` | the conveniences over `segment`, `spline` and `trace` |

Deposition pushes and pops the pen around the mark, so the style and
transform it found are restored while the transform still moves the
mark. The round tips' sprite is promoted to a texture once per pen and
kept in the pen's `Retained` store.

### Strokes and fields

| word | what it is |
| --- | --- |
| `Sample`, `Stroke` | `{position, pressure}` and a vector of them: reusable geometry, painted by any tool. The pressure is the LANE a `geometry::path::Polyline` carries, which is why every resampling below interpolates it without being told to |
| `segment(from, to, spacing, p0, p1)` | a straight centreline with a linear pressure ramp: `path::subdivide`, so no step is longer than the spacing |
| `spline(controls, spacing, curvature)` | `path::catmullRom` through the controls, blended toward the chord by `1 − curvature`, pressure interpolated |
| `Direction`, `DirectionField` | the field seam: anything answering a heading in radians for `(SkPoint, float seconds)` |
| `trace(start, length, spacing, seconds, field)` | integrates a start through a field |
| `warp(polygon, spacing, amount, seconds, field)` | a polygon subdivided and displaced along the field, closed |
| `Curl`, `Vortex`, `Wave` | stock fields as values; `Curl` owns its own seeded noise, so two curls with one seed agree whichever pen paints them |
| `stockFields()` | the seven named fields an engine starts with: `hand`, `curved`, `zigzag`, `waves`, `seabed`, `spiral`, `columns` |

### Interiors

| word | what it is |
| --- | --- |
| `Hatch`, `hatch(pen, tool, polygon, style)` | `path::lattice` at `angle` radians and `spacing`, cut to the even-odd interior so holes are skipped; `jitter` moves each mark's ends after the cut, by up to twice that fraction of the spacing, so a jittered mark may cross the edge; `gradient` opens or crowds the lattice's gaps by a tenth per lane; `continuous` joins the marks into one serpentine line. Every mark is thinned at both ends |
| `Wash`, `wash(pen, pigment, polygon)` | a wet interior: `layers` translucent deposits, each the polygon's edge pushed out by a gaussian of the `bleed` and rippled by noise, blooms lifted out and grains settled in by `texture`, pigment gathered at the edge by `border`, the whole composited once with `blend`. It is built in one layer on the pen's canvas, so its pixels are wherever the pen's are |
| `Mass`, `mass(pen, tool, polygon, style)` | chords across the shape at the tool's scatter, each bent into an arc around a pivot outside it and painted only where the arc stays inside; `strength` sets one to three passes, later passes displaced by up to twice the scatter; `precision` steadies the hand — narrower lane jitter, less wobble on each arc; `outline` finishes the boundary |

### Stored geometry

| word | what it is |
| --- | --- |
| `Polygon` | vertices, the whole of its state; `intersect(line)` (`path::edgeCrossings`, nearest the line's start first), `translated`, and `draw`/`fill`/`wash`/`hatch`/`mass` with a tool or through an engine; `show` is every active interior in the engine's order |
| `Plot` | a path by turns: `addSegment(angle, length, pressure)`, `endPlot`, `rotate`; `angle(distance)` and `pressure(distance)`; `path(origin, spacing, curvature, scale)` and `polygon(x, y, …)` place it anywhere at any scale. `fromStroke` records a stroke's turns relative to its first sample. A plot is always relative |
| `PlacedPlot` | a plot and the origin it was first drawn at — what the engine's `circle`, `arc`, `spline` and `endShape` answer |
| `Position` | a cursor: `moveTo(direction, length, step)` walks with its field's answer added to the direction, `plotTo(plot, length, step, scale)` walks a plot's headings; `plotted()` accumulates; with bounds it stops once it has left them by half their size |
| `hatchArray`, `massArray` | one gesture through an even-odd collection: the first polygon is the boundary, the rest cut holes or stand as islands |

The geometry an engine answers is the geometry as sampled, before the
field bent it.

### The engine

`Engine` owns a catalogue, the selected tool with its colour and weight,
a pigment wash and a flat wash (independent, both can be active), a
hatch with an optional dedicated tool, a mass with its tool, a field with
its influence, and a clip; `push`/`pop` save and restore all of it. A
name lookup — `add`, `pick`, `set`, `hatchStyle`, `mass` — answers the
tool it found or null; `field` answers whether the name is known; every
other setter and verb answers nothing.

| verb | what it does |
| --- | --- |
| `set(name, colour, weight)`, `pick`, `stroke`, `noStroke`, `strokeWeight`, `tool()` | the selection; the weight scales width and scatter |
| `fill(colour, opacity)`, `fillBleed`, `fillTexture`, `noFill` | the pigment wash (`Wash`) |
| `wash(colour, opacity)`, `noWash` | the flat wash |
| `hatch(Hatch)`, `hatch(pen, spacing, angle, …)`, `hatchStyle`, `noHatch` | the hatch; the value's angle is radians, the scalar's is the pen's mode; until `hatchStyle`, the selected tool hatches |
| `mass(name, colour, Mass)`, `noMass` | the mass and its tool |
| `field(name)`, `addField(name, field, units)`, `listFields`, `noField`, `wiggle(amount)` | the field; `wiggle` selects `hand` and scales its influence |
| `clip(rect)`, `clip(pen, rect)`, `noClip` | a rectangle every mark, interior and outline is confined to; with a pen, captured in the pen's space at the call and applied there whatever the transform is later — for that canvas |
| `paint`, `line`, `flowLine`, `spline` | strokes with the selected tool through the field; `spline` answers its plot |
| `polygon`, `rect(…, mode)`, `rect(…, radius)`, `circle(…, irregularity)`, `arc`, `beginShape`/`vertex`/`endShape` | surfaces: wash, fill, mass, hatch and the outline in that order, all under the clip, all through one bent boundary; `rect` takes p5's `CORNER`, `CORNERS` or `CENTER` |
| `draw`/`fill`/`wash`/`hatch`/`mass(pen, Polygon)` and `(pen, Plot, x, y, scale)` | one interior over stored geometry |
| `hatchArray`, `massArray` | over a collection |
| `position(x, y)`, `position(pen, x, y)` | a cursor through the field; with a pen, at the pen's clock and bounded by the canvas |
| `beginInput`, `moveInput`, `endInput`, `cancelInput` | live input through the sampler and the executor; nothing is deposited before the first movement, and the tool's randomness is rolled once at `beginInput` |
| `beginStroke(kind, at)`, `move(pen, angle, length, pressure)`, `endStroke(pen, angle)`, `cancelStroke` | a stroke by turns, in the pen's angle mode |

A closed shape's interiors and outline come from one boundary: the
polygon is resampled at a fixed step, bent through the field once, and
the drift the bends accumulate is taken back out along the way, so the
outline sits on the wash's edge and the shape closes. Two engines draw
through one pen without seeing each other's state.

### Custom brushes

A tool built from pictures is the brush a painting program means by the
word: an artwork stamped along the stroke, a texture the mark is laid
through, and curves the device drives. Three values on `Tool` say it,
and a tool that sets none of them behaves exactly as a tool without
them.

| word | what it is |
| --- | --- |
| `Shape` | the SHAPE SOURCE: the artwork stamped at every dab, its `mask` saying whether coverage is the alpha channel or one minus the luminance — so dark artwork on white works as drawn — plus `spacing`, `scatter` and `angleJitter`. Those three are FRACTIONS OF THE STAMP, not canvas units, which is how a brush that travels between programs states them: a `spacing` of a tenth is a dense continuous mark and one is a chain of separate stamps |
| `Grain` | the GRAIN SOURCE: a texture tiled in both axes whose LUMINANCE is coverage — the mark survives where the texture is white and is taken away where it is black — with `scale` in the pen's space and `depth` for how much may be taken |
| `GrainSpace` | `Stroke`, the texture fixed in the pen's space, so two marks crossing one place meet one surface; `Dab`, the texture riding each stamp, turning and travelling with it. A dab-space grain needs a stamp to ride, so it applies to a shape tip; every other tip deposits as one sprite batch and takes its grain standing still whichever space it asks for |
| `Curve` | one response curve, `minimum` at zero to `maximum` at one with a `bend` between them, or a caller's own function. The answer is a MULTIPLIER on what the tool already decided, so a flat curve at one changes nothing |
| `Drive` | what a curve reads, each arriving as a unit value: `Pressure` (the stylus with the tool's envelope already applied), `Velocity` (one at `speedReference` units per second and above), `Tilt` |
| `Dynamics` | a `Response` — a curve and its drive — for `size`, `opacity` and `flow`. Opacity is the tool's load and flow is the one dab's; nothing buffers a dab before the canvas here, so the two multiply into the same alpha, and they are separate because one may follow the stylus while the other follows the hand |

```cpp
brush::Tool ink = brush::marker({0.1f, 0.1f, 0.12f, 1}, 26.0f);
ink.tip = brush::Tip::Image;
ink.shape = brush::Shape{.image = tipArtwork, .spacing = 0.08f,
                         .scatter = 0.15f, .angleJitter = 0.4f};
ink.grain = brush::Grain{.image = paperTexture, .scale = 1.5f, .depth = 0.7f};
ink.dynamics.size =
    brush::Response{.drive = brush::Drive::Pressure,
                    .curve = {.minimum = 0.25f, .maximum = 1.0f}};
brush::line(pen, ink, {30, 200}, {570, 240});
```

**The native format is a directory.** `<name>.sigilbrush/` holds
`brush.json`, `shape.png` and an optional `grain.png`, so the artwork
stays a picture a painting program can open and edit in place and the
numbers stay a text file a person can read. `brush.json` names every value
of the tool that is a number, a flag or a word — the envelope under
`pressure`, with its optional `gaussian` and `variation` — beside a
`shape`, a `grain` and a `dynamics` object of up to three responses;
every key it leaves out keeps the library's default. What it does not
carry is what a text file cannot: the two pictures, which sit beside it,
and the callables a caller writes in code — a pressure curve, a response
curve, a custom tip. `format::encodeBrush(tool)` writes that text back,
and a tool through it and back is the tool that went in.

**Loading is `SigilDrawBrushFormat`, and it never opens a file.**
Everything there takes bytes — from a hub, a fixture or a caller's own
array — and the pictures inside them are decoded by SigilImage:

```cpp
#include <sigildraw/brush/format/Load.h>
namespace format = sigil::draw::brush::format;

hub.registerDecoder<brush::Tool>(format::BrushDecoder{});
auto ink = format::loadBrush(hub, "res://brushes/ink.sigilbrush");   // a directory
auto abr = hub.load<brush::Tool>("res://brushes/library.abr");       // one file
```

A directory has no bytes of its own, which is why loading one goes
through a byte source: the three parts are three ordinary resources
under it. Anything that is ONE file — a packed `.sigilbrush` archive, a
Photoshop `.abr`, a Procreate `.brush` — is what `decodeBrush` sniffs and
what a hub's registered decoder runs, so one decoder answers for every
form a brush arrives as.

**The two importers are bounded, and each header states its bounds.**
`decodePhotoshopBrushes` reads `.abr` versions 6, 7 and 10 and answers
every SAMPLED tip's bitmap, raw or PackBits-compressed, 8 or 16 bits
deep. It does not parse the Photoshop DESCRIPTOR those versions keep the
names, spacing, scattering, shape dynamics, texture, dual brush and
transfer in, and computed brushes — the ones with no bitmap — are left
out; an imported tool is its shape and this library's defaults for
everything else. `decodeProcreateBrush` reads the shape and grain
pictures out of the zip a `.brush` is. It does not read `Brush.archive`,
the NSKeyedArchiver property list in Apple's binary encoding that holds
every number the brush states, so the same applies: the pictures are the
file's and the numbers are this library's.

### Sketches

`brush_live_tutorial` moves through field lines, the stock-tool wheel,
overlapping hatches, accumulating watercolor and pressure-bearing splines
in one authoring example. `brush_engine_atlas` and `brush_dynamics` are
compact plates for tool definitions and stylus input, and `brush_custom`
is the one for a tool built from one shape source and one grain source.
`brush_rain`,
`brushwork_currents` and `brush_botanical_study` use the same parts in
complete compositions. `bristle_bloom` and `bristle_current` are
lower-level companion studies that build brush bundles from the pen's
curves, lines and blend modes with no brush library in them.
