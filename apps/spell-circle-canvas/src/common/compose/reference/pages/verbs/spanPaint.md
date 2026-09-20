---
kind: verb
library: SigilCompose
name: spanPaint
qualified: sigil::compose::Element::spanPaint
header: sigilcompose/core/Text.h
group: Content
python: sigil.compose.Element.spanPaint
status: stable
---

# spanPaint

Repaint the range this selector finds — a colour, a shader, an
underline, an added glow pass.

## Syntax

```cpp
Element& spanPaint(sigil::weave::Selector where,
                   sigil::weave::PaintStyle paint);
```

```python
def spanPaint(self, where: weave.Selector,
              paint: weave.PaintStyle) -> Element: ...
```

## Description

**PAINT ONLY, so it NEVER re-shapes and never relayouts.** The glyphs
are exactly the glyphs the unrestyled text shaped, drawn differently.

**The paint it declares is the one the range keeps.** A
[`spanStyle`](spanStyle.md) on the same text after it restyles
everything else and leaves this colour alone.

The rules the two restyles share — ordering, what a selector resolves
to, and what a line selector costs — are on
[`spanStyle`](spanStyle.md).

## See also

[`spanStyle`](spanStyle.md), `weave::PaintStyle`,
`weave::selectors::`.
