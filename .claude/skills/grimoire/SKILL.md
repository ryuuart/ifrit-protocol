---
name: grimoire
description: Run Grimoire, the SigilSketch host, from the command line - headless plate sweeps, one still, window-bench, video montage, plate comparison, thumbnails and the protocol endpoint a client drives
---

# Grimoire from the command line

Everything renderable is a **sketch**: one file (or one directory) under
`apps/grimoire/sketches/`, addressed by its stem, in one registry;
`apps/spell-circle-canvas/src/sketch/README.md` is the canon for what a
sketch is and `apps/grimoire/RUNNING.md` for every flag below.
**Grimoire** (`apps/grimoire/`) drives all of it and is an app bundle,
built into the library tree's build, so headless runs go through the
binary inside it, from `apps/spell-circle-canvas`:

```sh
build/bin/<config>/Grimoire.app/Contents/MacOS/Grimoire \
  --headless [<outdir>] [--gpu] [--sketch <name>] [--kind scene2d|scene3d]
```

Pointed at a file with no `--headless`, Grimoire opens on it, from
anywhere on disk, and hot-swaps the recompiled sketch on every save;
`--frame out.png` renders one still and `--window-bench` measures the
rate it reaches the screen at in a window of its own — a scene3d sketch
is drawn on a device and nowhere else, so every headless lane that draws
one (`--frame`, `--headless`, `--video`) needs `--gpu`, and without it the scene3d sketch is refused, skipped or
listed unavailable with one reason; `--list --gpu` and `--catalog --gpu`
list the scene3d sketches a device can draw — `--window-bench` presents it in the
real window, `--shot <png>` captures the app. `--video <out.mp4>
[--video-frames <n>] [--video-size <WxH>] [--video-bitrate <bits>]`
encodes the selection into one vertical montage, and needs `--gpu` for a
selection holding a scene3d sketch exactly as the sweep does. A sweep with no
directory writes into `sketch_plates/`. `--at <sec>` is the one flag for
the moment: it takes every plate of a sweep at that scene time instead
of at each sketch's own, and moves a `--frame` still the same way.
`python3 scripts/sigil.py plates compare <dir-a> <dir-b>`, or
`Grimoire --compare <dir-a> <dir-b>` printing the same lines,
differences two directories of plates channel by channel; the plate
ledger reads the same rows as values through `sigil.sketch.compare`, so
`sigil.py plates` needs the `sigil_python` target built beside
Grimoire. A headless sweep is opened deterministic. The ledger's device tier, its default and its reference, judges
every scene2d and scene3d sketch on byte identity against one machine-local
device manifest, adopted with `--rebase`; `sigil.py plates --tier cpu` is a
smoke run of the raster path that reads each scene2d sketch's CPU plate
against its device plate and fails only for a sketch that did not render.
Both take a sketch by its filed name or its file stem. `--thumbnails [--sketch <name>] [--kind scene2d|scene3d]
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
left and leaves nothing behind. `--pipeline-names` puts one line on
stderr per device program built or replayed, naming it — how to see which
program a first frame still had to build.

The window mounts the protocol's endpoint on loopback without being
asked; `--inspect[=<port>]` only picks the port. `Grimoire --headless
--inspect[=<port>] --state <dir>` with no plate directory, `--sketch` or
`--kind` opens no window and writes no plates: it is a host a client
drives over the protocol until it is interrupted, with its address in
`<dir>/protocol-address`; the Python client reaches it through
`sigil.protocol.launch(state=...)` or `connect(<address or state dir>)`,
and `sigil.testing` drives the same host in process. `Grimoire
--headless <dir> --inspect ...` is a sweep that answers `host` and
`registry` between sketches, with its plates unchanged. `--inspect` is
refused on `--frame`, `--video`, `--list`, `--catalog`,
`--compare` and `--thumbnails`. `--deterministic` and `--no-deterministic`
name the clock policy a `--frame` session is opened for, `Advance` or the
wall's; the defaults stand: a still is taken under `Advance`, and a
window under the wall's clock.
