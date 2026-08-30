#pragma once

/** @file
 * Pattern presets: the Islamic geometric panel with its named palettes.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/pattern/Tile.h>

namespace sigil::material::kit {

/** Zellige colour roles for the girih generators. */
struct GirihPalette {
  Color ground;     ///< the crosses (the leftover between stars)
  Color star;       ///< the khatam star fill
  Color strap;      ///< the ribbon
  Color strapEdge;  ///< the ribbon's dark outline
};
/** Fez palette: blue stars on teal ground, bone straps outlined in ink. */
inline GirihPalette fezPalette() {
  return {{0.078f, 0.463f, 0.420f, 1},
          {0.106f, 0.294f, 0.608f, 1},
          {0.914f, 0.878f, 0.796f, 1},
          {0.180f, 0.129f, 0.106f, 1}};
}
/** Nasrid-leaning variant: parchment stars on deep blue. */
inline GirihPalette nasridPalette() {
  return {{0.204f, 0.329f, 0.612f, 1},
          {0.918f, 0.890f, 0.816f, 1},
          {0.663f, 0.435f, 0.180f, 1},
          {0.149f, 0.125f, 0.110f, 1}};
}

/** The 8-fold star-and-cross panel — real polygons-in-contact on the
 *  4.8.8 tiling, in closed form: octagons of edge @p edge sit on a square
 *  lattice of spacing s = edge·(1+√2), and the octagon APOTHEM equals s/2
 *  exactly, so one s×s tile (octagon at centre, square fillers at the
 *  corners) repeats seamlessly. The 45° contact angle turns every octagon
 *  into the {8/2} khatam and every filler square into its inscribed
 *  square — the strapwork of the classic panel. The crosses are the
 *  leftover ground. @p strapWidth 0 means 0.12·edge. */
pattern::Tile girih8(float edge, GirihPalette pal = fezPalette(),
                     float strapWidth = 0);

}  // namespace sigil::material::kit
