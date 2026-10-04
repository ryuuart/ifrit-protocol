# Type / Matter

A 1440 × 1100 material typography atelier. Real Compose `OB8` text, ribbon
bands and fine rules are blurred into one held grayscale height texture. That
height changes the colour, metallic and roughness channels of a continuous
porcelain and gold surface, and its gradients supply signed normals. A smaller
comparison separates contour-lit glyph foregrounds from the shared surface.
The type stays text, with open counters.

## States

The 24-second loop has six four-second states: glazed impression, flat and
mirror, raw and matte, raking and raised, copper intaglio, and counters cut.
`heldPhase` pins one. `kStates` gives each its depth in logical pixels,
roughness, normal strength, coating, overprint opacity and blur width; zero and
signed depth, roughness 0 and 1, opacity 0 and 1 and a clipped decorated glyph
are deliberate endpoints. The studio direction and environment rotation keep
moving; the height texture changes only when the state does.

## The lettering rows

A lettering strip samples one held copper and gold texture, lit once by its own
fixed light. The prefix stays unlit; `FOIL` reads the same texture across the
phrase, afresh per letter, or once across the word, and a larger `I` keeps the
word's single mapping across a font-size change. The strip stays the same
through all six states.

The lower row lights selected `FOIL` text under three independent scenes: a
frontal directional key on Glyph ink, and a moving Point and a moving Spot on
Word ink. `kLive` sets their strength, ambient, range, key elevation and spot
cone, and the roughness and normal strength of the static gold ink. The positioned
sources sweep in X, Y and height (240 ± 36); copper dots mark their placement
and a needle marks the key's bearing. The ink samples a held grain normal
texture, and these lights keep moving when `heldPhase` pins the material state.

## What it does not do

This is bump shading on a flat surface, not displaced geometry. The coating
shares the surface normal; there is no separate coat normal. Outline relief is
a decoration rather than an ink, so the comparison uses transparent ink, glyph
boundary selection and a relief foreground. Fine rules are filled material
shapes so they keep the surface response. No image asset is used.
