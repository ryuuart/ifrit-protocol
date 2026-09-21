# Mixed text

A chapter of [TYPOGRAPHY.md](../TYPOGRAPHY.md), the type chapter of
[SigilCompose](../README.md).

**There is no markup language.** Text that is not all set the same way is a
`weave::RichText` value plus selector styling, and the two cover different
halves of the problem: `weave::rich()` says what the CONTENT is, and
`spanPaint` / `spanStyle` say what a RANGE of it looks like.

```cpp
auto p = weave::rich(base)
             .add(u8"Signal ")
             .add(u8"woven", accent)
             .add(u8" through ")
             .add(u8"noise", mono);

text(p)
    .spanPaint(weave::selectors::regex(u8"[0-9]+"),
               sigil::weave::PaintStyle(SK_ColorRED))
    .maxTextLines(3)
    .textOverflow(u8"…");
```

`weave::RichText::slot` reserves an INLINE SLOT in the run stream — a box of
blank space the flow weaves in, and the name a child of this text node is
laid out into:

```cpp
text(weave::rich(body)
         .add(u8"press ")
         .slot("key", {28, 18})
         .add(u8" to continue"))
    .children({box().key("key").fill(ink).borderRadius({4})});
```

The reserved box is one UNBREAKABLE word: no line breaks inside it, and a
box taller than the type opens the lines of its BLOCK — bands are asked of
the geometry before anyone knows which words land on them, so the room is
in the strut before a break is decided rather than found afterwards. The child is an ordinary
subtree — it animates, caches and hit-tests like any other element — and it
re-lands wherever the placeholder lands when the text reflows. It is a
POSITIONED subtree: the placeholder rect is its box, so flex layout does not
run inside it and its own children take explicit rects, exactly as under
`positioned()`.

The slot size is `{inline advance, cross-axis band}` in both writing modes.
With `weave::WritingMode::kVerticalRL`, `{80, 30}` reserves 80 px down the
column and gives its child a physical box 30 px wide and 80 px tall, centred
across the column axis. The horizontal baseline drop does not apply there.
Changing writing mode does not require exchanging the declared dimensions.

**A TEXT SLOT IS NOT A MOUNT SLOT.** `slot()` and `Composer::renderSlot` name
a hole a HOST fills from outside the description, and those names live in one
registry for the whole composition. These names live in the rich-text value
alone and are matched against the `key()` of that text node's own children —
so two captions may both reserve a slot called `"icon"` without colliding,
and neither is reachable by `renderSlot`. A child keyed for a slot the
content does not declare draws nothing and says so once; a slot the geometry
could not place is silent, like every other word that did not fit.

`weave::RichText::add` takes a run in the base style, a run in its own
whole `sigil::weave::TextStyle`, a run in a PARTIAL `sigil::weave::Type`
that overrides the base field by field, or a run under a NAME — a class
— resolved through a `sigil::weave::StyleSheet` supplied by
`weave::RichText::styles` as a `sigil::weave::TypeSheet` or in force on the
tree through `Element::styleSheet`, when the leaf is shaped. An explicit sheet beats
the one in force, and a name the sheet does not register resolves to the base
`weave::rich()` was given, so a misspelling shows as content set in the
default rather than as content that did not draw. A rich text started
with NO base, `weave::rich()`, is an inheriting passage: it is set in the
font and ink in force where the leaf lands in the tree, a run added with
a partial keeps the inherited face and size in every field it does not
name, and only a run added with a whole style keeps the style it was
written with — the same rule `text(utf8)` follows for a plain leaf, in
[CASCADE.md](CASCADE.md). `weave::RichText::runs` and
`weave::RichText::base` read the finished value back.

**It is a comparable value, and that is the point.** Two rich texts with
the same base and the same runs in the same styles are equal, so a
component that rebuilds its spans every describe prunes exactly like a
static leaf. The `text(std::shared_ptr<sigil::weave::Paragraph>, options)`
overload cannot answer that question — a fresh pointer is a fresh identity
and reads as changed content every time — which is why it stays the escape
hatch for the passage too custom for either verb, not the way to set two
colours in a sentence.

**Selector styling.** `Text::spanPaint` and `Text::spanStyle`
restyle whatever the SAME `selectors::` selectors the tracks use address, on
every content form alike — plain text, `weave::rich()` spans and the paragraph
overload. They are ordered by **what they are allowed to disturb**:

| verb | changes | re-shapes |
| --- | --- | --- |
| `spanPaint` | paint alone — a colour, a shader, an underline, a glow pass | never |
| `spanStyle` | anything a `sigil::weave::TextStyle` holds | the words its range covers — unless the only change is advance-invariant axes |

Both are ordered lists — a LATER DECLARATION WINS on overlap, so a broad
rule followed by a narrow exception reads in the order it is written — and
both are comparable values, so a re-described list prunes and only a
changed one re-resolves.

That rule holds **per dimension** where the two verbs meet. The paint of a
range is `spanPaint`'s to say, so a `spanStyle` over text an earlier
`spanPaint` coloured applies its other dimensions and leaves that colour
standing: either declaration order draws the same passage, and neither
verb has to know what the other declared. A `spanStyle` with no
`spanPaint` under it paints with the style it is given, as ever.

The middle ground is `spanStyle`'s own. A style that differs from the text
it covers only in variable-font axes the face carries advance-invariantly
does not re-shape: a grade is advance-invariant *by construction* — it
thickens a letter without moving the letter after it — so it is exactly
the restyle that can keep the layout the paragraph already has, and the
restyle keeps it:

```cpp
sigil::weave::TextStyle graded = base;
graded.variation("GRAD", 780);
text(copy, base).spanStyle(weave::selectors::regex(u8"[0-9]+"), graded);
```

Such a restyle is carried as a track holding `fx::variableAxis`,
and inherits what that means. The coordinate is a `GlyphModifier::axis`, so it
goes through the same size-scaled ladder a driven axis does and composes
with entrances and loops instead of being hidden by them; and the leaf
then draws through the batched glyph path, where a span style's band
stands at its rest placement while the letters move. Anything else the
style changes — another face or size, an axis the face moves advances on,
an axis the text was shaped with and the restyle drops — is a reshape; and
an earlier axis-only restyle under a later reshaping one over the same text
re-shapes too, so the later declaration is the one that stands.

`spanPaint` and `spanStyle` resolve their selection as TEXT RANGES rather
than glyphs, because a restyle runs on the paragraph before there are glyphs
to point at: `weave::selectors::text` and `weave::selectors::regex` through weave's
query layer, `weave::selectors::word`, `weave::selectors::words` and
`weave::selectors::range` through the paragraph's own structure, `selectors::style`
through the named runs the content declared, and `weave::selectors::line` through
the layout. Two consequences follow. `weave::Selector::take` and
`weave::Selector::drop` slice glyphs inside a unit, which no text range can
express — an `weave::selectors::each` selector restyles its whole units and the
slice warns once. And a `weave::selectors::line` restyle costs a second layout
pass and addresses the layout of the text BEFORE the restyle: it does not
chase its own result, so a `spanStyle` that moves the line breaks leaves the
selection where the first breaking put it.

**Layout options, fluently.** `Element::block` states every layout-wide
field a passage inherits, as one partial: its alignment, its breaking strategy
(greedy or Knuth-Plass), its hyphenation, its last line, its justification, its
tab stops, the three tables a house's own setting is stated in
(`Block::kinsoku`, `Block::hanging`, `Block::mojikumi` with `Block::tsume`),
and the tailoring the segmentation runs under (`Block::lineBreakLocale`), which
belongs to the Paragraph and lands there the way `Block::writingMode` does.
`Text::textOverflow`, `Text::maxTextLines`, `Text::textLineMargin` and
`Text::textWillChange` are the leaf's own, set on any content form. The rest
of that struct — Knuth-Plass tolerance, line-metric overrides — stays behind
the paragraph overload, which takes the whole options value. **On that overload
the setters override FIELD BY FIELD**, and only the fields actually set:
everything a setter did not name keeps the value that was passed in.

A horizontal text leaf's measured height includes the room from its content
origin to the first line. A reservation before the line, leading, or space
before its first paragraph therefore remains inside the leaf's background
and above its following sibling. Baseline alignment reads the resulting
first baseline, including that room.

SigilWeave's `FEATURES.md` carries the control-by-control table, with the
verb or field on this side that reaches each one; it is the fastest
answer to "how do I set X".
