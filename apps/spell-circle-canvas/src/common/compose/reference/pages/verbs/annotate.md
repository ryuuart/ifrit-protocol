---
kind: verb
library: SigilCompose
name: annotate
qualified: sigil::compose::Element::annotate
header: sigilcompose/core/Text.h
group: Content
python: sigil.compose.Element.annotate
status: stable
---

# annotate

A reading set beside the type — furigana over a compound, emphasis dots
down a column, a gloss under a phrase.

## Description

```cpp
text(passage, body)
    .block({.writingMode = weave::WritingMode::kVerticalRL})
    .annotate({.where = weave::selectors::text(u8"漢字"),
               .unit = weave::Unit::Selection,     // group ruby
               .readings = {u8"かんじ"},
               .style = furigana});
```

**A reading is PART OF THE TEXT** rather than a thing standing next to
it. Where it reserves, the band it occupies goes into the base's strut
BEFORE the base is broken, so the base is laid out once with the room
already there and the readings are then placed on the result.

**Mono, group and jukugo ruby are the `unit` choice**, and a base that
breaks across a line or a column splits its reading with it, in
proportion to the base's advance either side.

**`kit::annotate` is the other half of the idea.** Marginalia, word
labels and callouts belong there — a sibling that reserves nothing and
reads the finished text.

## See also

[`mark`](mark.md), `Annotation`, `reserve`, `kit::annotate`.
