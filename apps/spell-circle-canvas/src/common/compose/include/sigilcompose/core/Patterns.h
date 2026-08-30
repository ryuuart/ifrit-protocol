#pragma once

/** @file
 * SigilCompose stock pattern generators — SigilMaterial's tiles and
 * fields spelled as compose values. TWO return types live here and the
 * call shape differs:
 *
 *  - the baked-tile generators return a `Pattern` — `.material()` turns it
 *    into a fill, `.seed(n)` re-rolls the seeded ones, `.rotate()`/
 *    `.scale()` remap without rebaking;
 *  - the shader-field generators (`halftoneRamp`, `noise`, `grain`) return
 *    a `Material` directly. They are fields, not tiles, so there is no
 *    `.material()` call on them.
 *
 * A returned Material carries a freshly minted shader, so its identity is
 * fresh too: hold it as a member if the node that fills with it should
 * prune across re-describes.
 */

#include <sigilcompose/core/Material.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Patterns.h>
#include <sigilmaterial/pattern/Patterns.h>

#include <utility>
#include <vector>

namespace sigil::compose::patterns {

namespace detail {
inline sigil::material::Color color(SkColor4f c) {
  return {c.fR, c.fG, c.fB, c.fA};
}
inline std::vector<sigil::material::Color> colors(
    const std::vector<SkColor4f>& in) {
  std::vector<sigil::material::Color> out;
  out.reserve(in.size());
  for (SkColor4f c : in) out.push_back(color(c));
  return out;
}
}  // namespace detail

/** Square-grid halftone dots (stagger via a second offset row). */
inline Pattern halftone(float spacing, float radius, SkColor4f color,
                        bool staggered = true) {
  return sigil::material::pattern::halftone(spacing, radius,
                                            detail::color(color), staggered);
}

/** Stripes along +x (rotate the Pattern for diagonals — stays seamless).
 *  @p on is the painted width and @p off the gap; a non-positive @p on
 *  draws nothing. */
inline Pattern stripes(float on, float off, SkColor4f color) {
  return sigil::material::pattern::stripes(on, off, detail::color(color));
}

/** A COLOURED SEQUENCE of runs along +x — a tartan sett, an awning, a
 *  ribbon edge, a chart axis: as many colours as there are runs. Each run
 *  is {width px, color}; the tile period is their sum; @p phase slides
 *  the whole sequence along +x (px, wrapped). Rotate the Pattern for
 *  diagonals — stays seamless. If no run has a positive width the result
 *  draws nothing. */
inline Pattern sequence(std::vector<std::pair<float, SkColor4f>> runs,
                        float phase = 0.0f) {
  std::vector<std::pair<float, sigil::material::Color>> out;
  out.reserve(runs.size());
  for (const auto& [w, c] : runs) out.emplace_back(w, detail::color(c));
  return sigil::material::pattern::sequence(std::move(out), phase);
}

/** 2×2 checkerboard. */
inline Pattern checker(float cell, SkColor4f a, SkColor4f b) {
  return sigil::material::pattern::checker(cell, detail::color(a),
                                           detail::color(b));
}

/** Grid lines (graph/blueprint paper). */
inline Pattern gridLines(float spacingX, float spacingY, float width,
                         SkColor4f color) {
  return sigil::material::pattern::gridLines(spacingX, spacingY, width,
                                             detail::color(color));
}
/** Square pitch — the common case, forwarding to the two-pitch form. */
inline Pattern gridLines(float spacing, float width, SkColor4f color) {
  return gridLines(spacing, spacing, width, color);
}

/** Seeded speckle (paper grain, star fields): `count` marks per tile with
 *  radii in [rMin, rMax], colors cycled from the palette — deterministic
 *  per seed, `.seed(n)` re-rolls the field. */
inline Pattern speckle(float tileSize, int count, float rMin, float rMax,
                       std::vector<SkColor4f> palette) {
  return sigil::material::pattern::speckle(tileSize, count, rMin, rMax,
                                           detail::colors(palette));
}

/** The halftone RAMP: dot radius swells from `rMin` at the node's top to
 *  `rMax` at its bottom, evaluated in one pass. A Material, not a baked
 *  tile — the ramp reads the node's height, so it resolves when the node
 *  records and stays picture-cached between layouts. `angleDeg` rotates
 *  the dot grid; the ramp stays vertical. `rampFrom`/`rampTo` remap where
 *  the swell runs, as fractions of the node's height. To DRIFT the
 *  field, bind uDriftX / uDriftY (`.uniform("uDriftX", &phase)`): the
 *  material goes live and the dots slide under a fixed ramp. Keep rMax
 *  below roughly 0.45·spacing or neighbouring dots fuse. */
inline Material halftoneRamp(float spacing, float rMin, float rMax,
                             SkColor4f color, float angleDeg = 0.0f,
                             float rampFrom = 0.0f, float rampTo = 1.0f) {
  return Material::recipe(sigil::material::field::halftoneRamp(
      spacing, rMin, rMax, detail::color(color), angleDeg, rampFrom, rampTo));
}

/** Perlin fractal noise as a Material — the organic texture floor.
 *  `frequency` is features-per-px (0.01–0.05 reads as clouds/paper at UI
 *  scale; ~0.9 as film grain); `turbulence` uses the abs-value variant
 *  (sharper, veiny). The three channels are INDEPENDENT fields, which is
 *  right for a displacement source and wrong for grain — see `grain()`. */
inline Material noise(float frequency, int octaves = 4, float seed = 1.0f,
                      bool turbulence = false) {
  return Material::recipe(
      sigil::material::field::noise(frequency, octaves, seed, turbulence));
}

/** LUMINANCE noise — value-noise fBm collapsed to one channel, so a blend
 *  mode over a coloured surface reads as light rather than as a hue shift:
 *  paper tooth, film grain, stone veining, worn metal. `contrast` scales
 *  the field about 0.5; `stretch` divides the x frequency and multiplies
 *  the y one, so > 1 runs the fibre lengthwise. Keep
 *  `frequency · stretch · 2^(octaves-1)` under roughly 0.4 or the y axis
 *  aliases. GRAIN WANTS AN OPAQUE SURFACE: the shader returns its own
 *  opaque luminance, so over a near-transparent base it composites as that
 *  luminance instead of modulating what is beneath. */
inline Material grain(float frequency, int octaves = 4, float seed = 1.0f,
                      float contrast = 1.0f, float stretch = 1.0f) {
  return Material::recipe(sigil::material::field::grain(
      frequency, octaves, seed, contrast, stretch));
}

// ---------------------------------------------------------------------------
// Islamic geometric pattern (girih) — the kit's panel and its palettes.

using sigil::material::kit::fezPalette;
using sigil::material::kit::GirihPalette;
using sigil::material::kit::nasridPalette;

/** The 8-fold star-and-cross panel: octagons of edge `edge` on a square
 *  lattice, the {8/2} khatam at each centre, the crosses as the leftover
 *  ground. */
inline Pattern girih8(float edge, GirihPalette pal = fezPalette(),
                      float strapWidth = 0 /* 0 → 0.12·edge */) {
  return sigil::material::kit::girih8(edge, pal, strapWidth);
}

}  // namespace sigil::compose::patterns
