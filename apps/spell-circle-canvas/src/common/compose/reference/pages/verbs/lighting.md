---
kind: verb
library: SigilCompose
name: lighting
qualified: sigil::compose::Element::lighting
header: sigilcompose/core/verbs/Cascade.h
group: The cascade
status: stable
---

# lighting

The light a lit surface under this node is shaded under — a directional
key light, an environment, or both — inherited as the ink is.

## Description

**Stated once, on a parent or the page, it reaches every lit surface
under it.** A fill, an ink or a stroke whose material states a
`surface()` is shaded under it in 2D: the colours are painted as they
would be flat, and a lighting pass over them reads the surface's normal
map for relief, and its roughness, metallic, occlusion and emission.

```cpp
motion::Animatable<float> sun = motion::animatable(0.0f);
stack().lighting(material::studio({.direction = sun, .elevation = 35.0f}))
    .children({box().fill(plate), text(u8"GILT").ink(gold),
               box().stroke(gold, {.width = 6})});
```

`material::studio()` is a directional light: `direction` is where it comes
from, in degrees counter-clockwise from three o'clock (120 is the upper
left a bevel is lit from), `elevation` how far above the page, with a
`color`, an `intensity` and the `ambient` share every point receives.
`material::environment(pixels, {.rotation})` is a latitude-longitude
picture the surface reflects by its normal and roughness and takes its
ambient colour from. A `material::Lighting` holds either, or both.

**A bound light moves the relief, and nothing else.** Every angle and
strength takes an `Animatable<float>`: while one moves, the node repaints
the lighting pass each frame, and the colours beneath — gradients, images,
a Substance cook — were lowered once and are not painted again.

**A surface's own lighting stands over it**:
`material.surface({.lighting = …})` lights that material the same way
wherever it lands. With no lighting in force a lit surface is painted flat,
as its colours, and `initial(Property::Lighting)` ends an inherited one.

## See also

`fill`, `ink`, `stroke`, `material::Lighting`, `material::studio`.
