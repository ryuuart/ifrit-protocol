#pragma once

/** @file
 * SigilCompose line patterns — the cartography/diagram stroke vocabulary
 * beyond dashes: parallel casings (double/triple rails, highway pairs),
 * terminal caps (arrows, dots, bars — the node-graph direction language),
 * railway ties, wave and zigzag runs. One value DecorationScheme
 * (`lines::Line`) built from pure data, so patterned connectors prune and
 * cache like any static chrome; attach with `.stroke()` to dress any
 * outline, rail, or connector route.
 *
 * Extension-point note: Skia's own seam here would be a custom
 * SkPathEffect, but the public API seals subclassing (onFilterPath lives
 * in src/). This header mirrors that contract at OUR seam instead — the
 * geometry ops run on the outline before stroking, as comparable values —
 * and PathFormat::effect stays the raw sk_sp<SkPathEffect> escape hatch
 * for effects Skia does ship (dash, corner, discrete, 1D, trim).
 *
 *   rail(stops, routers::octilinear())
 *       .stroke(lines::Line{.width = 3, .fill = ink,
 *                           .parallels = 2, .gap = 5});      // transit pair
 *   connector("a", "b").stroke(lines::arrow(2, wire, 12));   // directed edge
 */

#include "sigilcompose/Compose.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/pathops/SkPathOps.h>
#include <include/effects/SkDashPathEffect.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sigil::compose::lines {

/** Wave/zigzag geometry op: resample @p src by arc length and displace
 *  along the normal. Grounded against shipped implementations (QGIS
 *  triangular/roundWaves, Chromium/Firefox text-decoration wavy): the
 *  wavelength is a MAXIMUM — per contour it snaps so an exact number of
 *  waves fit, and BOTH kinds are zero-phase at the endpoints, so the run
 *  starts and ends ON the route (heads computed from the undisplaced
 *  outline stay attached). Sine sampled at λ/16; zigzag emits triangular
 *  vertices at quarter-wave points. Shared by lines::Line and the ops::
 *  pipeline (Brushes.h). */
inline SkPath displace(const SkPath &src, float amplitude, float wavelength,
                       bool zigzag) {
  SkPathBuilder out;
  SkContourMeasureIter iter(src, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    // The QGIS fit rule: actual λ = len / round(len / λmax), never 0.
    const float lambdaMax = std::max(wavelength, 2.0f);
    const float lambda =
        len / std::max(1.0f, std::round(len / lambdaMax));
    const float step = zigzag ? lambda * 0.25f : lambda * 0.0625f;
    bool first = true;
    int k = 0;
    for (float d = 0;; d += step, ++k) {
      const float at = std::min(d, len);
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(at, &pos, &tan))
        break;
      const SkVector n{-tan.y(), tan.x()};
      float disp;
      if (zigzag) {
        // Quarter-wave vertices: 0, +a, 0, -a … zero at both endpoints.
        static constexpr float kQuarters[4] = {0, 1, 0, -1};
        disp = amplitude * kQuarters[k % 4];
      } else {
        disp = amplitude * std::sin(at * 6.2831853f / lambda);
      }
      const SkPoint p{pos.x() + n.x() * disp, pos.y() + n.y() * disp};
      if (first) {
        out.moveTo(p);
        first = false;
      } else {
        out.lineTo(p);
      }
      if (at >= len)
        break;
    }
  }
  return out.detach();
}

/** One-sided constant offset along the normal (Mapbox line-offset / QGIS
 *  line offset semantics: positive offsets to the RIGHT of the travel
 *  direction). A resampled approximation — exact on straights, smooth on
 *  gentle bends; also the dash-safe way to build parallel rails (each
 *  rail keeps the same arc parameterization, so dashes stay in phase). */
inline SkPath offsetAlong(const SkPath &src, float offset, float step = 4.0f) {
  if (offset == 0)
    return src;
  SkPathBuilder out;
  SkContourMeasureIter iter(src, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    bool first = true;
    for (float d = 0;; d += step) {
      const float at = std::min(d, len);
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(at, &pos, &tan))
        break;
      const SkPoint p{pos.x() - tan.y() * offset, pos.y() + tan.x() * offset};
      if (first) {
        out.moveTo(p);
        first = false;
      } else {
        out.lineTo(p);
      }
      if (at >= len)
        break;
    }
    if (contour->isClosed())
      out.close();
  }
  return out.detach();
}

