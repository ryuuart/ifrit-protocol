#pragma once
// Parameterized ornament components shared by the showcase scenes.
// Everything here is a component over the public API: colors arrive as
// data (a Palette), so the same flourish, sprig, or carved frame
// renders in any tint.
//
// The flourish language follows classic scrollwork references: long
// calligraphic sweeps that TAPER from a hairline, roll into tight
// spirals at both ends, and concentrate at the corners — accompanied
// by thin dashed rules and small diamond accents. Not uniform stamps.

#include "GalleryCore.h"

#include <sigilcompose/Decorations.h>

#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/effects/SkPerlinNoiseShader.h>

#include <cmath>
#include <vector>

namespace compose_gallery {

/** A manuscript palette — every ornament component below is driven by
 *  one of these, never by hard-coded colors. */
struct Palette {
  SkColor4f parchment; // page/panel ground
  SkColor4f ink;       // body text on that ground
  SkColor4f stem;      // flourish strokes (the watercolor "cobalt")
  SkColor4f leaf;      // sprig leaves (olive/ochre greens)
  SkColor4f gold;      // gilded accents: diamonds, dots, trims
};

inline Palette azurePalette() {
  return {{0.93f, 0.89f, 0.78f, 1},
          {0.17f, 0.14f, 0.11f, 1},
          {0.13f, 0.23f, 0.52f, 1},  // cobalt
          {0.44f, 0.47f, 0.20f, 1},  // olive
          {0.82f, 0.62f, 0.20f, 1}}; // ochre gold
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
inline Fill parchmentFill(SkColor4f base, float frequency = 0.045f) {
  sk_sp<SkShader> noise =
      SkShaders::MakeFractalNoise(frequency, frequency, 3, 7.0f);
  return Fill::shader(SkShaders::Blend(
      SkBlendMode::kSoftLight,
      SkShaders::Color(base, nullptr), std::move(noise)));
}

// ---------------------------------------------------------------------------
// Tapered calligraphic strokes — the swirl primitive

/** Appends `steps` samples of a cubic bezier to `out`. */
inline void appendCubic(std::vector<SkPoint> &out, SkPoint p0, SkPoint c1,
                        SkPoint c2, SkPoint p1, int steps = 22) {
  for (int i = out.empty() ? 0 : 1; i <= steps; ++i) {
    const float t = (float)i / (float)steps, u = 1 - t;
    out.push_back({u * u * u * p0.x() + 3 * u * u * t * c1.x() +
                       3 * u * t * t * c2.x() + t * t * t * p1.x(),
                   u * u * u * p0.y() + 3 * u * u * t * c1.y() +
                       3 * u * t * t * c2.y() + t * t * t * p1.y()});
  }
}

/** Appends an Archimedean spiral: radius sweeps r0→r1 while the angle
 *  sweeps a0→a1 (radians) around `center`. Roll tips with r1 ≈ 0. */
inline void appendSpiral(std::vector<SkPoint> &out, SkPoint center,
                         float r0, float r1, float a0, float a1,
                         int steps = 26) {
  for (int i = out.empty() ? 0 : 1; i <= steps; ++i) {
    const float t = (float)i / (float)steps;
    const float r = r0 + (r1 - r0) * t;
    const float a = a0 + (a1 - a0) * t;
    out.push_back({center.x() + std::cos(a) * r,
                   center.y() + std::sin(a) * r});
  }
}

/** Builds a filled path that strokes the centerline with a width that
 *  swells in the middle and thins to hairlines at both tips — the
 *  calligraphic taper every scrollwork flourish reads by. */
inline SkPath taperedStroke(const std::vector<SkPoint> &pts, float wMax,
                            float wTip = 0.4f) {
  SkPathBuilder b;
  const size_t n = pts.size();
  if (n < 2)
    return b.detach();
  auto normalAt = [&](size_t i) -> SkVector {
    const SkPoint &a = pts[i == 0 ? 0 : i - 1];
    const SkPoint &c = pts[i + 1 < n ? i + 1 : n - 1];
    SkVector d = {c.x() - a.x(), c.y() - a.y()};
    const float len = std::sqrt(d.x() * d.x() + d.y() * d.y());
    if (len < 1e-4f)
      return {0, 0};
    return {-d.y() / len, d.x() / len};
  };
  auto halfWidth = [&](size_t i) {
    const float t = (float)i / (float)(n - 1);
    // max() guards the tip: sin(pi*1.0f) is a tiny NEGATIVE float and
    // pow(negative, fractional) is NaN — one NaN voids the whole path.
    const float swell =
        std::pow(std::max(0.0f, std::sin(t * 3.14159265f)), 0.65f);
    return 0.5f * (wTip + (wMax - wTip) * swell);
  };
  b.moveTo(pts[0].x() + normalAt(0).x() * halfWidth(0),
           pts[0].y() + normalAt(0).y() * halfWidth(0));
  for (size_t i = 1; i < n; ++i)
    b.lineTo(pts[i].x() + normalAt(i).x() * halfWidth(i),
             pts[i].y() + normalAt(i).y() * halfWidth(i));
  for (size_t i = n; i-- > 0;)
    b.lineTo(pts[i].x() - normalAt(i).x() * halfWidth(i),
             pts[i].y() - normalAt(i).y() * halfWidth(i));
  b.close();
  return b.detach();
}

/** One tapered sweep with spiral-rolled ends, described by its rough
 *  course; fills in pal-colored ink. */
inline void drawTaperedSweep(SkCanvas &c, const std::vector<SkPoint> &pts,
                             SkColor4f color, float weight) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(color, nullptr);
  c.drawPath(taperedStroke(pts, weight), p);
}

/** Small gilded diamond, the reference's corner stud. */
inline void drawDiamond(SkCanvas &c, SkPoint at, float r, SkColor4f color) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(color, nullptr);
  SkPathBuilder d;
  d.moveTo(at.x(), at.y() - r);
  d.lineTo(at.x() + r, at.y());
  d.lineTo(at.x(), at.y() + r);
  d.lineTo(at.x() - r, at.y());
  d.close();
  c.drawPath(d.detach(), p);
}

