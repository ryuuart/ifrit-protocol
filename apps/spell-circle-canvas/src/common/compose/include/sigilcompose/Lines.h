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
#include "sigilcompose/Material.h" // Stop — the along-arc gradient ramp
#include "sigilcompose/Shapes.h"   // detail::bisectTransition — one copy of
                                   // the contour-scan refinement, shared

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkStrokeRec.h> // dashed-parallel filterPath
#include <include/pathops/SkPathOps.h>
#include <include/effects/Sk2DPathEffect.h>
#include <include/effects/SkDashPathEffect.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sigil::compose::lines {

/** Wave/zigzag geometry op: resample @p src by arc length and displace
 *  along the normal.
 *
 *  Two conventions worth knowing. The wavelength is a MAXIMUM: per contour
 *  it is snapped down so a whole number of waves fits, which is what stops
 *  a run ending mid-crest. And both kinds are zero-phase at the endpoints,
 *  so the displaced run starts and ends ON the original route — heads and
 *  caps computed from the undisplaced outline stay attached to it.
 *
 *  The sine is sampled at λ/16; the zigzag emits triangular vertices at
 *  quarter-wave points. Shared by `lines::Line` and the kit's wave and
 *  zigzag shapers. */
inline SkPath displace(const SkPath &src, float amplitude, float wavelength,
                       bool zigzag) {
  SkPathBuilder out;
  SkContourMeasureIter iter(src, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    // Fit a whole number of waves: actual λ = len / round(len / λmax),
    // and never 0.
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
      if (at >= len)
        disp = 0; // both kinds are zero-phase at the endpoints — float
                  // step accumulation must not spike the final vertex
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
    if (contour->isClosed())
      out.close(); // keep closedness — trim/caps logic keys off it
  }
  return out.detach();
}

/** Square-wave (battlement) displacement: the run holds at +amp for half a
 *  wavelength then drops to −amp for the next, with the verticals coming
 *  from doubled points at each step. Zero at both endpoints and snapped to
 *  fit, exactly like displace() — the boxy member of the same family. */
inline SkPath displaceSquare(const SkPath &src, float amplitude,
                             float wavelength) {
  SkPathBuilder out;
  SkContourMeasureIter iter(src, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    const float lambdaMax = std::max(wavelength, 2.0f);
    const float lambda = len / std::max(1.0f, std::round(len / lambdaMax));
    auto plot = [&](float d, float disp, bool first) {
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(std::min(d, len), &pos, &tan))
        return;
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
    if (contour->isClosed())
      out.close();
  }
  return out.detach();
}

