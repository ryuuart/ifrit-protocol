#pragma once

/** @file
 * SigilCompose stock pattern generators. TWO return types live here and
 * the call shape differs:
 *
 *  - the baked-tile generators return a `Pattern` — `.material()` turns it
 *    into a fill, `.seed(n)` re-rolls the seeded ones, `.rotate()`/
 *    `.scale()` remap without rebaking;
 *  - the shader-field generators (`halftoneRamp`, `noise`, `grain`) return
 *    a `Material` directly. They are SkSL fields, not tiles, so there is
 *    no `.material()` call on them.
 *
 * A returned Material carries a freshly minted shader, so its identity is
 * fresh too: hold it as a member if the node that fills with it should
 * prune across re-describes.
 */

#include <utility>
#include <vector>

#include "sigilcompose/Material.h"  // halftoneRamp is a Material (SkSL)
#include "sigilcompose/Pattern.h"
#include "sigilcompose/Shapes.h"

namespace sigil::compose::patterns {

/** Square-grid halftone dots (stagger via a second offset row). */
Pattern halftone(float spacing, float radius, SkColor4f color,
                 bool staggered = true);

/** Stripes along +x (rotate the Pattern for diagonals — stays seamless).
 *  @p on is the painted width and @p off the gap; a non-positive @p on
 *  draws nothing. */
Pattern stripes(float on, float off, SkColor4f color);

/** A COLOURED SEQUENCE of runs along +x — a tartan sett, an awning, a
 *  ribbon edge, a chart axis: as many colours as there are runs, which
 *  neither stripes()' single colour nor a gradient's stop list expresses
 *  (a 24-run sett against six stops). Each run is {width px, color}; the
 *  tile period is their sum; @p phase slides the whole sequence along +x
 *  (px, wrapped). Rotate the Pattern for diagonals — stays seamless.
 *
 *  If no run has a positive width the period is zero and the result is a
 *  degenerate pattern that draws nothing. */
Pattern sequence(std::vector<std::pair<float, SkColor4f>> runs,
                 float phase = 0.0f);

/** 2×2 checkerboard. */
Pattern checker(float cell, SkColor4f a, SkColor4f b);

/** Grid lines (graph/blueprint paper). */
Pattern gridLines(float spacingX, float spacingY, float width, SkColor4f color);
/** Square pitch — the common case, forwarding to the two-pitch form. */
inline Pattern gridLines(float spacing, float width, SkColor4f color) {
  return gridLines(spacing, spacing, width, color);
}

/** Seeded speckle (paper grain, star fields): `count` marks per tile with
 *  radii in [rMin, rMax], colors cycled from the palette — deterministic
 *  per seed, `.seed(n)` re-rolls the field. */
Pattern speckle(float tileSize, int count, float rMin, float rMax,
                std::vector<SkColor4f> palette);

/** The halftone RAMP: dot radius swells from `rMin` at the node's top to
 *  `rMax` at its bottom, evaluated in one SkSL pass. A Material, not a
 *  baked tile — the ramp reads the node's height, so it resolves when the
 *  node records and stays picture-cached between layouts.
 *
 *  `angleDeg` rotates the dot grid; the ramp stays vertical.
 *  `rampFrom`/`rampTo` remap where the swell runs, as fractions of the
 *  node's height (0.25→0.9 confines it to the lower band).
 *
 *  To DRIFT the field, bind uDriftX / uDriftY
 *  (`.uniform("uDriftX", &phase)`): the material goes live and the dots
 *  slide under a fixed ramp. Drift wraps seamlessly at a period of
 *  2·spacing·√2 px along a 45° grid (in general, d·cosθ ≡ 0 mod
 *  2·spacing).
 *
 *  Keep rMax below roughly 0.45·spacing or neighbouring dots fuse. */
Material halftoneRamp(float spacing, float rMin, float rMax, SkColor4f color,
                      float angleDeg = 0.0f, float rampFrom = 0.0f,
                      float rampTo = 1.0f);

/** Perlin fractal noise as a Material (SkPerlinNoiseShader — no SkSL):
 *  the organic texture floor. `frequency` is features-per-px (0.01–0.05
 *  reads as clouds/paper at UI scale; ~0.9 as film grain); `turbulence`
 *  uses the abs-value variant (sharper, veiny — brushed-metal fodder).
 *  Each call mints a fresh shader, so HOLD the returned Material as a
 *  member if the node filled with it should prune across re-describes.
 *
 *  The three channels are INDEPENDENT fields, which is right for a
 *  displacement source and wrong for grain — see `grain()` below. */
Material noise(float frequency, int octaves = 4, float seed = 1.0f,
               bool turbulence = false);

/** LUMINANCE noise — value-noise fBm collapsed to one channel.
 *
 *  `noise()` above wraps Skia's Perlin shader, whose three channels are
 *  INDEPENDENT fields. That is the right thing for a displacement source
 *  and the wrong thing for grain: composited over a coloured surface with
 *  kOverlay or kSoftLight it does not darken and lighten the surface, it
 *  hue-shifts it — porphyry comes out as rainbow terrazzo. Every "make
 *  this surface less perfect" move — paper tooth, film grain, stone
 *  veining, dither, worn metal — wants THIS one, where all three channels
 *  carry the same value and a blend mode reads as light.
 *
 *  `frequency` is features-per-px on the same scale as `noise()`.
 *  `contrast` scales the field about 0.5 — wood tooth lives around
 *  0.25–0.35, and at 1.0 a soft-light pass turns timber into polished
 *  granite. `stretch` is anisotropy: the cells are divided by it in x and
 *  multiplied in y, so > 1 runs the fibre lengthwise (wood, brushed
 *  metal) and 1.0 is isotropic (dust, paper, stone).
 *
 *  GRAIN WANTS AN OPAQUE SURFACE. The shader returns its own opaque
 *  luminance, so over a near-transparent base it COMPOSITES AS THAT
 *  LUMINANCE instead of modulating what is beneath — a nebula authored at
 *  15% alpha comes back a white cloud. Multiply grain over a solid ground
 *  (or into an opaque blend() stack); do not expect it to read through its
 *  own node's alpha.
 *
 *  MIND THE PRODUCT. `stretch` divides the x frequency but MULTIPLIES the
 *  y one by the same factor, so it is not free: keep
 *  `frequency · stretch · 2^(octaves-1)` under roughly 0.4, or the y axis
 *  aliases and you get hash noise with no diagnostic. Brushed metal at ×3
 *  wants something like `frequency 0.075, stretch 5` — not
 *  `frequency 6, stretch 6`, which asks for 36 cycles per pixel.
 *
 *  TWO IMPLEMENTATION RULES BIND EVERY STOCK SkSL MATERIAL HERE, this one
 *  included, and violating either crashes rather than misdraws. A sketch
 *  dylib carries its OWN copy of Skia: Skia is built hidden-visibility, so
 *  a dylib links it directly instead of resolving it from the host.
 *  `SkRuntimeEffect::MakeForShader` therefore builds the SkSL AST inside
 *  the dylib's Skia image while the program generator and the SkSL inliner
 *  run inside the host's, and virtual dispatch across that boundary faults
 *  on pointer authentication. So:
 *
 *    1. keep `main()` MONOLITHIC — no user-defined SkSL functions, so the
 *       inliner never runs;
 *    2. avoid a UNIFORM-GUARDED `break` — bake the loop count into the
 *       source string and cache one effect per count instead, which is
 *       what the octave cache below is for.
 *
 *  Neither is detectable at compile time: `MakeForShader` returns a valid
 *  effect and an empty error string either way, and the fault only lands
 *  when a sketch paints it. `sketches/stock_materials.cpp` paints one of
 *  each stock material from a sketch dylib and runs as a test, so the rule
 *  is enforced by the build rather than by memory. */
Material grain(float frequency, int octaves = 4, float seed = 1.0f,
               float contrast = 1.0f, float stretch = 1.0f);

// ---------------------------------------------------------------------------
// Islamic geometric pattern (girih)

/** Zellige color roles for the girih generators. */
struct GirihPalette {
  SkColor4f ground;     // the crosses (the leftover between stars)
  SkColor4f star;       // the khatam star fill
  SkColor4f strap;      // the ribbon
  SkColor4f strapEdge;  // the ribbon's dark outline
};
/** Fez palette: blue stars on teal ground, bone straps outlined in ink. */
inline GirihPalette fezPalette() {
  return {{0.078f, 0.463f, 0.420f, 1},   // #14766B
          {0.106f, 0.294f, 0.608f, 1},   // #1B4B9B
          {0.914f, 0.878f, 0.796f, 1},   // #E9E0CB
          {0.180f, 0.129f, 0.106f, 1}};  // #2E211B
}
/** Nasrid-leaning variant: parchment stars on deep blue. */
inline GirihPalette nasridPalette() {
  return {{0.204f, 0.329f, 0.612f, 1},   // #34549C
          {0.918f, 0.890f, 0.816f, 1},   // #EAE3D0
          {0.663f, 0.435f, 0.180f, 1},   // #A96F2E
          {0.149f, 0.125f, 0.110f, 1}};  // #26201C
}

/** The 8-fold star-and-cross panel ("Breath of the Compassionate") — real
 *  Hankin polygons-in-contact on the 4.8.8 tiling, in closed form: octagons
 *  of edge `a` sit on a square lattice of spacing s = a(1+√2), and the
 *  octagon APOTHEM equals s/2 exactly, so one s×s tile (octagon at center,
 *  square fillers at the corners) repeats seamlessly. The contact angle
 *  θ = 45° turns every octagon into the {8/2} khatam (two overlapped
 *  squares through the edge midpoints) and every filler square into its
 *  inscribed square — the strapwork of the classic panel. The crosses are
 *  the leftover ground, exactly as on the walls of Fez. */
Pattern girih8(float edge, GirihPalette pal = fezPalette(),
               float strapWidth = 0 /* 0 → 0.12·edge */);

}  // namespace sigil::compose::patterns
