# Soft Metal

An original material-led type poster. The two large words carry a folded
chrome surface, with a slowly revolving studio reflection, a directional
key light and a bevel on the glyph coverage. The smaller type is matte ink
on green paper, keeping the reading hierarchy independent of reflections.

`folds.sksl` supplies a stationary tangent-space normal field. `studio.sksl`
supplies an equirectangular environment with softboxes and dark flags. The
native material surface combines their response; the poster animates light
and reflection angles rather than rebuilding its text or geometry.

Open `soft_metal.cpp` in Sketchbook. The declared still is at 3 seconds.

The normal field, studio environment and paper composition are retained.
The moving light shades a small texture; the full-resolution text samples that
smooth field. This keeps the letter outlines sharp without evaluating a studio
reflection independently at every display pixel. The reflection and bevel remain on one glyph material so they share the same coverage.
