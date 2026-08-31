# Findings

A work queue. Each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Delete an entry as it is fixed, and delete this file when it is empty.

## A text style helper in a dozen files that the library nearly owns

**What it does.** `compose::type({.face, .size, .color, .track,
.condense, .weight, .slant})` builds a `weave::TextStyle` out of the
numbers a call site has. A dozen sketches still carry a local helper that
does a subset of the same thing — `ty(face, size, colour, track)` with
`mono()` / `sb()` / `it()` spellings over it in `black_watch` and
`chevreul_circle`, `type(face, size, colour, track)` in
`chaucer_astrolabe`, `chladni_tab1`, `genesis_fire`, `psx_doom_fire` and
`fallout2_charsheet`, and `type(size, colour, track, condense, bold)` in
`ds2_bench` and `eva_magi_defense`.

**What it was evidently intended to do.** Every one of those is a
`compose::type` call with `.face` (and, for some, `.condense` or
`.weight`) filled in. The helpers that took only a size, a colour and a
tracking have already gone; these are the ones that also resolve a face,
and the face is the only thing they add.

**Why it has not moved.** Each of these files names its faces once and
then spells two or three *named* styles over the helper — `mono`,
`monoB`, `ui` — and those names are the artefact's own vocabulary rather
than a mechanism. What should move is the body of the helper, leaving the
named styles as one `compose::type` call each; that is a per-file edit
rather than a substitution.

**What a test should assert.** The plate ledger's full tier AND its quick
tier, byte-identical, per file. Both, because the two disagree about one
thing that matters here (see the next entry).

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
