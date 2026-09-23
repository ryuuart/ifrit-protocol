---
kind: type
library: SigilCompose
name: FontStyle
qualified: sigil::compose::FontStyle
group: The cascade
status: stable
---

# FontStyle

How the type leans — CSS's `font-style` as a value: upright, italic, or
an oblique angle.

## Anatomy

`FontStyle::kind` is which of CSS's three keywords the value is —
`FontStyle::Kind::Normal`, `Italic` or `Oblique` — and `FontStyle::degrees`
the lean under `Oblique`, POSITIVE LEANING RIGHT, zero otherwise.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `FontStyle::Normal` | C++ | upright |
| `FontStyle::Italic` | C++ | the family's italic |
| `FontStyle::oblique(12)` | C++ | a lean of 12 degrees to the right |
| `FontStyle::oblique()` | C++ | CSS's default lean, 14 degrees |
| `12` | C++ | a bare number is that oblique, so `fontStyle(12)` is the whole call |
| `compose.FontStyle.Normal`, `compose.FontStyle.Italic` | Python | the two keywords |
| `compose.FontStyle.oblique(12)`, `compose.FontStyle(12)` | Python | a lean of 12 degrees |

Two styles compare equal when they say the same thing, and in Python they
hash alike, so a style can key a table. Anything else handed to Python's
`fontStyle` — a string, say — is a `TypeError`.

## Pass it to

[`fontStyle`](../verbs/fontStyle.md), on an element, a text leaf, a rule
or a span's declarations.
