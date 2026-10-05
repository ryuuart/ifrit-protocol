# Type / Matter

A 1440 × 1100 material typography atelier. Real Compose `OB8` text, ribbon
bands and fine rules are blurred into one held grayscale height texture. That
height changes the colour, metallic and roughness channels of a continuous
porcelain and gold surface, and its gradients supply signed normals. A smaller
comparison separates relief glyph foregrounds from the shared surface. Every
material on the page paints its colour stack flat. The type stays text, with
open counters.

## States

The 24-second loop has six four-second states: glazed impression, flat and
mirror, raw and matte, raised, copper intaglio, and counters cut.
`heldPhase` pins one. `kStates` gives each its depth in logical pixels,
roughness, normal strength, coating, overprint opacity and blur width; zero and
signed depth, roughness 0 and 1, opacity 0 and 1 and a clipped decorated glyph
are deliberate endpoints. The height texture changes only when the state does.

## The lettering rows

A lettering strip samples one held copper and gold texture. The prefix keeps
the page ink; `FOIL` reads the same texture across the phrase, afresh per
letter, or once across the word, and a larger `I` keeps the word's single
mapping across a font-size change. The strip stays the same
through all six states.

The lower row paints selected `FOIL` text in a metallic gold ink, once per
letter and once across the word. `kMetalInk` sets the roughness and normal
strength of that ink, which samples a held grain normal texture.

## What it does not do

This is a surface description on a flat page, not displaced geometry. The
coating shares the surface normal; there is no separate coat normal. Outline relief is
a decoration rather than an ink, so the comparison uses transparent ink, glyph
boundary selection and a relief foreground. Fine rules are filled material
shapes so they keep the surface description. No image asset is used.
