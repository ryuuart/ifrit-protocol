---
kind: element
library: SigilCompose
name: text
qualified: sigil::compose::text
header: sigilcompose/core/Factories.h
group: Leaves
status: stable
example: text_element
common_verbs: [font, ink, paragraph, maxTextLines, textOverflow, textStroke, paragraphStyles]
---

# text

A text leaf. Four content forms, one node: words set in the type in
force, words set in a style of their own, mixed-style words as one
comparable value, and a whole prebuilt paragraph with its layout
options.

<!-- example: text_element -->

## Syntax

```cpp
Element text(Utf8 utf8);
Element text(Utf8 utf8, sigil::weave::TextStyle style);
Element text(sigil::weave::RichText spans);
Element text(std::shared_ptr<sigil::weave::Paragraph> paragraph,
             sigil::weave::ParagraphLayoutOptions options = {});
```

```python
def text(value: str, size: FloatLike | None = None,
         color: ColorLike | None = None) -> Text: ...
def text(value: str, style: weave.TextStyle) -> Text: ...
def text(content: weave.RichText) -> Text: ...
```

## Parameters

| Parameter | What it is | Where one comes from |
|---|---|---|
| `utf8` | The words. | [`Utf8`](../../VALUES.md#the-text-values) — a `char` or `char8_t` string, a `std::string`, or a value that reads itself out as text |
| `style` | A TOTAL style, so the leaf inherits nothing. | [`weave::TextStyle`](../../VALUES.md#the-text-values) |
| `spans` | Mixed-style text as a comparable value. | [`weave::RichText`](../../VALUES.md#the-text-values) |
| `paragraph` | A prebuilt paragraph, shared by pointer so the shaping caches stay warm. | SigilWeave's paragraph vocabulary |
| `options` | Justification, hyphenation, the optimizing breaker, overflow. | SigilWeave's layout options |

Python's first overload is the friendly one: `size` and `color` write
the two fields a caption usually wants, and everything else is a fluent
verb — `fontSize`, `letterSpacing`, `fontWeight`, `ink`.

## Description

**The form with no style inherits.** A leaf written as `text(words)` is
set in the nearest ancestor's `font` and `ink`, wherever the code that
built it ran, so a component handed to a dark panel is recoloured by the
panel rather than by its author. Its own `font` overrides field by
field: `font({.size = 22})` is the inherited face and colour at another
size.

**The form with a style inherits nothing.** A `weave::TextStyle` is
total — every field of it is stated — so the leaf draws exactly that
whatever its ancestors said. Reach for it when a leaf must be one
specific thing, and for everything else prefer the partial.

**Rich text is a value.** `weave::rich()` builds mixed runs that compare
equal to an identical re-described value, so the node prunes. The
paragraph overload takes a pointer instead: reuse one pointer across
renders to keep the shaping caches warm, and hand over a fresh one to
say the content changed.

A text leaf declares the whole *text leaf* group of verbs — the
paragraph styling, the threading, the annotations, the span restyles,
the textFx tracks — and no other node declares them, so writing one on
anything else does not compile. Hold the leaf as a `Text` for as long as
those verbs are still to be written; it converts to `Element` wherever a
node is wanted. Its children are its marks and its slot mounts.

## Examples

- `reference/examples/text_element.cpp` — the three content forms under
  one inherited font and ink.
- `reference/examples/text_element.py` — the same picture in Python.

## See also

`frame` for one frame of a story, [`ink`](../verbs/ink.md) for what a
leaf inherits and for painting the glyphs with a whole paint,
[`textStroke`](../verbs/textStroke.md) for the pass under them, and the
*text leaf* group on [the verb index](../../VERBS.md).
