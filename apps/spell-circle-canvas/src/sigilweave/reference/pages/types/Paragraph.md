---
kind: type
library: SigilWeave
name: Paragraph
qualified: sigil::weave::Paragraph
group: The document model
status: stable
---

Owns styled UTF-16 text and its incremental analysis and shaping
pipeline. Mutations invalidate only the necessary analysis: re-analysis
resolves unchanged words through `FontContext`'s content-addressed shape
cache, so an edit normally sends only the changed word back through
HarfBuzz.

## Description

### What a mutation costs

Every member below that changes the text, the styles or the break
settings marks the paragraph dirty in one of two grades, and the grade is
the whole of what the next analysis pays.

A TEXT or STYLE edit re-runs segmentation and re-derives the word list.
Unchanged words resolve through the shape cache, so the cost is the
changed word rather than the paragraph.

A PAINT edit moves span boundaries and nothing else: analysis — words,
scripts, bidi — stands, and the shaped prefix only needs its segments
re-derived. `Paragraph::setPaint` is the one that does this, and its cost
is bounded by the shaped prefix rather than by the text.

### Where a line may break

Break opportunities are decided during ANALYSIS, which is why the four
members that shape them belong to the paragraph rather than to a layout
pass. Each of them re-runs the ICU segmentation.

`Paragraph::setSoftHyphenBreaks` sets whether a soft hyphen (U+00AD)
opens a break opportunity. True (the default) splits the word there, so a
breaker may end a line at the hyphen and render `Word::hyphenGlyph`.
False fuses the word back into one unbreakable `Word` whose text spans
the hyphen: no breaker can split it, no hyphen is ever rendered, and the
word wraps or overflows whole. The fused word is a different string from
either half, so it is a different content-addressed shaping entry;
toggling back finds both sets of entries warm.

`Paragraph::setHyphenator` sets what is asked where INSIDE a word may
break. Empty (the default) leaves the soft hyphens the author typed as
the only discretionary opportunities. A hyphenator is consulted once per
word during analysis, in the shaping style's own language tag, and the
offsets it names become break opportunities carrying a hyphen glyph
exactly as a typed soft hyphen does. It has no effect while soft-hyphen
breaks are off, because that setting is the switch for the whole
discretionary idea. The paragraph KEEPS the hyphenator: every re-analysis
asks it again, and those happen for as long as the text is edited, so a
hyphenator built for one document need not be kept alive by anyone else.

`Paragraph::setKinsoku` sets which characters may not stand at a line's
edge. A prohibition is a break opportunity that is never opened, so it is
decided during segmentation like every other break opportunity, and
neither breaker learns a rule.

`Paragraph::setLineBreakLocale` sets the TAILORING the line segmentation
runs under: a BCP 47 tag, optionally carrying ICU's line-break keyword —
`"ja@lb=strict"` is the strict Japanese rule set a printed page is set
under, `"zh@lb=loose"` the loose Chinese one — and empty is the
untailored behaviour a text that says nothing gets. It is where a
script's own prohibitions come from before any table does: a tailoring
the segmentation applies is a boundary that never opens, so nothing
downstream learns a rule. A `KinsokuTable` stays the seam for a HOUSE's
additions on top of it.

Three of the four have a caller above them. `layoutParagraph` sets the
soft-hyphen switch and the hyphenator from `HyphenationOptions` and the
prohibitions from `ParagraphLayoutOptions::kinsoku` before it analyzes,
so callers who go through it never call them directly.

### Telling one paragraph, and one word list, from another

`Paragraph::identity` is THIS PARAGRAPH, TOLD APART FROM EVERY OTHER — a
number issued once when it is built and never issued again, so a cache
keyed on it cannot be answered for a different paragraph that happens to
stand where a freed one stood.

`Paragraph::wordRevision` is A NUMBER THAT CHANGES WHENEVER THE WORD LIST
CAN HAVE CHANGED — an edit, a restyle, a change of break settings.
Anything that keeps an answer computed from this paragraph's WORDS keys
on it, and a change of content is then a miss rather than a stale answer.
It is not `Paragraph::revision`, the text revision, which counts edits
alone and stands still while a style or a break setting moves every word
in the paragraph. Shaping more of the text does NOT change it: a word's
advance is settled when the word is shaped and never moves after, so an
answer computed over the words a pass had shaped stays the answer.

`Paragraph::revision` is the text revision itself. Every text mutation is
recorded under it, so external structures can keep UTF-16 ranges in sync
without wrapping every edit call. History is bounded and, when it fills,
the older half is discarded in one go — so the lookback a consumer can
count on is half the cap, not the cap. A consumer that falls further
behind than that must rebuild its ranges, which is what
`Paragraph::editsSince` answering false is telling it.

### Analysis, and how little of it a pass has to run

`Paragraph::ensureShaped` runs segmentation and shaping if anything
changed since the last call.

`Paragraph::ensureAnalyzed` is segmentation only — ICU boundaries, bidi,
scripts, no HarfBuzz work: `Paragraph::words` gets its break and
direction structure but no glyphs or widths yet. `ParagraphLayout` drives
shaping lazily from here, so a paragraph that overflows its geometry only
ever shapes the words that can actually land.

`Paragraph::ensureShapedTo` lazily shapes words ascending and
idempotently — the breakers call it just ahead of their frontier. A
frontier that has already passed the requested word count answers there,
without a call: both breakers ask this once per word they consider, and
on all but the few words that actually advance the frontier the answer is
that there is nothing to do.

`Paragraph::sentenceStarts` is ICU sentence segmentation, run on first
call and reused until the text changes: a paragraph nobody asks never
runs the pass, and one whose text is unchanged runs it once no matter how
many frames read it. Style and paint edits leave it valid. It is
independent of `Paragraph::ensureAnalyzed` — no shaping, no words, no
fonts are involved.

### Measuring

`Paragraph::strut` returns the line-height inputs of the FIRST span.
`Paragraph::strutAt` returns the same, measured from the first span the
given UTF-16 offset falls in — A BLOCK'S OWN STRUT, which is what its
pitch is measured from. A text of one style answers identically wherever
it is asked, which is why a layout that says nothing about blocks lays
out exactly as it always did.

`Paragraph::naturalWidth` is the unwrapped single-line width: content
plus inter-word glue, the final word's trailing whitespace excluded. It
shapes on demand.

### Building one

`Paragraph::appendText` and `Paragraph::appendPlaceholder` append
directly. `ParagraphBuilder` is the SkParagraph-style builder for the
push/pop idiom, thin sugar over `Paragraph::appendText`.

Inline slots map to records by occurrence order, so direct text edits
should not add or remove object-replacement characters.
`Paragraph::setPlaceholder` resizes a slot and invalidates layout while
leaving real words cache-hot.

`Paragraph::setStyle` applies shaping and paint configuration to a UTF-16
range, splitting spans as needed, and re-shapes only words whose shaping
inputs actually changed — the rest hit the cache.

`Paragraph::setPaint` has a batch form taking several `CharRange` values:
restyling N marker ranges costs one span-list rebuild, not N quadratic
ones. Ranges may arrive unsorted or overlapping; they are sanitized
internally.

## See also

`ParagraphBuilder`, `RichText`, `Word`, `FontContext`,
`layoutParagraph`