namespace detail {

/** Says so when a corner scan found nothing but the shape clearly has
 *  vertices — the alternative being a bracket set that renders blank while
 *  the API does exactly what it was told.
 *
 *  Why this is a diagnostic and NOT an adaptive default: the threshold's
 *  whole job is to tell a VERTEX from a finely-sampled CURVE, and a
 *  rounded corner is meant to take no bracket (stated at `cornerAngleDeg`).
 *  The scan steps 2 px, so an arc of radius r turns ~114/r degrees per
 *  sample — about 11° at r = 10. Any auto-lowered threshold low enough to
 *  catch a 20-gon's 18° vertices is also low enough to shatter a small
 *  rounded corner into a run of false ones. So the number stays the
 *  author's, and the library explains what to pass. */
inline void warnNoCornersFound(float sharpestDeg, float angleDeg) {
  // A SET of seen shapes, not the last one: two different failing shapes in
  // one frame alternate, and a last-seen guard then prints both on every
  // frame forever — a diagnostic that floods is a diagnostic people turn
  // off. Capped so a procedurally varied scene cannot grow it without
  // bound.
  static std::vector<int> seen;
  const int key = (int)std::lround(sharpestDeg);
  for (int k : seen)
    if (k == key)
      return;
  if (seen.size() >= 16)
    return;
  seen.push_back(key);
  SkDebugf("lines: no corner cleared the %.1f\xc2\xb0 threshold, but the "
           "sharpest tangent break on this contour is %.1f\xc2\xb0 — so "
           "weightedCorners and spans::corners() will "
           "draw nothing here, and spans::edges() (their complement) will "
           "claim the WHOLE boundary instead of stopping short of "
           "anything. A "
           "regular n-gon turns 360/n per vertex, which puts EVERY polygon "
           "above 12 sides under the 30\xc2\xb0 default (a 20-gon turns "
           "18\xc2\xb0). Pass a smaller angleDeg, e.g. %.0ff.\n",
           angleDeg, sharpestDeg, std::max(4.0f, sharpestDeg * 0.6f));
}

/** A resolved tangent break: WHERE the vertex is, and the two leg tangents
 *  the search converged on. Carrying the legs is what makes a corner
 *  bisector exact — re-probing at a fixed offset cannot know whether
 *  either leg is that long. */
struct CornerHit {
  float d = 0;
  SkVector in{0, 0}, out{0, 0};
};

/** THE corner scanner — the single one. Every decoration that asks where a
 *  shape's corners are comes through here, so the same shape cannot report
 *  different corners depending on which one asked, and the diagnostic
 *  below reaches all of them.
 *
 *  The scan STRADDLES the vertex: it compares the tangent at d − step with
 *  the tangent at d, so a break is first SEEN one step past the bend. The
 *  bracket is then bisected rather than guessed at — taking its midpoint
 *  would put the answer up to half a step off the real vertex. Eight
 *  halvings take a 6 px step under 0.03 px, below what the rasterizer can
 *  resolve, and the two leg tangents fall out of the search, which is what
 *  makes an exact bisector available to corner art. */
inline std::vector<CornerHit> findCorners(SkContourMeasure &contour,
                                          float angleDeg,
                                          float minSpacing = 3.0f,
                                          float step = 2.0f,
                                          bool warnWhenNone = true) {
  std::vector<CornerHit> corners;
  const float len = contour.length();
  if (len <= 0)
    return corners;
  const float cosThresh = std::cos(angleDeg * 0.017453293f);
  const float stride = std::max(step, 0.25f);
  // The sharpest break actually present, so a scan that finds nothing can
  // say what it DID see instead of failing silently.
  float sharpestDot = 1.0f;
  SkVector prev{0, 0}, atStart{0, 0};
  bool havePrev = false;
  for (float d = 0; d <= len; d += stride) {
    SkPoint pos;
    SkVector tan;
    if (!contour.getPosTan(std::min(d, len), &pos, &tan))
      continue;
    if (!havePrev) {
      atStart = tan;
    } else {
      const float dot = prev.x() * tan.x() + prev.y() * tan.y();
      sharpestDot = std::min(sharpestDot, dot);
      if (dot < cosThresh) {
        // Narrow the bracket through the shared refinement, which also
        // serves the quadrant scan in shapes::edges(). Only the predicate
        // differs, and one copy is what keeps the convention — the answer
        // is the far end of the bracket, never its midpoint — in one
        // place.
        SkVector inTan = prev, outTan = tan;
        const float at = shapes::detail::bisectTransition(
            std::max(0.0f, d - stride), std::min(d, len), [&](float mid) {
              SkVector mt;
              if (!contour.getPosTan(mid, nullptr, &mt))
                return true; // unevaluable: keep the near side
              if (inTan.x() * mt.x() + inTan.y() * mt.y() < cosThresh) {
                outTan = mt;
                return false;
              }
              inTan = mt;
              return true;
            });
        if (corners.empty() || at - corners.back().d > minSpacing)
          corners.push_back({at, inTan, outTan});
      }
    }
    prev = tan;
    havePrev = true;
  }
  // The SEAM of a closed contour is a corner too: the forward scan never
  // compares across the wrap. Its legs are known outright.
  if (contour.isClosed() && havePrev) {
    const float dot = prev.x() * atStart.x() + prev.y() * atStart.y();
    sharpestDot = std::min(sharpestDot, dot);
    if (dot < cosThresh &&
        (corners.empty() || corners.front().d > minSpacing))
      corners.insert(corners.begin(), {0.0f, prev, atStart});
  }
  if (corners.empty() && warnWhenNone) {
    const float sharpestDeg =
        std::acos(std::clamp(sharpestDot, -1.0f, 1.0f)) * 57.29578f;
    // 4° is above the noise a smooth curve produces at this step size.
    if (sharpestDeg >= 4.0f)
      warnNoCornersFound(sharpestDeg, angleDeg);
  }
  return corners;
}

} // namespace detail

/** One-sided constant displacement along the normal — **positive is LEFT
 *  of travel**, which in screen space (y grows down) is OUTSIDE a
 *  clockwise path. That is the one convention across this library:
 *  `bandPointAt`, `Profile::across`, `strand::offset`, `Line::across`,
 *  `Rail::across` and `TextPath::offset` all mean the same sign by
 *  "across".
 *
 *  Exact on straights, resampled on curves, and correct at hard vertices
 *  through the repair below. It is also the dash-safe way to build
 *  parallel rails: every rail keeps the original arc parameterization, so
 *  dashes stay in phase across the whole set.
 *
 *  THE CORNER REPAIR. A uniform resample cannot offset a polygon: it
 *  samples either side of a vertex and draws a chord between the two
 *  offset points, so the outer side comes out CHAMFERED and the inner side
 *  lays a spur across the turn. On a plain rectangle both are visible at a
 *  glance.
 *
 *  So the real vertices are found first (one shared scanner) and each one
 *  gets a proper join: the offset point on the incoming leg, an arc of
 *  radius |offset| about the vertex, and the offset point on the outgoing
 *  leg. On the convex side that arc IS the round join; on the concave
 *  side the two legs overlap and `Simplify` removes the crossing. The
 *  result is exact for polygons and unchanged for smooth curves, which
 *  have no breaks for the scanner to find. */
