/** @file
 * The path operators: booleans over Skia's pathops, the stroke-expansion
 * offset, and the four distortions applied to resampled points.
 */

#include "sigilgeometry/path/Ops.h"

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/pathops/SkPathOps.h>
#include <sigilcore/compute/Noise.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Segments.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path::ops {

namespace {

SkPath binary(const SkPath& a, const SkPath& b, SkPathOp op) {
  SkPath out;
  if (!Op(a, b, op, &out)) return SkPath();
  return out;
}

glm::vec2 normalAt(const std::vector<glm::vec2>& pts, size_t i, bool closed) {
  const size_t n = pts.size();
  const glm::vec2 prev = pts[closed ? (i + n - 1) % n : (i == 0 ? 0 : i - 1)];
  const glm::vec2 next = pts[closed ? (i + 1) % n : std::min(i + 1, n - 1)];
  glm::vec2 t = next - prev;
  const float len = length(t);
  t = len > 0 ? t / len : glm::vec2{1, 0};
  return {t.y, -t.x};
}

template <typename Fn>
SkPath overSamples(const SkPath& path, float segmentPx, bool smooth,
                   Fn perContour) {
  SkPathBuilder out;
  for (const Polyline& contour : flatten(path, 0.25f)) {
    const float len = contour.length();
    const int count =
        std::max(8, (int)std::ceil(len / std::max(segmentPx, 0.5f)));
    Sampled samples = resample(contour, count);
    perContour(samples);
    out.addPath(toPath(samples, smooth));
  }
  return out.detach();
}

SkPaint::Join skJoin(Join join) {
  switch (join) {
    case Join::Round:
      return SkPaint::kRound_Join;
    case Join::Miter:
      return SkPaint::kMiter_Join;
    case Join::Bevel:
      return SkPaint::kBevel_Join;
  }
  return SkPaint::kRound_Join;
}

SkPaint::Cap skCap(Cap cap) {
  switch (cap) {
    case Cap::Butt:
      return SkPaint::kButt_Cap;
    case Cap::Round:
      return SkPaint::kRound_Cap;
    case Cap::Square:
      return SkPaint::kSquare_Cap;
  }
  return SkPaint::kButt_Cap;
}

/** The unit vector 90 degrees to the LEFT of travel, in Skia's y-down
 *  space — the library-wide across-the-path direction. */
glm::vec2 leftOf(glm::vec2 direction) { return {direction.y, -direction.x}; }

glm::vec2 unitOr(glm::vec2 v, glm::vec2 fallback) {
  const float len = glm::length(v);
  return len > 1e-9f ? v / len : fallback;
}

/** Which way a piece leaves the node it starts at, and arrives at the
 *  node it ends at: the first and last chord that is not degenerate. */
glm::vec2 leavingAlong(const Segment& piece) {
  for (int i = 1; i < piece.size(); ++i) {
    const glm::vec2 chord = piece.points[(size_t)i] - piece.points[0];
    if (glm::length(chord) > 1e-9f) return chord / glm::length(chord);
  }
  return {1, 0};
}
glm::vec2 arrivingAlong(const Segment& piece) {
  const int last = piece.size() - 1;
  for (int i = last - 1; i >= 0; --i) {
    const glm::vec2 chord =
        piece.points[(size_t)last] - piece.points[(size_t)i];
    if (glm::length(chord) > 1e-9f) return chord / glm::length(chord);
  }
  return {1, 0};
}

/** THE OFFSET THAT MOVES THE SOURCE'S OWN NODES. Each node travels along
 *  the bisector of the two edges meeting there, far enough that both
 *  offset edges pass through it — `distance / sin(theta/2)` — capped at
 *  `miterLimit` distances so a needle-sharp corner blunts rather than
 *  shoots off. Each handle travels along its own chord's normal. The
 *  node count, their order and their kinds are untouched, which is the
 *  whole point: the answer still interpolates against the source. */
SkPath movedNodes(const SkPath& path, float distance,
                  const OffsetOptions& options) {
  std::vector<SegmentContour> contours = segments(path);
  const float cap = std::max(options.miterLimit, 1.0f);
  for (SegmentContour& contour : contours) {
    const size_t pieces = contour.segments.size();
    if (pieces == 0) continue;
    const bool spelled =
        contour.closed && contour.segments.back().end() != contour.start();
    const size_t nodes = (contour.closed && !spelled) ? pieces : pieces + 1;

    std::vector<glm::vec2> shift(nodes, glm::vec2{0, 0});
    for (size_t j = 0; j < nodes; ++j) {
      const glm::vec2 out =
          j < pieces
              ? leavingAlong(contour.segments[j])
              : (contour.closed
                     ? unitOr(contour.start() - contour.segments.back().end(),
                              arrivingAlong(contour.segments.back()))
                     : arrivingAlong(contour.segments.back()));
      glm::vec2 in;
      if (j > 0) {
        in = arrivingAlong(contour.segments[j - 1]);
      } else if (contour.closed) {
        in = spelled ? unitOr(contour.start() - contour.segments.back().end(),
                              arrivingAlong(contour.segments.back()))
                     : arrivingAlong(contour.segments.back());
      } else {
        in = out;
      }
      const glm::vec2 bisector = unitOr(leftOf(in) + leftOf(out), leftOf(out));
      const float reach = std::max(glm::dot(bisector, leftOf(in)), 1.0f / cap);
      shift[j] = bisector * (distance / reach);
    }

    for (size_t j = 0; j < pieces; ++j) {
      Segment& piece = contour.segments[j];
      const int last = piece.size() - 1;
      const glm::vec2 start = piece.points[0];
      const glm::vec2 end = piece.points[(size_t)last];
      for (int i = 1; i < last; ++i) {
        const glm::vec2 chord =
            i * 2 <= last
                ? unitOr(piece.points[(size_t)i] - start, leavingAlong(piece))
                : unitOr(end - piece.points[(size_t)i], arrivingAlong(piece));
        piece.points[(size_t)i] += leftOf(chord) * distance;
      }
      piece.points[0] += shift[j];
      piece.points[(size_t)last] += shift[(j + 1) % nodes];
    }
  }
  return toPath(contours, path.getFillType());
}

void appendCurve(SkPathBuilder& out, const Segment& piece) {
  switch (piece.kind) {
    case SegmentKind::Line:
      out.lineTo(toSk(piece.points[1]));
      break;
    case SegmentKind::Quad:
      out.quadTo(toSk(piece.points[1]), toSk(piece.points[2]));
      break;
    case SegmentKind::Conic:
      out.conicTo(toSk(piece.points[1]), toSk(piece.points[2]), piece.weight);
      break;
    case SegmentKind::Cubic:
      out.cubicTo(toSk(piece.points[1]), toSk(piece.points[2]),
                  toSk(piece.points[3]));
      break;
  }
}

/** How far along each of its two legs one corner is cut, or nothing
 *  where the corner is not one this selection rounds. */
struct Cut {
  bool rounds = false;
  float step = 0;
};

/** ROUNDING THE CORNERS A SELECTION NAMES. A corner here is two straight
 *  legs meeting at a node: the arc is the quadratic through the two
 *  points a step along each leg with the node as its control, which is
 *  the curve a corner effect lays down. `visual` holds the arc's
 *  stand-off from the node constant rather than the step, taking a right
 *  angle as the reference — so an acute corner takes a smaller step and
 *  an obtuse one a larger, and every corner reads as the same weight. */
SkPath selectedCorners(const SkPath& path, float radius,
                       const CornerOptions& options) {
  const float reference = std::cos(kPi * 0.25f);
  SkPathBuilder out(path.getFillType());
  for (const SegmentContour& contour : segments(path)) {
    if (contour.segments.empty()) continue;
    // The closing line is a leg like any other, so a rectangle rounds
    // the corner its walk started at as well as the three it passes.
    std::vector<Segment> legs = contour.segments;
    const bool closure = contour.closed && legs.back().end() != contour.start();
    if (closure) {
      Segment closing;
      closing.kind = SegmentKind::Line;
      closing.points[0] = legs.back().end();
      closing.points[1] = contour.start();
      legs.push_back(closing);
    }
    const size_t pieces = legs.size();
    const bool ring = contour.closed;
    const std::vector<Polyline> flat = flatten(toPath({&contour, 1}));
    const float area = flat.empty() ? 0.0f : flat.front().signedArea();

    // One cut per node that starts a piece; only a closed contour can
    // round the node its walk starts at, since an open contour's first
    // node is an end rather than a corner.
    std::vector<Cut> cuts(pieces);
    for (size_t j = 0; j < pieces; ++j) {
      if (j == 0 && !ring) continue;
      const Segment& before = legs[(j + pieces - 1) % pieces];
      const Segment& after = legs[j];
      if (before.kind != SegmentKind::Line || after.kind != SegmentKind::Line)
        continue;
      const glm::vec2 in = arrivingAlong(before);
      const glm::vec2 leaving = leavingAlong(after);
      const float turnDeg =
          std::acos(std::clamp(glm::dot(in, leaving), -1.0f, 1.0f)) * kRadToDeg;
      if (!(turnDeg > options.minTurnDeg)) continue;
      const float cross = in.x * leaving.y - in.y * leaving.x;
      const bool outward = area == 0 || (cross > 0) == (area > 0);
      if (options.outwardOnly && !outward) continue;
      const float halfAngle = (180.0f - turnDeg) * 0.5f * kDegToRad;
      float step = options.visual ? radius * reference /
                                        std::max(std::cos(halfAngle), 1e-2f)
                                  : radius;
      step = std::min({step, glm::length(before.end() - before.start()) * 0.5f,
                       glm::length(after.end() - after.start()) * 0.5f});
      if (!(step > 0)) continue;
      cuts[j] = Cut{true, step};
    }

    const auto leaveOf = [&](size_t j) {
      return legs[j].start() + leavingAlong(legs[j]) * cuts[j].step;
    };
    const auto arriveOf = [&](size_t j) {
      const Segment& before = legs[(j + pieces - 1) % pieces];
      return before.end() - arrivingAlong(before) * cuts[j].step;
    };

    out.moveTo(toSk(cuts[0].rounds ? leaveOf(0) : contour.start()));
    for (size_t j = 0; j < pieces; ++j) {
      const size_t nextNode = (j + 1) % pieces;
      const bool cutAhead = (j + 1 < pieces || ring) && cuts[nextNode].rounds;
      const bool implied = closure && j + 1 == pieces && !cutAhead;
      if (legs[j].kind == SegmentKind::Line) {
        // The closing leg with no arc on either side is the closure
        // itself, and `close()` draws it.
        if (!implied)
          out.lineTo(toSk(cutAhead ? arriveOf(nextNode) : legs[j].end()));
      } else {
        appendCurve(out, legs[j]);
      }
      if (cutAhead)
        out.quadTo(toSk(legs[nextNode].start()), toSk(leaveOf(nextNode)));
    }
    if (contour.closed) out.close();
  }
  return out.detach();
}

}  // namespace

