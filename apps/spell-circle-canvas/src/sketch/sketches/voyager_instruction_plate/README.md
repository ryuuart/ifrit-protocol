# Voyager instruction plate

A native Python study of the Golden Record's engraved cover and its symbolic
decoding instructions. The plate has a scratched metallic surface, native
vector grooves and stylus, a waveform, image raster, calibration circle,
fourteen radial directions, binary line marks and hydrogen-state diagrams.
An animated vertical scan crosses the explanatory raster.

## Reference and interpretation

The primary source is [NASA's Golden Record cover explanation](https://science.nasa.gov/mission/voyager/golden-record-cover/).
Its [photograph and diagram](https://science.nasa.gov/wp-content/uploads/2024/03/voyager-record-diagram.jpeg)
were inspected directly. The cover's main arrangement, stylus/groove motif,
binary notation, calibration circle, hydrogen states and radial diagram
inform this reconstruction. NASA specifies the 3.6-second revolution, a
hydrogen-transition time unit of approximately 0.70 nanoseconds and an image
format of 512 vertical lines.

The surrounding archival composition, type, metal shader, wear and live scan
are original. Pulsar angles and bit strings are illustrative visual data:
this study cannot be used to locate the Solar System. The page states that
limitation within the diagram. It does not reproduce every engraved number.
No reference bitmap is loaded or bundled.

## Construction

- The metal shader uses fixed diagonal reflection, elongated grain, small
  scratches and faint concentric machining marks.
- Native Pen commands construct all engraved figures, symbols and labels.
  Binary values use line marks, preserving the cover's visual alphabet.
- The rim and engraving each use a retained Picture cache. The raster scan
  is a separate live Pen leaf, so its motion does not rebuild the plate or
  accumulate the previous scan positions.
- The explanatory column keeps the fundamental clock, playback time, image
  geometry and location diagram legible at different scales.
- Menlo and Helvetica Neue are system font dependencies. Their roles are
  technical annotations and an archival title, rather than cover facsimile.

## Capture

From `apps/spell-circle-canvas`:

```sh
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/voyager_instruction_plate/voyager_instruction_plate.py \
  --frame /tmp/voyager-instruction-plate.png --at 2.4 \
  --state /tmp/voyager-instruction-state
```

Native CPU captures at 0.5 and 2.4 seconds were visually inspected. Their
changed pixels stay within the live raster region; the disc and engraved
figures remain identical. An elevated capture with `--gpu` at 3.5 seconds
initialized the Apple M1 Pro device and was visually inspected, with no
shader error. The file-capture lane rasterizes this 2D canvas, so that run
is a device smoke test rather than Graphite optical-effect evidence.
The final CPU evidence is `/tmp/voyager-instruction-plate.png`, and the
device-enabled evidence is `/tmp/voyager-instruction-gpu.png`.

A separate bounded file-window run selected only this study and reported
`renderer: Graphite GPU`. The gold program, engraved vectors and moving
raster rendered without shader errors. To repeat that backend check:

```sh
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/voyager_instruction_plate/voyager_instruction_plate.py \
  --window-bench 2.5 --window-size 1280x900 --gpu \
  --state /tmp/voyager-window-state
```

Use `--shot /tmp/voyager-window.png` in place of the benchmark options to
photograph the fitted canvas through the app window. This is separate
from the full-resolution, exact-time raster still.
