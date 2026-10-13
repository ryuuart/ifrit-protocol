# What this is

The Sigil libraries make a GPU-forward 2D renderer whose pages are
materials on 3D surfaces. A page is laid out, typeset and drawn with
Skia on the GPU, and the world renders it as the material of a lit
surface, live, at the pace of a game. Grimoire is the host that runs it.

Almost everything in this repository is a set of independent C++
libraries — a layout and composition engine,
a text engine, a paint and material model, geometry, motion, data,
resource access, video, a 3D world — each usable on its own, each with
its own README, and each with the same values reachable from Python.

Three things a reader usually wants next:

- **[The libraries](overview:libraries)** — what each one owns, and how
  they relate.
- **[The guides](/guides/index.html)** — the shortest path from nothing
  to a picture, in C++ and in Python.
- **[The glossary](glossary:)** — the house words, and which library
  each one belongs to.
- **[The values](/values/index.html)** — every value in the tree, with
  what makes one and what takes one.

## The data path

A frame's path, end to end:

1. **What arrived is read.** A door on a resource hub — UDP, OSC,
   WebSocket, shared memory, MIDI — holds what an external process sent,
   a TouchDesigner patch among them, and the frame takes it out in order.
2. **A sketch describes the scene.** In C++ or in Python, from its own
   state and whatever arrived.
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
7. **The texture is published.** With `--publish`, Grimoire offers the
   frame over Syphon under the name it is given.

## Sketches

Everything renderable is a **sketch**: one file that declares a scene,
opened live, hot-swapped on every save. That is how the libraries are
exercised and how most of the pictures in this documentation were drawn.
A sketch is C++ or Python, and the two spell the same API:

```sh
# open a C++ or Python sketch live, hot-swapping on save
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire path/to/sketch.cpp

# render one still, headless
sigil render sketch.py --output preview.png --at 2
```

## Where the canon is

Each library's own `README.md` is the canon for that library, written
for someone with no prior context and compile-checked against its
headers. This overview layer points at them; it does not restate them.
The generated per-library API reference sits beside this page, one site
per library, cross-linked.

One thing every library shares is how it is built, tested and measured,
so that is written here rather than twenty times over:
**[building and testing a library](overview:testing)**. A library's own
README states what is true of it alone.