SkPath unite(const SkPath& a, const SkPath& b) {
  return binary(a, b, kUnion_SkPathOp);
}
SkPath subtract(const SkPath& a, const SkPath& b) {
  return binary(a, b, kDifference_SkPathOp);
}
SkPath intersect(const SkPath& a, const SkPath& b) {
  return binary(a, b, kIntersect_SkPathOp);
}
SkPath exclude(const SkPath& a, const SkPath& b) {
  return binary(a, b, kXOR_SkPathOp);
}

SkPath unite(const std::vector<SkPath>& paths) {
  SkOpBuilder builder;
  for (const SkPath& p : paths) builder.add(p, kUnion_SkPathOp);
  SkPath out;
  if (!builder.resolve(&out)) return SkPath();
  return out;
}

SkPath simplify(const SkPath& path) {
  SkPath out;
  if (!Simplify(path, &out)) return path;
  return out;
}

SkPath offset(const SkPath& path, float distance,
              const OffsetOptions& options) {
  if (options.keepCompatible) return movedNodes(path, distance, options);
  if (std::abs(distance) < 1e-3f) return path;

  const float position = std::clamp(options.position, 0.0f, 1.0f);
  // The band's centreline slides from one side of the source to the
  // other as `position` runs 0 to 1, and its half-width opens from
  // nothing at either end to the whole distance in the middle.
  const float centre = distance * (1.0f - 2.0f * position);
  const float halfWidth =
      std::abs(distance) * (1.0f - std::abs(1.0f - 2.0f * position));

  const SkPath spine =
      centre == 0 ? path : parallel(path, centre, options.step);
  if (halfWidth <= 0) return spine;

  SkPaint stroke;
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(halfWidth * 2.0f);
  stroke.setStrokeJoin(skJoin(options.join));
  stroke.setStrokeCap(skCap(options.cap));
  stroke.setStrokeMiter(options.miterLimit);
  const SkPath band = skpathutils::FillPathWithPaint(spine, stroke);

  // A band that reaches across the source encloses the source's own
  // edge, so the source and the band together are the grown area and
  // the source without it the shrunk one. A band to one side encloses
  // nothing of the source and is the answer itself.
  if (centre - halfWidth < 0 && centre + halfWidth > 0)
    return distance > 0 ? unite(path, band) : simplify(subtract(path, band));
  return band;
}

