# Soft Metal — Terminator 2 title study

A native reconstruction study of the machined **Terminator 2: Judgment Day**
(1991) title frame: two extended lines of recessed lettering, a plate seam,
horizontal brushed steel and reflected fire. Its palette, proportions and
letter terminals are compared with an actual film frame. The vector contours
and procedural flames are approximations, not extracted film artwork.

`Lettering.h` draws the required alphabet from closed contours with real
counters. Native material effects recess those outlines into the plate.
`steel.sksl` supplies the stationary brushed finish; the complete engraving
and its edges are rendered once into a retained texture.

`heat.sksl` supplies moving broad illumination and `fire.sksl` supplies
premultiplied flame coverage. Both run on smaller texture scenes; flame
coverage is limited to the lower part of the frame. Bright illumination
scales as one color to preserve its warm hue.
Native image multiply compositing combines the illumination with the
full-resolution plate, preserving sharp letter edges and scratches. The light
texture is opaque: coverage is not used to transport illumination data.
The two plate halves close, hold under moving firelight and retreat over a
12-second loop. This study uses a procedural lighting field rather than a
three-dimensional surface or a physical combustion simulation.

Open `soft_metal.cpp` in Sketchbook. The declared still is at 4 seconds.

Visual reference:
[Terminator 2: Judgment Day title frame — IMDb](https://www.imdb.com/title/tt0103064/mediaviewer/rm196894721/).
[Film and title-frame context — Filmsite](https://www.filmsite.org/terminator2.html).
The reference image is used for comparison and is not bundled in the sketch.
