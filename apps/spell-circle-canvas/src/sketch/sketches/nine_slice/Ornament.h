#pragma once

// THE PIECES A MANUSCRIPT BORDER IS MADE OF, as this sketch draws them:
// the palettes and the carved nine-slice frame. Colour arrives as DATA — a `Palette` — so one piece renders in
// any tint.
#include <glm/vec2.hpp>
#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmedia/core/Image.h>

#include <functional>
#include <memory>
#include <vector>

namespace nine_slice {

using namespace sigil::compose;
namespace draw = sigil::draw;
namespace geometry = sigil::geometry;
namespace material = sigil::material;

/** A manuscript palette — every ornament component below is driven by
 *  one of these, never by hard-coded colors. */
struct Palette {
  material::Color parchment;  ///< page/panel ground
  material::Color ink;        ///< body text on that ground
  material::Color stem;       ///< flourish strokes (the watercolor "cobalt")
  material::Color leaf;       ///< sprig leaves (olive/ochre greens)
  material::Color gold;       ///< gilded accents: diamonds, dots, trims
};

inline Palette azurePalette() {
  return {{0.93f, 0.89f, 0.78f, 1},
          {0.17f, 0.14f, 0.11f, 1},
          {0.13f, 0.23f, 0.52f, 1},   // cobalt
          {0.44f, 0.47f, 0.20f, 1},   // olive
          {0.82f, 0.62f, 0.20f, 1}};  // ochre gold
}

inline Palette crimsonPalette() {
  return {{0.94f, 0.86f, 0.76f, 1},
          {0.23f, 0.10f, 0.08f, 1},
          {0.55f, 0.14f, 0.16f, 1},  // alizarin
          {0.40f, 0.42f, 0.16f, 1},
          {0.84f, 0.60f, 0.22f, 1}};
}

inline Palette oakPalette() {
  return {{0.88f, 0.80f, 0.62f, 1},
          {0.20f, 0.13f, 0.07f, 1},
          {0.36f, 0.22f, 0.11f, 1},
          {0.45f, 0.33f, 0.15f, 1},
          {0.85f, 0.64f, 0.22f, 1}};
}

/** Draws a carved dialog frame onto an intermediate canvas and hands
 *  back the image: rounded wood band, gilded trim, corner bosses, edge
 *  studs, translucent parchment center. The nine-slice source —
 *  generate once per palette, stretch everywhere. */
std::shared_ptr<const sigil::media::Image> makeCarvedFrame(const Palette& pal,
                                                           int size = 96);

/** The carved frame as a nine-slice decoration for any box size. @p density
 *  is the texture's pixels per layout unit: makeCarvedFrame's default 96 is
 *  the frame at its on-page size, so a texture generated at 192 to stay sharp
 *  on a 2x device passes 2 and its band stays the width the padding around it
 *  was measured against. */
inline Slice carvedFrameSlice(
    const std::shared_ptr<const sigil::media::Image>& asset,
    float density = 1.0f) {
  Slice nine;
  nine.asset = asset;
  const int size = asset ? asset->size().width() : 96;
  nine.xDivs = {size / 3, size * 2 / 3};
  nine.yDivs = {size / 3, size * 2 / 3};
  nine.density = density;
  return nine;
}

}  // namespace nine_slice
