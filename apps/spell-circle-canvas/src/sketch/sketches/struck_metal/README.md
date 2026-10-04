# Struck metal

An invented 1440 × 960 control register built from physical World solids. Its
bronze face has enamel-filled recessed `STRIKE` lettering, a pierced brass
medallion with a bright scale, raised `AURUM` and `08` glyphs, ribbed turned
controls and an engraved strip sliding behind a real window. Three lower
specimens compare shallow relief, deep relief and a bridged through-cut
stencil. Compose supplies only the status overlay; the lettering on the object
is mesh geometry with caps, counters and side walls.

Weave shapes and places the glyphs, Skia provides their outlines, and Geometry
extrudes them with their holes intact. The recessed lettering is subtracted
from the face outline and given a lower enamel floor. The bronze carries
generated colour, normal and roughness maps that suggest dark oxidation and
muted green deposits. Each frame changes the poses, the moving register, the
letter depth and the selected brass roughness; relief changes independently of
the enclosure, so the hardware spacing holds. The study models raised and
recessed results, not hammering, displaced metal or corrosion.

## Timeline and controls

The 24-second motion and light cycle is periodic and can be entered at any
time. Each row is a moment's peak.

| Seconds | Moment |
|---|---|
| 0 | Front view: lettering counters and the pierced aperture. |
| 2.4 | Hero: machining ribs, recessed enamel, the scale and the sliding digits. |
| 6 | 70° yaw: the relief walls and the open medallion's depth. |
| 9 | Roughness zero on the pierced brass, raised title and counter. |
| 12 | The same brass at roughness one. |
| 15 | Main raised lettering at 0.8-unit depth. |
| 18 | Main raised lettering at 16-unit depth. |
| 21 | Reverse: back caps and the aperture's open silhouette. |

`kControls` holds `yaw` ±180°, `pitch` ±75°, `relief` 0.8–18 units,
`brassRoughness` and `bronzeRoughness` 0–1, `patina` 0–1, `normalStrength`
0–2, `lightSwing` ±90°, `registerTravel` 0–28 units, `nearPlane` 1–1400 and
`farPlane` 100–7000, with far at least 100 beyond near. Nonfinite values take
their defaults. The named moments override yaw, relief or brass roughness; the
lower specimens keep their fixed geometry. Normal strength zero leaves the
colour marks.
