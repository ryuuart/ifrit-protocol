#pragma once

// THE GIRIH PANEL: the eight-fold star-and-cross tile a Fes or Nasrid
// zellige panel is cut from, as a pattern tile — the ground, the khatam,
// the outlined straps and the corner fillers, the classic 45° panel in
// closed form and Hankin's rays at any other contact angle.

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/pattern/Tile.h>

namespace zellige {

using sigil::material::Color;
namespace pattern = sigil::material::pattern;

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
 *  4.8.8 tiling, in closed form: octagons of edge @p edge on a square
 *  lattice of spacing edge·(1+√2), whose one tile repeats seamlessly.
 *  @p contactDegrees is Hankin's CONTACT ANGLE in degrees, the one dial of
 *  the construction, and the star sharpens as it grows; 45 is the
 *  classic panel. @p strapWidth 0 means 0.12·edge. */
pattern::Tile girih8(float edge, GirihPalette palette = fezPalette(),
                     float strapWidth = 0, float contactDegrees = 45.0f);

}  // namespace zellige
