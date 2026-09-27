/** @file
 * The ornament pieces' drawing: the parchment ground, the tapered sweep
 * and its spiral ends, the diamond stud, the edge flourish, the sprig,
 * the corner curls and the carved frame, each painted with Skia's canvas
 * behind the kit's declarations.
 */

#include <sigildraw/Pen.h>
#include "sigilcompose/kit/Ornament.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkDashPathEffect.h>
#include <include/effects/SkPerlinNoiseShader.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilgeometry/path/Skia.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>

#include <cmath>
#include <vector>

namespace sigil::compose::kit::ornament {

namespace {


/** Appends `steps` samples of a cubic bezier to `out`. */
void appendCubic(std::vector<SkPoint>& out, SkPoint p0, SkPoint c1,
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
void appendSpiral(std::vector<SkPoint>& out, SkPoint center, float r0,
                         float r1, float a0, float a1, int steps = 26) {
  for (int i = out.empty() ? 0 : 1; i <= steps; ++i) {
    const float t = (float)i / (float)steps;
    const float r = r0 + (r1 - r0) * t;
    const float a = a0 + (a1 - a0) * t;
    out.push_back({center.x() + std::cos(a) * r, center.y() + std::sin(a) * r});
  }
}

/** Builds a filled path that strokes the centerline with a width that
 *  swells in the middle and thins to hairlines at both tips — the
 *  calligraphic taper every scrollwork flourish reads by. */
SkPath taperedStroke(const std::vector<SkPoint>& pts, float wMax,
                            float wTip = 0.4f) {
  SkPathBuilder b;
  const size_t n = pts.size();
  if (n < 2) return b.detach();
  auto normalAt = [&](size_t i) -> SkVector {
    const SkPoint& a = pts[i == 0 ? 0 : i - 1];
    const SkPoint& c = pts[i + 1 < n ? i + 1 : n - 1];
    SkVector d = {c.x() - a.x(), c.y() - a.y()};
    const float len = std::sqrt(d.x() * d.x() + d.y() * d.y());
    if (len < 1e-4f) return {0, 0};
    return {-d.y() / len, d.x() / len};
  };
  auto halfWidth = [&](size_t i) {
    const float t = (float)i / (float)(n - 1);
    // max() guards the tip: sin(pi*1.0f) is a tiny NEGATIVE float and
    // pow(negative, fractional) is NaN — one NaN voids the whole path.
    const float swell =
        std::pow(std::max(0.0f, std::sin(t * geometry::path::kPi)), 0.65f);
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
void drawTaperedSweep(SkCanvas& c, const std::vector<SkPoint>& pts,
                             material::Color color, float weight) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(material::skia::toSkColor(color), nullptr);
  c.drawPath(taperedStroke(pts, weight), p);
}

/** Small gilded diamond, the reference's corner stud. */
void drawDiamond(SkCanvas& c, SkPoint at, float r,
                        material::Color color) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(material::skia::toSkColor(color), nullptr);
  SkPathBuilder d;
  d.moveTo(at.x(), at.y() - r);
  d.lineTo(at.x() + r, at.y());
  d.lineTo(at.x(), at.y() + r);
  d.lineTo(at.x() - r, at.y());
  d.close();
  c.drawPath(d.detach(), p);
}

}  // namespace

/** Parchment ground: the base tint modulated by fractal noise — the
 *  patterned dialog background, in any color. */
Fill parchmentFill(material::Color base, float frequency) {
  sk_sp<SkShader> noise =
      SkShaders::MakeFractalNoise(frequency, frequency, 3, 7.0f);
  return Fill{material::skia::paint(SkShaders::Blend(
      SkBlendMode::kSoftLight,
      SkShaders::Color(material::skia::toSkColor(base), nullptr),
      std::move(noise)))};
}

void appendCubic(std::vector<glm::vec2>& out, glm::vec2 p0, glm::vec2 c1,
                 glm::vec2 c2, glm::vec2 p1, int steps) {
  std::vector<SkPoint> points;
  for (glm::vec2 point : out) points.push_back({point.x, point.y});
  const size_t before = points.size();
  appendCubic(points, {p0.x, p0.y}, {c1.x, c1.y}, {c2.x, c2.y}, {p1.x, p1.y},
              steps);
  for (size_t i = before; i < points.size(); ++i)
    out.push_back({points[i].x(), points[i].y()});
}

void appendSpiral(std::vector<glm::vec2>& out, glm::vec2 center, float r0,
                  float r1, float a0, float a1, int steps) {
  std::vector<SkPoint> points;
  for (glm::vec2 point : out) points.push_back({point.x, point.y});
  const size_t before = points.size();
  appendSpiral(points, {center.x, center.y}, r0, r1, a0, a1, steps);
  for (size_t i = before; i < points.size(); ++i)
    out.push_back({points[i].x(), points[i].y()});
}

geometry::path::Outline taperedStroke(const std::vector<glm::vec2>& pts,
                                      float wMax, float wTip) {
  std::vector<SkPoint> points;
  points.reserve(pts.size());
  for (glm::vec2 point : pts) points.push_back({point.x, point.y});
  return geometry::path::fromSk(taperedStroke(points, wMax, wTip));
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
PaintProgram edgeFlourish(const Palette& pal, int quadrant, bool vertical) {
  return [pal, quadrant, vertical](sigil::draw::Pen& pen, const PaintContext& ctx) {
    SkCanvas& c = *pen.canvas();
    const float w = ctx.size.x, h = ctx.size.y;
    const bool right = quadrant == 1 || quadrant == 2;
    const bool bottom = quadrant >= 2;
    if (bottom) {
      c.translate(0, h);
      c.scale(1, -1);
    }
    if (right) {
      c.translate(w, 0);
      c.scale(-1, 1);
    }
    if (vertical)  // draw in (u,v): u along the edge, v into the page
      c.concat(SkMatrix::MakeAll(0, 1, 0, 1, 0, 0, 0, 0, 1));
    const float L = vertical ? h : w;

    // Hairline rules: one solid, one dashed and shorter.
    SkPaint rule;
    rule.setAntiAlias(true);
    rule.setStyle(SkPaint::kStroke_Style);
    rule.setStrokeWidth(1.0f);
    rule.setColor4f(material::skia::toSkColor(pal.stem), nullptr);
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
    appendSpiral(sweep, {L * 0.83f, 16}, 8.5f, 1.0f, -1.9f, -1.9f + 7.6f, 40);
    drawTaperedSweep(c, sweep, pal.stem, 3.0f);

    // Under-curl, rolling the opposite way beneath the sweep.
    std::vector<SkPoint> counter;
    appendCubic(counter, {L * 0.08f, 36}, {L * 0.16f, 35}, {L * 0.26f, 33},
                {L * 0.36f, 32});
    appendSpiral(counter, {L * 0.43f, 28}, 6.5f, 0.9f, 2.4f, 2.4f - 6.8f, 36);
    drawTaperedSweep(c, counter, pal.stem, 1.9f);

    // Gilded accents: the corner diamond (horizontal band only, so the
    // meeting bands don't double it) and dots in the spiral eyes.
    if (!vertical) drawDiamond(c, {11, 11}, 5.0f, pal.gold);
    SkPaint dot;
    dot.setAntiAlias(true);
    dot.setColor4f(material::skia::toSkColor(pal.gold), nullptr);
    c.drawCircle(L * 0.83f, 16, 2.2f, dot);
    c.drawCircle(L * 0.43f, 28, 1.7f, dot);
  };
}

/** A leafy sprig for edge midpoints: one tapered curling stem, olive
 *  leaves either side, gilded berry at the tip. Points up; rotate at
 *  the call site. */
PaintProgram sprig(const Palette& pal) {
  return [pal](sigil::draw::Pen& pen, const PaintContext& ctx) {
    SkCanvas& c = *pen.canvas();
    const float w = ctx.size.x, h = ctx.size.y;
    c.translate(w / 2, h);

    std::vector<SkPoint> stem;
    appendCubic(stem, {0, 0}, {2, -h * 0.35f}, {-4, -h * 0.6f},
                {2, -h * 0.82f});
    appendSpiral(stem, {7, -h * 0.86f}, 6.0f, 1.0f, 3.4f, 0.2f);
    drawTaperedSweep(c, stem, pal.stem, 2.2f);

    SkPaint leaf;
    leaf.setAntiAlias(true);
    auto drawLeaf = [&](SkPoint at, float rot, float len, material::Color col) {
      leaf.setColor4f(material::skia::toSkColor(col), nullptr);
      c.save();
      c.translate(at.x(), at.y());
      c.rotate(rot);
      c.drawOval(SkRect::MakeXYWH(0, -len * 0.22f, len, len * 0.44f), leaf);
      c.restore();
    };
    material::Color leafDark = pal.leaf;
    leafDark.r *= 0.75f;
    leafDark.g *= 0.75f;
    leafDark.b *= 0.75f;
    drawLeaf({0, -h * 0.30f}, -140, 13, pal.leaf);
    drawLeaf({1, -h * 0.46f}, -40, 12, leafDark);
    drawLeaf({-1, -h * 0.62f}, -150, 11, pal.leaf);

    SkPaint berry;
    berry.setAntiAlias(true);
    berry.setColor4f(material::skia::toSkColor(pal.gold), nullptr);
    c.drawCircle(7, -h * 0.86f, 2.6f, berry);
  };
}

void SwirlCorners::paint(draw::Pen& pen, const PaintContext& ctx) const {
    SkCanvas& c = *pen.canvas();
    const float w = ctx.size.x, h = ctx.size.y;
    for (int q = 0; q < 4; ++q) {
      const bool right = q == 1 || q == 2;
      const bool bottom = q >= 2;
      c.save();
      c.translate(right ? w : 0, bottom ? h : 0);
      c.scale(right ? -1 : 1, bottom ? -1 : 1);
      std::vector<SkPoint> curl;
      appendSpiral(curl, {size * 0.94f, size * 0.34f}, 0.8f, size * 0.20f,
                   -1.0f, 3.4f);
      appendCubic(curl, curl.back(), {size * 0.42f, size * 0.02f},
                  {size * 0.10f, size * 0.14f}, {size * 0.16f, size * 0.72f});
      appendSpiral(curl, {size * 0.34f, size * 0.86f}, size * 0.18f, 0.8f,
                   3.6f + geometry::path::kPi, 0.2f + geometry::path::kPi);
      drawTaperedSweep(c, curl, pal.stem, weight);
      drawDiamond(c, {size * 0.16f, size * 0.16f}, size * 0.11f, pal.gold);
      c.restore();
    }
  }



/** Draws a carved dialog frame onto an intermediate canvas and hands
 *  back the texture: rounded wood band, gilded trim, corner bosses,
 *  edge studs, translucent parchment center. The nine-slice source —
 *  generate once per palette, stretch everywhere. */
std::shared_ptr<const sigil::media::Image> makeCarvedFrame(const Palette& pal,
                                                           int size) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size, size));
  SkCanvas& c = *surface->getCanvas();
  c.clear(SK_ColorTRANSPARENT);
  const float s = (float)size;

