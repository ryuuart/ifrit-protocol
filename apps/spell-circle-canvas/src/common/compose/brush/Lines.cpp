/** @file
 * The line vocabulary's bodies: the dashed geometry, the parallel casings
 * and terminal caps of a Line, the rails and the hatches.
 */

#include <sigildraw/Pen.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkStrokeRec.h>  // dashed-parallel filterPath
#include <include/effects/Sk2DPathEffect.h>
#include <include/effects/SkDashPathEffect.h>
#include <include/pathops/SkPathOps.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilgeometry/path/StrokeSkia.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilgeometry/path/Contour.h"  // the contour walkers: corners,
                                         // parallels, displacement, windows
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose::lines {

namespace {

/** The dash pattern applied as geometry to a Skia path: what the public
 *  outline form and the rails' own passes both come down to. */
SkPath dashPath(const SkPath& src, SkSpan<const SkScalar> intervals,
                float phase) {
  if (intervals.empty() || src.isEmpty()) return src;
  sk_sp<SkPathEffect> fx = SkDashPathEffect::Make(intervals, phase);
  if (!fx) return src;
  SkPathBuilder dashed;
  // Hairline, not kFill: a fill rec hands the dash effect the solid path
  // back and the pattern never appears.
  SkStrokeRec rec(SkStrokeRec::kHairline_InitStyle);
  if (!fx->filterPath(&dashed, src, &rec)) return src;
  return dashed.detach();
}

}  // namespace

geometry::path::Outline dashGeometry(const geometry::path::Outline& src,
                                     std::span<const float> intervals,
                                     float phase) {
  return geometry::path::fromSk(
      dashPath(geometry::path::toSk(src),
               SkSpan<const SkScalar>(intervals.data(), intervals.size()),
               phase));
}

geometry::path::Outline cornerBrackets(const geometry::path::Outline& src,
                                       float arm, float angleDeg) {
  const SkPath path = geometry::path::toSk(src);
  sigil::compose::detail::warnIfNoCorners(path, angleDeg);
  return geometry::path::fromSk(
      geometry::path::cornerWindows(path, arm, true, angleDeg));
}

geometry::path::Outline cornerGaps(const geometry::path::Outline& src,
                                   float gap, float angleDeg) {
  const SkPath path = geometry::path::toSk(src);
  sigil::compose::detail::warnIfNoCorners(path, angleDeg);
  return geometry::path::fromSk(
      geometry::path::cornerWindows(path, gap, false, angleDeg));
}

float Line::bleed() const {
  const float casing = parallels > 1 ? gap * (float)(parallels - 1) : 0.0f;
  return width + casing + waveAmplitude + std::abs(across) +
         std::max({tickLength * 0.5f, markerSize, 0.0f});
}

void Line::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& canvas = *pen.canvas();
  if (ctx.outline.empty() || width <= 0) return;

  // 1. The body run: offset, then displaced into a wave, then trimmed
  //    back from under Arrow and Bar heads, which also stops dashes
  //    cleanly instead of letting them show through the head.
  SkPath body =
      across != 0 ? geometry::path::parallel(geometry::path::toSk(ctx.outline), across) : geometry::path::toSk(ctx.outline);
  if (waveAmplitude > 0)
    body = geometry::path::displace(body, waveAmplitude, waveLength, zigzag);
  // Markers ride the FINAL geometry (offset + wave applied), not the raw
  // outline — a head must sit on the line it terminates.
  const SkPath capPath = body;
  const float headTrim = trimFor(endMarker);
  const float tailTrim = trimFor(startMarker);
  if (headTrim > 0 || tailTrim > 0) {
    SkPathBuilder trimmed;
    for (const geometry::path::Contour& contour :
         geometry::path::Contour::of(body)) {
      const float len = contour.length();
      if (contour.closed()) {
        // Closed contours have no terminals — keep whole.
        contour.appendSegment(trimmed, 0, len);
      } else {
        contour.appendSegment(trimmed, std::min(tailTrim, len * 0.4f),
                              len - std::min(headTrim, len * 0.4f));
      }
    }
    body = trimmed.detach();
  }

  SkPaint stroke;
  stroke.setAntiAlias(true);
  stroke.setStyle(SkPaint::kStroke_Style);
  // Round unless asked otherwise; the rails always end round.
  stroke.setStrokeJoin(geometry::path::toSk(join));
  stroke.setStrokeCap(SkPaint::kRound_Cap);
  applyFill(stroke, ctx);
  if (!dashIntervals.empty())
    stroke.setPathEffect(SkDashPathEffect::Make(
        SkSpan(dashIntervals.data(), dashIntervals.size()), phase()));

  // 1b. The along-arc gradient: chunked solid strokes (single run only).
  if (!alongStops.empty() && parallels <= 1 && dashIntervals.empty()) {
    SkPaint chunk;
    chunk.setAntiAlias(true);
    chunk.setStyle(SkPaint::kStroke_Style);
    chunk.setStrokeWidth(width);
    chunk.setStrokeCap(SkPaint::kRound_Cap);
    chunk.setStrokeJoin(SkPaint::kRound_Join);
    auto rampAt = [&](float t) {
      if (t <= alongStops.front().offset) return alongStops.front().color;
      for (size_t i = 1; i < alongStops.size(); ++i)
        if (t <= alongStops[i].offset) {
          const float span = alongStops[i].offset - alongStops[i - 1].offset;
          const float k =
              span > 1e-6f ? (t - alongStops[i - 1].offset) / span : 1.0f;
          const material::Color& a = alongStops[i - 1].color;
          const material::Color& b2 = alongStops[i].color;
          return material::mixToward(a, b2, k, a.a + (b2.a - a.a) * k);
        }
      return alongStops.back().color;
    };
    for (const geometry::path::Contour& contour :
         geometry::path::Contour::of(body)) {
      const float len = contour.length();
      const int chunks = std::clamp((int)(len / 6.0f), 8, 48);
      for (int i = 0; i < chunks; ++i) {
        const float a = len * (float)i / (float)chunks;
        const float b2 = len * (float)(i + 1) / (float)chunks;
        const SkPath seg = contour.segment(a, b2);
        chunk.setColor4f(material::skia::toSkColor(
                             rampAt(((float)i + 0.5f) / (float)chunks)),
                         nullptr);
        canvas.drawPath(seg, chunk);
      }
    }
    // Ties/caps still run below; skip the flat body strokes.
  } else
    // 2. Parallels. Undashed rails ride the stroke-OUTLINE construction,
    //    which gives exact parallel curves on bends; round joins plus
    //    Simplify() remove the miter spikes and the self-intersection
    //    knots a tight bend produces. Dashed rails are built per line
    //    through `geometry::path::parallel` instead, so every rail's pattern is
    //    measured on one arc parameterization and the dashes stay in phase.
    if (parallels <= 1) {
      stroke.setStrokeWidth(width);
      canvas.drawPath(body, stroke);
    } else if (!dashIntervals.empty()) {
      // Dash FIRST, offset EACH DASH after. Offsetting the continuous rail
      // and dashing afterwards shears the phase on any curve, because the
      // inner and outer rails have different arc lengths; dashing the
      // centreline once and displacing the resulting segments keeps every
      // rail in register. Note dashGeometry's stroke-rec requirement — the
      // obvious fill rec silently yields a solid path.
      const SkPath dashedBody = dashPath(
          body, SkSpan(dashIntervals.data(), dashIntervals.size()), phase());
      SkPaint p = stroke;
      p.setPathEffect(nullptr);  // geometry already dashed
      const int n = parallels;
      for (int i = 0; i < n; ++i) {
        const float o = gap * ((float)i - (float)(n - 1) * 0.5f);
        p.setStrokeWidth(parallels % 2 && i == n / 2
                             ? width * std::max(coreWidthFactor, 0.1f)
                             : width);
        canvas.drawPath(o == 0 ? dashedBody
                               : geometry::path::parallel(dashedBody, -o, 2.0f),
                        p);
      }
    } else {
      const int pairs = parallels / 2;
      if (parallels % 2) {
        stroke.setStrokeWidth(width * std::max(coreWidthFactor, 0.1f));
        canvas.drawPath(body, stroke);
      }
      for (int i = 0; i < pairs; ++i) {
        const float span = (parallels % 2) ? gap * 2.0f * (float)(i + 1)
                                           : gap * (float)(2 * i + 1);
        SkPaint spread;
        spread.setStyle(SkPaint::kStroke_Style);
        spread.setStrokeWidth(std::max(span, 0.5f));
        // The offset contour inherits the join, so mitre rails jog sharp.
        spread.setStrokeJoin(geometry::path::toSk(join));
        spread.setStrokeCap(SkPaint::kRound_Cap);
        SkPath loop = skpathutils::FillPathWithPaint(body, spread);
        if (std::optional<SkPath> simple = Simplify(loop))
          loop = std::move(*simple);  // tight-bend self-intersection repair
        stroke.setStrokeWidth(width);
        canvas.drawPath(loop, stroke);
      }
    }

  // 3. Railway ties: perpendicular ticks sampled by arc length.
  if (tickSpacing > 0 && tickLength > 0) {
    SkPathBuilder ties;
    for (const geometry::path::Contour& contour :
         geometry::path::Contour::of(body)) {
      const float len = contour.length();
      // the loop walks a distance; the accumulated float is the position
      // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
      for (float d = tickSpacing * 0.5f; d < len; d += tickSpacing) {
        const auto sample = contour.at(d);
        if (!sample) continue;
        const glm::vec2 pos = sample->position;
        const glm::vec2 n{-sample->tangent.y, sample->tangent.x};
        ties.moveTo(pos.x - n.x * tickLength * 0.5f,
                    pos.y - n.y * tickLength * 0.5f);
        ties.lineTo(pos.x + n.x * tickLength * 0.5f,
                    pos.y + n.y * tickLength * 0.5f);
      }
    }
    SkPaint tiePaint;
    tiePaint.setAntiAlias(true);
    tiePaint.setStyle(SkPaint::kStroke_Style);
    tiePaint.setStrokeWidth(tickWidth > 0 ? tickWidth : width);
    applyFill(tiePaint, ctx);
    canvas.drawPath(ties.detach(), tiePaint);
  }

  // 4. Markers, FILLED with the line's own fill: the arrow TIP sits AT the
  //    endpoint and the head extends BACKWARD over the run. Mid-path
  //    chevrons reuse the same glyphs at intervals.
  if (startMarker != Marker::None || endMarker != Marker::None ||
      (midMarker != Marker::None && midSpacing > 0)) {
    SkPaint head;
    head.setAntiAlias(true);
    applyFill(head, ctx);
    using geometry::path::toSk;
    for (const geometry::path::Contour& contour :
         geometry::path::Contour::of(capPath)) {
      const float len = contour.length();
      const bool closed = contour.closed();
      if (!closed) {
        if (endMarker != Marker::None)
          if (const auto end = contour.at(len))
            drawMarker(canvas, head, endMarker, toSk(end->position),
                       toSk(end->tangent));
        if (startMarker != Marker::None)
          if (const auto start = contour.at(0))
            drawMarker(canvas, head, startMarker, toSk(start->position),
                       toSk(-start->tangent));
      }
      if (midMarker != Marker::None && midSpacing > 0) {
        // Closed contours have no terminals: chevrons run the full loop.
        const float from = closed ? midSpacing : midSpacing + tailTrim;
        const float until = closed ? len : len - headTrim;
        // the loop walks a distance; the accumulated float is the position
        // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
        for (float d = from; d < until; d += midSpacing)
          if (const auto sample = contour.at(d))
            drawMarker(canvas, head, midMarker, toSk(sample->position),
                       toSk(sample->tangent));
      }
    }
  }
}

