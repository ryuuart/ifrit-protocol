---
kind: verb
library: SigilMaterial
name: slot
qualified: sigil::material::skia::Paint::slot
group: Uniforms and layer properties
status: stable
---

# slot

## Description

The effect declares `uniform shader NAME;` and this fills it with
another paint, so one shader can read two sources and combine them by a
rule only SkSL can state: an index texture sampled through a palette
lookup (index arithmetic on the sampled value, which no blend mode can
express), a mask channel, a noise field, a second gradient.
`Effect::slot` has exactly one slot, `content`, which is the
already-painted layer; this is the door for sources the node has NOT
painted.

Any paint can fill a slot, including another `Paint::sksl` one — slots
nest, and the whole tree still compiles to ONE shader, with no saved
layer. For an image, wrap it: `slot("uIndex", Paint::image(img, ...))`
— and pass a nearest filter mode for anything whose pixel VALUES are
data, because an index texture read linearly samples a blend of two
unrelated palette entries.

**Tier inheritance is the load-bearing half.** The parent inherits the
volatility of what fills its slots. A live source — a bound animatable,
`uTime` — makes the parent live; a geometry-dependent source
(`uResolution`) propagates the geometry tier. The slots also ride the
prune signature, so two paints with DIFFERENT sources never compare
equal and two with identical ones prune. That is required, not
incidental: a slot left out of equality would let a node prune while its
second source had changed, and it would sample the old texture
indefinitely.

**Guardrails match `Paint::uniform`'s.** A name the effect does not
declare as a `uniform shader` is warned and IGNORED — assigning a slot
the effect lacks aborts in a debug build, which would take a live-reload
host down over one typo — and on a paint that is not effect-backed there
is nothing to fill, so the call is a no-op with a warning.
Copy-on-write, like every other recipe mutation.

## See also

- [Paint](../types/Paint.md) — the value this sits on
- [`uniform`](uniform.md) — the scalar and array doors on the same effect
- [Effect](../types/Effect.md) — the same word over an already-rendered
  layer, where the one slot is the layer itself
