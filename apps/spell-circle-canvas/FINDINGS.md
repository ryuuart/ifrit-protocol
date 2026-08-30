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

## OpenUSD's plugin registry races its remnant refcount (TSan)

**What the code does.** `sigil::usd::Writer::Writer` opens an in-memory
stage; USD's plugin registry registers plugins on a TBB worker thread,
which constructs a `Tf_Remnant` (a `TfRefBase` whose atomic counter is
default-initialised non-atomically) that the main thread then `AddRef`s.
TSan reports the write-vs-atomic-fetch_add pair in `Usd.RoundTripsAMeshWithSlotsAndMaterials`
in `usd_test`. All frames are in `pxr` headers and `libusd_plug`; the
repository's code only holds a `TfWeakPtr`.

**What it was evidently intended to do.** Register plugins with the
publication of each remnant ordered before its first use, which USD's
own lock evidently provides in practice but not in a form TSan can see.

**What a test should assert once intent is restored.** With a TSan
suppression naming `pxrInternal_*::Tf_Remnant` (or an upstream fix),
the TSan lane runs `usd_test` clean.

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