inline SkPath offsetAcross(const SkPath &src, float across,
                           float step = 4.0f) {
  // ONE negation, here and nowhere else: the construction below works in
  // right-of-travel signs, and this is the single place the public
  // left-positive convention is converted to it.
  const float offset = -across;
  if (offset == 0)
    return src;
  // `step` is public authoring data (shapers::Offset forwards it directly).
  // Never let a zero/negative/non-finite value stall the sampling loop.
  const float stride = std::isfinite(step) ? std::max(step, 0.5f) : 0.5f;
  const float radius = std::abs(offset);
  SkPathBuilder out;
  SkContourMeasureIter iter(src, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    const auto offsetOf = [&](SkPoint pos, SkVector tan) {
      return SkPoint{pos.x() - tan.y() * offset, pos.y() + tan.x() * offset};
    };
    // Resolve every join BEFORE sampling, because a miter swallows a
    // window of the contour either side of its vertex and the sample loop
    // has to know not to draw into it.
    struct Join {
      float d = 0;
      bool miter = false;
      SkPoint pt{0, 0};   // miter: the single replacement point
      SkPoint pIn{0, 0}, pOut{0, 0};
      SkPoint vertex{0, 0};
      float a0 = 0, sweep = 0;
      bool arc = false;
    };
    std::vector<Join> joins;
    // 20° is well below any join a reader would call a corner and well
    // above the per-sample turn of a smooth curve at this stride.
    for (const detail::CornerHit &hit :
         detail::findCorners(*contour, 20.0f, std::max(stride, 1.0f), stride,
                             /*warnWhenNone=*/false)) {
      Join j;
      j.d = hit.d;
      if (!contour->getPosTan(hit.d, &j.vertex, nullptr))
        continue;
      j.pIn = offsetOf(j.vertex, hit.in);
      j.pOut = offsetOf(j.vertex, hit.out);
      const float turn = hit.in.x() * hit.out.y() - hit.in.y() * hit.out.x();
      if (turn * offset >= 0.0f && std::abs(turn) > 1e-4f) {
        // INSIDE the turn: the two offset lines cross BEFORE the vertex, so
        // both endpoints lie past the crossing and the correct join is the
        // single point where they meet.
        const SkVector d{j.pOut.x() - j.pIn.x(), j.pOut.y() - j.pIn.y()};
        const float t = (d.x() * hit.out.y() - d.y() * hit.out.x()) / turn;
        if (std::abs(t) <= radius * 4.0f) { // a near-reversal miters to
          j.miter = true;                   // infinity — bevel instead
          j.pt = {j.pIn.x() + hit.in.x() * t, j.pIn.y() + hit.in.y() * t};
        }
      } else if (turn * offset < 0.0f) {
        // OUTSIDE: the legs diverge, the gap is real, the join is an arc
        // of the offset radius about the vertex.
        j.arc = true;
        j.a0 = std::atan2(j.pIn.y() - j.vertex.y(), j.pIn.x() - j.vertex.x());
        const float a1 =
            std::atan2(j.pOut.y() - j.vertex.y(), j.pOut.x() - j.vertex.x());
        j.sweep = a1 - j.a0;
        while (j.sweep > 3.14159265f) j.sweep -= 6.28318531f;
        while (j.sweep < -3.14159265f) j.sweep += 6.28318531f;
      }
      joins.push_back(j);
    }
    size_t next = 0;
    bool first = true;
    const auto push = [&](SkPoint p) {
      if (first) {
        out.moveTo(p);
        first = false;
      } else {
        out.lineTo(p);
      }
    };
    for (float d = 0;; d += stride) {
      const float at = std::min(d, len);
      while (next < joins.size() && joins[next].d <= at) {
        const Join &j = joins[next++];
        if (j.miter) {
          push(j.pt);
        } else {
          push(j.pIn);
          if (j.arc) {
            const SkRect oval = SkRect::MakeLTRB(
                j.vertex.x() - radius, j.vertex.y() - radius,
                j.vertex.x() + radius, j.vertex.y() + radius);
            out.arcTo(oval, j.a0 * 57.29578f, j.sweep * 57.29578f, false);
          }
          push(j.pOut);
        }
      }
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(at, &pos, &tan))
        break;
      // A mitered vertex absorbs the contour within `radius` of itself:
      // samples in that window offset to points PAST the crossing and
      // would stick out as a stub on the inside of every corner.
      bool swallowed = false;
      for (const Join &j : joins)
        if (j.miter && ((at > j.d - radius && at < j.d + radius) ||
                        // On a closed contour d = 0 and d = len are the SAME
                        // vertex, so the seam's window has to wrap or the
                        // run-out leaves a stub where the contour began.
                        (contour->isClosed() && j.d < radius &&
                         at > len - (radius - j.d)))) {
          swallowed = true;
          break;
        }
      if (!swallowed)
        push(offsetOf(pos, tan));
      if (at >= len)
        break;
    }
    if (contour->isClosed())
      out.close();
  }
  // NOT Simplify(): this path is going to be STROKED, and Simplify computes
  // a FILL region — on an open contour that is degenerate, and it dropped
  // segments and opened the very corners it was meant to repair.
  return out.detach();
}

/** Convert a path into its DASHED GEOMETRY — the dash segments as real
 *  contours, rather than a paint-time effect. The building block for any
 *  construction that has to dash BEFORE it does something else to the
 *  geometry (offset each dash onto a parallel rail, stamp along the marks,
 *  measure them).
 *
 *  IT EXISTS BECAUSE THE OBVIOUS SPELLING SILENTLY DOES NOTHING. Skia's
 *  dash effect opens with
 *
 *      // we do nothing if the src wants to be filled
 *      if (kFill_Style == style || kStrokeAndFill_Style == style) return false;
 *
 *  so `filterPath` with a `SkStrokeRec(kFill_InitStyle)` returns false and
 *  leaves the destination untouched. Every other path effect used here —
 *  corner, trim, discrete — accepts a fill rec, so a fill rec is the
 *  natural thing to reach for, and the one effect that refuses it fails by
 *  leaving a SOLID path behind rather than by reporting anything. Nothing
 *  in the render says the line was meant to be dashed.
 *
 *  Hairline is the rec to use. Returns the input unchanged when the
 *  pattern is empty or Skia declines. */
