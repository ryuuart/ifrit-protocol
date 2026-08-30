# Findings

Defects met while working, as a work queue: delete an entry when it is
fixed, and the file when it is empty.

## A whole-style `spanStyle` silently discards an earlier `spanPaint` on the same selection

**What the code does.** `Element::spanStyle(where, style)` compares the
requested style against each covered span's current style, paint
included. When an earlier `spanPaint` has coloured those spans, a style
that differs only in variable-font axes is no longer variation-only, so
it re-shapes and, being later, overwrites the paint with the style's own
— the author's colour vanishes with no message.

**What it was evidently intended to do.** Let an author restyle one
aspect of a selection without knowing the declaration order of the
others, or say once that the order mattered.

**What a test should assert once intent is restored.** Either a
`spanStyle` that changes only axes folds regardless of the spans' paint
(the paint stays), or declaring it after `spanPaint` on the same
selection produces one warning naming both verbs and the element key.

## OpenImageIO's thread pool races on its own worker map (TSan)

**What the code does.** `sigil::image::decodeImage` constructs
OpenImageIO's `thread_pool`, whose worker threads swap an internal
`tsl::robin_map<std::thread::id, int>` while another worker reads it.
TSan reports the race inside `OpenImageIO::thread_pool::thread_pool`
(vendor code, uninstrumented vcpkg archive) during
`LoaderOiio.ExrDecodesToFloatImage` in `loader_hub_test`. Nothing in
this repository touches that map.

**What it was evidently intended to do.** Decode an EXR through a
library whose thread pool is internally consistent.

**What a test should assert once intent is restored.** Either the
vendor's pool is fixed upstream, or the decoder constructs it with a
single worker (or suppresses the report by a TSan suppression file
naming `OpenImageIO::v3_1::thread_pool`), and the TSan lane runs
`loader_hub_test` clean.

## OpenUSD's plugin registry races its own name table (TSan)

**What the code does.** Opening a stage instantiates USD's
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

**What a test should assert once intent is restored.** With a TSan
suppression naming `pxrInternal_*::PlugRegistry` (or an upstream fix),
the TSan lane runs `usd_write_test` and `usd_read_test` clean.

## Every target carries Qt automoc, including libraries with no Qt in them

**What the code does.** The root `qt_standard_project_setup()` turns
`AUTOMOC` on for every target that follows, so plain static libraries
such as `SigilComposeBrush` gain a `*_autogen` directory and a generated
`mocs_compilation` translation unit per configuration; two targets
already opt out by hand.

**What it was evidently intended to do.** Moc only the targets that
declare Qt objects — the Qt Quick apps, `Ifrit.Ui` and the Qt interop
libraries.

**What a test should assert once intent is restored.** The compile
database lists no `*_autogen` translation unit for a target that does
not link Qt.

## The static analysis queue is unanswered in compose and in world

**What the code does.** `scripts/check.py --all --tidy-all` reports no
findings in geometry, skia, core, measure, scry, material, usd,
substance or the text engine. Neither `src/common/compose/` nor
`src/common/world/` is covered by that sentence: world does not compile
while its rewrite is in flight, and compose has been analyzed only in
part — the translation units measured so far report nothing.

**What it was evidently intended to do.** Keep the queue closed: a
finding is either fixed or answered in place with the reason it stands,
which is how the rest of the tree reads.

**What a test should assert once intent is restored.**
`scripts/check.py --all --tidy-all` reports no findings in this
repository's sources.

## Two benchmarks stand outside their band with no change to attribute

**What the code does.** On a quiet machine
`scripts/bench_ledger.py` reports
`compose_shape_bench:BM_Reconcile_Shapes_RawCallable/100` about 14 %
above its baseline and `motion_bind_bench:BM_Apply_Envelope/4` about
11 % above, and both reproduce across repeated scoped runs while every
other slower row of a loaded run does not. Both measure work whose
bodies moved from headers into one translation unit apiece — the
reconciler's matching loop and the binding chain's evaluator — so the
inlining a caller used to get across the header boundary is gone.

**What it was evidently intended to do.** Compile each body once
without paying for it at every call site that used to inline it.

**What a test should assert once intent is restored.** Either the two
benchmarks return inside their band, or the baseline records the cost
with the boundary that causes it named beside it.

## `world_diligent_bench:BM_DeviceBringUp` moves more than the ledger's band

**What the code does.** The benchmark creates a whole Vulkan device per
iteration. Its median rises within one session — a device made after
several others costs measurably more than the first — and rises again
across back-to-back ledger invocations, so a baseline seeded from one
sweep is outside the ledger's band by the third. Nothing about the
library changes between those sweeps.

**What it was evidently intended to do.** State what a process pays once
on its way to its first frame, as a number a change to this library
could move.

**What a test should assert once intent is restored.** That the number
is a property of the code: either the benchmark measures the part of
bring-up this library owns — the adoption, the loader, the shim — with
the driver's own device creation outside the timed region, or the ledger
carries a per-benchmark band wide enough to be a real gate for it and
states why this one is wider.
