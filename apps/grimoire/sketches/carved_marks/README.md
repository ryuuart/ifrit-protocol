# Carved / Marks

A 1440 × 1120 Compose material atelier: porous limestone beside brushed copper,
with the same letter die and layered marks pressed into both finishes. Real
`OB8` text keeps its counters. A pressure ribbon and two crossing fibre passes
overlap the lettering in one opaque grayscale height field. Dark ink and partial
gilding sample that field, so the marks change the material rather than sit
above it.

The stone takes the height as a cut; the copper reverses and reduces it into a
raised, chased finish. Local grain supplies a second normal layer. Every
material paints its colour stack flat; the normal, roughness and metallic
channels describe the surface without shading it. Stone pores, copper scores,
oxide tint and gold masks are authored fields.

The 16-second loop has four four-second phases:

| Seconds | Phase |
| --- | --- |
| 0–4 | Cut stone and chased copper |
| 4–8 | Zero macro depth; grain remains |
| 8–12 | Half gilding, dark ink retained |
| 12–16 | Reflected Glyph mapping; Word ink spans font-size changes |

`HOLD` pins the finish. `AUTO` restores the timed presets; `PREVIOUS` and
`NEXT` select a held phase. Four rails edit signed cut depth, grain depth,
copper roughness and gilding opacity; dragging one enters manual mode.
`NORMALS` toggles the hero and selected-letter normals.

Beside the stone, a height preview and a fixed reference normal at −2.8 pixels
stay unchanged through every phase and finish edit. The hero's own normals
follow the current signed depth, so they can differ from that fixed reference.

The lower cards apply a procedural height field directly to material ink in the
Element, Glyph and Word domains. Element ink covers its whole leaf; on the
Glyph and Word cards the `cut /` prefix stays ordinary ink outside the selected
material span. The Word card changes `I` and `B` font sizes within one mapping,
and the final phase reflects the Glyph card horizontally and stretches it
vertically. The edge rail shows signed and zero depth, fine lines, both
roughness endpoints and a matching green-axis normal pair; it keeps its normals
when `NORMALS` turns the hero's off.

The relief is described, not cut: there is no V-shaped groove, displaced metal,
changed silhouette or groove shadow. Gilding and ink are masks, with no leaf
thickness, wiping or ink flow. The copper's brushing is scalar roughness, with
no anisotropic highlight. Every receiver is a flat Compose element. Georgia,
with serif fallbacks, supplies the letter shapes.
