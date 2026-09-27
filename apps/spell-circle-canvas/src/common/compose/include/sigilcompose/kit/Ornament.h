#pragma once

/** @file
 * @ingroup compose-kit
 *
 * SigilCompose KIT — ornament: the pieces a manuscript border is made of.
 *
 * Everything here is a component over the public API, and colour arrives
 * as DATA — a `Palette` — so one flourish, sprig or carved frame renders
 * in any tint. The flourish language follows classic scrollwork: long
 * calligraphic sweeps that TAPER from a hairline, roll into tight spirals
 * at both ends, and concentrate at the corners, accompanied by thin
 * dashed rules and small diamond accents. Not uniform stamps.
 */

#include <glm/vec2.hpp>
#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmedia/core/Image.h>

#include <functional>
#include <memory>
#include <vector>

/** THE PIECES A MANUSCRIPT BORDER IS MADE OF: calligraphic sweeps that
 *  taper from a hairline and roll into spirals at both ends, sprigs,
 *  diamond accents, dashed rules and carved frames.
 *
 *  Every one is a component over the public API, and colour arrives as
 *  DATA — a `Palette` — so one sprig or frame renders in any tint. They
 *  are the strokes; `flourish::` assembles them into whole cards. */
namespace sigil::compose::kit::ornament {

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

inline Palette emeraldPalette() {
  return {{0.87f, 0.90f, 0.79f, 1},
          {0.10f, 0.17f, 0.12f, 1},
          {0.12f, 0.36f, 0.24f, 1},
          {0.35f, 0.50f, 0.20f, 1},
          {0.80f, 0.66f, 0.25f, 1}};
}

inline Palette oakPalette() {
  return {{0.88f, 0.80f, 0.62f, 1},
          {0.20f, 0.13f, 0.07f, 1},
          {0.36f, 0.22f, 0.11f, 1},
          {0.45f, 0.33f, 0.15f, 1},
          {0.85f, 0.64f, 0.22f, 1}};
}

/** Parchment ground: the base tint modulated by fractal noise — the
 *  patterned dialog background, in any color. */
Fill parchmentFill(material::Color base, float frequency = 0.045f);

// ---------------------------------------------------------------------------
// Tapered calligraphic strokes — the swirl primitive

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

// ---------------------------------------------------------------------------
// Carved nine-slice frames (generated on intermediate canvases)

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
      .background(sigil::compose::shadow({0, 0, 0, 0.35f}, {2, 3}, 8))
      .foreground(sigil::compose::stroke(1.8f, Fill::color(pal.stem)))
      .foreground(SwirlCorners{pal, 20.0f, 1.7f})
      .children({box().inset(5).foreground(goldDash)});
}

/** Starburst outline for spiky shout dialogs: `spikes` points, `depth`
 *  0..1 how deep the valleys cut.
 *
 *  The star inscribed in the box, with the valleys at `1 - depth` of the
 *  outer radius — `geometry::shapes::star()` says exactly that, and says it
 *  once for every consumer. */
inline geometry::shapes::Radial starburstOutline(int spikes, float depth) {
  return geometry::shapes::star(spikes, 1.0f - depth);
}

/** Scalloped outline: rounded lobes bulging out of each edge — the
 *  cloud-bubble / wax-seal silhouette. */
std::function<geometry::path::Outline(glm::vec2)> scallopOutline(
    float lobe = 14.0f);

}  // namespace sigil::compose::kit::ornament