/** How a line run terminates (per contour end). Heads are FILLED with the
 *  line's own fill — solid arrowheads, station dots, buffer bars. */
enum class Cap : uint8_t { None, Arrow, Dot, Bar };

/** The patterned line: everything is data (defaulted equality — a static
 *  patterned connector prunes without memo). Compose freely: a wavy
 *  double line with arrowheads and ties is one value. */
struct Line {
  float width = 2.0f;
  Fill fill = Fill::color({1, 1, 1, 1});

  /** Parallel casing: 1 = plain, 2 = the transit pair, 3 = triple rail
   *  (odd counts keep a center line, weighted by `coreWidthFactor` — the
   *  bold-spine + light-outriders look). `gap` is CENTER-TO-CENTER spacing
   *  between adjacent lines (Mapbox's line-gap-width is the inner CLEAR
   *  gap instead — convert with gap = clear + width). Parallels follow
   *  curves exactly (offset contours via the stroke-outline construction;
   *  dashed parallels switch to per-rail offsets so dashes stay in
   *  phase). */
  int parallels = 1;
  float gap = 4.0f;
  float coreWidthFactor = 1.0f;

  /** Wave/zigzag displacement of the run itself (the y2k squiggle, the
   *  hand-drawn nerve): amplitude in px, wavelength in px along the arc.
   *  Zigzag alternates hard vertices instead of the sine. */
  float waveAmplitude = 0.0f;
  float waveLength = 24.0f;
  bool zigzag = false;

  /** Railway ties: short perpendicular ticks every `tickSpacing` px
   *  (0 = none), `tickLength` px long — the rail/blueprint idiom (QGIS
   *  hashed-line default ratio is spacing:length = 1:1). `tickWidth` 0
   *  strokes ties at the line's width; real maps often want ~2× it. */
  float tickSpacing = 0.0f;
  float tickLength = 8.0f;
  float tickWidth = 0.0f;

  /** One-sided offset of the whole run (Mapbox line-offset semantics:
   *  positive = right of travel) — bus lanes beside the road, half-side
   *  hachures. */
  float offset = 0.0f;

  /** Terminal caps per contour (start = the path's first point). The
   *  grounded convention (leaflet-polylinedecorator, tldraw, D3 practice):
   *  the arrow TIP sits AT the endpoint, the head extends BACKWARD over
   *  the run, and the body is trimmed under Arrow/Bar heads (dashes stop
   *  cleanly under the head). 60° apex; capSize ≈ 3× width reads
   *  canonical. */
  Cap startCap = Cap::None;
  Cap endCap = Cap::None;
  float capSize = 10.0f;

  /** Mid-path repeated caps (the polylinedecorator pattern): a cap glyph
   *  every `midSpacing` px — direction chevrons down a wire. */
  Cap midCap = Cap::None;
  float midSpacing = 0.0f;

  /** Dashing still composes with everything above (applied to the body
   *  strokes, never to heads or ties). */
  std::vector<SkScalar> dashIntervals;
  float dashPhase = 0.0f;

  bool operator==(const Line &) const = default;

  /** Paint reach beyond the outline (cull growth): outer parallels, tie
   *  arms, and heads all overhang. */
  float bleed() const {
    const float casing = parallels > 1 ? gap * (float)(parallels - 1) : 0.0f;
    return width + casing + waveAmplitude + std::abs(offset) +
           std::max({tickLength * 0.5f, capSize, 0.0f});
  }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    if (ctx.outline.isEmpty() || width <= 0)
      return;

