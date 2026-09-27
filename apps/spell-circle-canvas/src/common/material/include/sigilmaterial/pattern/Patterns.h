#pragma once

/** @file
 * @ingroup material-pattern
 *
 * The stock tile generators — every one fully parameterised: a halftone
 * dot grid, stripes, a coloured sequence of runs, a checker, grid lines
 * and a seeded speckle. Each returns a Tile: `texture()` samples it,
 * `seed(n)` re-rolls the seeded ones, `rotate()` and `scale()` remap
 * without rebaking.
 *
 * Beside them stand the two LATTICE SOURCES, scanlines and a stipple:
 * hard-edged repeats read per pixel rather than baked, each a Material a
 * `layer()` blends over a base through its own options.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
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
Tile checker(float cell, Color first, Color second);

/** Grid lines (graph paper), one pitch per axis. */
Tile gridLines(float spacingX, float spacingY, float width, Color color);
/** Square pitch. */
inline Tile gridLines(float spacing, float width, Color color) {
  return gridLines(spacing, spacing, width, color);
}

/** Seeded speckle (paper grain, star fields): @p count marks per tile with
 *  radii in [minimumRadius, maximumRadius], colours cycled from the palette — deterministic
 *  per seed, `seed(n)` re-rolls the field. */
Tile speckle(float tileSize, int count, float minimumRadius, float maximumRadius,
             std::vector<Color> palette);

/** @name Lattice sources
 *  A repeat on the pixel lattice, read per pixel of the box it fills and
 *  compared by its numbers, so a node layered with one prunes like any
 *  other static fill. Neither bakes a tile: a tile's identity is its
 *  bake, which one described afresh would never share. How it meets what
 *  is beneath it — `Plus` for the phosphor reading of scanlines, a low
 *  black alpha through `Normal` for the print reading — is the layer's
 *  `blend`, and where it lands is the layer's `mask`:
 *
 *      material::from(panel).layer(material::pattern::scanlines({.color = tint}),
 *                                  {.blend = BlendMode::Plus})
 *  @{ */

/** Hard rows across the box. */
struct ScanlineOptions {
  Color color = {0, 0, 0, 0.2f};
  /** Px from one row to the next. */
  float period = 4.0f;
  /** Px of each period the row covers; a pixel takes the colour where
   *  its centre lies in them. */
  float on = 2.0f;
  /** Px the rows are slid down by. */
  float phase = 0.0f;
  bool operator==(const ScanlineOptions&) const = default;
};

/** SCANLINES: a band `on` px tall every `period` px, in `color` and clear
 *  between — the raster of the tube, laid over a panel. Hard rows,
 *  deliberately: a raised-cosine beam is a different picture, a shader a
 *  sketch writes. */
Material scanlines(const ScanlineOptions& options = {});

/** One colour through a repeating 1-bit mask. */
struct StippleOptions {
  Color color = {0, 0, 0, 1};
  /** Bit `y * size + x`, counting from the low bit, is the cell at
   *  (x, y): set takes the colour, clear leaves the cell clear. The
   *  default is the 50 % checkerboard, cells (0, 0) and (1, 1) of a
   *  2 × 2 lattice. */
  uint64_t bits = 0b1001;
  /** Cells on a side, one to eight. */
  int size = 2;
  /** Px per cell. Whole numbers keep the mask on the lattice. */
  float cell = 1.0f;
  bool operator==(const StippleOptions&) const = default;
};

/** THE STIPPLE: `color` laid through the mask `bits` names, repeating
 *  from the box's corner — the greyed-out control of a toolkit with no
 *  alpha, a dither tone, a printer's screen. The mask is bits rather than
 *  a picture so one colour or another is the same mask. A size outside
 *  one to eight, a cell of no width or an empty mask paints nothing. */
Material stipple(const StippleOptions& options = {});

/** THE ORDERED-DITHER MASK of @p on cells in every `size × size` block,
 *  filled in the order the recursive Bayer matrix fills them — each step
 *  quadruples the lattice with the quadrants 0, 2, 3 and 1 quarters of
 *  the range up, which spreads a tone's cells as far apart as the lattice
 *  allows. @p size is 2, 4 or 8; @p on runs from 0 (nothing) to
 *  `size * size` (solid). For `StippleOptions::bits`. */
uint64_t ditherBits(int on, int size = 4);

/** @} */

}  // namespace sigil::material::pattern
