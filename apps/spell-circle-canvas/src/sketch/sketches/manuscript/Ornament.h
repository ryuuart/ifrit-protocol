#pragma once

// THE PIECES A MANUSCRIPT BORDER IS MADE OF, as this sketch draws them:
// tapered sweeps rolling into spirals, sprigs, corner curls and the illuminated panel. Colour arrives as DATA — a `Palette` — so one piece renders in
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

namespace manuscript {

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

/** Parchment ground: the base tint modulated by fractal noise — the
 *  patterned dialog background, in any color. */
Fill parchmentFill(material::Color base, float frequency = 0.045f);

/** A half-edge flourish, drawn from a corner toward the edge's
 *  midpoint: long hairline rules threading beneath a tapered sweep
 *  that lifts off the corner and rolls into a tight spiral eye short
 *  of the midpoint, with an under-curl answering it the other way.
 *  Eight of these (4 corners × both adjacent edges) build the classic
 *  mirrored scrollwork border — facing spiral pairs meet at every edge
 *  midpoint and the sweeps cross at the corner diamonds.
 *  `quadrant`: 0=NW, 1=NE, 2=SE, 3=SW; `vertical` runs the band down
 *  the side edge instead of along the top/bottom. */
PaintProgram edgeFlourish(const Palette& pal, int quadrant, bool vertical);

/** A leafy sprig for edge midpoints: one tapered curling stem, olive
 *  leaves either side, gilded berry at the tip. Points up; rotate at
 *  the call site. */
PaintProgram sprig(const Palette& pal);

/** Mini tapered curls hugging the four corners of a panel — the
 *  small-scale cousin of cornerFlourish for dialogs and scrolls.
 *  A DecorationScheme: attach with .foreground()/.background(). */
struct SwirlCorners {
  Palette pal;
  float size = 24.0f;
  float weight = 2.0f;

  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

/** An illuminated panel: parchment ground, ink rule, gilded dashed
 *  inner trim, tapered corner curls — the parameterized dialog every
 *  palette shares. */
inline Element illuminatedPanel(const Palette& pal) {
  PathFormat goldDash;
  goldDash.width = 1.1f;
  goldDash.strokeFill = Fill::color(pal.gold);
  goldDash.dashIntervals = {8, 5};
  return box()
      .borderRadius({8})
      .fill(parchmentFill(pal.parchment))
      .background(sigil::compose::shadow(sigil::material::Color{0, 0, 0, 0.35f}, {2, 3}, 8))
      .foreground(sigil::compose::stroke(1.8f, Fill::color(pal.stem)))
      .foreground(SwirlCorners{pal, 20.0f, 1.7f})
      .children({box().inset(5).foreground(goldDash)});
}

}  // namespace manuscript
