# Macrodata Refinement

A television-interface study of the *Severance* Macrodata Refinement
terminal, authored in Python and rendered through native Compose. The
1440 × 1000 composition includes a recessed navy cabinet, a small keyboard,
cyan screen rules, a wireframe Lumon globe, 242 drifting numeric glyphs,
an enlarged selection, a traveling collection packet, five completion
bins, a four-temper drawer, and a hexadecimal status address.

## Reference and interpretation

The [production-screen photograph reproduced by Nerdist](https://cdn.nerdist.com/wp-content/uploads/2025/01/17104528/Severance-Computer.jpg)
was visually inspected before authoring. Its observed conventions are
the deep blue screen, cyan luminous digits, the Cold Harbor heading,
segmented completion rail, wireframe Lumon mark, a loose numeric field
whose selected digits grow, five numbered bins with percentage bars,
and a hexadecimal address underneath. The study preserves that hierarchy
and the photographed 67% completion and bin percentages.

[Apple's production account](https://www.apple.com/newsroom/2025/03/how-the-mind-splitting-world-of-severance-comes-together-on-mac/)
supplies context about the show's production and post-production workflow.
It does not document the terminal's interface design or its implementation.

The numeric data, selected cluster, collection path, drawer timing,
temper balance values, cabinet wear and simplified physical keyboard are
authored extrapolations. The four abbreviations WO, FC, DR and MA identify
the fictional temper categories; their balances do not claim to reproduce
a canonical work file. The original grid remains visible during the
collection event so the packet is an explicit motion study rather than a
complete simulation of the show's interaction rules.

## Craft exercised

Each digit has a seeded phase and a distinct motion period. Their small
independent drifts create a living field without a uniform wave. Seven
digits grow through a separate selection envelope. A cached arrow moves
on its own horizontal and vertical periods. The collection packet follows
an envelope toward bin 03, and the temper drawer opens later in the
classification cycle. These activities share one scene clock but retain
separate motion bindings.

The small type uses Menlo with enough weight to hold a phosphor edge;
the file heading and Lumon wordmark use a contrasting heavier sans-serif
face. Native optical bloom adds a narrow halo around the entire screen.
An original transparent shader overlays faint raster rows and corner
falloff. The bezel and keyboard remain outside that optical pass. No
photographic or generated bitmap is used in the composition.

The cabinet, rules, globe and cursor are cached native pen leaves. Text is
described once as keyed native text nodes. Frame updates set one native
animatable value; they do not rebuild the scene or issue Python drawing
callbacks. The file uses the same retained rendering and motion vocabulary
as native C++ studies through the compiled Python bindings.

## Render

From `apps/spell-circle-canvas`:

```sh
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/severance_macrodata/severance_macrodata.py \
  --frame /tmp/severance-selection.png --at 4.6 \
  --state /tmp/severance-python-study-state
```

Add `--gpu` to initialize the device executor. The file-capture lane still
rasterizes this 2D canvas. Use `--at 8.4` for the traveling
packet and `--at 12` for the expanded
temper drawer. Give each concurrent capture its own output path. The
state directory is private to this study and avoids another study's saved
selection or viewport state.

The Python package path is `apps/python/sigil/sigil`. Sketchbook manages
its Python environment and loads the installed bindings; no standalone
Python drawing runner is required.

For a complete motion cycle, capture numbered native frames and encode
only those frames:

```sh
mkdir -p /tmp/severance-motion-frames
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/severance_macrodata/severance_macrodata.py \
  --gpu --frame /tmp/severance-motion-frames/frame.png \
  --frames 216 --fps 12 --at 0 --state /tmp/severance-motion-state
ffmpeg -framerate 12 -start_number 1 \
  -i /tmp/severance-motion-frames/frame_%04d.png \
  -c:v libx264 -crf 16 -pix_fmt yuv420p -movflags +faststart \
  /tmp/severance-motion.mp4
```

## Authoring observations

**Retained native motion is sufficient for the numeric field.** Individual
digits, grouped digits and the pointer all accept native bound transforms.
Python only writes the scene clock. No sketch-local interpolation engine,
glyph rasterization or drawing loop was needed.

**String convenience is uneven at adjacent layout methods.** The overflow
method accepts the exported `Overflow.Clip` enum. Passing the natural
`"clip"` string raises an argument-type error, while `alignItems("center")`
and `justifyContent("space_between")` accept categorical strings. A small
live-host probe verified all three forms. The supported enum is used in
the study. A consistent
string convenience layer would reduce interruptions while authoring;
this is an API request, not a rendering defect. If added, a regression
should assert that both forms produce identical clipping and that an
unknown string produces a clear value error.

**Material tuning exposes many coupled controls.** The stock bloom can
express the intended narrow phosphor halo, but specifying the look means
coordinating threshold, knee, sigma, spread, tail, whitening, dilation and
deepening. These controls are available, so the study needs no new bloom
implementation. An authoring preview that exposes them together, or
documented starter values for narrow luminous text, would make finding a
subtle optical treatment easier without placing a named film look in the
core library. A future preview should use the same effect implementation
as the renderer so its displayed result matches a captured frame.

**The video lane exports registry selections.** The source accepts and
validates a file argument before dispatch, but video dispatch passes only
the registry selection and kind into the montage. It does not pass the
file path into a live host. A newly authored file therefore has no
file-specific video route through that command. An export command that
loads the file directly would complete the still-to-motion authoring
workflow. Until that route exists, combining a file with video should
fail explicitly instead of silently choosing a registry montage. A
regression should provide a file and assert that only that file is encoded,
or that the command refuses before invoking the registry montage. This
finding is source-verified; no unintended montage was launched.

## Verification

The live Python host rendered selection at 4.6 seconds, collection at 8.4
seconds, and the temper drawer at 12 seconds on the CPU executor. All
three refined images were visually inspected against the reference
photograph: the field fits its ruled bounds, the header and five bins
remain legible, and the drawer fits above bin 03. The saved evidence is
`/private/tmp/severance-refined-4-6.png`,
`/private/tmp/severance-refined-8-4.png`, and
`/private/tmp/severance-refined-12.png`.

An elevated capture with `--gpu` at 4.6 seconds initialized the Apple M1 Pro
Vulkan device and was visually inspected. The 2D file-capture canvas remains
raster, so this is a device smoke test rather than Graphite bloom evidence.
Its selected cluster and bins preserve the intended CPU composition. The
image is `/private/tmp/severance-gpu-4-6.png`; its log is
`/private/tmp/severance-gpu.log`. The log reports no shader diagnostics;
its sole warning says that thread naming is unavailable on the platform.
A separate bounded file-window run selected only this study, identified
`renderer: Graphite GPU` and completed without shader errors. Its luminous
digits, selection and bins therefore also exercised the interactive backend.
The window log is preserved with the study evidence. The file still and
motion export continue to use the raster capture lane.

The complete motion sequence rendered 216 native frames and encoded as
an 18-second, 1440 × 1000 H.264 clip at 12 frames per second. The encoded
selection, packet, drawer and closed end-state frames were extracted and
visually inspected. The artifact is
`build/media-study-evidence/severance-motion.mp4`, with render and encode
logs beside it. This is an authored export cadence, not a measurement of
interactive playback performance.
