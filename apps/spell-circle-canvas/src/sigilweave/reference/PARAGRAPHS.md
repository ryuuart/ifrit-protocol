# SigilWeave — the document model

The chapter on what a passage of text IS before anything lays it out:
the document a `Paragraph` is, the value a `RichText` is, the atomic
`Word` the analysis produces, the granularity `Unit` addresses it at,
and the language facts a hyphenator and a kinsoku table bring in from
outside. `README.md` beside the library is the front page; the layout
pass that consumes all of this is `reference/LAYOUT.md`.

## Paragraph: the document

The document model is UTF-16 text carrying normalized style spans,
optional inline placeholders, and a horizontal or vertical-RL writing
mode. Build one with `ParagraphBuilder` (fluent `ParagraphBuilder::addText`
/ `ParagraphBuilder::pushStyle`) or append directly, then edit it in
place — `Paragraph::replaceText`, `Paragraph::setPaint`,
`Paragraph::setStyle`, `Paragraph::appendPlaceholder` — with the shape
cache absorbing every unchanged word. Hand the finished `Paragraph`,
plus a `FontContext` and a flow geometry, to `layoutParagraph`. The words
the analysis produces are `Word` and its neighbours; the styling types
are the type vocabulary; the range-query and marker conveniences are
`Query` and `MarkerSet`.

Every member of `Paragraph` has its own entry on that type's page.

`StyleSpan` is the style applied to a contiguous UTF-16 range. Spans are
kept normalized: sorted, non-overlapping, covering the whole text.
`CharRange` is the UTF-16 code-unit range into a paragraph's text, end
exclusive — the common currency between the query layer, scoped searches
and batch restyling.

`WritingMode` is the flow direction the paragraph is shaped for.
Vertical-RL is the CJK book layout: characters run top to bottom, columns
right to left, and the layout geometry must match — columns with
`dir=(0,1)`, which is what `VerticalBlockFlow` supplies.

## RichText: mixed text as one value

MIXED TEXT AS ONE VALUE: `RichText`, a passage described as runs and the
styles they are set in, and `rich()`, which starts one.

A paragraph whose words are not all set the same way is a VALUE here,
not a document to be marked up. There is no markup language: a run of
text carries a style, or the name of one, and that is the whole
vocabulary. Whatever else a passage needs — a colour on the numbers, a
weight on one phrase — is asked for by SELECTION after the fact, so the
content stays content and the type treatment stays in one place.

`Paragraph` is the other end of this: a document built span by span, with
an edit log and a shaping cache, for the passage too custom for a value.
A rich text is what a caller who only wants to SAY what the text is
writes instead, and appending its runs to a paragraph in order is the
whole of turning one into the other.

```cpp
RichText passage = rich(base)
                       .add(u8"Signal ")
                       .add(u8"woven", accent)
                       .add(u8" through ")
                       .add(u8"noise", mono);
```

`RichText::add(utf8)` sets a run in the base style; `add(utf8, style)`
sets it in its own, whole; `add(utf8, partial)` changes the base in the
fields that partial names and leaves the rest; `add(utf8, name)` sets it
in a class looked up by NAME. Runs are appended in order and concatenate
into one passage — nothing is inserted between them, so the spaces are
the author's.

**Why it is a value.** Two rich texts describing the same runs in the
same styles are EQUAL, so a caller that rebuilds its text every frame can
ask whether the text actually changed and shape nothing when it did not.
A `Paragraph` cannot answer that question — it is a document with an
identity and an edit history, and a freshly built one reads as new
content however familiar its words are.

**Names** resolve through a `TypeSheet` — the type half of a sheet —
which `RichText::styles` supplies. A name the sheet does not register
resolves to the base handed to `rich()`: the base is this text's one
default, and a misspelled name shows as content set in it rather than as
content that did not draw. Resolution happens as the run is added, and
again over every named run when `RichText::styles` arrives, so the two
may be written in either order and the finished value holds real styles
rather than a reference to a registry that may since have gone. A host
that offers a sheet AMBIENTLY to everything described in a scope asks
`RichText::hasStyles` and calls `RichText::styles` itself when the answer
is no; a sheet the value already carries always wins.

