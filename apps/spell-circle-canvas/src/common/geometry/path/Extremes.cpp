/** @file
 * The turns of a curve, found with the arithmetic Skia already has, and
 * the split that puts a node at each of them.
 *
 * `src/core/SkGeometry.h` is Skia's own private header. It ships in this
 * install beside the static archive, so its symbols link; it carries no
 * export marking, so nothing outside a Skia build may rely on it being
 * there. THIS IS THE ONLY FILE IN THE LIBRARY THAT INCLUDES IT: the
 * extrema, inflection and curvature solvers are a page of well-known
 * cubic arithmetic each, and re-deriving them here would mean two
 * spellings of one formula and a second place for a rounding to differ.
 */

#include "sigilgeometry/path/Extremes.h"

#include <src/core/SkGeometry.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "sigilgeometry/path/Segments.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path {

namespace {

std::array<SkPoint, 4> skPoints(const Segment& piece) {
  std::array<SkPoint, 4> out{};
  for (int i = 0; i < piece.size(); ++i)
    out[(size_t)i] = toSk(piece.points[(size_t)i]);
  return out;
}

/** How far past its own two ends a curve reaches at `t`, measured on the
 *  axis the extreme was found on. A turn shallower than the tolerance is
 *  a rounding artefact rather than a feature of the drawing. */
float depthAt(float value, float from, float to) {
  const float low = std::min(from, to);
  const float high = std::max(from, to);
  if (value < low) return low - value;
  if (value > high) return value - high;
  return 0;
}

SkPoint evalAt(const Segment& piece, float t) {
  const std::array<SkPoint, 4> p = skPoints(piece);
  switch (piece.kind) {
    case SegmentKind::Line:
      return {p[0].fX + (p[1].fX - p[0].fX) * t,
              p[0].fY + (p[1].fY - p[0].fY) * t};
    case SegmentKind::Quad: {
      SkPoint at{};
      SkEvalQuadAt(p.data(), t, &at);
      return at;
    }
    case SegmentKind::Conic:
      return SkConic(p.data(), piece.weight).evalAt(t);
    case SegmentKind::Cubic: {
      SkPoint at{};
      SkEvalCubicAt(p.data(), t, &at, nullptr, nullptr);
      return at;
    }
  }
  return p[0];
}

/** Every parameter of `piece` at which the options say a node belongs,
 *  in order and without repeats. */
std::vector<float> turnsOf(const Segment& piece,
                           const ExtremeOptions& options) {
  std::vector<float> ts;
  const std::array<SkPoint, 4> p = skPoints(piece);
  const auto axis = [&](float value, float from, float to, float t) {
    if (t > 0 && t < 1 && depthAt(value, from, to) >= options.minDepthPx)
      ts.push_back(t);
  };

  if (has(options.where, Where::Axis)) {
    SkScalar found[2] = {0, 0};
    switch (piece.kind) {
      case SegmentKind::Line:
        break;
      case SegmentKind::Quad:
        if (SkFindQuadExtrema(p[0].fX, p[1].fX, p[2].fX, found) == 1)
          axis(evalAt(piece, found[0]).fX, p[0].fX, p[2].fX, found[0]);
        if (SkFindQuadExtrema(p[0].fY, p[1].fY, p[2].fY, found) == 1)
          axis(evalAt(piece, found[0]).fY, p[0].fY, p[2].fY, found[0]);
        break;
      case SegmentKind::Conic: {
        const SkConic conic(p.data(), piece.weight);
        SkScalar t = 0;
        if (conic.findXExtrema(&t))
          axis(conic.evalAt(t).fX, p[0].fX, p[2].fX, t);
        if (conic.findYExtrema(&t))
          axis(conic.evalAt(t).fY, p[0].fY, p[2].fY, t);
        break;
      }
      case SegmentKind::Cubic: {
        int count =
            SkFindCubicExtrema(p[0].fX, p[1].fX, p[2].fX, p[3].fX, found);
        for (int i = 0; i < count; ++i)
          axis(evalAt(piece, found[i]).fX, p[0].fX, p[3].fX, found[i]);
        count = SkFindCubicExtrema(p[0].fY, p[1].fY, p[2].fY, p[3].fY, found);
        for (int i = 0; i < count; ++i)
          axis(evalAt(piece, found[i]).fY, p[0].fY, p[3].fY, found[i]);
        break;
      }
    }
  }
  if (has(options.where, Where::Inflection) &&
      piece.kind == SegmentKind::Cubic) {
    SkScalar found[2] = {0, 0};
    const int count = SkFindCubicInflections(p.data(), found);
    for (int i = 0; i < count; ++i)
      if (found[i] > 0 && found[i] < 1) ts.push_back(found[i]);
  }
  if (has(options.where, Where::MaxCurvature)) {
    if (piece.kind == SegmentKind::Cubic) {
      SkScalar found[3] = {0, 0, 0};
      const int count = SkFindCubicMaxCurvature(p.data(), found);
      for (int i = 0; i < count; ++i)
        if (found[i] > 0 && found[i] < 1) ts.push_back(found[i]);
    } else if (piece.kind == SegmentKind::Quad) {
      const SkScalar t = SkFindQuadMaxCurvature(p.data());
      if (t > 0 && t < 1) ts.push_back(t);
    }
  }

  std::ranges::sort(ts);
  const auto duplicates = std::ranges::unique(
      ts, [](float a, float b) { return std::abs(a - b) < 1e-4f; });
  ts.erase(duplicates.begin(), duplicates.end());
  return ts;
}

/** The piece split at one parameter, as two pieces of its own kind, or
 *  nothing where the split cannot be taken — a conic whose chop the
 *  rational form refuses. A caller that cannot split keeps the piece
 *  whole; emitting the two halves anyway would emit the curve twice. */
std::optional<std::array<Segment, 2>> splitAt(const Segment& piece, float t) {
  const std::array<SkPoint, 4> p = skPoints(piece);
  std::array<Segment, 2> out{piece, piece};
  const auto take = [&](Segment& into, const SkPoint* from) {
    for (int i = 0; i < into.size(); ++i)
      into.points[(size_t)i] = fromSk(from[i]);
  };
  switch (piece.kind) {
    case SegmentKind::Line: {
      const SkPoint mid = evalAt(piece, t);
      out[0].points[1] = fromSk(mid);
      out[1].points[0] = fromSk(mid);
      break;
    }
    case SegmentKind::Quad: {
      SkPoint dst[5];
      SkChopQuadAt(p.data(), dst, t);
      take(out[0], dst);
      take(out[1], dst + 2);
      break;
    }
    case SegmentKind::Conic: {
      SkConic dst[2];
      if (!SkConic(p.data(), piece.weight).chopAt(t, dst)) return std::nullopt;
      take(out[0], dst[0].fPts);
      take(out[1], dst[1].fPts);
      out[0].weight = dst[0].fW;
      out[1].weight = dst[1].fW;
      break;
    }
    case SegmentKind::Cubic: {
      SkPoint dst[7];
      SkChopCubicAt(p.data(), dst, t);
      take(out[0], dst);
      take(out[1], dst + 3);
      break;
    }
  }
  return out;
}

}  // namespace

