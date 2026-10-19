# Findings

## A sized light's highlight is a representative point's, not the lobe's integral

`src/common/world/diligent/shaders/Surface.slang` lights the specular side
of a rect, sphere or disk light from the point of the light nearest the
reflected ray, with the lobe widened by the solid angle the light fills
(`areaNormalization` in `src/common/material/core/shaders/Shading.slang`).
The outline a mirror shows is exact; the brightness on a rough surface is
an estimate, the highlight of a light partly below the horizon is lit as
if all of it were above, and the rectangle's highlight on a rough surface
is rounder than the lobe integrated over the rectangle would be. The
intended treatment for a rect is linearly transformed cosines: two tables,
fitted on the host by roughness and viewing angle the way the split-sum
table is integrated, mapping the GGX lobe onto a clamped cosine so the
rectangle clipped to the horizon is integrated against it exactly. A test
should compare the rect's specular response on a plate at roughness 0.3,
0.6 and 0.9 against a direct numerical integral of the lobe over the
rectangle, within a stated bound, and the fitted tables against direct
integration at their texel centres.

## A sized light's sheen and brushing are simplified

Under a rect, sphere or disk light a sheen is lit as by a point light at
the light's centre carrying the whole light, and a brushing keeps the
isotropic representative point and stretches the lobe there; neither
follows the light's shape. A brushed strip aligned with the brushing
already shows a longer highlight than one across it, and a cloth plate
under a narrow and a wide grazing panel show nearly the same profile, so
no image test tells the shape-following treatment from today's; a
discriminating test needs a numerical integral of the lobe over the
light, as the representative-point entry states.

## A casting light's shadow filter and tint cost a lit pixel in proportion to its program's size

The lens, the layered refraction and the environment cost a transmitting
pixel little, and dispersion's three readings are the cost of the
picture; the disproportion is the casting light. The shadow filter and
the tint read cost a transmitting or every-lobe surface about three times
what they cost an opaque one, following the program's size rather than
any transmission work, so a pane under a casting light pays more for its
shadow than for its glass. A test should time the seen phase of a
full-frame pane under a casting light against the same pane under a light
that casts nothing, and find the shadow's share within a stated bound of
the opaque card's.

## A leaf set in a whole text style ignores its own colour ink

A leaf built with a whole `TextStyle`, `text(utf8, style)`, draws in the
style's colour when given `.ink(Color)` or a colour material, while a
non-colour material ink applies; one rule should hold for both kinds of
ink, so that a clear copy of a leaf can be written the same way whatever
its ink is. A test should give such a leaf a clear colour ink and find
it drawing nothing.

## A bevel or inner shadow on a glyph or shape boundary blurs its band on every replay

Only `Boundary::Coverage` holds its band masks (`HeldCoverageMasks`); a
bevel or inner shadow on a glyph or shape boundary blurs its band again
at every replay, so a stationary edge treatment beside a changing ink is
cut again on each light-only update. Holding the masks for every outline
reuses the edge coverage across ink changes; every bevel plate moves
when it lands. A test should replay a bevelled leaf under a changing
shader ink and find the band blurred once.

## Texture sets are unbound in Python

`src/common/python/material/TextureSets.cpp` is an empty stub, so
`material::TextureMaps`, `Volume` and `fromFiles` have no Python
spelling, while every other material door is bound. A Python author
should load a texture set from files and wear it as C++ does. A test
should build a texture set from the demo assets in Python and read its
maps back.

## A few kit parts and the typeface door still answer the old types

`Ruler.tickLine` (a Python callable as the part), `kit::Channel`,
`specimenTheme`, and the compose kit's `table` and `bars` are unbound,
and `weave.typeface(family, weight, italic)` answers a `skia.Typeface`
where the rest of the package now answers a `weave.Face`. Each should
reach Python in the same shape as its C++ twin, with a test per name.

## A sketch's own runtime effects build on its first frame

A program over an effect made while the sketch's setup runs, such as
`crt_bloom`'s overlay made after the Graphite context exists, carries no
key, so no recorded key set can stand it up and `crt_bloom` builds two
programs on every open. The host is meant to stand up every program a
sketch's first frame needs. A test should open `crt_bloom` twice and
find the second open's first frame building no program.

## A raw effect program cannot state a sampling reach

`Filter.program` in Python and `material::skia::program` in C++ always
pass a sampling radius of zero, so a body that reads its neighbours
reads undefined texture slack past the layer's right and bottom edge,
which is why `nostromo_monitor` states its reach through `Filter.of`
instead. The raw-effect door should state a reach as `Filter.of` does.
A test should apply a neighbour-reading program through it with a stated
reach and capture byte-identically twice.

## Scry inserts its own recording without the recorder's waits

`src/common/scry/gpu/metal/MetalInterop.mm` calls `insertRecording`
itself, as the compose, scry and media benches do, so the waits
registered on the recorder are not attached to those inserts; every
insert should go through `GraphiteContext::submit`. A test should insert
a scry page after a registered wait and find `pendingWaits` at zero.

## The sparkle stretched over a passage is one streak, not points of light

An ink maps its unit square onto the passage's box and the program
cannot see that box's aspect, so on a wide word a sparkle cell spans
about a third of the word and a box holds about ten cells; the SIGIL
wordmark shows one streak across its S where points of light across the
word were meant. The ink would have to hand the program its box size,
which `TextInk.cpp` pins to one by one; that is core or material work,
not the kit's. A test should ink the SIGIL wordmark with the sparkle at
its capture moment and find at least three separate lit regions on at
least three glyphs.

## The kit text paint has no Python spelling

`compose::kit::textPaint` and its `TextPaint` record are new in the kit
and unbound, so the ledger's `text_paints` entry requires it and the
sketch cannot be written in Python. A test should ink a word with
`kit.textPaint(field=kit.TextField.Sparkle, time=6.4)` from Python and
find it painted.

## Held coverage masks are remade on the CPU at each settle

`HeldCoverageMasks` remakes a part's cast mask on the CPU raster, blur
included, whenever the density changes, so every zoom settle
re-rasterises and re-blurs each held mask at full size on the processor
even on a device, where the mask should be made and blurred on the
device. A settle-style Graphite arm of `BM_Draw_CoverageSheet_TurningChild`
that alternates density should cost within a stated ratio of the steady
arm; the change moves every device plate that uses held coverage.

## A pass lane misses the canvas scale in the outline route

A `textFx::pass` lane records into its own picture with an identity
canvas, and the outline route reads only the replay scale there, so a
driven letter inside a pass under a canvas zoom can stay in the atlas
past the device size where it should draw as an outline; the lane's
batches should read the full device scale, replay times canvas. A test
should put a driven pass lane under a canvas scale past the threshold
and find its picture reached as outlines.

