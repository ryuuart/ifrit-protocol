# Mother wall

A frontal study of the illuminated Mother computer room in *Alien*,
authored as native retained geometry at 1800 × 1125. Six equipment banks
surround a thick molded CRT bezel. Stepped panel relief, clear acrylic
edges, black and red transfer legends, fasteners, amber incandescent
cores and broad lamp spill give the display a physical setting.

The display follows a complete 30-second scene: power rises, a dense
circuit is active, the circuit clears for a typed inquiry, the request is
held, the circuit returns, and the tube powers down. Circuit topology,
numeric labels and the inquiry are original study content.

## Inspected references

The [frontal Mother wall film frame](https://www.avpcentral.com/images/mother/mother-interface-wall-hero.webp)
was the principal visual reference. It supplies the cream nested molding,
three upper banks, flanking indicator fields, thick central bezel, black
CRT, green octagonal circuit, warm bulbs and lower access panels. This is
a film frame reproduced by a secondary host. The study redraws these
conventions; it imports none of the photograph's pixels.

The [room view with Ripley](https://www.avpcentral.com/images/mother/ripley-in-mother-room.webp)
and [production photograph with Ripley and Ash](https://www.avpcentral.com/images/mother/ripley-ash-mother.webp)
were also inspected. They establish the warm enveloping wall light and
the layered cream enclosure. The
[Mother order readout](https://www.avpcentral.com/images/mother/special-order-937.webp)
shows wide geometric green capitals and generous black space. The
study's machine glyphs were drawn for this work. They are an interpretation
of that construction, not the production typeface.

[Ron Cobb's original Nostromo bridge drawing](https://www.roncobb.net/img/filmography/05-Alien/FB-158-on-Nostromo_Control_Bridge_3-alien.jpg)
was inspected as a primary production artifact. It supports treating the
hardware as built equipment with distinct access and control surfaces.
The [original acrylic Mother wall prop](https://www.icollector.com/Alien-Mother-Computer-Room-Display-Panel_i23631607)
is documented as clear acrylic carrying applied labels; its description,
rather than an unobserved image, informs the transfer layer. The
[contemporary production account](https://www.gigerdb.com/articles/files/Mediascene_35_1979.pdf)
provides context about the ship's functional industrial construction.

The proportions, individual codes, hardware layout, circuit topology,
access story, clock periods and material wear are authored adaptations.
This is not a frame reconstruction or a functional ship circuit.

## Native craft

The wall is one cached grayscale height field. Its nested molded steps
are softened with native blur, then an original material reads neighboring
heights for normals, contact recesses, dull sheen and painted surface
variation. The stock bevel-normal primitive addresses an outer silhouette;
the interior steps here require distinct heights inside that outline.

The acrylic edge and fasteners are separate cached geometry. Native type
carries the applied black and red legends. Shape-fitted radial gradients
and native bloom supply lamp halos. Eighteen bound lamps vary independently;
the other lamp geometry is retained. The tube's green spill is a separate
layer over the enclosure.

The circuit contains eight interleaved bus sectors, nested chip traces,
contact pads and a central cross. Its geometry is cached. Twenty-four
packets use native path travel, including its arc-length parameter and
orientation. Native bloom, an original beam/curvature filter, glass sheen,
surface scoring and a corner mask act at distinct physical layers.
Only the clock and a few native phase/visibility values change per frame.

The preserved C++ experiment loads `data/MachineType.ttf` into native Weave
and uses its native typewriter entrance. `data/make_type.py` builds that original font
with FontTools. `data/glyphs.json` carries the same authored stroke data.
The canonical Python entry uses cached native vector glyphs with bound entrances,
because the current compiled Python surface lacks the native textFx
entrance and a bytes/file typeface constructor. No bitmap type is baked
or imported. The Python entry preserves the wall, shader, optical and
native path-motion composition. The C++ experiment is kept outside the
sketch registry at `build/media-study-redo/nostromo_monitor/candidate-source.cpp`.

## Render

From `apps/spell-circle-canvas`:

```sh
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/nostromo_monitor/nostromo_monitor.py \
  --frame /tmp/mother-circuit.png --at 5.5 \
  --state /tmp/mother-study-state
```

Use 21.5 seconds for the held inquiry, 12.7 for a partly typed row, and
28.8 for tube decay. Python is the sole canonical sketch entry.
Reload the session after changing either shader, which is compiled in setup.

File `--frame` captures use raster Skia for this Compose canvas. A bounded
file window with `--gpu --window-bench 3` exercises actual Graphite.

For a complete native frame sequence:

```sh
mkdir -p /tmp/mother-frames
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/nostromo_monitor/nostromo_monitor.py \
  --frame /tmp/mother-frames/frame.png --frames 300 --fps 10 --at 0 \
  --state /tmp/mother-motion-state
ffmpeg -framerate 10 -start_number 1 -i /tmp/mother-frames/frame_%04d.png \
  -vf 'pad=ceil(iw/2)*2:ceil(ih/2)*2' \
  -c:v libx264 -crf 17 -pix_fmt yuv420p -movflags +faststart \
  /tmp/mother-motion.mp4
```

The pad adds one bottom row for the encoder's even-height requirement.
The movie is made only from these native frames.

## Authoring findings

The material-to-filter conversion snapshots its uniforms. This study uses
the supported direct runtime-program filter for a live beam clock.
A binding-preserving conversion would remove that assembly step.

Python supports family-based typeface lookup, but the inspected compiled
surface does not expose a file or bytes constructor. The C++ experiment can
adapt a native Skia typeface into Weave. A resource-hub face loader shared
by both authoring languages would let a bundled font participate in the
same resource and reload contract as other assets. A regression should
load a bundled face and verify its family and glyph metrics in both hosts.

The Python textFx/typewriter gap is documented in the binding parity
chapter. Native cached glyph geometry expresses this study's entrance,
but duplicates spacing and glyph assembly that Weave already owns. Binding
the native entrance should preserve identical glyph selection, timing and
layout for the same face, text and scene clock.

The pen's default gradient fitting is canvas-wide. Lamp halos must name
shape fitting explicitly. That is a documented contract, not a rendering
bug; the first refinement corrected an authoring mistake.

## Verification

The native Python host captured and visually verified the active circuit
at 5.5 seconds, the partial inquiry at 12.7, the held inquiry at 21.5 and
the fading circuit at 28.8. Relief, lamp halos, circuit boundaries and
machine glyphs remain distinct. The final still evidence is
`/private/tmp/nostromo-redo-optical-5-5.png` and
`/private/tmp/nostromo-redo-final-inquiry-21-5.png`.

The bounded native file-window run and separate own-window capture
completed with `renderer: Graphite GPU` and no shader or runtime errors.
The window image was visually inspected with its Graphite footer visible:
`/private/tmp/nostromo-redo-graphite-window.png`. Its logs are
`/private/tmp/nostromo-redo-window.log` and
`/private/tmp/nostromo-redo-shot.log`.

The complete Python composition rendered 300 native frames and encoded
as a 30-second H.264 clip at 10 frames per second. Its 1800 × 1126 frame
includes one padded bottom row. Partial-inquiry and tube-decay frames
were visually inspected. The movie is
`build/media-study-redo/nostromo-motion.mp4`, with render and encode logs
beside it. File frames remain raster evidence; the window run supplies
the Graphite evidence.

The preserved C++ experiment passed a syntax-only check. Its live run is guarded because
the available host binary predates changing public framework headers.
The guard has not been bypassed, and no C++ runtime validation is claimed.
