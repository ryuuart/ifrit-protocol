#pragma once

/** @file
 * @ingroup material-pattern
 *
 * The stock tile generators — every one fully parameterised: a halftone
 * dot grid, stripes, a coloured sequence of runs, a checker, grid lines
 * and a seeded speckle. Each returns a Tile: `texture()` samples it,
 * `seed(n)` re-rolls the seeded ones, `rotate()` and `scale()` remap
 * without rebaking.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/pattern/Tile.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace sigil::material::pattern {

/** Square-grid halftone dots (staggered via a second offset row). */
Tile halftone(float spacing, float radius, Color color, bool staggered = true);

/** Stripes along +x (rotate the tile for diagonals — stays seamless).
 *  @p on is the painted width and @p off the gap; a non-positive @p on
 *  draws nothing. */
Tile stripes(float on, float off, Color color);

/** WHICH WAY A RUN OF COLOUR TRAVELS across the tile: along +x, or down
 *  +y. A sett is woven both ways, and the two together are the check —
 *  which is a sequence over a sequence and not one pattern. */
enum class Axis : uint8_t { U, V };

/** A COLOURED SEQUENCE of runs along @p along — a tartan sett, an
 *  awning, a ribbon edge. Each run is {width px, colour}, the period is
 *  their sum, and @p phase slides the sequence along that axis in px,
 *  wrapped.
 *  @trap The axis is asked for rather than left to a rotation, because
 *  rotating remaps the sampling instead of baking the runs down the
 *  tile. No run of positive width draws nothing. */
Tile sequence(std::vector<std::pair<float, Color>> runs, float phase = 0.0f,
              Axis along = Axis::U);

/** 2×2 checkerboard. */
Tile checker(float cell, Color a, Color b);

/** Grid lines (graph paper), one pitch per axis. */
Tile gridLines(float spacingX, float spacingY, float width, Color color);
/** Square pitch. */
inline Tile gridLines(float spacing, float width, Color color) {
  return gridLines(spacing, spacing, width, color);
}

/** Seeded speckle (paper grain, star fields): @p count marks per tile with
 *  radii in [rMin, rMax], colours cycled from the palette — deterministic
 *  per seed, `seed(n)` re-rolls the field. */
Tile speckle(float tileSize, int count, float rMin, float rMax,
             std::vector<Color> palette);

}  // namespace sigil::material::pattern