SkPath roundCorners(const SkPath& path, float radius,
                    const CornerOptions& options) {
  if (radius <= 0) return path;
  if (options == CornerOptions{}) {
    SkPathBuilder dst;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(radius);
        fx && fx->filterPath(&dst, path, &rec))
      return dst.detach();
    return path;
  }
  return selectedCorners(path, radius, options);
}

SkPath Roughen::apply(const SkPath& path) const {
  uint32_t contourIndex = 0;
  return overSamples(path, segmentPx, smooth, [&](Sampled& samples) {
    core::chance::Stream stream =
        core::chance::Stream::of(source, seed + contourIndex++ * 7919u);
    for (size_t i = 0; i < samples.points.size(); ++i) {
      const glm::vec2 n = normalAt(samples.points, i, samples.closed);
      samples.points[i] += n * (stream.signedUnit() * amplitude);
    }
  });
}

SkPath Zigzag::apply(const SkPath& path) const {
  // Sample at quarter wavelength so hard teeth land on their vertices.
  const float segment = std::max(wavelengthPx * 0.25f, 0.5f);
  return overSamples(path, segment, smooth, [&](Sampled& samples) {
    const size_t n = samples.points.size();
    const float cycles =
        std::max(1.0f, std::round(samples.sourceLength / wavelengthPx));
    std::vector<glm::vec2> original = samples.points;
    for (size_t i = 0; i < n; ++i) {
      const float phase = (float)i / (float)n * cycles * kTau;
      const float wave =
          smooth ? std::sin(phase)
                 : (std::asin(std::sin(phase)) * (2.0f / kPi));  // triangle
      samples.points[i] +=
          normalAt(original, i, samples.closed) * (wave * amplitude);
    }
  });
}

