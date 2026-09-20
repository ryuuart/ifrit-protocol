---
kind: verb
library: SigilMaterial
name: quantizeTime
qualified: sigil::material::skia::Paint::quantizeTime
group: Uniforms and layer properties
status: stable
---

# quantizeTime

## Description

Steps the auto-injected `uTime` at a rate, as `floor(t·hz)/hz` —
deliberate choppiness declared as a property of the PAINT rather than
plumbed through whatever drives the clock.

Stop-motion and flipbook surfaces read as intentional at a low rate
where a smoothly interpolated one reads as sliding.

It is meaningful only on a `Paint::sksl` paint whose effect declares
`uTime`; warned and ignored otherwise, and zero restores continuous
time.

## See also

- [Paint](../types/Paint.md) — the value this sits on
- [`uniform`](uniform.md) — where the injected clock arrives
