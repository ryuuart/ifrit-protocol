---
kind: type
library: SigilCompose
name: Dimension
qualified: sigil::compose::Dimension
group: Box model
status: stable
---

# Dimension

A LENGTH, in whichever of the twelve units it was written in: pixels,
points, a percentage of the parent, a percentage of the canvas either
way, a multiple of the font in force or of the root's, a multiple of the
line height or of the width of a figure, a custom property read where it
lands, a sum over several of those, or left for layout to decide. Constructing one from a bare number
gives pixels, so the common case reads as a number and nothing else.

## Anatomy

`Dimension::unit` is which of the twelve, and `Dimension::value` is the
number — except under `Dimension::Unit::Var` and `Dimension::Unit::Calc`,
where an id is bit-cast into the float and never read as one;
`Dimension::reference` reads a property's back.

The units, and what each is a fraction or a multiple OF:

| Unit | Written | Of what |
| --- | --- | --- |
| `Dimension::Unit::Px` | `24`, `24_px` | canvas pixels — absolute |
| `Dimension::Unit::Pt` | `9_pt` | printer's points — absolute, four pixels to every three |
| `Dimension::Unit::Percent` | `50_percent` | the PARENT's box, which is Yoga's own percent |
| `Dimension::Unit::Vw` | `6_vw` | the CANVAS's width, wherever in the tree the node sits |
| `Dimension::Unit::Vh` | `6_vh` | the CANVAS's height |
| `Dimension::Unit::Vmin` | `6_vmin` | the smaller of the CANVAS's width and height |
| `Dimension::Unit::Vmax` | `6_vmax` | the larger of the CANVAS's width and height |
| `Dimension::Unit::Em` | `1.5_em` | the node's own resolved font size |
| `Dimension::Unit::Rem` | `2_rem` | the root element's computed font size |
| `Dimension::Unit::Lh` | `0.5_lh` | the node's own line height |
| `Dimension::Unit::Ch` | `3_ch` | the advance of "0" in the face in force |
| `Dimension::Unit::Var` | `var("gutter")` | the length the nearest ancestor set under that name |
| `Dimension::Unit::Calc` | `2 * 1_em + 12_px` | each term against what its own unit measures, summed |
| `Dimension::Unit::Auto` | `autoDimension()` | nothing — layout decides |

`Dimension::relative` answers whether resolving the length needs
something the number does not carry: the font in force, a custom
property, or the canvas. It is everything but a pixel, a point, a
parent-relative percent and auto. A point is absolute — the PostScript
point, 72 to the inch, against the 96-per-inch pixel — so it carries
everything it needs and a change of font never moves it.

A `ch` IS THE FIGURE ZERO, not the average letter and not half the em:
the advance of "0" in the face in force at the size in force, which is
why a column written in it holds a known count of digits. A face that
carries no zero is measured as half its type size.

THE VIEWPORT UNITS ARE THE CANVAS, not the parent's box: the canvas the
composer renders into is the viewport, and `vw`, `vh`, `vmin` and `vmax`
are CSS's own words for a percentage of it. A poster whose margin is
`6_vw` keeps its proportions at every canvas size, however many boxes
deep it is written. `Vw`, `Vh`, `Vmin` and `Vmax` are resolved into
pixels before Yoga sees them, because a percentage of something that is not the
containing block is a thing Yoga cannot express.

THE FONT UNITS ARE SIGILWEAVE'S. `weave::Length` is the length a text
style is written in, with `weave::em`, `weave::rem`, `weave::lh`,
`weave::ch` and `weave::pt` and their `_em`, `_rem`, `_lh`, `_ch` and
`_pt` suffixes, and it converts here implicitly — so a padding written in
ems follows the type it surrounds, and a change to an ancestor's font
relays out everything measured in it. SigilWeave resolves a `ch` of its
own against half the type size, because a resolver holding numbers
rather than faces cannot measure a glyph; SigilCompose holds the face
and measures it.

## Arithmetic, which is CSS's calc()

