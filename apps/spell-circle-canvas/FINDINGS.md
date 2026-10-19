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

## A bevel or inner shadow on a glyph or shape boundary blurs its band on every replay

Only `Boundary::Coverage` holds its band masks (`HeldCoverageMasks`); a
bevel or inner shadow on a glyph or shape boundary blurs its band again
at every replay, so a stationary edge treatment beside a changing ink is
cut again on each light-only update. Holding the masks for every outline
reuses the edge coverage across ink changes; every bevel plate moves
when it lands. A test should replay a bevelled leaf under a changing
shader ink and find the band blurred once.

## A sketch's own runtime effects build on its first frame

A program over an effect made while the sketch's setup runs, such as
`crt_bloom`'s overlay made after the Graphite context exists, carries no
key, so no recorded key set can stand it up and `crt_bloom` builds two
programs on every open. The host is meant to stand up every program a
sketch's first frame needs. A test should open `crt_bloom` twice and
find the second open's first frame building no program.

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

## Held coverage masks are remade on the CPU at each settle

`HeldCoverageMasks` remakes a part's cast mask on the CPU raster, blur
included, whenever the density changes, so every zoom settle
re-rasterises and re-blurs each held mask at full size on the processor
even on a device, where the mask should be made and blurred on the
device. A settle-style Graphite arm of `BM_Draw_CoverageSheet_TurningChild`
that alternates density should cost within a stated ratio of the steady
arm; the change moves every device plate that uses held coverage.

## The shadow tint read costs glass several times what it costs an opaque surface

With a pane over a card under a sun that casts, the tint read alone
costs the pane's seen phase about five times what it costs the opaque
card behind the same glass, while the filter alone costs the pane in
proportion to its pixel; every lit program now reads its shadows once
per pixel (`shadowSharesAt` in `Surface.slang`), so the program's size
is not the cause. The tint should cost what two texture reads cost, in
proportion to the pixel. On the `BM_ShadowShareOnDevice` glass arm the
share with the tint read should exceed the share without it by no more
than the card's own tint share times the pane-to-card ratio of their
unshadowed costs.

## A frame whose casting light passes through glass redraws its shadow stage every frame

With a still scene, the shadow stage is drawn on every timed frame in
the glass-and-casting bench arm, while every other arm keeps its map
after the first frame. A map, and its tint, that nothing moved in is
kept. `Shadow.AStillSetDrawsNoShadowMapOnItsSecondFrame` should extend
to a tinted frame and find no shadow map drawn on the second frame of a
still glass-and-card scene.

## Having the shadow filter in a program costs it with no light casting

A plain card under a light that casts nothing costs measurably more
with the shadow terms compiled in than with them compiled out, so a
program pays for shadow work it never does. A frame in which no light
casts should build the program without the terms, which wants a variant
bit the renderer does not yet set. A bench arm pair of the card under
no casting light, built with and without the terms, should agree.

## The device residency still packs a primitive attribute no program reads

`MeshVertex::primitive`, the `withPrimitiveLane` choice in `meshLayout`
and `PipelineKey::primitive` fed the retired mesh painter's shader and
nothing else reads them, while `Surface.slang` still names the fifth
input. `meshLayout` should have one form per instanced and tangent
choice and the vertex no primitive field. A test should assert that.

## A mesh drawn two ways in one frame is repacked every frame

The residency keeps one packing per artefact, so the same mesh drawn
tinted through `primitiveColor` and untinted in one frame is repacked
twice every frame; it should hold one packing per artefact and lane. A
test should draw one mesh both ways and find both uploads returning held
buffers on the second frame.

## A scene3d frame has no backdrop under its bodies

A scene3d frame carries an overlay over its bodies but nothing laid
under them, so a 2D grid or picture behind a scene has no place and
python_mesh_observatory lost its grid backdrop when it became a scene3d
sketch. A frame should take a backdrop picture under the scene. A test
should assert a frame's backdrop shows where no body covers it.