  SkPaint p;
  p.setAntiAlias(true);

  // Parchment center (stretches under content).
  material::Color ground = pal.parchment;
  ground.a = 0.96f;
  p.setColor4f(material::skia::toSkColor(ground), nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(5, 5, s - 5, s - 5), s * 0.14f, s * 0.14f,
                  p);

  // Wood band.
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(s * 0.10f);
  p.setColor4f(material::skia::toSkColor(pal.stem), nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.07f, s * 0.07f, s * 0.93f, s * 0.93f),
                  s * 0.16f, s * 0.16f, p);

  // Gilded trims inside and outside the band.
  p.setStrokeWidth(1.6f);
  p.setColor4f(material::skia::toSkColor(pal.gold), nullptr);
  c.drawRoundRect(
      SkRect::MakeLTRB(s * 0.135f, s * 0.135f, s * 0.865f, s * 0.865f),
      s * 0.10f, s * 0.10f, p);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.02f, s * 0.02f, s * 0.98f, s * 0.98f),
                  s * 0.20f, s * 0.20f, p);

  // Corner bosses: darker disc + gilded diamond stud.
  p.setStyle(SkPaint::kFill_Style);
  const float bossAt[4][2] = {{s * 0.13f, s * 0.13f},
                              {s * 0.87f, s * 0.13f},
                              {s * 0.87f, s * 0.87f},
                              {s * 0.13f, s * 0.87f}};
  for (auto& at : bossAt) {
    material::Color dark = pal.stem;
    dark.r *= 0.6f;
    dark.g *= 0.6f;
    dark.b *= 0.6f;
    p.setColor4f(material::skia::toSkColor(dark), nullptr);
    c.drawCircle(at[0], at[1], s * 0.085f, p);
    drawDiamond(c, {at[0], at[1]}, s * 0.042f, pal.gold);
  }

  // Edge studs at the halves (in the stretchable bands).
  p.setColor4f(material::skia::toSkColor(pal.gold), nullptr);
  c.drawCircle(s * 0.5f, s * 0.075f, 2.6f, p);
  c.drawCircle(s * 0.5f, s * 0.925f, 2.6f, p);
  c.drawCircle(s * 0.075f, s * 0.5f, 2.6f, p);
  c.drawCircle(s * 0.925f, s * 0.5f, 2.6f, p);

  return sigil::media::Image::of(surface->makeImageSnapshot());
}


