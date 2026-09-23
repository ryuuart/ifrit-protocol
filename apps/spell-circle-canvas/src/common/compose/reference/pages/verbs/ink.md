---
kind: verb
library: SigilCompose
name: ink
qualified: sigil::compose::Element::ink
header: sigilcompose/core/verbs/Font.h
group: Paint
status: stable
example: ink_verb
---

# ink

What text under this node is set in, and what every mark that names no
colour is painted in. It is CSS's `color` spelled alone, and like CSS's
it INHERITS: a node that sets it hands it to everything under it,
wherever the code that built a child ran.

It takes everything [`fill`](fill.md) takes. A plain colour is the ink
lane as it has always been — it eases, it is read back by
`Fill::currentInk()`, and a custom property can stand in for it. Any
other paint — a ramp, a sprite, a recipe, SkSL — inherits the same way,
and the glyphs under it are painted with it.

<!-- example: ink_verb -->

## Syntax

```cpp
Element& ink(material::Color colour);
Element& ink(VarRef reference);
Element& ink(SurfacePaint paint, PaintBox box = PaintBox::Element);
```

```python
def ink(self, value: ElementInkLike, box: PaintBox = ...) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `material::Color` | The colour outright. | `hexColor(0xRRGGBB)`, or the four channels |
| `VarRef` | The custom property to read it from: `ink(var("accent"))`. | [`VarRef`](../../VALUES.md#the-custom-properties), through `compose::var` |
| `SurfacePaint` | Everything a surface takes — a `Fill`, a material paint, a recipe — because the ink and the fill dress the same kinds of thing. | [`SurfacePaint`](../types/SurfacePaint.md) |
| `PaintBox` | The rectangle a paint's unit square is stretched over: the element's own, the subtree's, the canvas, or each unit of a passage. | [`PaintBox`](../types/PaintBox.md): `Element`, `Subtree`, `Canvas`, `Glyph`, `Cluster`, `Word`, `Line`, `Sentence` |

A `Fill` holding one colour is that colour, and a box handed with it is
accepted and changes nothing: a colour has no unit square to stretch. A
`Paint` holding one colour
is a paint whose picture happens to be flat, and it overrides the glyphs
of a leaf set in a style of its own, which an inherited colour does not
reach.

Two spellings an ink paint cannot hold: a cascade reference and a live
fill binding. A reference to the ink is the ink, and a bound fill has no
paint to inherit, so both leave the ink where it was. An EMPTY paint
states the lane and holds nothing, which clears an ancestor's paint and
leaves the colour in force standing.

## Description

**Three things flow down the tree**: the font, the ink — this — and the
custom properties. Everything else a node says about itself stays on
that node, which is CSS's own split between the properties that inherit
and the ones that do not. A node that leaves the ink unset takes the
nearest ancestor's, and the root's is the composer's inherited default.

**What reads it.** Every text leaf that names no colour of its own; every
decoration whose paint is unnamed, which is what makes `stroke(2)` a
stroke in the colour in force; and `Fill::currentInk()`, wherever a slot
demands a fill and the answer is "whatever the ink is". A paint program
reads the same value off `PaintContext::ink`, and the paint form off
`PaintContext::inkPaint`.

**A colour eases and does not relayout.** A node whose ink changes under
a `transition` eases it, and every descendant with no `transition` of its
own follows — repainted while the colour moves, cached again once it
settles. A PAINT snaps, as
a fill does: there is no ramp between two pictures. A BOUND ink is not
offered: a live value inherited from above would make the whole subtree
under it volatile.

**However the change was written.** What the lane watches is the colour
the cascade RESOLVED for the node, not the one the node spelled, so a
class toggled, a rule that started matching or a custom property given a
new value eases exactly as this verb does. A node resolving its colour
for the first time has nothing to ease from, so that colour simply
stands.

**An inherited ink eases under the node's own transition.** A node with
no `transition` of its own takes the colour arriving from above as it
arrives, so it follows an ancestor's ramp while that ramp runs. A node
with a `transition` of its own eases the ink it inherits with a lane of
its own, over its own duration, toward the colour the ancestor is headed
for — never toward the colour the ancestor's ramp stands at this frame.
That is CSS's rule, and the one a fill already follows.

**A paint over the element's own box lands on the text metrics.** A
text leaf's own box is its text-metric box — x across the widest line, y from
the first line's cap top to the last line's baseline. That mapping is
what makes a chrome wordmark work at any size: author the ramp once in
the unit square and its horizon crosses the capitals whatever the type
size, with no hand-positioned gradient. A vertical passage has no cap
band to hang it on, so the unit square maps onto the column block
instead and the ramp reads down the page.

**A paint can restart on each unit of the passage.** Given a text unit
— `PaintBox::Glyph`, `Cluster`, `Word`, `Line` or `Sentence`, Weave's
own words — the unit square lands on each such unit's own text-metric
box instead: across the unit's advances, from its cap top down to its
baseline, so a ramp runs afresh through every letter, every word or
every line. Over the element's own box, the default, the paint is laid
once across the passage. Each unit is one more draw, so a passage that
names none pays nothing:

```cpp
text(u8"EMBER GLASS").ink(ramp, PaintBox::Glyph);
```

A text unit needs a passage to cut: on a node that is no text leaf it
is dropped, with a warning once, and the paint is stretched over the
element's own box; a rule and a span keep theirs, since they land on
text. `Padding` and `Content` place a fill inside its box and read as
`Element` here, said the same way. An upright
letter in a vertical column has no cap band across it, so its box is
one em across the column and its advance down it. A letter on a
`textOnPath` curve takes the box the curve set it in,
and a letter in flight under a `textFx` track draws with its unit's
paint where the unit rests, sampled where the letter now is. The
decoration bands keep the passage's mapping, since a band spans a run
rather than a unit. A [`span`](span.md) takes the same box over the
range it finds, and so does a rule.

**Under a unit the passes draw band by band** across the whole
passage: every underlay — a shadow, the [`textStroke`](textStroke.md)
outline — then every fill, then every overlay, so one letter's outline
never lands on its neighbour's fill. A passage in one style stacks the
same way without a unit; one of several spans, drawn without a unit,
draws each span's passes together.

**Two boxes spread one paint across several elements.**
`PaintBox::Subtree` stretches the unit square over the box of the
element that stated the ink, so everything under it shows its own
slice; `PaintBox::Canvas` stretches it over the whole canvas, so
elements anywhere in the tree line up and moving one of them moves which
slice it shows.

**A live paint re-resolves per frame**, so a ramp bound to an output
moves under the letters without re-shaping them, and it composes with
the textFx tracks: a letter in flight is painted with it exactly as a
resting one is.

**A property that was never set leaves the inherited ink standing** and
says so once, as every silent no-op in this library does.

## Examples

- `reference/examples/ink_verb.cpp` — one panel taking the inherited
  ink and one setting its own, with the text, the stroke and the swatch
  in each following it.
- `reference/examples/ink_verb.py` — the same picture in Python.
- `reference/examples/inkPaint_verb.cpp` — one unit-square ramp
  painting the same word at two sizes.
- `reference/examples/inkPaint_verb.py` — the same picture in Python.
- `src/sketch/sketches/ink_units.cpp` — one ramp laid across a passage
  and restarted on every letter, word and line.

## See also

[`fill`](fill.md) for the node's own box and the same boxes,
[`textStroke`](textStroke.md) for the pass under the glyphs, `font` and
`paragraph` for the other two inherited lanes, `var` and `varDefaults` for
the properties an ink can be read from, and the *cascade* group on [the
verb index](../../VERBS.md).
