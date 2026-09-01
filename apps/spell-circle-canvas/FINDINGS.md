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

**What a test should assert.** The plate ledger's full tier,
byte-identical, per file, and its quick tier within its ceilings.

## A filtered rebase drops the arms it did not measure

**What it does.** `scripts/bench_ledger.py --rebase` with `--filter`
writes the baseline entry for each named binary from the sweep it just
took, and a filtered sweep took only the arms the filter selected. The
binary's other arms are therefore not written, and a baseline that had
thirty-two arms for `geometry_mesh_curve_bench` comes back with the
sixteen the filter named. The next unfiltered run reports the other
sixteen as `new` and judges nothing.

**What it was evidently intended to do.** `--rebase` already merges when
`--benches` names a subset of the binaries, so the same posture for a
subset of the ARMS is what a reader expects: adopt what was measured,
leave what was not.

**What a test should assert.** A rebase under a filter that selects one
arm leaves every other arm of that binary at the value it had, and a
judging run afterwards reports none of them as new.
