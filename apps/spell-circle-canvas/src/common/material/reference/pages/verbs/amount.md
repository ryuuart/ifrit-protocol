---
kind: verb
library: SigilMaterial
name: amount
qualified: sigil::material::skia::Paint::amount
group: Uniforms and layer properties
status: stable
---

# amount

## Description

Layer strength inside a `Paint::blend` — "soft-light this noise at 30%".

Layer-opacity semantics, as an image editor's layer panel means them:
the layer composites with its blend mode IN FULL, and the result then
mixes back toward the accumulation by the amount. That is not the same
picture as thinning the layer's own alpha first, which changes what the
blend mode sees.

It is read ONLY by `Paint::blend`, and there only from the SECOND layer
up: the first layer is the accumulation itself, and a paint used
directly as a fill has no accumulation either — in both places there is
nothing to mix back toward, so the value is ignored. It participates in
equality like every recipe field.

## See also

- [Paint](../types/Paint.md) — the value this sits on, and the blend
  stack the amount is read inside
- [`slot`](slot.md) — the other way two sources meet
