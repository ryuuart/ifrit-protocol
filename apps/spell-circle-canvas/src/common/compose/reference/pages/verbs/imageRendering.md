---
kind: verb
library: SigilCompose
name: imageRendering
qualified: sigil::compose::Element::imageRendering
header: sigilcompose/core/verbs/Cascade.h
group: The cascade
python: sigil.compose.Element.imageRendering
status: stable
---

# imageRendering

How image leaves under this node sample their source — CSS's
`image-rendering`, and inherited exactly as that is.

## Description

**Linear when nothing states it**, which is right for photographs and
wrong for every pixel grid: art, tilemaps, fonts baked as sprites,
simulation buffers.

```cpp
element.image(tileset).imageRendering(SkSamplingOptions(SkFilterMode::kNearest));
```

**It is set on any node and inherited by every image leaf under it**,
so a panel of pixel art states nearest once rather than once per
sprite. `material::skia::Paint::image` takes the same options for a sprite
fill.

## See also

`image`, `imageRegion`, `material::skia::Paint`.
