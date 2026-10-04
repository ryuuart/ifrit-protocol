# Carved / Marks

A 1440 × 1120 Compose material atelier: porous limestone beside brushed copper,
with the same letter die and layered marks pressed into both finishes. Real
`OB8` text keeps its counters. A pressure ribbon and two crossing fibre passes
overlap the lettering in one opaque grayscale height field. Dark ink and partial
gilding sample that field, so the marks change the material rather than sit
above it.

The stone takes the height as a cut; the copper reverses and reduces it into a
raised, chased finish. Local grain supplies a second normal layer. A
near-frontal warm key, ambient light and a broad environment keep the body
visible between the raking states. Stone pores, copper scores, oxide tint and
gold masks are authored fields.

The 24-second loop has six four-second phases:

| Seconds | Phase |
| --- | --- |
| 0–4 | Frontal light, cut stone and chased copper |
| 4–8 | Low light from the left |
| 8–12 | Low light from the right |
| 12–16 | Zero macro depth; grain remains |
| 16–20 | Half gilding, dark ink retained |
| 20–24 | Reflected Glyph mapping; Word ink spans font-size changes |

`HOLD` pins the finish while the lights keep moving. `AUTO` restores the timed
presets; `PREVIOUS` and `NEXT` select a held phase. Six rails edit signed cut
depth, grain depth, copper roughness, gilding opacity, light bearing and light
elevation; dragging one enters manual mode. `NORMALS` toggles the hero and
selected-letter normals. The bearing readout is the moving key's base angle;
the key adds a periodic sweep on top of it.

Beside the stone, a height preview and a fixed reference normal at −2.8 pixels
stay unchanged through every light, phase and finish edit. The hero's own
normals follow the current signed depth, so they can differ from that fixed
reference.

The lower cards apply a procedural height field directly to material ink in the
Element, Glyph and Word domains, each under its own scene: a directional key on
the Element card, a moving Point and a moving Spot on the other two. Element ink
covers its whole leaf; on the Glyph and Word cards the `cut /` prefix stays
ordinary ink outside the selected material span. Reticles show the positioned
sources' placement; their height moves around 260 ± 24. The Word card changes
`I` and `B` font sizes within one mapping, and the final phase reflects the
Glyph card horizontally and stretches it vertically. The edge rail shows signed
and zero depth, fine lines, both roughness endpoints and a matching green-axis
normal pair; it keeps its normals when `NORMALS` turns the hero's off.

The relief is shaded, not cut: there is no V-shaped groove, displaced metal,
changed silhouette or groove shadow. Gilding and ink are masks, with no leaf
thickness, wiping or ink flow. The copper's brushing is scalar roughness, with
no anisotropic highlight. Every receiver is a flat Compose element. Georgia,
with serif fallbacks, supplies the letter shapes.