    // 1. The body run: offset, then displaced into a wave, then trimmed
    //    back under Arrow/Bar heads (tldraw clips the body from the head
    //    region; trimming also stops dashes cleanly under the head).
    SkPath body = offset != 0 ? offsetAlong(ctx.outline, offset) : ctx.outline;
    if (waveAmplitude > 0)
      body = displace(body, waveAmplitude, waveLength, zigzag);
    const float headTrim = trimFor(endCap);
    const float tailTrim = trimFor(startCap);
    if (headTrim > 0 || tailTrim > 0) {
      SkPathBuilder trimmed;
      SkContourMeasureIter iter(body, false);
      while (sk_sp<SkContourMeasure> contour = iter.next()) {
        const float len = contour->length();
        if (contour->isClosed()) {
          // Closed contours have no terminals — keep whole.
          contour->getSegment(0, len, &trimmed, true);
        } else {
          contour->getSegment(std::min(tailTrim, len * 0.4f),
                              len - std::min(headTrim, len * 0.4f), &trimmed,
                              true);
        }
      }
      body = trimmed.detach();
    }

    SkPaint stroke;
    stroke.setAntiAlias(true);
    stroke.setStyle(SkPaint::kStroke_Style);
    stroke.setStrokeJoin(SkPaint::kRound_Join); // grounded: round joins on
    stroke.setStrokeCap(SkPaint::kRound_Cap);   // rails (osm-carto/leaflet)
    applyFill(stroke);
    if (!dashIntervals.empty())
      stroke.setPathEffect(SkDashPathEffect::Make(
          SkSpan(dashIntervals.data(), dashIntervals.size()), dashPhase));

