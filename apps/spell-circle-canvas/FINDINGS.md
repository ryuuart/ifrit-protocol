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

## `measureRun` drops the gaps between words, so its prefix sums mis-place

**What the code does.** `measureRun` (`Composer.cpp`) lays the run out
through the real path — one `Paragraph`, one unconstrained `BlockFlow`,
`forEachPlacedGlyph` — and pushes each placed glyph's advance. An
inter-word space is a gap the flow leaves between positioned runs rather
than a glyph, so it contributes no entry: at 40 px Helvetica Neue,
`measureRun(u8"A B", …)` returns TWO advances summing 53.36 px while the
laid-out line's last pen reaches 64.47 px, and
`u8"ONE PASS PER WORD PHASE"` returns 19 advances summing 534.94 px
against a laid-out 579.39 px. Every glyph after a space is therefore
placed short by the accumulated space advances, and the error grows with
each word.

**What it was evidently intended to do.** The header states it plainly:
"Pen positions are the running prefix sums, so hand-placing N glyphs costs
one layout here rather than N text() leaves and N measure() calls." That
contract holds for a single word and silently fails for a sentence — the
caller gets numbers that look right, are right at the left end, and drift
further wrong to the right. The two candidate repairs are opposite: report
the gap as an advance so the prefix sums reproduce the layout's pen
positions, or return positions rather than advances so the question cannot
be asked wrongly.

**What a test should assert.** In `compose_test`: for a run containing a
space, the prefix sums of `measureRun` place the last glyph's right edge
at the same x as `forEachPlacedGlyph`'s last `rest.x + advance` over the
same style and text — checked for one word, two words and a leading
space, so a fix cannot satisfy the single-word case alone.

## `Effect::shader` and `Effect::uniform` drop an undeclared uniform silently

**What the code does.** `Effect::shader` writes its constants straight
onto an `SkRuntimeShaderBuilder`, and `Effect::uniform` on a `shader()`
effect appends any name at all to `m_bound` for `buildFilter` to write the
same way (`Compose.cpp`). `SkRuntimeEffectBuilder::BuilderUniform::
operator=` answers a name the effect does not declare — or one whose
declared size is not four bytes, which is every `float2`, `float4` and
array uniform — with `SkDEBUGFAIL` and no write. This Skia is built
without `SK_DEBUG` (`SkUserConfig.h` leaves it commented out and nothing
in the build defines it), so `SkDEBUGFAIL` expands to nothing: the value
is dropped, the effect paints with a zeroed uniform, and no diagnostic is
produced anywhere.

**What it was evidently intended to do.** The seam documents Material's
guardrail and claims it for itself: "a name the effect doesn't declare as
a float uniform is warned and IGNORED (never a debug abort — one sketch
typo must not kill the hot-reload host)", and `Effect::uniform` performs
exactly that check for `directionalBlur()` and `blur()` recipe names and
for a null Output. The `shader()` path is the one that takes arbitrary
names, and it is the one with no check.

**What a test should assert.** In `compose_test`: `Effect::shader(fx,
{{"nope", 1.0f}})` and `Effect::shader(fx).uniform("nope", &out)` each
emit one diagnostic and leave the built filter equal to the one built
without the binding; the same for a name the effect declares at another
type. `Effect::uniform` on a `shader()` effect should also declare no
volatility for a binding it rejected, since an ignored binding that still
marks the node live costs a repaint per frame forever.