/** A half-edge flourish, drawn from a corner toward the edge's
 *  midpoint: long hairline rules threading beneath a tapered sweep
 *  that lifts off the corner and rolls into a tight spiral eye short
 *  of the midpoint, with an under-curl answering it the other way.
 *  Eight of these (4 corners × both adjacent edges) build the classic
 *  mirrored scrollwork border — facing spiral pairs meet at every edge
 *  midpoint and the sweeps cross at the corner diamonds.
 *  `quadrant`: 0=NW, 1=NE, 2=SE, 3=SW; `vertical` runs the band down
 *  the side edge instead of along the top/bottom. */
inline PaintProgram edgeFlourish(const Palette &pal, int quadrant,
                                 bool vertical) {
  return [pal, quadrant, vertical](SkCanvas &c, const PaintContext &ctx) {
    const float w = ctx.size.width(), h = ctx.size.height();
    const bool right = quadrant == 1 || quadrant == 2;
    const bool bottom = quadrant >= 2;
    if (bottom) { c.translate(0, h); c.scale(1, -1); }
    if (right) { c.translate(w, 0); c.scale(-1, 1); }
    if (vertical) // draw in (u,v): u along the edge, v into the page
      c.concat(SkMatrix::MakeAll(0, 1, 0, 1, 0, 0, 0, 0, 1));
    const float L = vertical ? h : w;

    // Hairline rules: one solid, one dashed and shorter.
    SkPaint rule;
    rule.setAntiAlias(true);
    rule.setStyle(SkPaint::kStroke_Style);
    rule.setStrokeWidth(1.0f);
    rule.setColor4f(pal.stem, nullptr);
    c.drawLine(26, 11, L - 10, 11, rule);
    rule.setStrokeWidth(0.8f);
    const SkScalar dash[2] = {11, 6};
    rule.setPathEffect(SkDashPathEffect::Make(SkSpan(dash, 2), 0));
    c.drawLine(26, 18, L * 0.62f, 18, rule);

    // Main sweep: a hairline tail landing at the corner diamond, then
    // shallow over the rule, rolling into a tight eye short of the
    // edge midpoint.
    std::vector<SkPoint> sweep;
    appendCubic(sweep, {14, 20}, {L * 0.05f, 8}, {L * 0.15f, 0},
                {L * 0.38f, 5});
    appendCubic(sweep, sweep.back(), {L * 0.55f, 8}, {L * 0.68f, 3},
                {L * 0.78f, 9});
    appendSpiral(sweep, {L * 0.83f, 16}, 8.5f, 1.0f, -1.9f,
                 -1.9f + 7.6f, 40);
    drawTaperedSweep(c, sweep, pal.stem, 3.0f);

    // Under-curl, rolling the opposite way beneath the sweep.
    std::vector<SkPoint> counter;
    appendCubic(counter, {L * 0.08f, 36}, {L * 0.16f, 35},
                {L * 0.26f, 33}, {L * 0.36f, 32});
    appendSpiral(counter, {L * 0.43f, 28}, 6.5f, 0.9f, 2.4f,
                 2.4f - 6.8f, 36);
    drawTaperedSweep(c, counter, pal.stem, 1.9f);

    // Gilded accents: the corner diamond (horizontal band only, so the
    // meeting bands don't double it) and dots in the spiral eyes.
    if (!vertical)
      drawDiamond(c, {11, 11}, 5.0f, pal.gold);
    SkPaint dot;
    dot.setAntiAlias(true);
    dot.setColor4f(pal.gold, nullptr);
    c.drawCircle(L * 0.83f, 16, 2.2f, dot);
    c.drawCircle(L * 0.43f, 28, 1.7f, dot);
  };
}