inline SkPath dashGeometry(const SkPath &src, SkSpan<const SkScalar> intervals,
                           float phase) {
  if (intervals.empty() || src.isEmpty())
    return src;
  sk_sp<SkPathEffect> fx = SkDashPathEffect::Make(intervals, phase);
  if (!fx)
    return src;
  SkPathBuilder dashed;
  SkStrokeRec rec(SkStrokeRec::kHairline_InitStyle); // NOT kFill — see above
  if (!fx->filterPath(&dashed, src, &rec))
    return src;
  return dashed.detach();
}

/** Offset a CLOSED outline inward (positive @p px) or outward (negative),
 *  following any silhouette — a chamfered panel, a star, a blob — not just
 *  rectangles.
 *
 *  How it works: stroking the outline at width 2|px| gives the RING
 *  straddling it, so subtracting that ring shrinks the shape and unioning
 *  it grows the shape by the same amount. Returns the input unchanged if
 *  the boolean op fails. */
inline SkPath insetOutline(const SkPath &outline, float px) {
  if (px == 0 || outline.isEmpty())
    return outline;
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

namespace detail {

/** Arc-length positions along @p contour where the tangent breaks by more
 *  than @p angleDeg — the corners, from the one shared scanner.
 *
 *  A gently ROUNDED corner deliberately yields nothing: there is no hard
 *  break, so there is no corner. That is the right answer and it surprises
 *  people, so it is stated here and at every call site. */
inline std::vector<float> cornerDistances(SkContourMeasure &contour,
                                          float angleDeg,
                                          float minSpacing = 3.0f,
                                          float step = 2.0f) {
  std::vector<float> out;
  for (const CornerHit &hit : findCorners(contour, angleDeg, minSpacing, step))
    out.push_back(hit.d);
  return out;
}

/** The shared machine behind brackets and gapped rules: keep (or drop) the
 *  arc within @p radius px of every corner. Windows wrap the seam and merge
 *  where they overlap, so a tight chamfer whose two breaks are 11 px apart
 *  reads as ONE bracket rather than two colliding ones. */
inline SkPath cornerWindows(const SkPath &src, float radius,
                            bool keepNearCorners, float angleDeg) {
  SkPathBuilder out;
  SkContourMeasureIter iter(src, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    if (len <= 0)
      continue;
    const bool closed = contour->isClosed();
    std::vector<float> corners = cornerDistances(*contour, angleDeg);
    if (!closed) {
      // An open run's two ends behave as corners for this purpose: a
      // bracket set on a rail is its two end ticks.
      corners.insert(corners.begin(), 0.0f);
      corners.push_back(len);
    }
    if (corners.empty()) {
      // No hard break anywhere (a circle, a rounded rect): there is
      // nothing for a bracket to sit on, and a gapped rule is simply the
      // whole contour.
      if (!keepNearCorners)
        (void)contour->getSegment(0, len, &out, true);
      continue;
    }
    std::vector<std::pair<float, float>> near;
    for (float c : corners) {
      float a = c - radius, b = c + radius;
      if (!closed) {
        a = std::max(a, 0.0f);
        b = std::min(b, len);
        if (b > a)
          near.push_back({a, b});
        continue;
      }
      a = std::fmod(a, len);
      if (a < 0)
        a += len;
      b = std::fmod(b, len);
      if (b < 0)
        b += len;
      if (a <= b)
        near.push_back({a, b});
      else { // the window straddles the seam
        near.push_back({a, len});
        near.push_back({0, b});
      }
    }
    std::sort(near.begin(), near.end());
    std::vector<std::pair<float, float>> merged;
    for (const auto &window : near) {
      if (!merged.empty() && window.first <= merged.back().second + 0.01f)
        merged.back().second = std::max(merged.back().second, window.second);
      else
        merged.push_back(window);
    }
    if (keepNearCorners) {
      for (const auto &window : merged)
        (void)contour->getSegment(window.first, window.second, &out, true);
    } else {
      float cursor = 0;
      for (const auto &window : merged) {
        if (window.first > cursor)
          (void)contour->getSegment(cursor, window.first, &out, true);
        cursor = std::max(cursor, window.second);
      }
      if (cursor < len)
        (void)contour->getSegment(cursor, len, &out, true);
    }
  }
  return out.detach();
}

} // namespace detail

/** CORNER BRACKETS as GEOMETRY: keep only the arc within @p arm px of each
 *  corner, so a rectangle becomes four L-shaped marks and nothing else —
 *  the reticle, the selection handle, the crop mark. It follows ANY
 *  silhouette, so chamfering the box puts the brackets on the chamfers,
 *  where four hand-placed corner elements would stay where they were put.
 *
 *  Prefer `spans::corners(arm)` when the marks belong to an element: that
 *  CLAIMS runs on the element's real boundary and leaves the rest of it
 *  free, where this returns a path that replaces the shape. Reach for this
 *  when you want the geometry itself. */
inline SkPath cornerBrackets(const SkPath &src, float arm,
                             float angleDeg = 30.0f) {
  return detail::cornerWindows(src, arm, true, angleDeg);
}

/** The complement: a rule that STOPS SHORT of every corner, leaving @p gap
 *  px of paper at each. The printer's open-corner box rule; also how a
 *  technical drawing keeps a frame from fighting its own dimension lines.
 *
 *  `spans::edges(gap)` is the same scan claimed on an element's own
 *  boundary; see cornerBrackets above for when to prefer which. */
inline SkPath cornerGaps(const SkPath &src, float gap,
                         float angleDeg = 30.0f) {
  return detail::cornerWindows(src, gap, false, angleDeg);
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
   *  between adjacent lines; map styles that specify the inner CLEAR gap
   *  instead convert as gap = clear + width. Parallels follow
   *  curves exactly (offset contours via the stroke-outline construction;
   *  dashed parallels switch to per-rail offsets so dashes stay in
   *  phase). */
  int parallels = 1;
  float gap = 4.0f;
  float coreWidthFactor = 1.0f;

  /** Corner treatment for the drawn strokes AND for the parallel-offset
   *  construction. The default round join also rounds the OFFSET contour,
   *  so a crisp 45° jog in a cased wire comes out as a soft S-curve; pass
   *  `SkPaint::kMiter_Join` to keep the jog sharp. The offset rails are
   *  built from a stroke outline either way, so this exposes that join
   *  rather than adding one. */
  SkPaint::Join join = SkPaint::kRound_Join;

  /** Wave/zigzag displacement of the run itself (the y2k squiggle, the
   *  hand-drawn nerve): amplitude in px, wavelength in px along the arc.
   *  Zigzag alternates hard vertices instead of the sine. */
  float waveAmplitude = 0.0f;
  float waveLength = 24.0f;
  bool zigzag = false;

  /** Railway ties: short perpendicular ticks every `tickSpacing` px
   *  (0 for none), `tickLength` px long — the rail and blueprint idiom,
   *  which reads well at a spacing-to-length ratio near 1:1. `tickWidth`
   *  0 strokes the ties at the line's own width; map conventions often
   *  want roughly twice it. */
  float tickSpacing = 0.0f;
  float tickLength = 8.0f;
  float tickWidth = 0.0f;

  /** One-sided displacement of the whole run, **positive LEFT of travel**
   *  (the one convention — see `offsetAcross`) — bus lanes beside the
   *  road, half-side hachures. Same semantics as
   *  `kit::brush::shapers::Offset` in a Brush pipeline: reach for the
   *  shaper when several layers share one displacement, this field for a
   *  single Line. */
  float across = 0.0f;

  /** Terminal caps per contour; start is the path's first point. The
   *  convention: the arrow TIP sits AT the endpoint and the head extends
   *  BACKWARD over the run, with the body trimmed out from under Arrow and
   *  Bar heads so dashes stop cleanly instead of showing through. The apex
   *  is 60°, and a capSize around 3× the line width reads as a normal
   *  arrowhead. */
  Cap startCap = Cap::None;
  Cap endCap = Cap::None;
  float capSize = 10.0f;

  /** Mid-path repeated caps: a cap glyph every `midSpacing` px — the
   *  direction chevrons that run down a wire. */
  Cap midCap = Cap::None;
  float midSpacing = 0.0f;

  /** Dashing still composes with everything above (applied to the body
   *  strokes, never to heads or ties). */
  std::vector<SkScalar> dashIntervals;
  float dashPhase = 0.0f;
  /** Bind it and the dashes march (see PathFormat::dashPhaseBinding). */
  const choreograph::Output<float> *dashPhaseBinding = nullptr;

  /** Along-arc gradient: colour as a ramp over the run's arc fraction — an
   *  energy fade, an elevation-coloured trail. Drawn as up to 48 arc chunks
   *  per contour, each solid at its own interpolated colour, with round
   *  joins hiding the seams; overrides `fill`'s colour when non-empty.
   *
   *  **It applies to a single run only.** With `parallels > 1` or a dash
   *  pattern set, this list is IGNORED and the casings paint flat. */
  std::vector<Stop> alongStops;

  bool operator==(const Line &) const = default;

  /** A bound dash phase makes the node volatile, the same declared-
   *  volatility contract PathFormat::trimPhase uses. */
  bool isAnimated() const { return dashPhaseBinding != nullptr; }
  float phase() const {
    return dashPhaseBinding ? dashPhaseBinding->value() : dashPhase;
  }

  /** Paint reach beyond the outline (cull growth): outer parallels, tie
   *  arms, and heads all overhang. */
  float bleed() const {
    const float casing = parallels > 1 ? gap * (float)(parallels - 1) : 0.0f;
    return width + casing + waveAmplitude + std::abs(across) +
           std::max({tickLength * 0.5f, capSize, 0.0f});
  }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    if (ctx.outline.isEmpty() || width <= 0)
      return;

    // 1. The body run: offset, then displaced into a wave, then trimmed
    //    back from under Arrow and Bar heads, which also stops dashes
    //    cleanly instead of letting them show through the head.
    SkPath body = across != 0 ? offsetAcross(ctx.outline, across) : ctx.outline;
    if (waveAmplitude > 0)
      body = displace(body, waveAmplitude, waveLength, zigzag);
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
          (void)contour->getSegment(
              std::min(tailTrim, len * 0.4f),
              len - std::min(headTrim, len * 0.4f), &trimmed, true);
        }
      }
      body = trimmed.detach();
    }

    SkPaint stroke;
    stroke.setAntiAlias(true);
    stroke.setStyle(SkPaint::kStroke_Style);
    stroke.setStrokeJoin(join);               // round unless asked otherwise
    stroke.setStrokeCap(SkPaint::kRound_Cap); // rails always end round
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
        if (t <= alongStops.front().pos)
          return alongStops.front().color;
        for (size_t i = 1; i < alongStops.size(); ++i)
          if (t <= alongStops[i].pos) {
            const float span = alongStops[i].pos - alongStops[i - 1].pos;
            const float k =
                span > 1e-6f ? (t - alongStops[i - 1].pos) / span : 1.0f;
            const SkColor4f &a = alongStops[i - 1].color;
            const SkColor4f &b2 = alongStops[i].color;
            return SkColor4f{a.fR + (b2.fR - a.fR) * k,
                             a.fG + (b2.fG - a.fG) * k,
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
    //    through offsetAcross instead, so every rail's pattern is measured
    //    on one arc parameterization and the dashes stay in phase.
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
      p.setPathEffect(nullptr); // geometry already dashed
      const int n = parallels;
      for (int i = 0; i < n; ++i) {
        const float o = gap * ((float)i - (float)(n - 1) * 0.5f);
        p.setStrokeWidth(parallels % 2 && i == n / 2
                             ? width * std::max(coreWidthFactor, 0.1f)
                             : width);
        canvas.drawPath(o == 0 ? dashedBody : offsetAcross(dashedBody, -o, 2.0f),
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
        spread.setStrokeJoin(join); // the offset contour inherits the join,
        spread.setStrokeCap(SkPaint::kRound_Cap); // so miter rails jog sharp
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

/** The cartographic railway: a dark line under a white dash overlay at
 *  about a third of its width, on a 50% duty cycle — the map convention,
 *  which uses no ties at all. Two decorations as one LayerStyle, so attach
 *  with `Element::style()`. */
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

// ---------------------------------------------------------------------------
// N-rail strokes — a parallel rule where EVERY rail is its own line
//
// `Line::parallels` gives N rails sharing one width, one fill, one dash and
// one phase; its only per-rail knob is `coreWidthFactor`, which applies to
// the centre rail and only when `parallels` is odd. `Rails` is for
// everything that needs more than that:
//
//   heavy outer + hairline inner at two rails  (the engraver's rule)
//   solid outer + DOTTED inner                 (a road under construction)
//   unequal gaps                               (road + kerb + lane)
//   per-rail colour
//
// Neither workaround covers it. Stacked strokes with different insets only
// work on concentric shapes, because an inset is not a displacement. A
// `Brush` with a per-layer offset shaper works on any path, but each layer
// dashes ITS OWN offset curve, whose arc length differs from its
// neighbour's on every bend, so the dashes shear apart.
//
// `Rails` is built on Line's dashed-parallel construction instead:
//
//     DASH IN CENTRELINE ARC-SPACE, THEN DISPLACE THE DASHES.
//
// Every rail's pattern is measured along the SAME curve, so rails sharing
// intervals stay in register through any curvature, and a rail with a
// different pattern still registers against the centreline — which is what
// makes an inner tick fall reliably between two outer ones.

/** One rail of a `Rails` stroke: its own displacement from the route, its
 *  own width, fill, dash pattern and phase. `across` is px **LEFT of
 *  travel** — the one convention (see `offsetAcross`), shared with
 *  `Line::across`, `strand::offset` and `Profile::across` — so a
 *  symmetric pair is {-gap/2, +gap/2}. */
struct Rail {
  float across = 0.0f;
  float width = 2.0f;
  Fill fill = Fill::color({1, 1, 1, 1});
  /** Empty → solid. Measured along the CENTRELINE, not this rail. */
  std::vector<SkScalar> dash;
  /** Added to the stroke's shared phase — the knob that slides ONE rail
   *  against its neighbours (staggered ties, a counter-dashed strand). */
  float dashPhase = 0.0f;
  SkPaint::Cap cap = SkPaint::kRound_Cap;
  SkPaint::Join join = SkPaint::kRound_Join;

  bool operator==(const Rail &) const = default;
};

/** The general parallel rule: an ordered set of `Rail`s sharing one route,
 *  one wave/zigzag displacement and one marching phase.
 *
 *      element.stroke(lines::Rails{.rails = {
 *          {.across = -4, .width = 2.4f, .fill = ink},
 *          {.across =  0, .width = 0.6f, .fill = ink, .dash = {1, 4}},
 *          {.across =  4, .width = 2.4f, .fill = ink}}});
 *
 *  A value like every other decoration: defaulted equality, so a static
 *  quad rail prunes and caches without a memo. */
struct Rails {
  std::vector<Rail> rails;

  /** Shared displacement of the route before any rail is offset — the
   *  whole set waves together (`Line::waveAmplitude` semantics). */
  float waveAmplitude = 0.0f;
  float waveLength = 24.0f;
  bool zigzag = false;

  /** Shared phase, added to every rail's own `dashPhase`. Bind it and the
   *  whole set marches in register (PathFormat::dashPhaseBinding). */
  float dashPhase = 0.0f;
  const choreograph::Output<float> *dashPhaseBinding = nullptr;

  /** Resample stride for the offset construction, px. 2 follows tight
   *  metro curves; loosen on long gentle routes. */
  float offsetStep = 2.0f;

  bool operator==(const Rails &) const = default;

  bool isAnimated() const { return dashPhaseBinding != nullptr; }
  float phase() const {
    return dashPhaseBinding ? dashPhaseBinding->value() : dashPhase;
  }

  float bleed() const {
    float worst = 0.0f;
    for (const Rail &r : rails)
      worst = std::max(worst, std::abs(r.across) + r.width * 0.5f);
    return worst + waveAmplitude;
  }

  /** Total centre-to-centre span of the set — the number a caller needs to
   *  reserve room, and the one `Line` never exposed. */
  float span() const {
    if (rails.empty())
      return 0.0f;
    float lo = rails.front().across, hi = rails.front().across;
    for (const Rail &r : rails) {
      lo = std::min(lo, r.across);
      hi = std::max(hi, r.across);
    }
    return hi - lo;
  }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    if (ctx.outline.isEmpty() || rails.empty())
      return;
    const SkPath body = waveAmplitude > 0
                            ? displace(ctx.outline, waveAmplitude, waveLength,
                                       zigzag)
                            : ctx.outline;
    const float base = phase();
    const float stride = std::isfinite(offsetStep)
                             ? std::max(offsetStep, 0.5f)
                             : 2.0f;
    for (const Rail &rail : rails) {
      if (rail.width <= 0)
        continue;
      // Dash the CENTRELINE (never this rail's own offset curve), so every
      // rail's pattern is measured in one arc parameterisation and the set
      // stays in register through any curvature.
      SkPath run =
          rail.dash.empty()
              ? body
              : dashGeometry(body,
                             SkSpan(rail.dash.data(), rail.dash.size()),
                             base + rail.dashPhase);
      if (rail.across != 0)
        run = offsetAcross(run, rail.across, stride);
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
};

/** N identical rails, symmetric about the route — the general form of
 *  `Line::parallels`, where 2 is `cased` and 3 is `triple` with a flat
 *  spine. `gap` is centre-to-centre between neighbours. */
inline Rails rails(int count, float width, Fill fill, float gap = 5.0f) {
  Rails r;
  const int n = std::max(count, 1);
  for (int i = 0; i < n; ++i)
    r.rails.push_back(Rail{.across = gap * ((float)i - (float)(n - 1) * 0.5f),
                           .width = width,
                           .fill = fill});
  return r;
}

/** Explicit rails, displacements and all. */
inline Rails rails(std::vector<Rail> set) {
  Rails r;
  r.rails = std::move(set);
  return r;
}

/** The four-rail rule, symmetric — `rails(4, …)` under a name that shows
 *  up in a completion list. */
inline Rails quad(float width, Fill fill, float gap = 4.0f) {
  return rails(4, width, std::move(fill), gap);
}

/** The engraver's asymmetric parallel rule: HEAVY / hair / HEAVY — the
 *  commonest printed rule after the plain one. */
inline Rails heavyHairHeavy(float heavy, float hair, Fill fill,
                            float gap = 5.0f) {
  return rails({{.across = -gap, .width = heavy, .fill = fill},
                {.across = 0, .width = hair, .fill = fill},
                {.across = gap, .width = heavy, .fill = fill}});
}

/** Solid casing with a DOTTED core — the map convention for a road under
 *  construction, a proposed route, a disused rail. `dotGap` is the spacing
 *  of the core's dots; the casing stays continuous. */
inline Rails dottedCore(float outer, float core, Fill fill, float gap = 5.0f,
                        float dotGap = 6.0f) {
  return rails({{.across = -gap, .width = outer, .fill = fill},
                {.across = 0,
                 .width = core,
                 .fill = fill,
                 .dash = {0.01f, dotGap},
                 .cap = SkPaint::kRound_Cap},
                {.across = gap, .width = outer, .fill = fill}});
}

/** Lattice hatching: parallel rules `spacing` px apart at `angleDeg`,
 *  `width` px each, filling the node's OUTLINE — clipped to it, so a
 *  concave shape hatches exactly rather than to its bounds. `cross` adds
 *  the perpendicular pass. A value decoration: compares, prunes and caches
 *  like any other. */
struct Hatch {
  Fill strokeFill = Fill::color({1, 1, 1, 1});
  float spacing = 6.0f;
  float width = 1.2f;
  float angleDeg = 45.0f;
  bool cross = false;
  /** Live pitch and live angle: a raw `Output<float>*`, the same
   *  convention `PathFormat::dashPhaseBinding`, `PathFormat::trimPhase`,
   *  `Line::dashPhaseBinding` and `Rails::dashPhaseBinding` already use.
   *
   *  A raw Output pointer and NOT an `Animatable`, because a decoration
   *  paints with only a `PaintContext` and has no instance against which a
   *  transition could be resolved. Binding either one makes
   *  `isAnimated()` true, which is what declares the node volatile and
   *  keeps a moiré, a tightening engraving or a rotating shade pass
   *  repainting. */
  const choreograph::Output<float> *spacingBinding = nullptr;
  const choreograph::Output<float> *angleBinding = nullptr;

  bool isAnimated() const {
    return spacingBinding != nullptr || angleBinding != nullptr;
  }
  float pitch() const {
    return spacingBinding ? spacingBinding->value() : spacing;
  }
  float angle() const { return angleBinding ? angleBinding->value() : angleDeg; }

  bool operator==(const Hatch &o) const {
    return strokeFill == o.strokeFill && spacing == o.spacing &&
           width == o.width && angleDeg == o.angleDeg && cross == o.cross &&
           spacingBinding == o.spacingBinding && angleBinding == o.angleBinding;
  }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    const float pitchPx = pitch();
    const float baseDeg = angle();
    if (pitchPx <= 0.5f)
      return;
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
    if (cross)
      pass(baseDeg + 90.0f);
    c.restore();
  }
};

inline Hatch hatch(Fill fill, float spacing = 6.0f, float width = 1.2f,
                   float angleDeg = 45.0f) {
  Hatch h;
  h.strokeFill = std::move(fill);
  h.spacing = spacing;
  h.width = width;
  h.angleDeg = angleDeg;
  return h;
}

inline Hatch crosshatch(Fill fill, float spacing = 6.0f, float width = 1.2f,
                        float angleDeg = 45.0f) {
  Hatch h = hatch(std::move(fill), spacing, width, angleDeg);
  h.cross = true;
  return h;
}

/** RADIAL hatching: rules that fan out of a centre, rings concentric with
 *  it, or both, clipped to the node's outline.
 *
 *  `lines::hatch` is a parallel lattice at one fixed angle, which cannot
 *  describe a field engraved out of a point; approximating one from many
 *  rotated wedges costs a node per wedge for a single field.
 *
 *  `spokes` rules every 360/spokes degrees; `rings` draws circles at even
 *  radii. Set either to 0 for the other alone. `centre` is a FRACTION of
 *  the node's box, so it survives a resize. A value decoration: compares,
 *  prunes and caches like the rest. */
struct RadialHatch {
  Fill strokeFill = Fill::color({1, 1, 1, 1});
  int spokes = 48;
  int rings = 0;
  float width = 1.2f;
  /** Skip the innermost `holeFraction` of the reach — a fan out of a
   *  point crowds to solid ink at the centre otherwise. */
  float holeFraction = 0.08f;
  SkPoint centre = {0.5f, 0.5f};
  float rotateDeg = 0.0f;
  /** STATED ring radii, in px from the centre. When non-empty this list
   *  replaces the `rings` spacing entirely — one circle per entry, exactly
   *  where it says. Use it whenever the radii matter: the even spacing
   *  runs out to the bounding box's HALF-DIAGONAL, so on a circular node
   *  the outermost ring lands at R·√2, outside the shape, and is clipped
   *  away entirely. Spokes keep their own reach either way. */
  std::vector<float> radiiPx;

  bool operator==(const RadialHatch &o) const {
    return strokeFill == o.strokeFill && spokes == o.spokes &&
           rings == o.rings && width == o.width &&
           holeFraction == o.holeFraction && centre == o.centre &&
           rotateDeg == o.rotateDeg && radiiPx == o.radiiPx;
  }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    if (width <= 0 || (spokes <= 0 && rings <= 0 && radiiPx.empty()))
      return;
    const SkRect box = ctx.outline.getBounds();
    if (box.isEmpty())
      return;
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
        if (r > 0)
          b.addCircle(origin.fX, origin.fY, r);
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
};

inline RadialHatch radialHatch(Fill fill, int spokes = 48, float width = 1.2f,
                               SkPoint centre = {0.5f, 0.5f}) {
  RadialHatch h;
  h.strokeFill = std::move(fill);
  h.spokes = spokes;
  h.width = width;
  h.centre = centre;
  return h;
}

/** The other half of the pair: rings only, no spokes. */
inline RadialHatch concentric(Fill fill, int rings = 12, float width = 1.2f,
                              SkPoint centre = {0.5f, 0.5f}) {
  RadialHatch h;
  h.strokeFill = std::move(fill);
  h.spokes = 0;
  h.rings = rings;
  h.width = width;
  h.centre = centre;
  return h;
}

/** Rings at STATED radii, px from the centre — `concentric(ink, {60, 64})`
 *  is a two-circle band exactly where it says. The evenly-spaced form
 *  above runs out to the bounding box's half-diagonal, which on a circular
 *  node clips its outermost ring away. */
inline RadialHatch concentric(Fill fill, std::vector<float> radiiPx,
                              float width = 1.2f,
                              SkPoint centre = {0.5f, 0.5f}) {
  RadialHatch h;
  h.strokeFill = std::move(fill);
  h.spokes = 0;
  h.rings = 0;
  h.radiiPx = std::move(radiiPx);
  h.width = width;
  h.centre = centre;
  return h;
}

} // namespace sigil::compose::lines