float Line::trimFor(Marker marker) const {
  switch (marker) {
    case Marker::Arrow:
      return markerSize * 0.9f;
    case Marker::Bar:
      return std::max(width, 2.0f) * 0.5f;
    case Marker::Dot:
    case Marker::None:
      break;
  }
  return 0.0f;
}

void Line::applyFill(SkPaint& p, const PaintContext& ctx) const {
  // A fill written as the ink in force, or as a custom property, takes
  // its colour from the node the line is painted under.
  const Fill resolved = resolveFill(fill, ctx);
  if (resolved.kind == Fill::Kind::Color)
    p.setColor4f(material::skia::toSkColor(resolved.colorValue), nullptr);
  else if (resolved.kind == Fill::Kind::Paint)
    p.setShader(material::skia::staticShader(detail::paintOf(resolved)));
}

void Line::drawMarker(SkCanvas& canvas, const SkPaint& head, Marker marker,
                      SkPoint pos, SkVector tan) const {
  const float t = std::hypot(tan.x(), tan.y());
  if (t < 1e-4f) return;
  tan = {tan.x() / t, tan.y() / t};
  const SkVector n{-tan.y(), tan.x()};
  switch (marker) {
    case Marker::Arrow: {
      // Tip AT the endpoint; barbs markerSize back at ±tan(30°)·markerSize,
      // which is the 60° apex.
      const SkPoint base{pos.x() - tan.x() * markerSize,
                         pos.y() - tan.y() * markerSize};
      SkPathBuilder tri;
      tri.moveTo(pos);
      tri.lineTo(base.x() - n.x() * markerSize * 0.577f,
                 base.y() - n.y() * markerSize * 0.577f);
      tri.lineTo(base.x() + n.x() * markerSize * 0.577f,
                 base.y() + n.y() * markerSize * 0.577f);
      tri.close();
      canvas.drawPath(tri.detach(), head);
      break;
    }
    case Marker::Dot:
      canvas.drawCircle(pos, markerSize * 0.5f, head);
      break;
    case Marker::Bar: {
      SkPaint bar = head;
      bar.setStyle(SkPaint::kStroke_Style);
      bar.setStrokeWidth(std::max(width, 2.0f));
      canvas.drawLine(
          {pos.x() - n.x() * markerSize * 0.5f,
           pos.y() - n.y() * markerSize * 0.5f},
          {pos.x() + n.x() * markerSize * 0.5f,
           pos.y() + n.y() * markerSize * 0.5f},
          bar);
      break;
    }
    case Marker::None:
      break;
  }
}