/** A leafy sprig for edge midpoints: one tapered curling stem, olive
 *  leaves either side, gilded berry at the tip. Points up; rotate at
 *  the call site. */
inline PaintProgram sprig(const Palette &pal) {
  return [pal](SkCanvas &c, const PaintContext &ctx) {
    const float w = ctx.size.width(), h = ctx.size.height();
    c.translate(w / 2, h);

    std::vector<SkPoint> stem;
    appendCubic(stem, {0, 0}, {2, -h * 0.35f}, {-4, -h * 0.6f},
                {2, -h * 0.82f});
    appendSpiral(stem, {7, -h * 0.86f}, 6.0f, 1.0f, 3.4f, 0.2f);
    drawTaperedSweep(c, stem, pal.stem, 2.2f);

    SkPaint leaf;
    leaf.setAntiAlias(true);
    auto drawLeaf = [&](SkPoint at, float rot, float len, SkColor4f col) {
      leaf.setColor4f(col, nullptr);
      c.save();
      c.translate(at.x(), at.y());
      c.rotate(rot);
      c.drawOval(SkRect::MakeXYWH(0, -len * 0.22f, len, len * 0.44f),
                 leaf);
      c.restore();
    };
    SkColor4f leafDark = pal.leaf;
    leafDark.fR *= 0.75f; leafDark.fG *= 0.75f; leafDark.fB *= 0.75f;
    drawLeaf({0, -h * 0.30f}, -140, 13, pal.leaf);
    drawLeaf({1, -h * 0.46f}, -40, 12, leafDark);
    drawLeaf({-1, -h * 0.62f}, -150, 11, pal.leaf);

    SkPaint berry;
    berry.setAntiAlias(true);
    berry.setColor4f(pal.gold, nullptr);
    c.drawCircle(7, -h * 0.86f, 2.6f, berry);
  };
}

