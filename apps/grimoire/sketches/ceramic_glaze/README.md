# Ceramic glaze

A 1440 × 1000 ceramic instrument: a crazed celadon panel, a porcelain rosette,
coloured glaze pads and six fixed finish coupons. The interface, labels and
arrangement are invented, and every texture is generated from a seed.

## What is drawn

Every piece is an ordinary Compose shape with a material fill. Three generated
texture sets provide albedo, roughness and normals: nearly smooth porcelain,
porous stoneware and crazed glaze. A local normal program adds rounded
shoulders, a domed rosette and flowing ridges over the placed finish map, which
follows each specimen's dimensions. Raised and impressed inscriptions take
their relief from the placed glyph outline; their ordinary ink is transparent
so the relief supplies the material once. A scoped directional lamp and a
studio environment light the surfaces and the lettering.

`glazeWeight` is the clearcoat weight: a glossy coating over the body, separate
from its roughness, colour and finish maps. The coating shares the body's
normal. Two coupons compare coating 0 and 1 over the same body.

The surfaces stay flat. Stacked faces and a static contact shadow give the
silhouettes their depth; normal maps give the apparent relief, so moving the
lamp changes its shading without changing geometry or casting self-shadows.
The crack network is a seeded Voronoi pattern; there is no firing, thermal
expansion, fracture or depth inside the glaze. Porosity and craze colour are
qualitative.

## Controls and states

`Controls` in `ceramic_glaze.cpp` holds `roughness`, `normalStrength`,
`glazeWeight`, `porosity`, `crackStrength`, `reliefDepth` and `lightSpeed`.
They are source values; finite values clamp to their ranges and nonfinite
values fall back to the defaults. `reliefDepth` scales the macro height and the
signed lettering relief. At the default light speed the environment turns once
per 36-second loop.

| Seconds | Hero state | What changes |
|---|---|---|
| 0–4 | Glazed / working | Celadon ridges, coloured crazing, porcelain shoulders and relief type. |
| 4–8 | Roughness / zero | Body roughness 0, with normals and coating retained. |
| 8–12 | Roughness / one | Body roughness 1; the glossy coating stays visible. |
| 12–16 | Normal / zero | Normal strength 0; stacked silhouettes remain. |
| 16–20 | Normal / strong | Normal strength 3: pore, crack and shoulder detail. |
| 20–24 | Coating / zero | Clearcoat 0; colour, roughness and maps stay connected. |
| 24–28 | Raking / studio light | Lamp at 9° elevation over lettering and shoulders. |
| 28–32 | Colour / no illumination | The hero's colour stack alone; the coupons stay lit. |
| 32–36 | Profile / zero height | Macro profile and inscription depth 0; fine maps remain. |

The light and environment keep moving within each state. The six coupons keep
their own finishes in every state and share the moving studio; the last pair
compares normal strength 0 and 3.
