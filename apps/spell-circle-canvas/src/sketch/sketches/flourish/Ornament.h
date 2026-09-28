#pragma once

// THE PIECES A MANUSCRIPT BORDER IS MADE OF, as this sketch draws them:
// the palette, tapered sweeps rolling into spirals, the carved nine-slice frame and the scalloped outline. Colour arrives as DATA — a `Palette` — so one piece renders in
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

namespace flourish {

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

/** Appends `steps` samples of a cubic bezier to `out`. */
void appendCubic(std::vector<glm::vec2>& out, glm::vec2 p0, glm::vec2 c1,
                 glm::vec2 c2, glm::vec2 p1, int steps = 22);

/** Appends an Archimedean spiral: radius sweeps r0→r1 while the angle
 *  sweeps a0→a1 (radians) around `center`. Roll tips with r1 ≈ 0. */
void appendSpiral(std::vector<glm::vec2>& out, glm::vec2 center, float r0,
                  float r1, float a0, float a1, int steps = 26);

/** The filled outline that strokes the centerline with a width that
 *  swells in the middle and thins to hairlines at both tips — the
 *  calligraphic taper every scrollwork flourish reads by. */
geometry::path::Outline taperedStroke(const std::vector<glm::vec2>& pts,
                                      float wMax, float wTip = 0.4f);

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
  const int size = asset ? asset->size().x : 96;
  nine.xDivs = {size / 3, size * 2 / 3};
  nine.yDivs = {size / 3, size * 2 / 3};
  nine.density = density;
  return nine;
}

/** Scalloped outline: rounded lobes bulging out of each edge — the
 *  cloud-bubble / wax-seal silhouette. */
std::function<geometry::path::Outline(glm::vec2)> scallopOutline(
    float lobe = 14.0f);

}  // namespace flourish
