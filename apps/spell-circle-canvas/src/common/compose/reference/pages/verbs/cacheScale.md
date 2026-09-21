---
kind: verb
library: SigilCompose
name: cacheScale
qualified: sigil::compose::Element::cacheScale
header: sigilcompose/core/verbs/Structure.h
group: Caching
status: stable
---

# cacheScale

Texture-bake resolution multiplier, `Cache::Texture` only: the bake
rasterizes at `factor` times the device scale and the blit scales it
back up with linear sampling. The value is clamped to 0.1–1.

## Description

**ALMOST ALWAYS THE WRONG LEVER.** It cheapens the BAKE, which happens
once, and taxes every BLIT with an upscaling resample, which happens
forever — backwards for the bake-once, blit-every-frame node
`Cache::Texture` exists for.

**Reach for it only when something forces FREQUENT re-bakes** — a live
material stepping at its own rate, a resizing node — AND the content is
soft enough to survive the resample. Sharp text and one-pixel hairlines
never belong under a reduced bake.

## See also

`cache`, `Cache`.
