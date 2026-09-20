# What this is

SpellCircle draws network-driven vector diagrams for live production. An
external process — a Python script, a TouchDesigner patch — describes a
scene and sends it as a FlatBuffers datagram over UDP. SpellCircle
receives it, draws it with Skia on the GPU, and publishes the result as a
transparent-background texture over Syphon, where a VJ or compositing
tool picks it up.

That application is thin. Almost everything in this repository is a set
of independent C++ libraries under it — a layout and composition engine,
a text engine, a paint and material model, geometry, motion, data,
resource access, video, a 3D world — each usable on its own, each with
its own README, and each with the same values reachable from Python.

Three things a reader usually wants next:

- **[The libraries](libraries.md)** — what each one owns, and how they
  relate.
- **[The guides](../guides/index.md)** — the shortest path from nothing
  to a picture, in C++ and in Python.
- **[The glossary](../glossary.md)** — the house words, and which
  library each one belongs to.

## The data path

The product's path, end to end:

1. **A sender describes a scene.** Circles, points placed on those
   circles' perimeters, edges between points, and labelled boxes. The
   wire schema is `SpellCircle.fbs`, and the Python package
   `apps/python/spellcircle` builds and sends one with no renderer in
   reach.
2. **A datagram arrives over UDP.** Scenes may arrive at animation frame
   rates, so a sender can use the receiver as a live output surface.
3. **The scene is turned into a description.** Not into draw calls: a
   value-typed tree describing what the frame should look like, built
   fresh and thrown away.
4. **The description is reconciled.** The new tree is diffed against the
   one the runtime retains, so only what actually changed is touched.
   This is the reconciler, and it is why re-describing an identical
   scene costs a comparison rather than a repaint.
5. **Layout runs.** Flexbox through Yoga, with text leaves measured by
   the text engine, then the derive phase for anything that had to wait
   for a neighbour's box — a connector between two keyed nodes, a rail,
   text flowing around a target.
6. **The tree paints.** In an explicit CSS-like stacking order, onto an
   `SkCanvas` the caller owns, with nodes cached as pictures or textures
   where they are provably still.
7. **The texture is published.** Over Syphon, under the server name
   `SpellCircle`.

## The other way in

The same engine hosts **sketches**: one file that declares a scene,
opened live, hot-swapped on every save. That is how the libraries are
exercised and how most of the pictures in this documentation were drawn.
A sketch is C++ or Python, and the two spell the same API:

```sh
# open a C++ or Python sketch live, hot-swapping on save
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook path/to/sketch.cpp

# render one still, headless
sigil render sketch.py --output preview.png --at 2
```

## Where the canon is

Each library's own `README.md` is the canon for that library, written
for someone with no prior context and compile-checked against its
headers. This overview layer points at them; it does not restate them.
The generated per-library API reference sits beside this page, one site
per library, cross-linked.