/** Mini tapered curls hugging the four corners of a panel — the
 *  small-scale cousin of cornerFlourish for dialogs and scrolls.
 *  A DecorationScheme: attach with .foreground()/.background(). */
struct SwirlCorners {
  Palette pal;
  float size = 24.0f;
  float weight = 2.0f;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    const float w = ctx.size.width(), h = ctx.size.height();
    for (int q = 0; q < 4; ++q) {
      const bool right = q == 1 || q == 2;
      const bool bottom = q >= 2;
      c.save();
      c.translate(right ? w : 0, bottom ? h : 0);
      c.scale(right ? -1 : 1, bottom ? -1 : 1);
      std::vector<SkPoint> curl;
      appendSpiral(curl, {size * 0.94f, size * 0.34f}, 0.8f,
                   size * 0.20f, -1.0f, 3.4f);
      appendCubic(curl, curl.back(), {size * 0.42f, size * 0.02f},
                  {size * 0.10f, size * 0.14f},
                  {size * 0.16f, size * 0.72f});
      appendSpiral(curl, {size * 0.34f, size * 0.86f}, size * 0.18f,
                   0.8f, 3.6f + 3.14159f, 0.2f + 3.14159f);
      drawTaperedSweep(c, curl, pal.stem, weight);
      drawDiamond(c, {size * 0.16f, size * 0.16f}, size * 0.11f,
                  pal.gold);
      c.restore();
    }
  }
};

// ---------------------------------------------------------------------------
// Carved nine-slice frames (generated on intermediate canvases)

/** Draws a carved dialog frame onto an intermediate canvas and hands
 *  back the texture: rounded wood band, gilded trim, corner bosses,
 *  edge studs, translucent parchment center. The nine-slice source —
 *  generate once per palette, stretch everywhere. */
inline sk_sp<SkImage> makeCarvedFrame(const Palette &pal, int size = 96) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size, size));
  SkCanvas &c = *surface->getCanvas();
  c.clear(SK_ColorTRANSPARENT);
  const float s = (float)size;

  SkPaint p;
  p.setAntiAlias(true);

  // Parchment center (stretches under content).
  SkColor4f ground = pal.parchment;
  ground.fA = 0.96f;
  p.setColor4f(ground, nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(5, 5, s - 5, s - 5), s * 0.14f,
                  s * 0.14f, p);

  // Wood band.
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(s * 0.10f);
  p.setColor4f(pal.stem, nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.07f, s * 0.07f, s * 0.93f,
                                   s * 0.93f),
                  s * 0.16f, s * 0.16f, p);

  // Gilded trims inside and outside the band.
  p.setStrokeWidth(1.6f);
  p.setColor4f(pal.gold, nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.135f, s * 0.135f, s * 0.865f,
                                   s * 0.865f),
                  s * 0.10f, s * 0.10f, p);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.02f, s * 0.02f, s * 0.98f,
                                   s * 0.98f),
                  s * 0.20f, s * 0.20f, p);

  // Corner bosses: darker disc + gilded diamond stud.
  p.setStyle(SkPaint::kFill_Style);
  const float bossAt[4][2] = {{s * 0.13f, s * 0.13f},
                              {s * 0.87f, s * 0.13f},
                              {s * 0.87f, s * 0.87f},
                              {s * 0.13f, s * 0.87f}};
  for (auto &at : bossAt) {
    SkColor4f dark = pal.stem;
    dark.fR *= 0.6f; dark.fG *= 0.6f; dark.fB *= 0.6f;
    p.setColor4f(dark, nullptr);
    c.drawCircle(at[0], at[1], s * 0.085f, p);
    drawDiamond(c, {at[0], at[1]}, s * 0.042f, pal.gold);
  }

  // Edge studs at the halves (in the stretchable bands).
  p.setColor4f(pal.gold, nullptr);
  c.drawCircle(s * 0.5f, s * 0.075f, 2.6f, p);
  c.drawCircle(s * 0.5f, s * 0.925f, 2.6f, p);
  c.drawCircle(s * 0.075f, s * 0.5f, 2.6f, p);
  c.drawCircle(s * 0.925f, s * 0.5f, 2.6f, p);

  return surface->makeImageSnapshot();
}

