---
kind: verb
library: SigilCompose
name: paragraph
qualified: sigil::compose::Element::paragraph
header: sigilcompose/core/verbs/Cascade.h
group: The cascade
status: stable
---

# paragraph

How every paragraph under this node is set, as a PARTIAL — the
paragraph half of what [`font`](font.md) does for the type.

## Description

**This is the ONE spelling of every paragraph field.** The leading, the
alignment, the justification, the hyphenation, the tab stops, the
first- and last-line indents, widows and orphans, balanced ragging, the
breaking strategy, the last line, the writing mode, the line-break
locale and the line tables are all fields of `weave::ParagraphBlock`, and
each is written here.

```cpp
element.paragraph({.alignment = weave::TextAlignment::kCenter});
element.paragraph({.writingMode = weave::WritingMode::kVerticalRL});
```

**The fields it names override the setting inherited and the rest
inherit**, exactly as the font does, and it cascades from wherever it
is written.

## Longhands

Each longhand is this verb with the fields it names, so the two
spellings fold into one partial and the later statement wins field by
field. A rule states them as a node does.

| Longhand | CSS | Field |
|---|---|---|
| `lineHeight` | `line-height` | `leading` |
| `textAlign` | `text-align` | `alignment` |
| [`textIndent`](textIndent.md) | `text-indent` | `firstLineIndent`, in pixels once resolved |
| `writingMode` | `writing-mode` | `writingMode` |
| `hyphens` | `hyphens` | `hyphenation` |
| `textWrap` | `text-wrap-style` | `lineBreak` and `balanceRaggedLines` |
| `textJustify` | `text-justify` | `justificationMethod` |

**`textWrap` writes two fields every time**, so a later `Pretty` undoes
an earlier `Balance`. `Auto` and `Stable` fill each line in turn;
`Pretty` weighs the whole paragraph at once; `Balance` weighs it and
then sets it in the narrowest measure that keeps its line count, which
evens the rag.

**`textJustify` is read only where `textAlign` justifies.** `Auto` opens
the word gaps, the gaps between ideographs and whatever letter and glyph
passes the justification is tuned for; `InterWord` opens the word
separators alone; `InterCharacter` opens every grapheme cluster and
every separator by the same amount, each up to a cap, and hands what the
cap holds back to the separators; `None` sets a justified line at its
start.

```cpp
element.textAlign(weave::TextAlignment::kJustify)
    .textJustify(TextJustify::InterCharacter)
    .textWrap(TextWrap::Pretty);
```

**A leaf that wrote a whole `weave::ParagraphStyle` inherits nothing**
for that block; a block named through `paragraphStyles(names)` is that
name's partial laid over the paragraph setting in force.

**A VERTICAL leaf measures on the other axis.** Its main extent is its
height, its intrinsic width is one column pitch per column, and for
`Align::Baseline` it reports its first character's baseline. Per
character the mode is UTR#50's — ideographs upright with their `vert`
forms, Latin on its side — and a run that wants otherwise says so in
its own style. A run on a path ignores the mode altogether: its
baseline is its own geometry and has no columns to advance, so setting
both warns once and the path wins.

## See also

[`font`](font.md), `weave::ParagraphBlock`, `weave::JustificationMethod`,
`paragraphStyles`, `textOnPath`.
