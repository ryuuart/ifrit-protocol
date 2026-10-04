# Pigment brushes

A 1440 × 1080 Compose pigment workshop: blue oil ridges over a dry ground, rust
marks, a metallic overprint, thick lettering and very small material lines. The
marks are native variable-width ribbons and brush layers. Their fills and the
letter inks share the same materials, ridge normals and inherited moving light.
The broad title takes its raised shoulder from its glyph outline.

The 24-second loop has six four-second states. `heldPhase` pins one;
`Controls` sets the ridge normal strength, wet roughness and metallic overprint
opacity. Fixed coupons keep dry, wet, metal, translucent overlap and small
clipped marks visible beside every state.

| Seconds | State | What changes |
| --- | --- | --- |
| 0–4 | Loaded bristles | Brush order, ridges, wet highlights and raised letters |
| 4–8 | Normal zero | Colours remain; ridge lighting disappears |
| 8–12 | Roughness zero | Sharp specular response, with coverage intact |
| 12–16 | Roughness one | Diffuse endpoint with the normal field retained |
| 16–20 | Overprint zero | The metallic brush disappears; the paint beneath remains |
| 20–24 | Overprint one | Opaque overlap, 0.75-pixel lines, clipped origins and open O/8 counters |

The paper, paths, bristle grooves and grain are generated from fixed seeds. The
pigment response is an authored appearance: there is no solvent flow, drying,
spectral mixing or transport between layers.
