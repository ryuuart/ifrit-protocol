---
kind: verb
library: SigilMaterial
name: uniform
qualified: sigil::material::skia::Paint::uniform
group: Uniforms and layer properties
status: stable
---

# uniform

## Description

What is set on a paint after it is built: named uniforms baked in or
bound to a moving value. It is meaningful ONLY on a `Paint::sksl` or
`Paint::recipe` paint — the kinds that have named uniforms to hook
against.

- `uniform(name, value)` bakes a constant in; the paint stays static.
- `uniform(name, animatable)` binds a `motion::Animatable<float>`; the
  paint becomes LIVE, re-resolved every frame from the value's current
  reading, and its node is declared volatile so it paints live. This is
  how a material animates.

Additionally `uTime` (float seconds), `uResolution` (float2 px) and
`uContentScale` (float) are auto-injected each frame IF the effect
declares them, at the matching size.

**The shapes.** A float2 uniform takes offsets, margins and direction
vectors. A float4 takes a colour — straight, not premultiplied, which is
what the SkSL declares — or four plain numbers for a rect, a quaternion,
anything that is not a colour; both fill the same slot.

**A constant array** is stored flat and matched against the declared
uniform's TOTAL float count: 12 floats fill `float4 uRect[3]`,
`float2 uPts[6]` and `float uWeights[12]` alike, because total size is
all the builder distinguishes. The whole array must be supplied — the
builder refuses a partial write, so a count that is not the
declaration's warns once and is ignored.

**A live array** is a `UniformBlock` the caller owns, writes and
commits, read at every paint. It makes the paint live exactly as a bound
scalar does: re-resolved per frame, its node declared volatile, no cache
able to freeze the table — and the resolve memo reads the block's
REVISION, so an uncommitted frame reuses the built shader. The binding
compares by block identity and the values never prune; hold the block
beside your model, not in the describe. It is size-checked at store
against the declared array's total float count.

**A live scalar** is read at every paint, so the paint is live and its
node volatile for as long as the binding is attached. It takes an
animatable, so the arithmetic that shapes the number — a wrapped ramp, a
raised cosine, a wiggle — sits beside the uniform it feeds rather than
in a second output somebody steps by hand. A paint holds no instance, so
a value carrying its own TRANSITION has nothing to run it and reads as
its target.

**Guardrails.** A name the effect does not declare as a float uniform is
warned and IGNORED, never a debug abort: one sketch typo must not kill
the hot-reload host. On any other kind of paint — a solid, a gradient,
an image, a blend — there is nothing to bind, so the call is a no-op
with a warning; reach for `Paint::sksl` when you want animatable
uniforms. Paints are VALUES: this copies on write, so binding on a copy
never affects the paint it was copied from.

## See also

- [Paint](../types/Paint.md) — the value this sits on
- [`slot`](slot.md) — the second-source door on the same effect
- [`quantizeTime`](quantizeTime.md) — stepping the injected clock