## A device case and two plates are order-sensitive under a parallel run

`Scene3DDoors.PaintsATextureSceneOntoABody` (gpu) fails inside
`ctest -j8` and passes alone, and in a full device sweep The Library of
Babel and `wet_glass_console` render a few pixels differently from the
same sketch rendered alone, so a device render is reading something
another process on the same device changes. A GPU case should pass
alongside the other gpu-labelled suites and a plate should be
byte-identical alone or in a sweep; the cases should name what they
share and isolate it, or the suites should carry a resource lock.

## A sketch without a pen cannot read input

The scene2d context answers no pointer or key reading: `Composer` has
`setPointer` and `setKey` and no getter, so only a pen or a paint
program inside a node sees input, a function sketch with no `draw` reads
its input globals at rest, and its input hooks never fire; and no
protocol route feeds pointer or key input to a session, so input hooks
cannot be driven end to end. `update` and `describe` hooks should read
input too, and a session should take pointer and key input over the
protocol. A test should set a pointer on the host before a step and find
`ctx.pointer` in `update` reflecting it, and a session-level pointer
call followed by a step firing `mousePressed`.

## The workspace scan misses a file of functions

Grimoire's workspace scan (`apps/grimoire/src/Workspace.cpp`,
`entrySource`) finds only `@sketch` classes, so a file of top-level
`setup`, `draw` or `describe` functions opens by path but is not
discovered in a workspace. A test should put such a file in a workspace
and find it listed.

## `overflow("hidden")` is refused

Compose accepts only `visible` and `clip` for overflow, while CSS's most
common value is `hidden`; it should clip as `clip` does. A test should
set `overflow("hidden")` and find the content clipped.

## A JSON number that is an integer comes back as a float

`data.Json.toPython()` answers `1792390191489.0` for `1792390191489` and
`0.0` for `0`, so a recording loaded through the hub hands a sketch
different types than `json.loads` of the same text. Integers should stay
integers. A test should decode `{"n": 1}` and find `n` an `int`. The hub's
JSON decoder also reads exactly one document, so a JSON-lines recording
has no door; one value per line, or a named dialect, should exist.

## Axis tick labels for large values print in exponent form

`kit.axis` prints ticks such as 82800 as `e+04` and clips them past the
plot's left edge. Ticks should print as plain numbers inside the plot's
bounds. A test should put ticks of 82800 and 82810 on an axis and find
them printed plainly within the plot.

## Directory Python studies are missing from the parity ledger

`typing/parity.py` globs only top-level `python_*.py`, so
`python_bundled_type` and `python_market_tape` never appear in
PARITY.md; the study list should include `python_<stem>/python_<stem>.py`.

## A restarted session keeps the failed status

After a sketch fails and `Host::restartSession` succeeds, the error log
clears and frames draw again, but `status()` still names the failure;
the status should describe the session now running, in the live window
and in Grimoire's replay alike. A test should fail a frame, restart the
session, and find `status()` no longer naming the failure.

## The host is not rebuilt when a framework header changes

`Grimoire --frame` on a source-built C++ sketch refuses because a
framework header is newer than the host binary, which fails
`python_protocol_routes` until Grimoire is rebuilt by hand; the host
target should depend on the framework headers it checks, so a build of
the test's dependencies rebuilds it. A test should assert that
`python_protocol_routes` passes after building only its stated
dependencies.

## The Nostromo tube program states no reach

`Tube.sksl` in `nostromo_monitor` goes through `Filter.program` with no
sampling reach although it reads warped neighbours, now that the raw
door can state one. A test should capture the tube twice and find the
captures byte-identical.

## A composer's first draw of a text effect node can come out blank

A fresh host's first `composer.draw` drew nothing for a `textFx` effect
node until its engine was advanced or a frame drawn. The first draw
should paint the node as the second does. A test should build a fresh
composer, render a text effect node and find its first draw non-empty.