    // 2. Parallels. Undashed rails ride the stroke-OUTLINE construction
    //    (exact parallel curves on bends; round joins + Simplify() kill
    //    the miter spikes and tight-bend knots the references repair by
    //    hand). Dashed rails are built per line via offsetAlong instead —
    //    both rails share one arc parameterization, so dashes stay in
    //    phase (the Mapbox property the loop can't give).
    if (parallels <= 1) {
      stroke.setStrokeWidth(width);
      canvas.drawPath(body, stroke);
    } else if (!dashIntervals.empty()) {
      stroke.setStrokeWidth(width);
      const int n = parallels;
      for (int i = 0; i < n; ++i) {
        const float o = gap * ((float)i - (float)(n - 1) * 0.5f);
        SkPaint p = stroke;
        if (parallels % 2 && i == n / 2)
          p.setStrokeWidth(width * std::max(coreWidthFactor, 0.1f));
        canvas.drawPath(o == 0 ? body : offsetAlong(body, o), p);
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
        spread.setStrokeJoin(SkPaint::kRound_Join);
        spread.setStrokeCap(SkPaint::kRound_Cap);
        SkPath loop = skpathutils::FillPathWithPaint(body, spread);
        if (std::optional<SkPath> simple = Simplify(loop))
          loop = std::move(*simple); // tight-bend self-intersection repair
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
          if (!contour->getPosTan(d, &pos, &tan))
            continue;
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

    // 4. Caps, FILLED. Grounded convention: the arrow TIP sits AT the
    //    endpoint and the head extends BACKWARD (leaflet-polylinedecorator
    //    builds [barb, tip, barb] on the sample; tldraw nudges the base
    //    back along the body; D3 sets refX to the tip). Mid-path chevrons
    //    reuse the same glyphs at intervals.
    if (startCap != Cap::None || endCap != Cap::None ||
        (midCap != Cap::None && midSpacing > 0)) {
      SkPaint head;
      head.setAntiAlias(true);
      applyFill(head);
      SkContourMeasureIter iter(ctx.outline, false);
      while (sk_sp<SkContourMeasure> contour = iter.next()) {
        const float len = contour->length();
        SkPoint pos;
        SkVector tan;
        if (!contour->isClosed()) {
          if (endCap != Cap::None && contour->getPosTan(len, &pos, &tan))
            drawCap(canvas, head, endCap, pos, tan);
          if (startCap != Cap::None && contour->getPosTan(0, &pos, &tan))
            drawCap(canvas, head, startCap, pos, {-tan.x(), -tan.y()});
        }
        if (midCap != Cap::None && midSpacing > 0)
          for (float d = midSpacing; d < len - headTrim; d += midSpacing)
            if (contour->getPosTan(d, &pos, &tan))
              drawCap(canvas, head, midCap, pos, tan);
      }
    }
  }

private:
  /** How much body to cut under a terminal (dashes stop under heads). */
  float trimFor(Cap cap) const {
    switch (cap) {
    case Cap::Arrow: return capSize * 0.9f;
    case Cap::Bar: return std::max(width, 2.0f) * 0.5f;
    case Cap::Dot:
    case Cap::None: break;
    }
    return 0.0f;
  }

  void applyFill(SkPaint &p) const {
    if (fill.kind == Fill::Kind::Color)
      p.setColor4f(fill.colorValue, nullptr);
    else if (fill.kind == Fill::Kind::Shader)
      p.setShader(fill.shaderValue);
  }

  void drawCap(SkCanvas &canvas, const SkPaint &head, Cap cap, SkPoint pos,
               SkVector tan) const {
    const float t = std::hypot(tan.x(), tan.y());
    if (t < 1e-4f)
      return;
    tan = {tan.x() / t, tan.y() / t};
    const SkVector n{-tan.y(), tan.x()};
    switch (cap) {
    case Cap::Arrow: {
      // Tip AT the endpoint; barbs capSize back at ±tan(30°)·capSize —
      // the canonical 60° apex (tldraw ±π/6, decorator headAngle 60).
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
      canvas.drawLine({pos.x() - n.x() * capSize * 0.5f,
                       pos.y() - n.y() * capSize * 0.5f},
                      {pos.x() + n.x() * capSize * 0.5f,
                       pos.y() + n.y() * capSize * 0.5f},
                      bar);
      break;
    }
    case Cap::None:
      break;
    }
  }
};

// ---- factory sugar ---------------------------------------------------------

/** The transit pair: two rails following the route. */
inline Line cased(float width, Fill fill, float gap = 5.0f) {
  Line l;
  l.width = width;
  l.fill = std::move(fill);
  l.parallels = 2;
  l.gap = gap;
  return l;
}

/** Triple rail with a weighted spine (bold center, light outriders). */
inline Line triple(float width, Fill fill, float gap = 5.0f,
                   float coreFactor = 1.8f) {
  Line l;
  l.width = width;
  l.fill = std::move(fill);
  l.parallels = 3;
  l.gap = gap;
  l.coreWidthFactor = coreFactor;
  return l;
}

/** Directed edge: plain body, filled arrowhead at the end. */
inline Line arrow(float width, Fill fill, float headSize = 10.0f) {
  Line l;
  l.width = width;
  l.fill = std::move(fill);
  l.endCap = Cap::Arrow;
  l.capSize = headSize;
  return l;
}

/** Railway: body + perpendicular ties. */
inline Line railway(float width, Fill fill, float tieSpacing = 12.0f,
                    float tieLength = 10.0f) {
  Line l;
  l.width = width;
  l.fill = std::move(fill);
  l.tickSpacing = tieSpacing;
  l.tickLength = tieLength;
  return l;
}

/** The cartographic railway (osm-carto roads.mss, verified): a dark line
 *  with a white dash overlay at ~1/3 width, 50% duty cycle (8 on, 8 off)
 *  — NOT ties; compose via Element::style(). scale 1 ≈ the z13 weights. */
inline LayerStyle railwayCarto(float scale = 1.0f,
                               SkColor4f dark = {0.439f, 0.439f, 0.439f, 1},
                               SkColor4f light = {1, 1, 1, 1}) {
  Line base;
  base.width = 3.0f * scale;
  base.fill = Fill::color(dark);
  Line dashes;
  dashes.width = 1.0f * scale;
  dashes.fill = Fill::color(light);
  dashes.dashIntervals = {8.0f * scale, 8.0f * scale};
  return LayerStyle{{}, {Decoration(base), Decoration(dashes)}};
}

/** The squiggle (sine) — set `zigzag` on the returned value for vertices. */
inline Line wavy(float width, Fill fill, float amplitude = 4.0f,
                 float wavelength = 18.0f) {
  Line l;
  l.width = width;
  l.fill = std::move(fill);
  l.waveAmplitude = amplitude;
  l.waveLength = wavelength;
  return l;
}

} // namespace sigil::compose::lines