float Rails::bleed() const {
  float worst = 0.0f;
  for (const Rail& r : rails)
    worst = std::max(worst, std::abs(r.across) + r.width * 0.5f);
  return worst + waveAmplitude;
}

float Rails::span() const {
  if (rails.empty()) return 0.0f;
  float lo = rails.front().across, hi = rails.front().across;
  for (const Rail& r : rails) {
    lo = std::min(lo, r.across);
    hi = std::max(hi, r.across);
  }
  return hi - lo;
}

void Rails::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& canvas = *pen.canvas();
  if (ctx.outline.empty() || rails.empty()) return;
  const SkPath body = waveAmplitude > 0
                          ? geometry::path::displace(geometry::path::toSk(ctx.outline), waveAmplitude,
                                                     waveLength, zigzag)
                          : geometry::path::toSk(ctx.outline);
  const float base = phase();
  const float stride =
      std::isfinite(offsetStep) ? std::max(offsetStep, 0.5f) : 2.0f;
  for (const Rail& rail : rails) {
    if (rail.width <= 0) continue;
    // Dash the CENTRELINE (never this rail's own offset curve), so every
    // rail's pattern is measured in one arc parameterisation and the set
    // stays in register through any curvature.
    SkPath run =
        rail.dash.empty()
            ? body
            : dashPath(body, SkSpan(rail.dash.data(), rail.dash.size()),
                           base + rail.dashPhase);
    if (rail.across != 0)
      run = geometry::path::parallel(run, rail.across, stride);
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(rail.width);
    p.setStrokeCap(geometry::path::toSk(rail.cap));
    p.setStrokeJoin(geometry::path::toSk(rail.join));
    const Fill railFill = resolveFill(rail.fill, ctx);
    if (railFill.kind == Fill::Kind::Color)
      p.setColor4f(material::skia::toSkColor(railFill.colorValue), nullptr);
    else if (railFill.kind == Fill::Kind::Paint)
      p.setShader(material::skia::staticShader(detail::paintOf(railFill)));
    canvas.drawPath(run, p);
  }
}