`+`, `-`, `*` and `/` on lengths are CSS's `calc()`: `width(2 * 1_em +
12_px)`, `padding(var("gutter") / 2)`. Lengths in one unit combine in
that unit, so `2 * 1_em` IS `2_em` and a sum that cancels back to one
unit is that unit again. Lengths in several units are one SUM, held as a
`Dimension::Unit::Calc` and resolved to pixels by the cascade pass with
the font, the custom properties and the canvas in force where the node
lands — so a sum with an em in it follows the type exactly as the em
alone does. A number beside a length is pixels, as it is everywhere
here; a length is scaled by a number and divided by one, never by
another length.

A PERCENTAGE IN A SUM is a term the sum keeps until the property reading
it knows the extent the percentage is of: `top(percent(100) + 12)` on a
positioned child or on a `Text::textAttach` mark is its containing box's
bottom and twelve pixels more, and it follows that box when the box
changes. A transform origin reads a sum's percentage as a fraction of the
box and the rest as an offset from there. Flex layout cannot hold such a
sum, because Yoga's percent is a unit of its own, so a flex-laid property
holding one warns once and is laid out as auto while the declaration
keeps the sum as written; a percentage beside a zero of pixels is still
that percentage. A custom property read inside a sum must hold a length
that resolves where the sum does: not a percentage, not auto. Arithmetic
on auto and a division by zero are refused, as the `calc()` text refuses
both: each warns once and stands as `autoDimension()`.

Python spells the same arithmetic on `compose.Dimension`, with a number
or a length string on either side, and raises `ValueError` at the call
for each refusal where C++ warns and stands as auto.

## A length written as text

`compose::parseDimension` is the ONE place text becomes a length, so a
declaration in a rule's text and a length handed in from another
language are one grammar. Case does not matter and surrounding blank
space is ignored; a bare number is pixels.

| Text | What it gives |
| --- | --- |
| `"12"`, `"12px"` | pixels |
| `"9pt"` | points |
| `"50%"` | a fraction of the parent |
| `"10vw"`, `"10vh"`, `"10vmin"`, `"10vmax"` | a fraction of the canvas |
| `"1.5em"`, `"2rem"`, `"0.5lh"`, `"3ch"` | the font units |
| `"var(gutter)"`, `"var(--gutter)"` | a custom property |
| `"auto"` | layout decides |
| `"calc(2em + 12px)"`, `"calc((1em + var(--gutter)) / 2)"` | a sum, as the arithmetic above builds it |

A leading sign is the number's, either way: `"+12px"` and `"-12px"` both
read, as they do in CSS.

Nothing is answered for text the grammar does not cover — an empty
string, a unit no `Dimension` carries, a space between the number and
its unit, a length times a length, a percentage inside a sum — so a caller can tell an unreadable length from a length that
was never stated. `"inf"`, `"infinity"` and `"nan"` are in that set
rather than out of it: they are numbers the reader will take and
distances nothing can lay out.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `24` | C++ | pixels — the implicit constructor from a float |
| `24_px` | C++ | pixels, said out loud |
| `50_percent` | C++, or `compose::percent(50)` | a fraction of the parent |
| `6_vw` | C++, or `compose::vw(6)` | a fraction of the canvas's width |
| `6_vh` | C++, or `compose::vh(6)` | a fraction of the canvas's height |
| `6_vmin`, `6_vmax` | C++, or `compose::vmin(6)`, `compose::vmax(6)` | a fraction of the canvas's smaller or larger side |
| `9_pt` | C++ | SigilWeave's `weave::Length`, implicitly |
| `1.5_em`, `2_rem`, `0.5_lh`, `3_ch` | C++ | SigilWeave's `weave::Length`, implicitly |
| `compose::var("gutter")` | C++ | a `VarRef`, implicitly — the custom property's length |
| `compose::autoDimension()` | C++ | the default: layout decides |
| `24`, `24.0` | Python | pixels |
| `"50%"`, `"1.5em"`, `"9pt"`, `"3ch"` | Python | any length the text grammar reads |
| `"auto"` | Python | layout decides |
| `compose.percent(50)`, `compose.vw(6)`, `compose.vh(6)`, `compose.vmin(6)`, `compose.vmax(6)` | Python | the percentage units |
| `compose.em(1.5)`, `compose.rem(2)`, `compose.lh(0.5)`, `compose.ch(3)`, `compose.pt(9)` | Python | the font units and the point, as a `Dimension` |
| `weave.em(1.5)`, `weave.rem(2)`, `weave.lh(0.5)`, `weave.ch(3)`, `weave.pt(9)` | Python | the same, as a `weave.Length` |
| `compose.var("gutter")` | Python | the custom property |
| `compose.Dimension(...)`, `weave.Length(...)` | Python | direct |

In Python the whole of that column is the union `DimensionLike`.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Element::width`, `Element::height` | verb | SigilCompose |
| `Element::minWidth`, `Element::maxWidth`, `Element::minHeight`, `Element::maxHeight` | verb | SigilCompose |
| `Element::padding`, `Element::margin` | verb | SigilCompose — one, two or four lengths |
| `Element::gap` | verb | SigilCompose |
| `Element::flexBasis` | verb | SigilCompose |
| `Element::inset` | verb | SigilCompose |
| `Element::left`, `Element::top`, `Element::right`, `Element::bottom` | verb | SigilCompose |
| `Element::var` | verb | SigilCompose — the length a subtree reads under a name |
| `Element::fontSize`, `Element::letterSpacing` | verb | SigilCompose — a length a font holds, through `compose::fontLength` |
| `Element::lineHeight` | verb | SigilCompose — an absolute pitch |
| `Element::textIndent` | verb | SigilCompose |
| `Element::translateX`, `Element::translateY` | verb | SigilCompose — a percentage of the node's own box |
| `Corners` | value | SigilCompose — each radius; a percentage of the node's own box |
| `Well::width`, `Well::height` | field | SigilCompose |
| `Line::length` | field | SigilCompose |

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| `compose::percent`, `compose::vw`, `compose::vh`, `compose::vmin`, `compose::vmax` | function | SigilCompose |
| `compose::autoDimension` | function | SigilCompose |
| `compose::parseDimension` | function | SigilCompose — nothing for text it cannot read |

## Description

A relative length is resolved where it is READ, not where it is written.
That is the whole point of carrying the unit in the value: an em written
into a shared component resolves against the font of whatever the
component was placed under, and a `var` length resolves against the
nearest ancestor that set the name. A length that had been converted at
authoring time would have frozen the wrong answer.

Two dimensions compare exactly, unit included, so `24` and `24_px` are
one value and `50_percent` is not `24` even where they measure the same on
one canvas.

## See also

- `core/Layout.h` — the header: `Dimension`, `percent`, `vw`, `vh`,
  `vmin`, `vmax`, `autoDimension`, `parseDimension`, `fontLength`,
  `parseFraction`, `Align`, `Justify`, `Echo`, `_px`, `_percent`, `_vw`,
  `_vh`, `_vmin`, `_vmax`
- [Corners](value:sigil::compose::Corners) — four radii, each a
  dimension
- The colour chapter on the [SigilCompose](doxygen:SigilCompose) site —
  the other value a custom property can hold, and how a reference
  resolves
