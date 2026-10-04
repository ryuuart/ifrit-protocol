# Macrodata Refinement workstation

A television study of the *Severance* Macrodata Refinement terminal,
authored in Python through native Compose at 1800 × 1280. The screen
sits to the left inside a deep navy molding, with a broad right front
panel, an ivory enclosure, recessed fasteners, a lower-right control,
stand, desk and a separately modeled blue keyboard and trackball.

The cyan interface contains a wireframe Lumon globe, segmented completion
rail, 242 individually drifting digits, an enlarged seven-digit selection,
five numbered bins, percentage bars and a hexadecimal status address.
A complete 26-second transaction removes the selected digits, carries
them along a curved collection path, classifies them in bin 03, commits
the totals and replenishes the field.

## Inspected references

The [production-screen photograph reproduced by Nerdist](https://cdn.nerdist.com/wp-content/uploads/2025/01/17104528/Severance-Computer.jpg)
was visually inspected. It establishes the offset CRT, wide navy right
panel, rounded deep bezel, ivory outer edge, front fastener and low right
control. Its screen establishes the deep blue field, cyan digits, Cold
Harbor heading, segmented completion rail, Lumon globe, grown selected
digits, five bins and the hexadecimal address. The study preserves that
hierarchy and the photographed initial 67% completion and 77%, 73%, 59%,
52%, 75% bin values. The photograph is a television-production image reproduced
by a secondary host, rather than an original design specification.

The [Figma interview with production designer Jeremy Hindle](https://www.figma.com/blog/free-association-jeremy-hindle/)
includes a room frame that was also visually inspected. Its sterile white
room and green office palette inform the pale desk and restrained setting.
The [Motion Picture Association interview](https://www.motionpictures.org/2022/06/severance-production-designer-jeremy-hindle/)
and [Film Independent interview](https://www.filmindependent.org/blog/creating-the-singular-disquieting-aesthetic-of-severance-with-emmy-winning-production-designer-jeremy-hindle/)
describe the original computers, keypads, trackballs and functioning
screen programs. These are primary production-design accounts. They
support treating the workstation as purpose-built equipment; they do not
supply the exact geometry or program reproduced here.

The numeric file, selected values, movement, collection path, four-temper
drawer, balance values, commit and replenishment are authored study
content. The blue keyboard, its key arrangement, trackball details,
vent-like side scoring, case wear and materials are authored adaptations.
Menlo and Helvetica Neue interpret the observed monospaced numeric field
and heavier headings; the production typefaces have not been identified.
This is a reference-informed workstation study, not a frame reconstruction
or a functional reproduction of the fictional application.

## Native craft

The cabinet uses native gradient materials, a subtle stock noise layer,
rough surfaces, stock bevel filters and native studio lighting. Separate
outer ivory, navy front and screen molding silhouettes give the case its
depth. Cached native pen geometry supplies recessed hardware, transfer
marks, the support, keyboard case, sculpted keycaps and trackball sphere.
Native text supplies the legends. No photographic or generated bitmap
is imported into the composition.

The screen hierarchy remains restrained. Rules and globe are cached
native pen leaves; headings, digits, bins, addresses and balances are
retained native text and boxes. Each field digit has a seeded phase and
period, using native motion bindings for small independent horizontal and
vertical drifts. Selected digits have a separate scale binding and remain
large until consumed. The original cells fade out and stay empty while
the packet travels. Replacement values enter later at those same cells.

The packet contains the same seven selected values. It uses a native
cubic path with native path travel and scale. The drawer and progress
bars use bound opacity and transforms. Static cabinet, keyboard, globe,
rules, cursor and glass geometry are retained separately from the
changing screen. The frame update writes native scalar clocks and
phase values; it does not rebuild the scene or issue Python drawing
callbacks.

Native bloom supplies a narrow phosphor halo. An original runtime filter
warps the full interface, applies horizontal beam jitter, slight color
registration, scan rows, restrained grain and edge falloff. Native glass
gradients, a thin reflection, surface scratches and a rounded clipping
boundary remain distinct from that optical screen pass. The case stays
outside the CRT filter, with a separate soft cyan spill.

## Motion cycle

The transaction is an authored deterministic sequence driven by scene
time:

| Time | Visible event |
| --- | --- |
| 0–3.6 s | The pointer approaches the cluster; seven digits grow. |
| 3.6–6.25 s | The enlarged selection is held. |
| 6.25–9 s | Selected cells empty; the same digits move and shrink into bin 03. |
| 8.4–12.15 s | The temper drawer opens; its bars fill; 67% becomes 68%, and bin 03 changes from 59% to 64%. |
| 13.3–16.8 s | The drawer closes and replacement digits refill the emptied cells. |
| 16.8–24.8 s | The replenished field and committed values are held. |
| 24.8–26 s | The screen dims slightly before the demonstration resets. |

Small numeric drifts and beam variation continue on their own periods.
The values and timing illustrate an interaction narrative; they do not
claim to reproduce the show's data-processing rules.

## Render

From `apps/spell-circle-canvas`:

```sh
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/severance_macrodata/severance_macrodata.py \
  --frame /tmp/severance-selection.png --at 4.6 \
  --state /tmp/severance-study-state
```

Use 7.6 seconds for the moving packet, 12.8 for the committed drawer,
and 18 for the replenished field. Give concurrent captures separate
state directories and output paths. Reload the session after editing
`Tube.sksl`, which is compiled during setup.

File `--frame` captures use raster Skia for this 2D Compose canvas, even
when `--gpu` initializes a device. For the actual interactive Graphite
backend, use a bounded file window with `--gpu --window-bench 3`. A
separate `--gpu --shot /tmp/severance-window.png` captures the study's own
window and exits. Device access requires the host's normal GPU permissions.

For the full native motion sequence:

```sh
mkdir -p /tmp/severance-frames
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/severance_macrodata/severance_macrodata.py \
  --frame /tmp/severance-frames/frame.png --frames 312 --fps 12 --at 0 \
  --state /tmp/severance-motion-state
ffmpeg -framerate 12 -start_number 1 \
  -i /tmp/severance-frames/frame_%04d.png \
  -c:v libx264 -crf 17 -pix_fmt yuv420p -movflags +faststart \
  /tmp/severance-motion.mp4
```

The movie is encoded only from the emitted native frames.

## Authoring observations

Retained native geometry, stock material surfaces, native bevels, text,
motion bindings and path travel express the workstation and complete
transaction without a sketch-local renderer or interpolation engine.
The shader is an optical treatment over native interface content, rather
than a bitmap mockup.

The layout overflow method accepts the exported `Overflow.Clip` enum.
Passing `"clip"` raises an argument-type error, while the neighboring
`alignItems("center")` and `justifyContent("space_between")` categorical
forms work in a live-host probe. Consistent string convenience would
reduce interruptions while authoring. This is an API request, not a
rendering defect; a regression should compare both forms and reject an
unknown string with a clear value error.

Bloom can express the intended narrow halo, but authoring it involves
several coupled controls. A preview using the same implementation as the
renderer, with documented starting values for luminous text, would make
subtle tuning easier. The supported bloom is used here without replacing
it with a shader implementation.

The file video lane currently dispatches through a registry selection
rather than opening the supplied file. A live ordinary-file probe exits
with `selection holds a set`. A file-specific export route would complete
the authoring workflow. A regression should encode the explicitly supplied
file, or reject it clearly before dispatching a registry montage. The
supported numbered-frame lane supplies the movie for this study.

## Verification

The native Python host captured and visually verified the refined
selection at 4.6 seconds, traveling packet at 7.6 and committed drawer
at 12.8. Header rules, digits, bin labels and the address stay inside the
rounded tube; the cabinet and keyboard preserve their separate material
layers. The selection values match the packet, and consumed cells remain
empty while the drawer commits. These captures are raster evidence:
`/private/tmp/severance-redo-refined-4-6.png`,
`/private/tmp/severance-redo-packet-7-6.png`, and
`/private/tmp/severance-redo-classified-12-8.png`.

The bounded native file-window run and its separate own-window screenshot
completed with `renderer: Graphite GPU` and no shader or runtime errors.
The window image was visually inspected with its Graphite footer visible.
Evidence is `/private/tmp/severance-redo-graphite-window.png`,
`/private/tmp/severance-redo-window.log`, and
`/private/tmp/severance-redo-shot.log`.

The full sequence rendered 312 native frames and encoded as a 26-second,
1800 × 1280 H.264 clip at 12 frames per second. The replenished field at
18 seconds and final dimmed hold were visually inspected from the emitted
frames. The movie is `build/media-study-redo/severance-motion.mp4`, with
render and encode logs beside it. The movie cadence is an export choice,
not a measurement of interactive playback.
