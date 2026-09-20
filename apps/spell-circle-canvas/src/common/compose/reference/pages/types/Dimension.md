---
kind: type
library: SigilCompose
name: Dimension
qualified: sigil::compose::Dimension
header: sigilcompose/core/Layout.h
group: Box model
python: sigil.compose.Dimension
status: stable
---

# Dimension

A LENGTH, in whichever of the nine units it was written in: pixels, a
percentage of the parent, a percentage of the canvas either way, a
multiple of the font in force or of the root's, a multiple of the line
height, a custom property read where it lands, or left for layout to
decide. Constructing one from a bare number gives pixels, so the common
case reads as a number and nothing else.

## Anatomy

`Dimension::unit` is which of the nine, and `Dimension::value` is the
number — except under `Dimension::Unit::Var`, where the reference id is
bit-cast into the float and never read as one; `Dimension::reference`
reads it back.

The units, and what each is a fraction or a multiple OF:

| Unit | Written | Of what |
| --- | --- | --- |
| `Dimension::Unit::Px` | `24`, `24_px` | canvas pixels — absolute |
| `Dimension::Unit::Pct` | `50_pct` | the PARENT's box, which is Yoga's own percent |
| `Dimension::Unit::Pw` | `6_pw` | the CANVAS's width, wherever in the tree the node sits |
| `Dimension::Unit::Ph` | `6_ph` | the CANVAS's height |
| `Dimension::Unit::Em` | `1.5_em` | the node's own resolved font size |
| `Dimension::Unit::Rem` | `2_rem` | the root's font size |
| `Dimension::Unit::Lh` | `0.5_lh` | the node's own line height |
| `Dimension::Unit::Var` | `var("gutter")` | the length the nearest ancestor set under that name |
| `Dimension::Unit::Auto` | `autoDimension()` | nothing — layout decides |

`Dimension::relative` answers whether resolving the length needs
something the number does not carry: the font in force, a custom
property, or the canvas. It is everything but a pixel, a parent-relative
percent and auto.

THE CANVAS UNITS ARE THE ROOT'S BOX, not the parent's. A poster whose
margin is `6_pw` keeps its proportions at every canvas size, however many
boxes deep it is written. `Pw`, `Ph` and `Pct` are resolved into pixels
before Yoga sees them, because a percentage of something that is not the
containing block is a thing Yoga cannot express.

THE FONT UNITS ARE SIGILWEAVE'S. `weave::Length` is the length a text
style is written in, with `weave::em`, `weave::rem` and `weave::lh` and
their `_em`, `_rem` and `_lh` suffixes, and it converts here implicitly —
so a padding written in ems follows the type it surrounds, and a change
to an ancestor's font relays out everything measured in it.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `24` | C++ | pixels — the implicit constructor from a float |
| `24_px` | C++ | pixels, said out loud |
| `50_pct` | C++, or `compose::pct(50)` | a fraction of the parent |
| `6_pw` | C++, or `compose::pw(6)` | a fraction of the canvas's width |
| `6_ph` | C++, or `compose::ph(6)` | a fraction of the canvas's height |
| `1.5_em`, `2_rem`, `0.5_lh` | C++ | SigilWeave's `weave::Length`, implicitly |
| `compose::var("gutter")` | C++ | a `VarRef`, implicitly — the custom property's length |
| `compose::autoDimension()` | C++ | the default: layout decides |
| `24`, `24.0` | Python | pixels |
| `"50%"` | Python | a percentage of the parent, as a string |
| `"auto"` | Python | layout decides |
| `compose.pct(50)`, `compose.pw(6)`, `compose.ph(6)` | Python | the three percentage units |
| `weave.em(1.5)`, `weave.rem(2)`, `weave.lh(0.5)` | Python | the three font units |
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
| `Element::basis` | verb | SigilCompose |
| `Element::inset` | verb | SigilCompose |
| `Element::left`, `Element::top`, `Element::right`, `Element::bottom` | verb | SigilCompose |
| `Element::var` | verb | SigilCompose — the length a subtree reads under a name |
| `Well::width`, `Well::height` | field | SigilCompose |
| `Line::length` | field | SigilCompose |

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| `compose::pct`, `compose::pw`, `compose::ph` | function | SigilCompose |
| `compose::autoDimension` | function | SigilCompose |

## Description

A relative length is resolved where it is READ, not where it is written.
That is the whole point of carrying the unit in the value: an em written
into a shared component resolves against the font of whatever the
component was placed under, and a `var` length resolves against the
nearest ancestor that set the name. A length that had been converted at
authoring time would have frozen the wrong answer.

Two dimensions compare exactly, unit included, so `24` and `24_px` are
one value and `50_pct` is not `24` even where they measure the same on
one canvas.

## See also

- `core/Layout.h` — the header: `Dimension`, `pct`, `pw`, `ph`,
  `autoDimension`, `Align`, `Justify`, `Echo`, `_px`, `_pct`, `_pw`,
  `_ph`
- [Corners](Corners.md) — a radius is a plain number, not a dimension
- [Colour, fill, paint and material](../../COLOURING.md) — the other
  value a custom property can hold, and how a reference resolves