std::function<geometry::path::Outline(glm::vec2)> scallopOutline(float lobe) {
  return [lobe](glm::vec2 size) {
    SkPathBuilder b;
    const float w = size.x, h = size.y, inset = lobe * 0.5f;
    auto edge = [&](SkPoint from, SkPoint to) {
      const float dx = to.x() - from.x(), dy = to.y() - from.y();
      const float len = std::sqrt(dx * dx + dy * dy);
      const int n = std::max(1, (int)(len / lobe));
      const float nx = dy / len, ny = -dx / len;
      for (int i = 0; i < n; ++i) {
        const float t0 = (float)i / n, t1 = (float)(i + 1) / n;
        const SkPoint mid = {
            from.x() + dx * (t0 + t1) * 0.5f + nx * lobe * 0.55f,
            from.y() + dy * (t0 + t1) * 0.5f + ny * lobe * 0.55f};
        b.quadTo(mid, {from.x() + dx * t1, from.y() + dy * t1});
      }
    };
    b.moveTo(inset, inset);
    edge({inset, inset}, {w - inset, inset});
    edge({w - inset, inset}, {w - inset, h - inset});
    edge({w - inset, h - inset}, {inset, h - inset});
    edge({inset, h - inset}, {inset, inset});
    b.close();
    return geometry::path::fromSk(b.detach());
  };
}

}  // namespace sigil::compose::kit::ornament
