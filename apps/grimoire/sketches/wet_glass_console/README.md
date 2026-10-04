# Wet glass console

A 1440 × 1040 Compose material study: water beads, wipe marks and rivulets over
a live environmental instrument. The dark console, telemetry, lettering and
calibration strip are invented.

The metal frame, title ink, glass cover and water caps are material surfaces
under a moving shared light, and two waveform materials animate beneath the
glass. The droplets move through Compose transforms; each one's optical profile
stays fixed and samples whatever is currently beneath it. Rounded display
clipping and the separate layers stay visible in the overlap state.

Tall separate caps and spread films are the two wet conditions; the cap height
is an authored normal amplitude, not a contact angle. Slow drift, pear-shaped
silhouettes and trails suggest beads sliding down the glass, without adhesion,
coalescence, gravity or a conserved volume. Each bead is a lens: a bounded
backdrop displacement with slight colour separation and optional diffusion,
not a traced optical model. Displacement is clamped to 18 local pixels, and
colour separation and diffusion stay within a 40-pixel sample radius. The lens
filter holds no time: motion belongs to the droplet node, and the moving trace
belongs to the material beneath.

## Controls and states

`Controls` holds wetness, bead scale, bend in local pixels, cap height,
roughness, coating opacity, film amount, light elevation and drift amplitude.
`WetGlassConsole::heldPhase` holds a state from 0 to 8; its default of −1
advances every four seconds through a 36-second loop.

| Seconds | State | What changes |
| --- | --- | --- |
| 0–4 | Beading | Distinct caps over live telemetry |
| 4–8 | Dry | Uncovered glass and moving waveforms |
| 8–12 | Wet | Shallow film, smears and elongated trails |
| 12–16 | Micro | Smaller caps and lower displacement |
| 16–20 | Macro | Larger clipped lenses over text and traces |
| 20–24 | Grazing | Six-degree light over the cap normals |
| 24–28 | Overlap | Stacked lenses crossing the display boundary |
| 28–32 | Zero | No coating and no displacement |
| 32–36 | One | An opaque coating, deliberately unlike water |

The fixed strip compares dry and wet versions of the same chart, 3–12-pixel
beads, a large clipped lens, roughness one, opacity zero and one, and zero- and
one-pixel shapes.