Rails rails(std::vector<Rail> set) {
  Rails r;
  r.rails = std::move(set);
  return r;
}

void Hatch::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& c = *pen.canvas();
  const float pitchPx = pitch();
  const float radians = angle();
  if (pitchPx <= 0.5f) return;
  SkPaint p;
  p.setAntiAlias(true);
  const Fill hatchFill = resolveFill(strokeFill, ctx);
  if (hatchFill.kind == Fill::Kind::Color)
    p.setColor4f(material::skia::toSkColor(hatchFill.colorValue), nullptr);
  else if (hatchFill.kind == Fill::Kind::Paint)
    p.setShader(material::skia::staticShader(detail::paintOf(hatchFill)));
  c.save();
  c.clipPath(geometry::path::toSk(ctx.outline), true);
  if (pattern.taper != 1.0f || pattern.origin || pattern.inset != 0.0f) {
    // A pattern whose gaps change, whose ladder is anchored or whose
    // region is narrowed is laid by Geometry's lattice as centrelines and
    // stroked at the width.
    geometry::shapes::Hatch laid = pattern;
    laid.spacing = pitchPx;
    laid.angle = radians;
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(width);
    c.drawPath(geometry::path::toSk(geometry::shapes::hatchOutline(
                   ctx.outline, laid)),
               p);
  } else {
    // An even pattern is the same lines as Skia's own line lattice lays
    // them, which fills the outline in one path effect per pass.
    const float sine = SkScalarSinSnapToZero(radians);
    const float cosine = SkScalarCosSnapToZero(radians);
    auto pass = [&](float sin, float cos) {
      SkMatrix lattice = SkMatrix::Scale(pitchPx, pitchPx);
      SkMatrix turn;
      turn.setSinCos(sin, cos);
      lattice.postConcat(turn);
      p.setPathEffect(SkLine2DPathEffect::Make(width, lattice));
      c.drawPath(geometry::path::toSk(ctx.outline), p);
    };
    pass(sine, cosine);
    // A quarter turn exactly: the sine and cosine trade places.
    if (pattern.cross) pass(cosine, -sine);
  }
  c.restore();
}

