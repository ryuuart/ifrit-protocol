# CRT shader attribution

`field/shaders/CrtBeam.sksl`, `field/shaders/CrtBloom.sksl` and
`field/shaders/CrtGlass.sksl` adapt the curvature, RGB spread and
scanline rasterization from cool-retro-term by Filippo Scognamiglio and
contributors:

- https://github.com/Swordfish90/cool-retro-term
- Revision: `1394ce82fa53d2a87d5adc4d99f9eeba598ea53d`
- Sources: `app/shaders/terminal_static.frag`, `terminal_dynamic.frag`
- License: GPL-3.0-or-later. The license text is in `COPYING.CRT`.

The adaptation uses SkSL child shaders, explicit local bounds and time,
a bloom taken from a second child the executor fills with the layer
blurred, black outside the curved screen, and procedural noise. It
splits the beam, the light and the glass into three recipes a caller
may take one at a time. It preserves input colour rather than applying
a terminal palette. It does not implement frame reflections or temporal
burn-in.
