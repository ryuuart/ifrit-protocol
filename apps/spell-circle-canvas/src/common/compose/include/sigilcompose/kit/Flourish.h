#pragma once

/** @file
 * SigilCompose KIT — flourish: the STATIC pieces of a gilt border.
 *
 * Everything here is bake-safe — no bindings, no live leaves — so a
 * flourish card records once and replays, or bakes into an atlas. What
 * moves (spinning medallions, draw-on scrollwork, shimmer) is the scene's
 * job and not a card's.
 */

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/kit/Ornament.h>
#include <sigilgeometry/kit/Silhouettes.h>

#include <string>
#include <vector>

namespace sigil::compose::kit::flourish {

using ornament::Palette;
using ornament::SwirlCorners;

/** The gilt-on-oxblood palette Aurelia is drawn in. */
struct FlourishStyle {
  SkColor4f gold{0.830f, 0.660f, 0.320f, 1};
  SkColor4f goldBright{0.980f, 0.860f, 0.540f, 1};
  SkColor4f bronze{0.470f, 0.300f, 0.150f, 1};
  SkColor4f leaf{0.680f, 0.560f, 0.280f, 1};
  SkColor4f parchment{0.760f, 0.665f, 0.485f, 1};
  SkColor4f ink{0.190f, 0.115f, 0.080f, 1};
  SkColor4f velvetCore{0.105f, 0.050f, 0.065f, 1};
  SkColor4f velvetEdge{0.028f, 0.018f, 0.028f, 1};
  SkColor4f rubric{0.560f, 0.150f, 0.130f, 1};
};

/** The ornament palette this style is, for the pieces that take one. */
inline Palette toOrnamentPalette(const FlourishStyle& s) {
  return {s.parchment, s.ink, s.bronze, s.leaf, s.goldBright};
}

/** Muted-grain parchment. Skia fractal noise is COLORED, so we drop it to
 *  grayscale (kLuminosity over a mid tone) before the soft-light pass —
 *  the grain without ornament::parchmentFill's rainbow speckle. */
inline Fill flourishParchment(const FlourishStyle& s, float freq = 0.04f) {
  sk_sp<SkShader> noise = SkShaders::MakeFractalNoise(freq, freq, 3, 7.0f);
  if (!noise) return Fill::color(s.parchment);
  sk_sp<SkShader> muted = SkShaders::Blend(
      SkBlendMode::kLuminosity,
      SkShaders::Color(SkColorSetARGB(255, 128, 128, 128)), std::move(noise));
  return Fill::shader(SkShaders::Blend(
      SkBlendMode::kSoftLight, SkShaders::Color(s.parchment.toSkColor()),
      std::move(muted)));
}

// ---------------------------------------------------------------------------
// The acanthus leaf stamp — a pointed leaf whose own contour is walked with
// gilt beads (recursion level 2) and split by a gilt midrib.

inline ::sigil::geometry::shapes::OutlineFn leafOutline() {
  return [](SkSize s) {
    const float w = s.width(), h = s.height();
    SkPathBuilder b;
    b.moveTo(w * 0.06f, h * 0.5f);
    b.quadTo(w * 0.42f, h * 0.04f, w * 0.96f, h * 0.5f);
    b.quadTo(w * 0.42f, h * 0.96f, w * 0.06f, h * 0.5f);
    b.close();
    return b.detach();
  };
}

inline Element acanthusLeaf(const FlourishStyle& s, float w = 28.0f,
                            float h = 20.0f) {
  ContourWalk veins;  // recursion level 2: the stamp walks its own contour
  veins.spacing = 4.0f;
  const SkColor4f bead = s.goldBright;
  veins.draw = [bead](SkCanvas& c, const PathSample&, const PaintContext&) {
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f(bead, nullptr);
    c.drawCircle(0, 0, 0.7f, p);
  };
  const SkColor4f rib = s.goldBright;
  Decoration midrib{PaintProgram([rib](SkCanvas& c, const PaintContext& ctx) {
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(1.1f);
    p.setColor4f(rib, nullptr);
    c.drawLine(ctx.size.width() * 0.1f, ctx.size.height() * 0.5f,
               ctx.size.width() * 0.92f, ctx.size.height() * 0.5f, p);
  })};
  return box()
      .width(w)
      .height(h)
      .shape(leafOutline())
      .fill(sigil::compose::linearGradient({0, 0}, {w, h}, {s.leaf, s.bronze}))
      .foreground(sigil::compose::stroke(1.1f, Fill::color(s.goldBright)))
      .foreground(midrib)
      .foreground(veins);
}

/** The acanthus vine as a contour-walked element stamp (bakes once). */
inline ContourWalk flourishVine(const FlourishStyle& s, float spacing = 18.0f,
                                float lw = 26.0f, float lh = 18.0f) {
  ContourWalk vine;
  vine.spacing = spacing;
  vine.stamp = acanthusLeaf(s, lw, lh);
  return vine;
}

/** A gilt diamond bead chain, stamped along the outline. */
inline PathFormat beadChain(SkColor4f color, float advance = 14.0f,
                            float r = 2.6f) {
  SkPathBuilder bead;
  bead.moveTo(0, -r);
  bead.lineTo(r, 0);
  bead.lineTo(0, r);
  bead.lineTo(-r, 0);
  bead.close();
  PathFormat f;
  f.width = 1.0f;
  f.strokeFill = Fill::color(color);
  f.stampPath = bead.detach();
  f.stampAdvance = advance;
  return f;
}

/** A broken gilt rule (dashed stroke) of the outline. */
inline PathFormat giltDash(SkColor4f color, float width = 1.2f) {
  PathFormat f;
  f.width = width;
  f.strokeFill = Fill::color(color);
  f.dashIntervals = {14, 8};
  return f;
}

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
      .corners({radius})
      .background(sigil::compose::shadow({0, 0, 0, 0.5f}, {0, 4}, 10))
      .fill(flourishParchment(s))
      .foreground(sigil::compose::stroke(2.4f, Fill::color(s.gold)))
      .foreground(flourishVine(s, 16.0f, 20.0f, 14.0f))
      .foreground(beadChain(s.goldBright, 12.0f, 2.0f))
      .foreground(SwirlCorners{pal, 16.0f, 1.4f})
      .column()
      .padding(20, 16)
      .gap(6);
}

}  // namespace sigil::compose::kit::flourish