SkPath PuckerBloat::apply(const SkPath& path) const {
  const float amount = std::clamp(this->amount, -1.0f, 1.0f);
  return overSamples(path, segmentPx, true, [&](Sampled& samples) {
    const glm::vec2 c = samples.centroid();
    float rMax = 1e-3f;
    for (const glm::vec2& p : samples.points)
      rMax = std::max(rMax, distance(p, c));
    // Radial power warp: exponent < 1 bloats (spherize), > 1 puckers.
    const float exponent = std::pow(2.0f, -amount * 1.6f);
    for (glm::vec2& p : samples.points) {
      const glm::vec2 d = p - c;
      const float r = length(d);
      if (r < 1e-6f) continue;
      const float rNew = rMax * std::pow(r / rMax, exponent);
      p = c + d / r * rNew;
    }
  });
}

SkPath Twirl::apply(const SkPath& path) const {
  return overSamples(path, segmentPx, true, [&](Sampled& samples) {
    const glm::vec2 c = samples.centroid();
    float rMax = 1e-3f;
    for (const glm::vec2& p : samples.points)
      rMax = std::max(rMax, distance(p, c));
    const float full = angleDeg * kDegToRad;
    for (glm::vec2& p : samples.points) {
      const glm::vec2 d = p - c;
      const float r = length(d);
      const float falloff = 1.0f - std::clamp(r / rMax, 0.0f, 1.0f);
      const float a = full * falloff * falloff;
      const float cs = std::cos(a), sn = std::sin(a);
      p = {c.x + d.x * cs - d.y * sn, c.y + d.x * sn + d.y * cs};
    }
  });
}

PathOp chain(std::vector<PathOp> steps) {
  return [steps = std::move(steps)](const SkPath& path) {
    SkPath current = path;
    for (const PathOp& step : steps)
      if (step) current = step(current);
    return current;
  };
}

// ---------------------------------------------------------------------------
// Corner and displacement treatments over a POLYLINE contour.