void RadialHatch::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& c = *pen.canvas();
  if (width <= 0 || (spokes <= 0 && rings <= 0 && radiiPx.empty())) return;
  const SkRect box = geometry::path::toSk(ctx.outline).getBounds();
  if (box.isEmpty()) return;
  const SkPoint origin{box.left() + box.width() * centre.x,
                       box.top() + box.height() * centre.y};
  // Far enough to leave the outline from anywhere inside it.
  const float reach =
      std::hypot(std::max(origin.fX - box.left(), box.right() - origin.fX),
                 std::max(origin.fY - box.top(), box.bottom() - origin.fY));
  const float inner = reach * std::clamp(holeFraction, 0.0f, 0.95f);

  SkPaint p;
  p.setAntiAlias(true);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(width);
  const Fill ringFill = resolveFill(strokeFill, ctx);
  if (ringFill.kind == Fill::Kind::Color)
    p.setColor4f(material::skia::toSkColor(ringFill.colorValue), nullptr);
  else if (ringFill.kind == Fill::Kind::Paint)
    p.setShader(material::skia::staticShader(detail::paintOf(ringFill)));

  c.save();
  c.clipPath(geometry::path::toSk(ctx.outline), true);
  if (spokes > 0) {
    SkPathBuilder b;
    const float step = geometry::path::kTau / (float)spokes;
    const float base = geometry::path::radians(rotateDeg);
    for (int i = 0; i < spokes; ++i) {
      const float a = base + (float)i * step;
      const float cs = std::cos(a), sn = std::sin(a);
      b.moveTo(origin.fX + cs * inner, origin.fY + sn * inner);
      b.lineTo(origin.fX + cs * reach, origin.fY + sn * reach);
    }
    c.drawPath(b.detach(), p);
  }
  if (!radiiPx.empty()) {
    SkPathBuilder b;
    for (float r : radiiPx)
      if (r > 0) b.addCircle(origin.fX, origin.fY, r);
    c.drawPath(b.detach(), p);
  } else if (rings > 0) {
    SkPathBuilder b;
    for (int i = 1; i <= rings; ++i) {
      const float r = inner + (reach - inner) * ((float)i / (float)rings);
      b.addCircle(origin.fX, origin.fY, r);
    }
    c.drawPath(b.detach(), p);
  }
  c.restore();
}

}  // namespace sigil::compose::lines
