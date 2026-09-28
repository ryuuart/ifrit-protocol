// The manuscript border's drawing, painted with Skia's canvas: each piece
// sets its own paint and path, so a change to one leaves the others as
// they are.
#include <sigilmedia/advanced/Skia.h>
#include "Ornament.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkDashPathEffect.h>
#include <include/effects/SkPerlinNoiseShader.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>

#include <cmath>
#include <vector>

namespace ui_particles {

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

}  // namespace ui_particles