**The base itself may be left unsaid.** `rich()` with no argument is a
passage that states no default at all, which is not the same as one whose
default happens to be a default-constructed style: the first expects to
be given a base by whatever sets it, the second has named one.
`RichText::hasBase` is the difference, and every run keeps the partial it
was written with in `RichText::Run::over` so a host that later supplies
the base can resolve the runs against it rather than over a style they
were never written against.

`RichText::Run::over` is THE PARTIAL THE RUN WAS WRITTEN WITH — its own,
or the copy its class resolved to out of the sheet. Empty when the run
was written with a whole style, or with none at all. It is kept because a
partial is the only form a run can be re-resolved from: a host that
supplies a base the passage never had overlays this onto it, where the
already-resolved `RichText::Run::style` would carry the old base's
fields. `RichText::Run::total` is whether that style is the run's OWN
WHOLE style, stated outright rather than resolved out of the base: true
for a run written with a `TextStyle`, false for one written with a
partial, with a class name, or with nothing — which all inherit, and all
carry an empty `over` unless a partial reached them.

### An inline slot

`RichText::slot` reserves an INLINE SLOT: a box of blank space woven into
the flow, and the name whatever is placed in that space answers to.

The reserved box is ONE UNBREAKABLE WORD: a line never breaks inside it,
and a box taller than the type opens the lines of its BLOCK. The room is
in the strut before anything is broken, because bands are asked of the
geometry before anyone knows which words land on them — a depth found
afterwards would be a depth decided after the break it decides.

`baselineDrop` is how far the box's BOTTOM sits below the baseline — 0
stands it on the baseline like an inline image, and about the face's
descent centres a pill on the x-height.

The slot occupies one code point (U+FFFC), so it counts as a cluster,
falls inside the ranges a selection names, and takes its turn in anything
that steps over units exactly as a letter does. The names are this
value's own and are matched in declaration order against the placeholder
boxes the layout reports; nothing outside the passage can reach one.

### What equality compares

Two values are equal when the base — whether one was named at all
included — the runs, their resolved styles, the partials and the names
they were written with all match: the question a caller asking "is this
the same text?" needs answered.

The style SHEET is deliberately not compared: a name is resolved as it is
added, so two values that resolved to the same styles describe the same
passage however they got there, and an entry neither of them names cannot
make them differ. Whether a base was NAMED is compared, because a passage
waiting for one is not the passage that settled on the default.

## Word: the atomic layout unit

A `Word` is the text between two line-break opportunities, measured as
content plus trailing glue, carrying its bidi level and break flags, and
— once shaped — its segments, each a cache-shared `ShapedWord` placed at
an offset in one of the forms a vertical column can take.
`WordSegmentList` is the segment storage: one segment lives inside the
object and only a split word allocates, behind bytes whose container type
the library alone sees.

`SegmentForm` is how a segment is placed relative to the flow direction.
`SegmentForm::kFlow` is every segment of a horizontal paragraph; the
others only appear in vertical paragraphs, resolved from
`ShapingStyle::verticalForm` and UTR#50.

`WordSegment::textBegin` is the first UTF-16 unit of the text this
segment shaped, so a glyph's cluster — an offset inside that text — maps
back to a position in `Paragraph::text`. Length-changing case mapping
through `ShapingStyle::textTransform` makes the mapping approximate,
exactly as it does for the clusters themselves.

`Placeholder` is an inline object slot woven into the flow: the breakers
treat it as an unbreakable word of the given size and the layout reports
the rect where it landed, so callers can draw pills, icons or images
*inside* the text flow. It is anchored in the text as an
object-replacement character (U+FFFC) and matched to its record by
occurrence order.

`Word::tabAfter` says the trailing whitespace contains a tab (U+0009).
With `ParagraphLayoutOptions::tabStops` configured, both breakers replace
this word's glue with an advance to the next stop — greedy fits against
tab-resolved widths as it goes, Knuth-Plass scores every candidate line
at its tab-resolved width. Without tab stops, tabs measure as spaces.

