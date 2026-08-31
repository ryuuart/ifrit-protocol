# Findings

A work queue. Each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Delete an entry as it is fixed, and delete this file when it is empty.

## The byte-identity sweep renders with measured numbers unpinned

**What it does.** `sketch::Session::setDeterministic()` exists so a
sketch can pin anything it measured about its own execution, and
`SketchContext::measured()` is how a sketch asks. The plate sweep never
turns it on, so a sketch that draws its own bake time into its own plate
differs from ITSELF between two runs.

**What it was evidently intended to do.** Exactly what the flag says: a
capture that will be diffed must not carry a number the machine decided.
`slitscan_2001` reads `ctx.deterministic` and writes
`deterministic_ ? 0.0 : bakeMs` into its sheet — and `slitscan_2001` is
one of the three names on the ledger's documented self-nondeterministic
list. The list and the flag are two answers to one problem, and only one
of them is a fix.

**Why it is not simply switched on.** Turning it on moves the plates of
every sketch that measures itself, so the change has to be made together
with adopting those baselines, and each mover has to be checked to be a
number rather than a picture.

**What a test should assert.** Two ledger renders of `slitscan_2001` in
two processes hash the same, with no name on a flapper list. Then the
list is checked again: `genesis_fire` and `hitman_verlet` may be flapping
for a different reason (both simulate), and if they are, the reason
belongs in their file headers rather than in a script's table.

## Looks and conveniences duplicated across sketch files

Each of these is the same code in more than one file, with no library
home. A sketch is meant to be pure declaration of its scene, so anything
here that is a LOOK belongs in a kit and anything that is a CONVENIENCE
belongs wherever its type is spelled. Every move has to be gated on the
plate ledger per scene, because these all reach the pixels.

- **`crtEffect()`** — a scanline, vignette and bloom overlay, byte-
  identical in `eva_magi_defense.cpp` and `eva_magi_interior.cpp`.
  Nothing in `src/common` draws a CRT. It is a preset: it fixes a look.
- **`disc()`** — a soft-dot sprite raster, verbatim in `geo_groups.cpp`
  and `pop_deform.cpp`. A point sink needs a sprite and the library ships
  none.
- **`fmt()`** — a printf-into-`std::string` helper, in eight files in two
  flavours (a variadic template with the `-Wformat-security` pragma pair
  in four; a `va_list` version with a fixed buffer in four).
- **`place(Element, x, y, w, h)`** / **`at(x, y, w, h)`** — absolute
  placement spelled as `box().left().top().width().height()`, in six
  files. `kit/Frame.h` has `centred()` and `disc()` but nothing for the
  commonest case of all.
- **`lift()` / `dark()` / `fade()`** — linear-RGB colour nudges, in up to
  six files. `sigilmaterial/color/Color.h` has Oklab and the sRGB
  transfer and nothing between.

**What a test should assert.** After each move: the plate ledger's full
tier, byte-neutral, for every scene that used it.

## A text style helper in eight files that the library nearly owns

**What it does.** Eight sketches carry a local `styleAt(size, colour)` or
`type(size, colour, tracking)`, six of them verbatim.
`compose::type({.size, .color, .track})` is the same idea with a wider
surface.

**Why it has not moved.** Two differences reach the pixels, and both are
in the library's favour rather than the sketch's: `compose::type` sets
the paint's antialias flag where the local helpers leave Skia's default,
and it sets the colour as `SkColor4f` where two of the local helpers
round-trip through `toSkColor()` and quantise it to eight bits per
channel. Moving them is a look change, small but real.

**What a test should assert.** The plate ledger's full tier for the eight
scenes, with the movers adopted deliberately and the round-trip
quantisation gone rather than reproduced.

## The device tier's plates are not reproducible across two executables

**What it does.** The GPU tier renders a plate that is stable across
processes, across the order sketches are rendered in, and across repeated
runs — and yet differs between two executables built from the same
drawing code. Measured on `botanical`: 502 of 9,216,000 colour channels
differ, scattered over the frame, the worst by 24. The authoritative CPU
tier is byte-identical for the same sketch.

**What it was evidently intended to do.** The quick tier hashes bytes, so
it assumes the device path is a function of the scene. It is a function
of the scene AND of the binary, which is a blind spot the tier does not
declare: a change that touches nothing about a sketch can still move its
quick hash, and the only way to tell that from a real mover is to render
it on the CPU tier as well.

**What a test should assert.** Either the device tier compares within a
tolerance the way the world-gpu tier already does — which is the same
argument, one rasteriser against another — or it states this blind spot
beside the two it already states, so a mover there is read as a question
rather than as a finding.
