# Findings

A work queue: each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Delete entries as they are fixed; delete the file when it is empty.

## `Ops.PathfinderBooleans` aborts under the address-sanitizer lane

**What the code does.** `scripts/sanitize.py --filter shape_test` builds
`shape_test` instrumented and the run dies in
`Ops_PathfinderBooleans_Test` with `SEGV in sk_realloc_throw` under
`SkOpBuilder::add` (`ops::unite`, `Ops.cpp`), inside Skia's prebuilt,
uninstrumented archive. The same test passes in the ordinary build, and
every other `shape_test` case passes under the sanitizer
(`--gtest_filter=-Ops.PathfinderBooleans`).

**What it was evidently intended to do.** The sanitizer lane exists to run
this repository's sources instrumented against uninstrumented vcpkg
archives; the top-level notes already pin Abseil's layout for that reason.
`SkOpBuilder` growing an `SkTDArray` across that boundary is the same
class of ABI/allocator mismatch, not a defect in `ops::unite` itself —
but the lane cannot currently vouch for the pathops code path.

**What a test should assert.** `Ops.PathfinderBooleans` runs green under
`scripts/sanitize.py` — either by pinning whatever Skia layout the
instrumented side disagrees on (as done for Abseil) or by routing the
booleans through an entry point that does not realloc across the
boundary. Until then the sanitizer lane's `shape_test` verdict excludes
this one case.

## `HyphenationOptions::enabled = false` still breaks at soft hyphens

**What the code does.** `Paragraph::analyze` splits a word at a trailing
U+00AD into two `Word`s and marks the first `hyphenBreak`
(`Paragraph.cpp`), and the greedy breaker then treats that boundary like
any other word boundary. `options.hyphenation.enabled` is read in exactly
two places — the hyphen-width reserve in `layoutParagraph`'s greedy loop
and the Knuth-Plass demerit — so disabling it drops the *visible hyphen*
while the line still breaks there. Observable through compose:
`text(u8"short extraordi­narily", …).width(150)` breaks after
`extraordi` with `.hyphenation({.enabled = false})`, one run shorter than
with it enabled, rather than keeping the long word whole.

**What it was evidently intended to do.** `HyphenationOptions::enabled`
is documented as "false ignores soft-hyphen break opportunities" — the
CSS `hyphens: none` behaviour, where a soft hyphen contributes no break at
all and the word wraps or overflows as one unit.

**What a test should assert.** In `weave_test`, one paragraph containing a
soft hyphen, laid out in a measure too narrow for the whole word: with
`hyphenation.enabled = false` the layout produces ONE line whose words are
the untruncated word (overflowing or forced), and with it true, two lines
with the hyphen glyph on the first. `compose_test`'s
`TextOptionSetters.HyphenationRendersTheHyphenAtASoftBreak` currently
asserts only the hyphen-glyph half, which is what the code actually
delivers; it should assert the break itself once intent is restored.

## `onPath` does not grow the recording cull by its baseline

**What the code does.** `Composer::Impl::ownPaintBounds` (`Paint.cpp`)
grows a node's local paint bounds by the widest of its decoration bleeds,
stroke half-widths, band profiles, echo offsets, material reserves and
`Track::reachPx()`. A `TextPath` baseline contributes nothing. The
baseline is resolved against the node's own box, so a `shapes::` generator
normally stays inside it — but nothing requires that: a custom `Shape` may
return a curve well outside the box, and `TextPath::offset` rides the type
further off it again. A run placed outside the cull is truncated at the
cached picture or texture bounds with no diagnostic, which is exactly the
failure mode `bleed()` and `reach()` exist to prevent.

**What it was evidently intended to do.** Every other producer of marks
outside the node's box declares how far it reaches, on the stated
over-reporting-is-safe contract. Text on a path is one of them.

**What a test should assert.** A text node whose `TextPath::path` resolves
to a curve outside its own box, inside a `Cache::Auto` parent that records:
the glyphs sitting outside the box still paint after the recording is
replayed. Today the same tree drawn with `Cache::None` and with the
default differ, which is the shape of the bug.