`Word::hyphenBreak` says the content ended with a soft hyphen (U+00AD,
stripped from shaping): a discretionary break. `Word::hyphenGlyph` is the
cached shaped "-" to render when a breaker actually breaks there. A
hyphen the analysis refused to break at is interior to its word and sets
nothing.

## Unit: the granularity a passage is addressed by

Every question something asks of finished text — which glyphs a selection
covers, what a stagger steps over, what an annotation stands beside — is
asked at one of five sizes, and they are the sizes this engine already
segments at: the shaper's clusters, the Unicode leaf's word and sentence
breaks, the breaker's lines. Nothing in `Unit` names a size the engine
would have to invent.

`Unit::Cluster` is the one that keeps text correct: a base letter and its
combining marks, or the several glyphs an emoji sequence shapes to, are
ONE cluster and move together. `Unit::Glyph` is the raw shaping unit and
will separate those marks from what they sit on.

## Hyphenation: where a word may break, asked outside the engine

The analysis knows every break opportunity BETWEEN words — that is UAX
#14 — and none at all inside one, because where a word may be split is a
fact about a language rather than about Unicode. A `Hyphenator` answers
that one question and nothing else; the engine inserts the opportunities
it names and the breakers then decide, under the limits a block states,
which of them a line actually takes.

The kit ships Liang pattern sets and a caller's own implementation is a
peer of them: a dictionary, a server, a table of exceptions for one
document.

### HyphenationLimits

WHICH OF A WORD'S BREAK POINTS BECOME OPPORTUNITIES AT ALL.

Each of these is a fact about the word rather than about the line it
lands on, so they are settled during segmentation and the whole text
shares them: the word list either carries an opportunity or it does not,
and no block can conjure one the analysis did not open. The limits that
depend on the LINE — how many hyphens in a row, how ragged is ragged
enough, may the last word of a block break — are break decisions and live
on the block's own style.

### Hyphenator

The one question: given a word and the language it is set in, where may
it be broken?

Offsets are UTF-16 code units from the START OF THE WORD, strictly inside
it — 0 and the length are not break points — and ascending. The word
arrives without its trailing whitespace and without any soft hyphen the
author typed, which is already an opportunity in its own right.

The language tag is the shaping style's own (BCP 47, possibly empty). An
implementation that does not know it should answer nothing rather than
guess: an unbroken word is a ragged line, and a word broken by the wrong
language's rules is a misspelling.

`Hyphenator::breakPoints` is called during analysis, which runs on the
`FontContext`'s thread, once per word per analysis. It must be cheap and
it must be pure — the same word and tag answer the same way every time,
or the shape cache and the breakers will disagree about the same text.

### KinsokuTable

WHICH CHARACTERS MAY NOT STAND AT A LINE'S EDGE — kinsoku shori, the
Japanese line-breaking prohibitions, and the same idea wherever else a
script has one.

`KinsokuTable::notLineStart` holds the characters that may not OPEN a
line: closing brackets, the small kana, the sound marks, a full stop or a
comma. A break that would put one there is simply not a break: the
boundary is dropped during segmentation, so the character before it comes
down to the next line with it and no breaker has to know the rule. That
is "push-out", the resolution a reader expects.

`KinsokuTable::notLineEnd` holds the characters that may not CLOSE one —
the opening brackets — and works the same way from the other side.

Both are plain UTF-16 strings, one character per prohibition, because a
prohibition set is a fact about a language's punctuation and a caller's
own set is a peer of the ones the kit ships.

### HangingEdge and HangingTable

HOW FAR A CHARACTER MAY HANG PAST THE MEASURE — optical margin
alignment, and in a column the same rule under the name burasagari.

A line that begins with an opening quote or ends in a comma reads as
indented and as short, because the eye squares a margin on the mass of
the type rather than on its advances. Letting those characters hang
OUTSIDE the measure squares it again. Each entry is a fraction of that
character's own advance, so the rule scales with the type and needs no
per-size table.

`HangingTable::find` is a linear scan: a table is a handful of
punctuation marks, and a scan of a handful beats a hash of one.

## See also

`reference/LAYOUT.md` for the pass that consumes a paragraph,
`reference/TYPE.md` for the styles its spans carry, and
`reference/UNICODE.md` for the analysis underneath it.
