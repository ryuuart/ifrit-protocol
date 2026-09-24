---
name: sketchbook
description: Run Sketchbook, the SigilSketch host, from the command line - headless plate sweeps, one still, bench and window-bench, video montage, plate comparison, texture promotion pins, thumbnails and the protocol endpoint a client drives
---

# Sketchbook from the command line

Everything renderable is a **sketch**: one file (or one directory) under
`src/sketch/sketches/`, addressed by its stem, in one registry;
`src/sketch/README.md` is the canon. **Sketchbook** drives all of it and
is an app bundle, so headless runs go through the binary inside it:

```sh
build/bin/<config>/Sketchbook.app/Contents/MacOS/Sketchbook \
  --headless [<outdir>] [--gpu] [--sketch <name>] [--kind canvas|set]
```

Pointed at a file with no `--headless`, Sketchbook opens on it, from
anywhere on disk, and hot-swaps the recompiled sketch on every save;
`--frame out.png` renders one still, `--bench` measures it against the
60 FPS gate on a raster surface, `--window-bench` presents it in the
real window, `--shot <png>` captures the app. `--video <out.mp4>
[--video-frames <n>] [--video-size <WxH>] [--video-bitrate <bits>]`
encodes the selection into one vertical montage, and needs `--gpu` for a
selection holding a set exactly as the sweep does. A sweep with no
directory writes into `sketch_plates/`. `--at <sec>` is the one flag for
the moment: it takes every plate of a sweep at that scene time instead
of at each sketch's own, and moves a `--frame` still the same way.
`python3 scripts/sigil.py plates compare <dir-a> <dir-b>`, or
`Sketchbook --compare <dir-a> <dir-b>` printing the same lines,
differences two directories of plates channel by channel; the plate
ledger reads the same rows as values through `sigil.sketch.compare`, so
`sigil.py plates` needs the `sigil_python` target built beside
Sketchbook. A headless sweep is opened deterministic and a deterministic
session holds automatic texture promotion off, so `--promotion` is the
one door that lets the promoter go with every other pin standing, and
`--no-promotion` pins it off on a backend whose default would not; the
plate ledger's promotion tier renders each scene both ways and judges
the pair. `--thumbnails [--sketch <name>] [--kind canvas|set]
[--thumbnail-budget <sec>] [--thumbnail-heavy]` renders the browser's
missing or stale thumbnails headless and exits non-zero naming any that
failed — the app owns its thumbnails, rendering them on demand into a
cache under the platform cache location; the plate ledger does not write
them.

`--state <dir>` is the one place a run keeps what it writes for a later
run, in place of the platform's own locations: the builds of sketch
files in `builds/`, the thumbnails in `thumbnails/`, the recorded device
programs in `pipelines/`, and the settings and recent workspaces in
`settings/`. A window or command the run starts is handed the same root,
so a scripted run that names a fresh one reads nothing an earlier run
left and leaves nothing behind.

The window mounts the protocol's endpoint on loopback without being
asked; `--inspect[=<port>]` only picks the port. `Sketchbook --headless
--inspect[=<port>] --state <dir>` with no plate directory, `--sketch` or
`--kind` opens no window and writes no plates: it is a host a client
drives over the protocol until it is interrupted, with its address in
`<dir>/protocol-address`; the Python client reaches it through
`sigil.protocol.launch(state=...)` or `connect(<address or state dir>)`,
and `sigil.testing` drives the same host in process. `Sketchbook
--headless <dir> --inspect ...` is a sweep that answers `host` and
`registry` between sketches, with its plates unchanged. `--inspect` is
refused on `--frame`, `--bench`, `--video`, `--list`, `--catalog`,
`--compare` and `--thumbnails`. `--deterministic` and `--no-deterministic`
name the clock policy a `--frame` session is opened for, `Advance` or the
wall's; the defaults stand: a still is taken under `Advance`, and
`--bench` runs under the wall's clock.
