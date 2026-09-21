---
kind: verb
library: SigilCompose
name: opacity
qualified: sigil::compose::Element::opacity
header: sigilcompose/core/verbs/Effects.h
group: Effects
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
```

```python
def opacity(self, value: ScalarLike) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `motion::Animatable<float>` | A number at rest, a number in transition, or a number bound to a live output. | [`motion::Animatable`](../../VALUES.md#motion-over-a-value) — `motion::animate`, `motion::bind`, or the bare number |

`opacity(1)` compiles: the animatable takes a plain number, and an
integer reaches it the same way a float does. In Python the parameter
is `ScalarLike`: a number, an animatable, a transition, or a bound
output.

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

**A mount entrance is this verb over an entering value.** The fade
every card, panel and strip says as it arrives is
`opacity(animate(from(0).to(1), how))`: the node is clear the frame it
mounts and ramps to solid over `how`. A staggered container adds its
own share of delay in front of that ramp, so the entrance is written
once per node and dealt by the container.

## Examples

- `reference/examples/opacity_verb.cpp` — one card with an overlapping
  child at three opacities.
- `reference/examples/opacity_verb.py` — the same picture in Python.

## See also

`transition` for the node's default easing, `staggerChildren` for the
delay a container deals its entering children,
[`blendMode`](blendMode.md), [`filter`](filter.md), and `mask` for a
reveal that is a shape rather than a level.
