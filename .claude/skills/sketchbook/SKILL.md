---
name: sketchbook
description: Run Sketchbook, the SigilSketch host, from the command line - headless plate sweeps, one still, bench and window-bench, video montage, plate comparison, texture promotion pins and thumbnails
---

# Sketchbook from the command line

Everything renderable is a **sketch**: one file (or one directory) under
`src/sketch/sketches/`, addressed by its stem, in one registry;
`src/sketch/README.md` is the canon. **Sketchbook** drives all of it and
is an app bundle, so headless runs go through the binary inside it:

```sh
build/bin/<config>/Sketchbook.app/Contents/MacOS/Sketchbook \
  --headless <outdir> [--gpu] [--sketch <name>] [--kind canvas|set]
```

Pointed at a file with no `--headless`, Sketchbook opens on it, from
anywhere on disk, and hot-swaps the recompiled sketch on every save;
`--frame out.png` renders one still, `--bench` measures it against the
60 FPS gate on a raster surface, `--window-bench` presents it in the
real window, `--shot <png>` captures the app. `--video <out.mp4>
[--video-frames <n>] [--video-size <WxH>] [--video-bitrate <bits>]`
encodes the selection into one vertical montage, and needs `--gpu` for a
selection holding a set exactly as the sweep does. `--compare <dir-a>
<dir-b>` differences two directories of plates channel by channel, which
is how the plate ledger's device tier judges. A headless sweep is opened
deterministic and a deterministic session holds automatic texture
promotion off, so `--promotion` is the one door that lets the promoter
go with every other pin standing, and `--no-promotion` pins it off on a
backend whose default would not; the plate ledger's promotion tier
renders each scene both ways and judges the pair. `--thumbnails [--sketch
<name>] [--kind canvas|set]` renders the browser's missing or stale
thumbnails headless and exits non-zero naming any that failed — the app
owns its thumbnails, rendering them on demand into a cache under the
platform cache location (`--thumbnails-dir` overrides it); the plate
ledger does not write them.
