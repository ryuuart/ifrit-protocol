# Voyager instruction plate

An engraved Golden Record cover on woven museum cloth, beside a working
decoding folio. The source's complete vector transcription supplies the
incisions; the native optical program turns those cuts into a reflective
gold surface with a machined rim, anisotropic grain, abrasion and contact
shadow. The folio reads the cover through four successive instructions.

## Source and interpretation

[NASA's cover explanation](https://science.nasa.gov/mission/voyager/golden-record-cover/)
and its photograph/diagram supply the arrangement and decoding facts. The
rotation lasts 3.6 seconds, measured in units of the hydrogen transition's
approximately 0.70 nanoseconds. The image instructions specify 512 vertical
lines and a calibration circle; the location diagram uses fourteen pulsars.

The bundled `data/cover.svg` is the public-domain vector transcription by
Nicolás González Montofré from
[Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Voyager_plaque.svg).
It supplies the original-style symbols and binary marks rather than
invented visual data. The study does not independently verify the pulsar
measurements or claim navigational authority.

The metal optics, archival arrangement, padded mounts, cloth, paper, native
type and animated explanatory diagrams are authored. The printed field
notes interpret the instructions; they are separate from the engraved
source, and their evenly spaced pulsar spokes are explanatory geometry.

## Native construction

- The native SVG loader rasterizes the transcription at the disc's
  resolution. The image's luminance supplies a height field for the cuts.
  Nearby samples produce the incision normal and small lip highlights.
- Soft reflected light shares the incision normal with the broader gold
  surface. Concentric machining, elongated grain and edge wear belong to
  the same optical material.
- Disc, contact shadow, paper and static Pen drawings are retained layers.
  The live instruction leaf changes without rebuilding the object.
- The twenty-four-second sequence establishes a clock, moves a stylus
  inward, assembles a circle from 512 columns, then reads pulsar directions.
  The active rule and marginal stage markers follow that sequence.
- Baskerville supplies the reading leaf and native italics. Menlo and
  Helvetica Neue supply archival identifiers.

## Capture

The canvas is 2100 × 1460. From `apps/spell-circle-canvas`:

```sh
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/voyager_instruction_plate/voyager_instruction_plate.py \
  --frame /tmp/voyager-instruction-plate.png --at 14.8 \
  --state /tmp/voyager-state
```

The exact-time still uses the raster canvas. A separate file-window capture
reported `renderer: Graphite GPU`, rendered the cut-metal program and folio,
and completed without shader errors:

```sh
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/voyager_instruction_plate/voyager_instruction_plate.py \
  --shot /tmp/voyager-window.png --gpu \
  --state /tmp/voyager-window-state
```

Full-resolution still and actual-window evidence are preserved separately
under `build/media-study-redo/`.
