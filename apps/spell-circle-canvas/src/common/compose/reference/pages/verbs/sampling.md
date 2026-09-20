---
kind: verb
library: SigilCompose
name: sampling
qualified: sigil::compose::Element::sampling
header: sigilcompose/core/verbs/Cascade.h
group: The cascade
python: sigil.compose.Element.sampling
status: stable
---

# sampling

How image leaves under this node sample their source — CSS's
`image-rendering`, and inherited exactly as that is.

## Description

**Linear when nothing states it**, which is right for photographs and
wrong for every pixel grid: art, tilemaps, fonts baked as sprites,
simulation buffers.

```cpp
element.image(tileset).sampling(SkSamplingOptions(SkFilterMode::kNearest));
```

**It is set on any node and inherited by every image leaf under it**,
so a panel of pixel art states nearest once rather than once per
sprite. `material::skia::Paint::image` takes the same options for a sprite
fill.

## See also

`image`, `region`, `material::skia::Paint`.