SkPath chamferCorners(const SkPath& path, float cut) {
  if (cut <= 0 || path.isEmpty()) return path;
  SkPathBuilder out;
  std::vector<SkPoint> run;  // current contour's polyline vertices
  SkPathBuilder verbatim;    // the same contour, copied exactly
  bool closed = false, anyCurve = false;

  // A vertex's cut points: entry on the incoming leg, exit on the
  // outgoing leg, each clamped to half its leg. False at a
  // straight-through or degenerate vertex (no corner to cut).
  const auto cutAt = [&](size_t i, SkPoint& entry, SkPoint& exit) {
    const size_t n = run.size();
    const SkPoint prev = run[(i + n - 1) % n], v = run[i],
                  next = run[(i + 1) % n];
    const SkVector in{v.x() - prev.x(), v.y() - prev.y()};
    const SkVector outV{next.x() - v.x(), next.y() - v.y()};
    const float lenIn = std::hypot(in.x(), in.y());
    const float lenOut = std::hypot(outV.x(), outV.y());
    if (lenIn < 1e-4f || lenOut < 1e-4f) return false;
    const float cross = in.x() * outV.y() - in.y() * outV.x();
    const float dot = in.x() * outV.x() + in.y() * outV.y();
    if (std::abs(cross) <= 1e-4f * lenIn * lenOut && dot > 0)
      return false;  // straight through — no corner
    const float cIn = std::min(cut, lenIn * 0.5f);
    const float cOut = std::min(cut, lenOut * 0.5f);
    entry = {v.x() - in.x() / lenIn * cIn, v.y() - in.y() / lenIn * cIn};
    exit = {v.x() + outV.x() / lenOut * cOut, v.y() + outV.y() / lenOut * cOut};
    return true;
  };

  const auto emitChamfered = [&] {
    if (run.empty()) return;
    if (closed && run.size() > 1 && run.front() == run.back())
      run.pop_back();  // the closing joint belongs to close()
    const size_t n = run.size();
    SkPoint entry, exit;
    if (n < 3) {  // nothing to cut — as collected
      out.moveTo(run.front());
      for (size_t i = 1; i < n; ++i) out.lineTo(run[i]);
      if (closed) out.close();
      return;
    }
    if (!closed) {
      out.moveTo(run.front());
      for (size_t i = 1; i + 1 < n; ++i) {
        if (cutAt(i, entry, exit)) {
          out.lineTo(entry);
          out.lineTo(exit);
        } else {
          out.lineTo(run[i]);
        }
      }
      out.lineTo(run.back());
    } else {  // every vertex is interior, the moveTo joint included
      bool started = false;
      // NOT named `emit`: this header reaches Qt TUs, where that is a macro.
      const auto put = [&](SkPoint p) {
        if (started)
          out.lineTo(p);
        else {
          out.moveTo(p);
          started = true;
        }
      };
      for (size_t i = 0; i < n; ++i) {
        if (cutAt(i, entry, exit)) {
          put(entry);
          out.lineTo(exit);
          started = true;
        } else {
          put(run[i]);
        }
      }
      out.close();
    }
  };

  const auto flushContour = [&] {
    if (anyCurve)  // chamfer is a polyline treatment: curves pass through
      out.addPath(verbatim.detach());
    else
      emitChamfered();
    verbatim = SkPathBuilder();
    run.clear();
    closed = false;
    anyCurve = false;
  };

  SkPath::Iter iter(path, false);
  SkPoint pts[4];
  SkPath::Verb verb;
  while ((verb = iter.next(pts)) != SkPath::kDone_Verb) {
    switch (verb) {
      case SkPath::kMove_Verb:
        flushContour();
        run.push_back(pts[0]);
        verbatim.moveTo(pts[0]);
        break;
      case SkPath::kLine_Verb:
        run.push_back(pts[1]);
        verbatim.lineTo(pts[1]);
        break;
      case SkPath::kQuad_Verb:
        anyCurve = true;
        verbatim.quadTo(pts[1], pts[2]);
        break;
      case SkPath::kConic_Verb:
        anyCurve = true;
        verbatim.conicTo(pts[1], pts[2], iter.conicWeight());
        break;
      case SkPath::kCubic_Verb:
        anyCurve = true;
        verbatim.cubicTo(pts[1], pts[2], pts[3]);
        break;
      case SkPath::kClose_Verb:
        closed = true;
        verbatim.close();
        break;
      default:
        break;
    }
  }
  flushContour();
  return out.detach();
}

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
    // the loop walks a distance; the accumulated float is the position
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
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

}  // namespace sigil::geometry::path::ops