SkPath extremes(const SkPath& path, const ExtremeOptions& options) {
  std::vector<SegmentContour> contours = segments(path);
  for (SegmentContour& contour : contours) {
    std::vector<Segment> split;
    split.reserve(contour.segments.size());
    for (const Segment& piece : contour.segments) {
      const std::vector<float> ts = turnsOf(piece, options);
      Segment rest = piece;
      float consumed = 0;
      for (const float t : ts) {
        // Each split is taken on what is LEFT of the piece, so the
        // parameter has to be measured in the remainder's own frame.
        const float local = (t - consumed) / (1.0f - consumed);
        if (!(local > 1e-4f && local < 1.0f - 1e-4f)) continue;
        const std::optional<std::array<Segment, 2>> two = splitAt(rest, local);
        // A piece that will not split stays whole: it is pushed once
        // below, and no later parameter of it can be taken either.
        if (!two) break;
        split.push_back((*two)[0]);
        rest = (*two)[1];
        consumed = t;
      }
      split.push_back(rest);
    }
    contour.segments = std::move(split);
  }
  return toPath(contours, path.getFillType());
}

std::vector<glm::vec2> extremeNodes(const SkPath& path,
                                    const ExtremeOptions& options) {
  std::vector<glm::vec2> nodes;
  for (const SegmentContour& contour : segments(path))
    for (const Segment& piece : contour.segments)
      for (const float t : turnsOf(piece, options))
        nodes.push_back(fromSk(evalAt(piece, t)));
  return nodes;
}

}  // namespace sigil::geometry::path
