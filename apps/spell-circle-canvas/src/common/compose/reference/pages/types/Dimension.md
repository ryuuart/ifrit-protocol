---
kind: type
library: SigilCompose
name: Dimension
qualified: sigil::compose::Dimension
group: Box model
status: stable
---

# Dimension

A LENGTH, in whichever of the eleven units it was written in: pixels,
points, a percentage of the parent, a percentage of the canvas either
way, a multiple of the font in force or of the root's, a multiple of the
line height or of the width of a figure, a custom property read where it
lands, or left for layout to decide. Constructing one from a bare number
gives pixels, so the common case reads as a number and nothing else.

## Anatomy

`Dimension::unit` is which of the eleven, and `Dimension::value` is the
number — except under `Dimension::Unit::Var`, where the reference id is
bit-cast into the float and never read as one; `Dimension::reference`
reads it back.

The units, and what each is a fraction or a multiple OF:

| Unit | Written | Of what |
| --- | --- | --- |
| `Dimension::Unit::Px` | `24`, `24_px` | canvas pixels — absolute |
| `Dimension::Unit::Pt` | `9_pt` | printer's points — absolute, four pixels to every three |
| `Dimension::Unit::Pct` | `50_pct` | the PARENT's box, which is Yoga's own percent |
| `Dimension::Unit::Pw` | `6_pw` | the CANVAS's width, wherever in the tree the node sits |
| `Dimension::Unit::Ph` | `6_ph` | the CANVAS's height |
| `Dimension::Unit::Em` | `1.5_em` | the node's own resolved font size |
| `Dimension::Unit::Rem` | `2_rem` | the root's font size |
| `Dimension::Unit::Lh` | `0.5_lh` | the node's own line height |
| `Dimension::Unit::Ch` | `3_ch` | the advance of "0" in the face in force |
| `Dimension::Unit::Var` | `var("gutter")` | the length the nearest ancestor set under that name |
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

THE CANVAS UNITS ARE THE ROOT'S BOX, not the parent's. A poster whose
margin is `6_pw` keeps its proportions at every canvas size, however many
boxes deep it is written. `Pw`, `Ph` and `Pct` are resolved into pixels
before Yoga sees them, because a percentage of something that is not the
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
| `"10pw"`, `"10ph"` | a fraction of the canvas either way |
| `"1.5em"`, `"2rem"`, `"0.5lh"`, `"3ch"` | the font units |
| `"var(gutter)"`, `"var(--gutter)"` | a custom property |
| `"auto"` | layout decides |

Nothing is answered for text the grammar does not cover — an empty
string, a unit no `Dimension` carries, a space between the number and
its unit — so a caller can tell an unreadable length from a length that
was never stated.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `24` | C++ | pixels — the implicit constructor from a float |
| `24_px` | C++ | pixels, said out loud |
| `50_pct` | C++, or `compose::pct(50)` | a fraction of the parent |
| `6_pw` | C++, or `compose::pw(6)` | a fraction of the canvas's width |
| `6_ph` | C++, or `compose::ph(6)` | a fraction of the canvas's height |
| `9_pt` | C++ | SigilWeave's `weave::Length`, implicitly |
| `1.5_em`, `2_rem`, `0.5_lh`, `3_ch` | C++ | SigilWeave's `weave::Length`, implicitly |
| `compose::var("gutter")` | C++ | a `VarRef`, implicitly — the custom property's length |
| `compose::autoDimension()` | C++ | the default: layout decides |
| `24`, `24.0` | Python | pixels |
| `"50%"`, `"1.5em"`, `"9pt"`, `"3ch"` | Python | any length the text grammar reads |
| `"auto"` | Python | layout decides |
| `compose.pct(50)`, `compose.pw(6)`, `compose.ph(6)` | Python | the three percentage units |
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
| `Well::width`, `Well::height` | field | SigilCompose |
| `Line::length` | field | SigilCompose |

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| `compose::pct`, `compose::pw`, `compose::ph` | function | SigilCompose |
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
one value and `50_pct` is not `24` even where they measure the same on
one canvas.

## See also

- `core/Layout.h` — the header: `Dimension`, `pct`, `pw`, `ph`,
  `autoDimension`, `parseDimension`, `Align`, `Justify`, `Echo`, `_px`,
  `_pct`, `_pw`, `_ph`
- [Corners](value:sigil::compose::Corners) — a radius is a plain
  number, not a dimension
- The colour chapter on the [SigilCompose](doxygen:SigilCompose) site —
  the other value a custom property can hold, and how a reference
  resolves
