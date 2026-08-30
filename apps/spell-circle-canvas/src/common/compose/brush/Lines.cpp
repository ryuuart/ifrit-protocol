/** @file
 * The line vocabulary's bodies: the dashed geometry, the parallel casings
 * and terminal caps of a Line, the rails and the hatches.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkStrokeRec.h>  // dashed-parallel filterPath
#include <include/effects/Sk2DPathEffect.h>
#include <include/effects/SkDashPathEffect.h>
#include <include/pathops/SkPathOps.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilgeometry/path/Contour.h"  // the contour walkers: corners,
                                         // parallels, displacement, windows
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose::lines {

SkPath displaceSquare(const SkPath& src, float amplitude, float wavelength) {
  SkPathBuilder out;
  SkContourMeasureIter iter(src, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    const float lambdaMax = std::max(wavelength, 2.0f);
    const float lambda = len / std::max(1.0f, std::round(len / lambdaMax));
    auto plot = [&](float d, float disp, bool first) {
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(std::min(d, len), &pos, &tan)) return;
      const SkPoint p{pos.x() - tan.y() * disp, pos.y() + tan.x() * disp};
      if (first)
        out.moveTo(p);
      else
        out.lineTo(p);
    };
    plot(0, 0, true);
    float cur = amplitude;
    plot(0, cur, false);
    for (float d = lambda * 0.5f; d < len - 0.25f; d += lambda * 0.5f) {
      plot(d, cur, false);
      cur = -cur;
      plot(d, cur, false);
    }
    plot(len, cur, false);
    plot(len, 0, false);
    if (contour->isClosed()) out.close();
  }
  return out.detach();
}

SkPath dashGeometry(const SkPath& src, SkSpan<const SkScalar> intervals,
                    float phase) {
  if (intervals.empty() || src.isEmpty()) return src;
  sk_sp<SkPathEffect> fx = SkDashPathEffect::Make(intervals, phase);
  if (!fx) return src;
  SkPathBuilder dashed;
  SkStrokeRec rec(SkStrokeRec::kHairline_InitStyle);  // NOT kFill — see above
  if (!fx->filterPath(&dashed, src, &rec)) return src;
  return dashed.detach();
}

SkPath insetOutline(const SkPath& outline, float px) {
  if (px == 0 || outline.isEmpty()) return outline;
  SkPaint offset;
  offset.setStyle(SkPaint::kStroke_Style);
  offset.setStrokeWidth(std::abs(px) * 2.0f);
  offset.setStrokeJoin(SkPaint::kMiter_Join);
  const SkPath ring = skpathutils::FillPathWithPaint(outline, offset);
  SkPath result;
  if (Op(outline, ring,
         px > 0 ? SkPathOp::kDifference_SkPathOp : SkPathOp::kUnion_SkPathOp,
         &result))
    return result;
  return outline;
}

SkPath cornerBrackets(const SkPath& src, float arm, float angleDeg) {
  sigil::compose::detail::warnIfNoCorners(src, angleDeg);
  return geometry::cornerWindows(src, arm, true, angleDeg);
}

SkPath cornerGaps(const SkPath& src, float gap, float angleDeg) {
  sigil::compose::detail::warnIfNoCorners(src, angleDeg);
  return geometry::cornerWindows(src, gap, false, angleDeg);
}

float Line::bleed() const {
  const float casing = parallels > 1 ? gap * (float)(parallels - 1) : 0.0f;
  return width + casing + waveAmplitude + std::abs(across) +
         std::max({tickLength * 0.5f, capSize, 0.0f});
}

void Line::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  if (ctx.outline.isEmpty() || width <= 0) return;

  // 1. The body run: offset, then displaced into a wave, then trimmed
  //    back from under Arrow and Bar heads, which also stops dashes
  //    cleanly instead of letting them show through the head.
  SkPath body =
      across != 0 ? geometry::parallel(ctx.outline, across) : ctx.outline;
  if (waveAmplitude > 0)
    body = geometry::displace(body, waveAmplitude, waveLength, zigzag);
  // Caps ride the FINAL geometry (offset + wave applied), not the raw
  // outline — a head must sit on the line it terminates.
  const SkPath capPath = body;
  const float headTrim = trimFor(endCap);
  const float tailTrim = trimFor(startCap);
  if (headTrim > 0 || tailTrim > 0) {
    SkPathBuilder trimmed;
    SkContourMeasureIter iter(body, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      if (contour->isClosed()) {
        // Closed contours have no terminals — keep whole.
        (void)contour->getSegment(0, len, &trimmed, true);
      } else {
        (void)contour->getSegment(std::min(tailTrim, len * 0.4f),
                                  len - std::min(headTrim, len * 0.4f),
                                  &trimmed, true);
      }
    }
    body = trimmed.detach();
  }

  SkPaint stroke;
  stroke.setAntiAlias(true);
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeJoin(join);                // round unless asked otherwise
  stroke.setStrokeCap(SkPaint::kRound_Cap);  // rails always end round
  applyFill(stroke);
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
      if (t <= alongStops.front().pos) return alongStops.front().color;
      for (size_t i = 1; i < alongStops.size(); ++i)
        if (t <= alongStops[i].pos) {
          const float span = alongStops[i].pos - alongStops[i - 1].pos;
          const float k =
              span > 1e-6f ? (t - alongStops[i - 1].pos) / span : 1.0f;
          const SkColor4f& a = alongStops[i - 1].color;
          const SkColor4f& b2 = alongStops[i].color;
          return SkColor4f{a.fR + (b2.fR - a.fR) * k, a.fG + (b2.fG - a.fG) * k,
                           a.fB + (b2.fB - a.fB) * k,
                           a.fA + (b2.fA - a.fA) * k};
        }
      return alongStops.back().color;
    };
    SkContourMeasureIter iter(body, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      const int chunks = std::clamp((int)(len / 6.0f), 8, 48);
      for (int i = 0; i < chunks; ++i) {
        const float a = len * (float)i / (float)chunks;
        const float b2 = len * (float)(i + 1) / (float)chunks;
        SkPathBuilder seg;
        (void)contour->getSegment(a, b2, &seg, true);
        chunk.setColor4f(rampAt(((float)i + 0.5f) / (float)chunks), nullptr);
        canvas.drawPath(seg.detach(), chunk);
      }
    }
    // Ties/caps still run below; skip the flat body strokes.
  } else
    // 2. Parallels. Undashed rails ride the stroke-OUTLINE construction,
    //    which gives exact parallel curves on bends; round joins plus
    //    Simplify() remove the miter spikes and the self-intersection
    //    knots a tight bend produces. Dashed rails are built per line
    //    through `geometry::parallel` instead, so every rail's pattern is
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
      const SkPath dashedBody = dashGeometry(
          body, SkSpan(dashIntervals.data(), dashIntervals.size()), phase());
      SkPaint p = stroke;
      p.setPathEffect(nullptr);  // geometry already dashed
      const int n = parallels;
      for (int i = 0; i < n; ++i) {
        const float o = gap * ((float)i - (float)(n - 1) * 0.5f);
        p.setStrokeWidth(parallels % 2 && i == n / 2
                             ? width * std::max(coreWidthFactor, 0.1f)
                             : width);
        canvas.drawPath(
            o == 0 ? dashedBody : geometry::parallel(dashedBody, -o, 2.0f), p);
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
        spread.setStrokeJoin(join);  // the offset contour inherits the join,
        spread.setStrokeCap(SkPaint::kRound_Cap);  // so miter rails jog sharp
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
    SkContourMeasureIter iter(body, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      for (float d = tickSpacing * 0.5f; d < len; d += tickSpacing) {
        SkPoint pos;
        SkVector tan;
        if (!contour->getPosTan(d, &pos, &tan)) continue;
        const SkVector n{-tan.y(), tan.x()};
        ties.moveTo(pos.x() - n.x() * tickLength * 0.5f,
                    pos.y() - n.y() * tickLength * 0.5f);
        ties.lineTo(pos.x() + n.x() * tickLength * 0.5f,
                    pos.y() + n.y() * tickLength * 0.5f);
      }
    }
    SkPaint tiePaint;
    tiePaint.setAntiAlias(true);
    tiePaint.setStyle(SkPaint::kStroke_Style);
    tiePaint.setStrokeWidth(tickWidth > 0 ? tickWidth : width);
    applyFill(tiePaint);
    canvas.drawPath(ties.detach(), tiePaint);
  }

  // 4. Caps, FILLED with the line's own fill: the arrow TIP sits AT the
  //    endpoint and the head extends BACKWARD over the run. Mid-path
  //    chevrons reuse the same glyphs at intervals.
  if (startCap != Cap::None || endCap != Cap::None ||
      (midCap != Cap::None && midSpacing > 0)) {
    SkPaint head;
    head.setAntiAlias(true);
    applyFill(head);
    SkContourMeasureIter iter(capPath, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      SkPoint pos;
      SkVector tan;
      const bool closed = contour->isClosed();
      if (!closed) {
        if (endCap != Cap::None && contour->getPosTan(len, &pos, &tan))
          drawCap(canvas, head, endCap, pos, tan);
        if (startCap != Cap::None && contour->getPosTan(0, &pos, &tan))
          drawCap(canvas, head, startCap, pos, {-tan.x(), -tan.y()});
      }
      if (midCap != Cap::None && midSpacing > 0) {
        // Closed contours have no terminals: chevrons run the full loop.
        const float from = closed ? midSpacing : midSpacing + tailTrim;
        const float until = closed ? len : len - headTrim;
        for (float d = from; d < until; d += midSpacing)
          if (contour->getPosTan(d, &pos, &tan))
            drawCap(canvas, head, midCap, pos, tan);
      }
    }
  }
}

float Line::trimFor(Cap cap) const {
  switch (cap) {
    case Cap::Arrow:
      return capSize * 0.9f;
    case Cap::Bar:
      return std::max(width, 2.0f) * 0.5f;
    case Cap::Dot:
    case Cap::None:
      break;
  }
  return 0.0f;
}

void Line::applyFill(SkPaint& p) const {
  if (fill.kind == Fill::Kind::Color)
    p.setColor4f(fill.colorValue, nullptr);
  else if (fill.kind == Fill::Kind::Shader)
    p.setShader(fill.shaderValue);
}

void Line::drawCap(SkCanvas& canvas, const SkPaint& head, Cap cap, SkPoint pos,
                   SkVector tan) const {
  const float t = std::hypot(tan.x(), tan.y());
  if (t < 1e-4f) return;
  tan = {tan.x() / t, tan.y() / t};
  const SkVector n{-tan.y(), tan.x()};
  switch (cap) {
    case Cap::Arrow: {
      // Tip AT the endpoint; barbs capSize back at ±tan(30°)·capSize,
      // which is the 60° apex.
      const SkPoint base{pos.x() - tan.x() * capSize,
                         pos.y() - tan.y() * capSize};
      SkPathBuilder tri;
      tri.moveTo(pos);
      tri.lineTo(base.x() - n.x() * capSize * 0.577f,
                 base.y() - n.y() * capSize * 0.577f);
      tri.lineTo(base.x() + n.x() * capSize * 0.577f,
                 base.y() + n.y() * capSize * 0.577f);
      tri.close();
      canvas.drawPath(tri.detach(), head);
      break;
    }
    case Cap::Dot:
      canvas.drawCircle(pos, capSize * 0.5f, head);
      break;
    case Cap::Bar: {
      SkPaint bar = head;
      bar.setStyle(SkPaint::kStroke_Style);
      bar.setStrokeWidth(std::max(width, 2.0f));
      canvas.drawLine(
          {pos.x() - n.x() * capSize * 0.5f, pos.y() - n.y() * capSize * 0.5f},
          {pos.x() + n.x() * capSize * 0.5f, pos.y() + n.y() * capSize * 0.5f},
          bar);
      break;
    }
    case Cap::None:
      break;
  }
}

Line cased(float width, Fill fill, float gap) {
  Line l;
  l.width = width;
  l.fill = std::move(fill);
  l.parallels = 2;
  l.gap = gap;
  return l;
}

Line triple(float width, Fill fill, float gap, float coreFactor) {
  Line l;
  l.width = width;
  l.fill = std::move(fill);
  l.parallels = 3;
  l.gap = gap;
  l.coreWidthFactor = coreFactor;
  return l;
}

Line arrow(float width, Fill fill, float headSize) {
  Line l;
  l.width = width;
  l.fill = std::move(fill);
  l.endCap = Cap::Arrow;
  l.capSize = headSize;
  return l;
}

Line railway(float width, Fill fill, float tieSpacing, float tieLength) {
  Line l;
  l.width = width;
  l.fill = std::move(fill);
  l.tickSpacing = tieSpacing;
  l.tickLength = tieLength;
  return l;
}

LayerStyle railwayCarto(float scale, SkColor4f dark, SkColor4f light) {
  Line base;
  base.width = 3.0f * scale;
  base.fill = Fill::color(dark);
  Line dashes;
  dashes.width = 1.0f * scale;
  dashes.fill = Fill::color(light);
  dashes.dashIntervals = {8.0f * scale, 8.0f * scale};
  return LayerStyle{{}, {Decoration(base), Decoration(dashes)}};
}

Line wavy(float width, Fill fill, float amplitude, float wavelength) {
  Line l;
  l.width = width;
  l.fill = std::move(fill);
  l.waveAmplitude = amplitude;
  l.waveLength = wavelength;
  return l;
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

void Rails::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  if (ctx.outline.isEmpty() || rails.empty()) return;
  const SkPath body =
      waveAmplitude > 0
          ? geometry::displace(ctx.outline, waveAmplitude, waveLength, zigzag)
          : ctx.outline;
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
            : dashGeometry(body, SkSpan(rail.dash.data(), rail.dash.size()),
                           base + rail.dashPhase);
    if (rail.across != 0) run = geometry::parallel(run, rail.across, stride);
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(rail.width);
    p.setStrokeCap(rail.cap);
    p.setStrokeJoin(rail.join);
    if (rail.fill.kind == Fill::Kind::Color)
      p.setColor4f(rail.fill.colorValue, nullptr);
    else if (rail.fill.kind == Fill::Kind::Shader)
      p.setShader(rail.fill.shaderValue);
    canvas.drawPath(run, p);
  }
}

Rails rails(int count, float width, Fill fill, float gap) {
  Rails r;
  const int n = std::max(count, 1);
  for (int i = 0; i < n; ++i)
    r.rails.push_back(Rail{.across = gap * ((float)i - (float)(n - 1) * 0.5f),
                           .width = width,
                           .fill = fill});
  return r;
}

Rails rails(std::vector<Rail> set) {
  Rails r;
  r.rails = std::move(set);
  return r;
}

Rails quad(float width, Fill fill, float gap) {
  return rails(4, width, std::move(fill), gap);
}

Rails heavyHairHeavy(float heavy, float hair, Fill fill, float gap) {
  return rails({{.across = -gap, .width = heavy, .fill = fill},
                {.across = 0, .width = hair, .fill = fill},
                {.across = gap, .width = heavy, .fill = fill}});
}

Rails dottedCore(float outer, float core, Fill fill, float gap, float dotGap) {
  return rails({{.across = -gap, .width = outer, .fill = fill},
                {.across = 0,
                 .width = core,
                 .fill = fill,
                 .dash = {0.01f, dotGap},
                 .cap = SkPaint::kRound_Cap},
                {.across = gap, .width = outer, .fill = fill}});
}

void Hatch::paint(SkCanvas& c, const PaintContext& ctx) const {
  const float pitchPx = pitch();
  const float baseDeg = angle();
  if (pitchPx <= 0.5f) return;
  SkPaint p;
  p.setAntiAlias(true);
  if (strokeFill.kind == Fill::Kind::Color)
    p.setColor4f(strokeFill.colorValue, nullptr);
  else if (strokeFill.kind == Fill::Kind::Shader)
    p.setShader(strokeFill.shaderValue);
  c.save();
  c.clipPath(ctx.outline, true);
  auto pass = [&](float deg) {
    SkMatrix lattice = SkMatrix::Scale(pitchPx, pitchPx);
    lattice.postRotate(deg);
    p.setPathEffect(SkLine2DPathEffect::Make(width, lattice));
    c.drawPath(ctx.outline, p);
  };
  pass(baseDeg);
  if (cross) pass(baseDeg + 90.0f);
  c.restore();
}

Hatch hatch(Fill fill, float spacing, float width, float angleDeg) {
  Hatch h;
  h.strokeFill = std::move(fill);
  h.spacing = spacing;
  h.width = width;
  h.angleDeg = angleDeg;
  return h;
}

Hatch crosshatch(Fill fill, float spacing, float width, float angleDeg) {
  Hatch h = hatch(std::move(fill), spacing, width, angleDeg);
  h.cross = true;
  return h;
}

void RadialHatch::paint(SkCanvas& c, const PaintContext& ctx) const {
  if (width <= 0 || (spokes <= 0 && rings <= 0 && radiiPx.empty())) return;
  const SkRect box = ctx.outline.getBounds();
  if (box.isEmpty()) return;
  const SkPoint origin{box.left() + box.width() * centre.fX,
                       box.top() + box.height() * centre.fY};
  // Far enough to leave the outline from anywhere inside it.
  const float reach =
      std::hypot(std::max(origin.fX - box.left(), box.right() - origin.fX),
                 std::max(origin.fY - box.top(), box.bottom() - origin.fY));
  const float inner = reach * std::clamp(holeFraction, 0.0f, 0.95f);

  SkPaint p;
  p.setAntiAlias(true);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(width);
  if (strokeFill.kind == Fill::Kind::Color)
    p.setColor4f(strokeFill.colorValue, nullptr);
  else if (strokeFill.kind == Fill::Kind::Shader)
    p.setShader(strokeFill.shaderValue);

  c.save();
  c.clipPath(ctx.outline, true);
  if (spokes > 0) {
    SkPathBuilder b;
    const float step = 2.0f * SK_FloatPI / (float)spokes;
    const float base = rotateDeg * SK_FloatPI / 180.0f;
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

RadialHatch radialHatch(Fill fill, int spokes, float width, SkPoint centre) {
  RadialHatch h;
  h.strokeFill = std::move(fill);
  h.spokes = spokes;
  h.width = width;
  h.centre = centre;
  return h;
}

RadialHatch concentric(Fill fill, int rings, float width, SkPoint centre) {
  RadialHatch h;
  h.strokeFill = std::move(fill);
  h.spokes = 0;
  h.rings = rings;
  h.width = width;
  h.centre = centre;
  return h;
}

RadialHatch concentric(Fill fill, std::vector<float> radiiPx, float width,
                       SkPoint centre) {
  RadialHatch h;
  h.strokeFill = std::move(fill);
  h.spokes = 0;
  h.rings = 0;
  h.radiiPx = std::move(radiiPx);
  h.width = width;
  h.centre = centre;
  return h;
}

}  // namespace sigil::compose::lines
