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

## OpenImageIO's thread pool races on its own worker map (TSan)

**What it does.** `sigil::image::decodeImage` constructs OpenImageIO's
`thread_pool`, whose worker threads swap an internal
`tsl::robin_map<std::thread::id, int>` while another worker reads it.
TSan reports the race inside `OpenImageIO::thread_pool::thread_pool`
(vendor code, uninstrumented vcpkg archive) during
`LoaderOiio.ExrDecodesToFloatImage` in `loader_hub_test`. Nothing in
this repository touches that map.

**What it was evidently intended to do.** Decode an EXR through a
library whose thread pool is internally consistent.

**What a test should assert.** Either the vendor's pool is fixed
upstream, or the decoder constructs it with a single worker (or the
report is suppressed by a TSan suppression file naming
`OpenImageIO::v3_1::thread_pool`), and the TSan lane runs
`loader_hub_test` clean.

## OpenUSD's plugin registry races its own name table (TSan)

**What it does.** Opening a stage instantiates USD's
`TfSingleton<PlugRegistry>`, which registers plugins on TBB worker
threads. Those workers rehash the registry's `__hash_table` of plugin
names while another thread reads the same buckets, and TSan reports the
pair with every frame inside `libusd_plug` and `pxr` headers. It fires
in `UsdWrite.AuthorsAMeshWithSubsetsAndMaterials` in `usd_write_test`
and in `UsdRead.ReadsAHandAuthoredStage` in `usd_read_test`. Nothing in
this repository touches that table; the repository's code only holds a
`TfWeakPtr`.

**What it was evidently intended to do.** Register plugins with each
insertion ordered before any other thread's read, which USD's own lock
evidently provides in practice but not in a form TSan can see.

**What a test should assert.** With a TSan suppression naming
`pxrInternal_*::PlugRegistry` (or an upstream fix), the TSan lane runs
`usd_write_test` and `usd_read_test` clean.
