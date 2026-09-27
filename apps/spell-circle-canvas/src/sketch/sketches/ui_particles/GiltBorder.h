#pragma once

// THE STATIC PIECES OF A GILT BORDER, as whole cards rather than as the
// strokes they are drawn from. Everything here is bake-safe — no bindings,
// no live leaves — so a card records once and replays; what moves belongs
// to the scene.
#include <glm/vec2.hpp>
#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmedia/core/Image.h>
#include "Ornament.h"

#include <functional>
#include <memory>
#include <vector>

namespace ui_particles {

using namespace sigil::compose;
namespace draw = sigil::draw;
namespace geometry = sigil::geometry;
namespace material = sigil::material;

/** The gilt-on-oxblood palette Aurelia is drawn in. */
struct FlourishStyle {
  material::Color gold{0.830f, 0.660f, 0.320f, 1};
  material::Color goldBright{0.980f, 0.860f, 0.540f, 1};
  material::Color bronze{0.470f, 0.300f, 0.150f, 1};
  material::Color leaf{0.680f, 0.560f, 0.280f, 1};
  material::Color parchment{0.760f, 0.665f, 0.485f, 1};
  material::Color ink{0.190f, 0.115f, 0.080f, 1};
  material::Color velvetCore{0.105f, 0.050f, 0.065f, 1};
  material::Color velvetEdge{0.028f, 0.018f, 0.028f, 1};
  material::Color rubric{0.560f, 0.150f, 0.130f, 1};
};

/** The ornament palette this style is, for the pieces that take one. */
inline Palette toOrnamentPalette(const FlourishStyle& s) {
  return {s.parchment, s.ink, s.bronze, s.leaf, s.goldBright};
}

/** Muted-grain parchment. Skia fractal noise is COLORED, so we drop it to
 *  grayscale (kLuminosity over a mid tone) before the soft-light pass —
 *  the grain without the raw noise's rainbow speckle. */
Fill flourishParchment(const FlourishStyle& s, float freq = 0.04f);

Shape leafOutline();

Element acanthusLeaf(const FlourishStyle& s, float w = 28.0f,
                            float h = 20.0f);

/** The acanthus vine as a contour-walked element stamp (bakes once). */
inline ContourWalk flourishVine(const FlourishStyle& s, float spacing = 18.0f,
                                float lw = 26.0f, float lh = 18.0f) {
  ContourWalk vine;
  vine.spacing = spacing;
  vine.stamp = acanthusLeaf(s, lw, lh);
  return vine;
}

/** A gilt diamond bead chain, stamped along the outline. */
PathFormat beadChain(material::Color color, float advance = 14.0f,
                            float r = 2.6f);

// ---------------------------------------------------------------------------
// A bordered post card wearing the flourish border — the reusable unit both
// the scene and the particle field build on. Returns a chainable box in
// column layout; the caller adds a title and paragraphs.

inline Element flourishCard(const FlourishStyle& s, float w, float h,
                            float radius = 14.0f) {
  const Palette pal = toOrnamentPalette(s);
  return box()
      .width(w)
      .height(h)
      .borderRadius({radius})
      .background(sigil::compose::shadow({0, 0, 0, 0.5f}, {0, 4}, 10))
      .fill(flourishParchment(s))
      .foreground(sigil::compose::stroke(2.4f, Fill::color(s.gold)))
      .foreground(flourishVine(s, 16.0f, 20.0f, 14.0f))
      .foreground(beadChain(s.goldBright, 12.0f, 2.0f))
      .foreground(SwirlCorners{pal, 16.0f, 1.4f})
      .column()
      .padding(16, 20)
      .gap(6);
}

}  // namespace ui_particles
