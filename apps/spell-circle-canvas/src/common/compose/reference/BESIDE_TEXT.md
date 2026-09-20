# Beside the text

A chapter of [TYPOGRAPHY.md](../TYPOGRAPHY.md), the type chapter of
[SigilCompose](../README.md).

`Composer::units` is what everything standing next to a passage is placed
from: one `TextUnit` per unit a selector addresses, in draw order, in the
composer's space.

```cpp
for (const TextUnit &u :
     composer.units("verse", weave::selectors::each(weave::Unit::Word),
                    weave::Unit::Word))
  ;  // u.rect, u.axis, u.pitch, u.ascent, u.range, u.style, u.lineIndex
```

`Beat` is the same rect under a schedule and needs an `fx()` track to
report it; this needs none, and carries the baseline (or the column's
axis), the pitch, the face's band, the writing mode, the vertical form,
the text range and the style beside each rect. It is read off the
placement rather than measured again, so a unit whose base broke across
two lines reports TWO entries, on the two lines — and the pieces are
known to be one base by the unit they came from, not by their text
ranges, because the space a line breaks at is placed on neither side of
the break.

Two things are built on it, and which one a case wants is decided by one
question — does the annotation need ROOM?

- **`Element::annotate`** is part of the text. Its band is put into the
  base's strut BEFORE the base is broken, so the pitch opens once and the
  reading is placed on the result; nothing chases anything. Ruby and
  kenten are `Annotation` values, and mono, group and jukugo ruby are the
  UNIT choice and nothing else — `weave::Unit::Cluster` for mono and for
  jukugo, `weave::Unit::Selection` for group, because a compound is the
  extent the selector named and not a break opportunity. `kit::ruby` and
  `kit::kenten` are the two stock spellings. The PLACEMENT is
  SigilWeave's — the band a reading needs, where it stands against its
  base, and how a broken base shares it out over its pieces, one reading
  or many — and this tier only says which units are annotated with what.
- **`kit::annotate`** is a sibling that reserves nothing and stands beside
  the finished text — marginalia, word labels, callouts. It resolves at
  describe time from the layout the last draw left standing, on the same
  terms as the instruments, so a text that reflows wants a re-describe for
  its annotations to follow.

`kit::annotate` says where an object goes in one of two ways, and they are
one mechanism with two placement values. `kit::Beside` does the arithmetic
of the READING DIRECTION — before or after the unit across it, at its
start or its end along it — so a note above a line and a note right of a
column are the same declaration. `kit::Anchored` hands the arithmetic to
the caller: the object is still tied to a text position and still moves
when the text reflows, but it stands at an offset the author states.

```cpp
kit::annotate(composer, "verse", weave::selectors::text(u8"Ishmael"),
              weave::Unit::Word,
              {.horizontal = kit::Anchored::From::Frame, .offset = {-44, 0}},
              [&](const TextUnit &u) { return figure(u); });
```

`kit::Anchored::horizontal` and `kit::Anchored::vertical` name what each
AXIS is measured from — the unit, the whole line it landed on, or the text
node's frame — separately, because the commonest anchored object in print
takes its x from the frame's edge and its y from the word it belongs to.
`kit::Anchored::at` picks the point of those rects to measure from, as
fractions, and `kit::Anchored::offset` how far. The offset is in the
composition's axes rather than the reading direction's, which is the whole
difference between the two values.

`kit::rules` cuts a rule or a shade to the extent a block's lines actually
occupy and `kit::bullets` hangs markers in a hanging indent.

A BLOCK'S OPENING LETTER is not kit at all: `Element::initialLetter` states
how many lines of cap the initial spans and the layout derives the size
from the block's own pitch and the face's own cap height, then seats the
letter's baseline on the line it sinks to and cuts the notch the following
lines wrap. An ORNAMENT — an illuminated frame, a flourish, a combined
letter — is the other case and stays an ordinary exclusion: a keyed element
with a silhouette, and a body that flows around that key, so the opening
lines follow that outline rather than its box.

```cpp
text(passage, bodyType).initialLetter({.lines = 3, .margin = 6.0f});

ornament.key("versal").absolute().left(Dimension(0.0f)).top(Dimension(0.0f));
text(rest, bodyType).flowAround("versal", 6.0f);
```

A NESTED STYLE — the opening of a paragraph set differently from the rest
of it — is a selector and a span restyle, and `kit::NestedStyle` is the
statement of where it stops: `kit::NestedStyle::Until::Words` counts the
paragraph's own words, `Until::Characters` counts a character range, and
`Until::Delimiter` runs through the first occurrence of a mark, inclusive.
`kit::nestedRun` answers the `weave::Selector` that means and
`Element::spanStyle` does the work, so an initial and the small caps that
carry a paragraph out of it are two properties of one leaf.

```cpp
const kit::NestedStyle opening{.count = 3, .style = smallCaps};
text(passage, bodyType)
    .initialLetter({.lines = 3, .margin = 6.0f})
    .spanStyle(kit::nestedRun(opening), opening.style);
```

Because it is a selector, the run re-resolves with the text: an edit that
adds a word before the delimiter extends it, and one that removes the
delimiter leaves it covering nothing rather than covering the paragraph.