/** The carved frame as a nine-slice decoration for any box size. */
inline Slice carvedFrameSlice(
    const std::shared_ptr<const sigil::image::ImageAsset> &asset) {
  Slice nine;
  nine.asset = asset;
  const int size = asset ? asset->width() : 96;
  nine.xDivs = {size / 3, size * 2 / 3};
  nine.yDivs = {size / 3, size * 2 / 3};
  return nine;
}

/** An illuminated panel: parchment ground, ink rule, gilded dashed
 *  inner trim, tapered corner curls — the parameterized dialog every
 *  palette shares. */
inline Element illuminatedPanel(const Palette &pal) {
  PathFormat goldDash;
  goldDash.width = 1.1f;
  goldDash.strokeFill = Fill::color(pal.gold);
  goldDash.dashIntervals = {8, 5};
  return box().corners({8})
      .fill(parchmentFill(pal.parchment))
      .background(sigil::compose::util::shadow({0, 0, 0, 0.35f}, {2, 3},
                                               8))
      .foreground(sigil::compose::util::stroke(1.8f,
                                               Fill::color(pal.stem)))
      .foreground(SwirlCorners{pal, 20.0f, 1.7f})
      .child(box().inset(5).foreground(goldDash));
}

/** Starburst outline for spiky shout dialogs: `spikes` points, `depth`
 *  0..1 how deep the valleys cut. */
inline std::function<SkPath(SkSize)> starburstOutline(int spikes,
                                                      float depth) {
  return [spikes, depth](SkSize s) {
    SkPathBuilder b;
    const float cx = s.width() / 2, cy = s.height() / 2;
    const int n = spikes * 2;
    for (int i = 0; i < n; ++i) {
      const float a = (float)i / (float)n * 6.28318f - 1.5708f;
      const float k = (i & 1) ? 1.0f - depth : 1.0f;
      const SkPoint pt = {cx + std::cos(a) * cx * k,
                          cy + std::sin(a) * cy * k};
      if (i == 0)
        b.moveTo(pt);
      else
        b.lineTo(pt);
    }
    b.close();
    return b.detach();
  };
}

/** Scalloped outline: rounded lobes bulging out of each edge — the
 *  cloud-bubble / wax-seal silhouette. */
inline std::function<SkPath(SkSize)> scallopOutline(float lobe = 14.0f) {
  return [lobe](SkSize s) {
    SkPathBuilder b;
    const float w = s.width(), h = s.height(), inset = lobe * 0.5f;
    auto edge = [&](SkPoint from, SkPoint to) {
      const float dx = to.x() - from.x(), dy = to.y() - from.y();
      const float len = std::sqrt(dx * dx + dy * dy);
      const int n = std::max(1, (int)(len / lobe));
      const float nx = dy / len, ny = -dx / len;
      for (int i = 0; i < n; ++i) {
        const float t0 = (float)i / n, t1 = (float)(i + 1) / n;
        const SkPoint mid = {from.x() + dx * (t0 + t1) * 0.5f +
                                 nx * lobe * 0.55f,
                             from.y() + dy * (t0 + t1) * 0.5f +
                                 ny * lobe * 0.55f};
        b.quadTo(mid, {from.x() + dx * t1, from.y() + dy * t1});
      }
    };
    b.moveTo(inset, inset);
    edge({inset, inset}, {w - inset, inset});
    edge({w - inset, inset}, {w - inset, h - inset});
    edge({w - inset, h - inset}, {inset, h - inset});
    edge({inset, h - inset}, {inset, inset});
    b.close();
    return b.detach();
  };
}

} // namespace compose_gallery
