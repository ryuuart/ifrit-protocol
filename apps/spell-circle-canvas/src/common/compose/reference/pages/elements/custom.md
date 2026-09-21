---
kind: element
library: SigilCompose
name: custom
qualified: sigil::compose::custom
header: sigilcompose/core/Factories.h
group: Leaves
status: cpp-only
example: custom_element
common_verbs: [cache, width, height, borderRadius, overflow, cover]
---

# custom

A box whose content is one paint program — the immediate-mode floor
under the retained tree. It is exactly `box().background(program)`, with
the program handed the canvas and the node's own paint context.

<!-- example: custom_element -->

## Syntax

```cpp
Element custom(PaintProgram program);
Element custom(std::string_view key, PaintProgram program);
```

```python
# C++ only. The Python door for a node that draws its own content is
# the pen:
def pen(key: str, program: Callable[[draw.Pen], None],
        cache: Cache = Cache.None_) -> Element: ...
```

## Parameters

| Parameter | What it is | Where one comes from |
|---|---|---|
| `program` | The drawing. It names only the parameters it reads, so `[](SkCanvas& canvas) {…}`, `[](SkCanvas& canvas, const PaintContext& context) {…}` and `[] {…}` are all paint programs. | [`PaintProgram`](../../VALUES.md#the-layer) |
| `key` | The program's IDENTITY: one key must always name one drawing at one parameterisation. | Any string the caller can keep stable |

## Description

**It sizes like an empty box, and the failure is silent.** Being
literally a box with one background, a custom leaf has no intrinsic
size: dropped into a covering parent it stretches on the cross axis and
measures ZERO on the main one, so the program runs against a zero-height
context and draws nothing. Give it dimensions, or make it cover its
parent itself.

**Two costs an author has to know.** It is cached like any static
subtree, so a program that reads the clock — or changes for any other
reason without a re-describe — must declare `cache(Cache::None)`, or its
first frame is recorded and replayed frozen. And an unkeyed program is
an incomparable callable, so the structural prune cannot prove the node
unchanged and it re-records on every render. The keyed form fixes the
second: two describes with equal keys compare equal and the node prunes.
Fold anything that varies into the key, or two different pictures
compare equal and the stale one replays.

**Value decorations prune for free.** `PathFormat`, `Shadow`, `Slice`
and any scheme of your own that compares equal are the first thing to
reach for; a paint program is for what none of them draws.

The program is handed a `PaintContext`: the laid-out size, the node's
outline and silhouette, the clock, the ink and font in force, the custom
properties, the pointer and the keys, and the matrix up to the composer
root.

## Examples

- `reference/examples/custom_element.cpp` — a keyed program drawing
  rings measured off the box the node was laid out at.
- `reference/examples/custom_element.py` — the same picture through the
  Python pen door.

## See also

`pen` and `graphics` for the same idea with SigilDraw's pen instead of
the canvas, `picture` for a recording already taken,
[`background`](../verbs/background.md) and
[`overlay`](../verbs/overlay.md) for the slots a decoration paints in,
and the *Caching* group on [the verb index](../../VERBS.md).
