---
kind: verb
library: SigilCompose
name: opacity
qualified: sigil::compose::Element::opacity
header: sigilcompose/core/Element.h
group: Effects
python: sigil.compose.Element.opacity
status: stable
example: opacity_verb
---

# opacity

Fades the node and everything under it as ONE group — a value, a
transition, or a live binding.

<!-- example: opacity_verb -->

## Syntax

```cpp
Element& opacity(motion::Animatable<float> o);
template <std::integral T> Element& opacity(T v);
```

```python
def opacity(self, value: ScalarLike) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `motion::Animatable<float>` | A number at rest, a number in transition, or a number bound to a live output. | [`motion::Animatable`](../../VALUES.md#motion-over-a-value) — `motion::animate`, `motion::bind`, or the bare number |

The integral overload is there so `opacity(1)` compiles: a plain `int`
does not convert into the animatable on its own, and the error it would
otherwise give is unreadable. In Python the parameter is `ScalarLike`: a
number, an animatable, a transition, or a bound output.

## Description

**As a group, not per node.** Two overlapping children inside a faded
card do not show through each other: the subtree is drawn into a layer
and the layer is faded. That is CSS's rule, and it is why a faded card
looks like a faded card rather than like a stack of translucent parts.

**Grouping has consequences.** An opacity below 1 flattens the node's
children onto its plane — it cannot host a shared 3D space — and it
forms the same stacking context an effect or a blend does.

**Paint-only.** Animating it never relayouts: the content's recording
replays under the new alpha.

**A mount entrance has its own word.** `appear(how)` is
`opacity(animate(from(0).to(1), how))` written once, because that
sentence is what every card, panel and strip says as it arrives. Use
this verb for a fade the node does at some other moment.

## Examples

- `reference/examples/opacity_verb.cpp` — one card with an overlapping
  child at three opacities.
- `reference/examples/opacity_verb.py` — the same picture in Python.

## See also

`appear` for the mount entrance, `transition` for the node's default
easing, [`blend`](blend.md), [`effect`](effect.md), and `mask` for a
reveal that is a shape rather than a level.
