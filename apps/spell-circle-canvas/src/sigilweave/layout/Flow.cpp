/** @file
 * The ready-made flow geometries and the stock silhouettes a block
 * subtracts: a rectangle and a circle answered analytically, a filled
 * path answered off its flattened outline — and, at a standoff, off the
 * outline of everything within the margin of it, which Skia's path ops
 * give exactly — and an image answered off its own alpha, at a standoff
 * off a distance field, since pixels have no outline to grow. The margin
 * is a disc either way and never a square. Plus the band scan itself,
 * turned a quarter turn by its FlowAxis so a column meets a silhouette
 * through the same code a line does, the vertical block, the explicit
 * line set, one line per path contour, and the placement of a pen
 * coordinate on a contour interval with its tangent snapped.
 */

#include "sigilweave/layout/Flow.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathTypes.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <glm/vec2.hpp>
#include <memory>
#include <numbers>
#include <utility>

#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"
#include "sigilimage/field/DistanceField.h"

namespace sigil::weave {

namespace {

constexpr float kEps = 0.01f;

// Snaps a unit tangent to one of `steps` directions (512 → 0.7° steps —
// invisible). Continuously varying per-glyph rotations would otherwise mint
// a fresh glyph-atlas strike every frame for every glyph on a moving path,
// turning animated curved text into a per-frame mask-rasterization storm.
SkVector quantizeTangent(SkVector tangent, int directionCount) {
  if (directionCount <= 0) return tangent;
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  const float angle = std::atan2(tangent.fY, tangent.fX);
  int directionIndex =
      static_cast<int>(std::lround(angle / kTwoPi * directionCount)) %
      directionCount;
  if (directionIndex < 0) directionIndex += directionCount;
  const float snapped =
      static_cast<float>(directionIndex) * kTwoPi / directionCount;
  return {std::cos(snapped), std::sin(snapped)};
}

// THE TWO COORDINATES EVERY BAND SCAN WORKS IN. `along` is the way the pen
// travels on this flow's lines; `across` is the way its bands stack. A line
// flow reads x along and y across, a column flow reads y along and x
// across, and that swap is the whole of the difference between wrapping
// around a shape in lines and wrapping around it in columns.
float alongOf(FlowAxis axis, const glm::vec2& point) {
  return axis == FlowAxis::kColumns ? point.y : point.x;
}
float acrossOf(FlowAxis axis, const glm::vec2& point) {
  return axis == FlowAxis::kColumns ? point.x : point.y;
}
float alongMin(FlowAxis axis, const SkRect& rect) {
  return axis == FlowAxis::kColumns ? rect.top() : rect.left();
}
float alongMax(FlowAxis axis, const SkRect& rect) {
  return axis == FlowAxis::kColumns ? rect.bottom() : rect.right();
}
float acrossMin(FlowAxis axis, const SkRect& rect) {
  return axis == FlowAxis::kColumns ? rect.left() : rect.top();
}
float acrossMax(FlowAxis axis, const SkRect& rect) {
  return axis == FlowAxis::kColumns ? rect.right() : rect.bottom();
}

// Removes [excludedStart, excludedEnd] from every sorted, disjoint interval.
void subtractSpan(std::vector<std::pair<float, float>>& availableSpans,
                  float excludedStart, float excludedEnd) {
  std::vector<std::pair<float, float>> remainingSpans;
  remainingSpans.reserve(availableSpans.size() + 1);
  for (const auto& [spanStart, spanEnd] : availableSpans) {
    if (excludedEnd <= spanStart || excludedStart >= spanEnd) {
      remainingSpans.emplace_back(spanStart, spanEnd);
      continue;
    }
    if (excludedStart > spanStart)
      remainingSpans.emplace_back(spanStart, excludedStart);
    if (excludedEnd < spanEnd)
      remainingSpans.emplace_back(excludedEnd, spanEnd);
  }
  availableSpans = std::move(remainingSpans);
}

// Occupied ALONG-intervals of a flattened polygon set within the band
// [bandStart, bandEnd] measured ACROSS: fill intervals sampled at three
// scanlines (respecting the fill rule, so holes and concave gaps stay open)
// unioned with every edge's along-travel through the band (conservative —
// catches features that fall between the samples, like a star tip). Appends
// unmerged occupied spans. `axis` names which coordinate is which, so a
// column meets a silhouette through this same scan.
void bandOccupancy(const std::vector<std::vector<glm::vec2>>& contours,
                   bool evenOdd, FlowAxis axis, float bandStart, float bandEnd,
                   std::vector<std::pair<float, float>>& occupiedSpans) {
  static thread_local std::vector<std::pair<float, int>> crossings;
  const float scanlines[3] = {bandStart + kEps, (bandStart + bandEnd) * 0.5f,
                              bandEnd - kEps};
  for (const float scanline : scanlines) {
    crossings.clear();
    for (const std::vector<glm::vec2>& polygon : contours) {
      for (size_t pointIndex = 0; pointIndex < polygon.size(); ++pointIndex) {
        const glm::vec2& startPoint = polygon[pointIndex];
        const glm::vec2& endPoint = polygon[(pointIndex + 1) % polygon.size()];
        const float startAcross = acrossOf(axis, startPoint);
        const float endAcross = acrossOf(axis, endPoint);
        if (startAcross == endAcross) continue;
        // Half-open [min, max) so shared vertices count exactly once.
        const bool travelsUp = endAcross > startAcross;
        if (travelsUp ? (scanline < startAcross || scanline >= endAcross)
                      : (scanline < endAcross || scanline >= startAcross))
          continue;
        const float interpolation =
            (scanline - startAcross) / (endAcross - startAcross);
        crossings.emplace_back(alongOf(axis, startPoint) +
                                   interpolation * (alongOf(axis, endPoint) -
                                                    alongOf(axis, startPoint)),
                               travelsUp ? 1 : -1);
      }
    }
    std::sort(crossings.begin(), crossings.end());
    int winding = 0;
    unsigned parity = 0;
    bool inside = false;
    float openAlong = 0;
    for (const auto& [crossingAlong, windingDelta] : crossings) {
      winding += windingDelta;
      parity ^= 1u;
      const bool nowInside = evenOdd ? parity != 0 : winding != 0;
      if (nowInside && !inside) {
        openAlong = crossingAlong;
        inside = true;
      } else if (!nowInside && inside) {
        occupiedSpans.emplace_back(openAlong, crossingAlong);
        inside = false;
      }
    }
  }

  for (const std::vector<glm::vec2>& polygon : contours) {
    for (size_t pointIndex = 0; pointIndex < polygon.size(); ++pointIndex) {
      const glm::vec2& startPoint = polygon[pointIndex];
      const glm::vec2& endPoint = polygon[(pointIndex + 1) % polygon.size()];
      const float startAcross = acrossOf(axis, startPoint);
      const float endAcross = acrossOf(axis, endPoint);
      const float edgeNear = std::min(startAcross, endAcross);
      const float edgeFar = std::max(startAcross, endAcross);
      if (edgeFar <= bandStart || edgeNear >= bandEnd) continue;
      float startFraction = 0;
      float endFraction = 1;
      if (startAcross != endAcross) {
        const float nearFraction =
            (bandStart - startAcross) / (endAcross - startAcross);
        const float farFraction =
            (bandEnd - startAcross) / (endAcross - startAcross);
        startFraction =
            std::clamp(std::min(nearFraction, farFraction), 0.0f, 1.0f);
        endFraction =
            std::clamp(std::max(nearFraction, farFraction), 0.0f, 1.0f);
      }
      const float startAlong = alongOf(axis, startPoint);
      const float alongTravel = alongOf(axis, endPoint) - startAlong;
      const float spanStart = startAlong + startFraction * alongTravel;
      const float spanEnd = startAlong + endFraction * alongTravel;
      occupiedSpans.emplace_back(std::min(spanStart, spanEnd),
                                 std::max(spanStart, spanEnd));
    }
  }
}

void mergeSpans(std::vector<std::pair<float, float>>& spans) {
  std::sort(spans.begin(), spans.end());
  size_t mergedCount = 0;
  for (const auto& span : spans) {
    if (mergedCount > 0 && span.first <= spans[mergedCount - 1].second)
      spans[mergedCount - 1].second =
          std::max(spans[mergedCount - 1].second, span.second);
    else
      spans[mergedCount++] = span;
  }
  spans.resize(mergedCount);
}

}  // namespace

bool LineInterval::placeAt(float pen, float phase, int rotationSteps,
                           SkPoint* position, SkVector* tangent) const {
  if (!contour.valid()) {
    // Straight: the pen simply travels along the interval's own direction.
    // Nothing to run off the end of, so the phase is a plain shift.
    const float travel = pen + phase;
    *position =
        origin + SkVector{direction.x() * travel, direction.y() * travel};
    *tangent = quantizeTangent(direction, rotationSteps);
    return true;
  }
  const float contourLength = contour.length();
  float contourPosition = contourStart + (pen * advanceScale) + phase;
  bool inside = true;
  if (contour.closed() || wrapContour) {
    // Closed contours wrap: text can march around the loop forever
    // (shift the phase for an infinite marquee). `wrapContour` wraps a
    // contour the geometry would clamp, so the wrap is spelled here rather
    // than left to `around`.
    contourPosition = geometry::path::wrap(contourPosition, contourLength);
  } else {
    inside = contourPosition >= 0 && contourPosition <= contourLength;
    contourPosition = std::clamp(contourPosition, 0.0f, contourLength);
  }
  const std::optional<geometry::path::Contour::Sample> sample =
      contour.at(contourPosition);
  if (!sample) {
    *position = {0, 0};
    *tangent = {1, 0};
    return false;
  }
  *position = geometry::path::toSk(sample->position);
  *tangent = geometry::path::toSk(sample->tangent);
  // Walking backwards faces the other way — turned before the snap, so the
  // reversed direction lands on a ladder step rather than beside one.
  if (advanceScale < 0) *tangent = {-tangent->fX, -tangent->fY};
  // Rotation snaps; position stays exact.
  *tangent = quantizeTangent(*tangent, rotationSteps);
  return inside;
}

namespace {

// The greatest along-extent a disc of `margin` adds at a band, given how
// far the band is from the shape ACROSS. Inside the shape's own across
// range the disc adds its whole radius; outside it, the Pythagorean
// remainder — which is exactly what makes a corner round and a diagonal
// edge stand off by the margin rather than by margin·root-two.
float discReach(float margin, float distanceAcross) {
  if (margin <= 0) return 0;
  if (distanceAcross <= 0) return margin;
  if (distanceAcross >= margin) return -1;  // the disc never reaches
  return std::sqrt(margin * margin - distanceAcross * distanceAcross);
}

// How far a band stands from an across range, 0 when they overlap.
float acrossGap(float bandStart, float bandEnd, float near, float far) {
  if (bandEnd < near) return near - bandEnd;
  if (bandStart > far) return bandStart - far;
  return 0;
}

class RectangleSilhouette final : public Silhouette {
 public:
  explicit RectangleSilhouette(const SkRect& bounds) : m_bounds(bounds) {}
  void bandSpans(FlowAxis axis, Band band, float margin,
                 std::vector<Span>& spans) override {
    const float near = acrossMin(axis, m_bounds);
    const float far = acrossMax(axis, m_bounds);
    const float reach =
        discReach(margin, acrossGap(band.start, band.end, near, far));
    if (margin > 0 && reach < 0) return;
    if (margin <= 0 && (band.end <= near || band.start >= far)) return;
    spans.push_back({alongMin(axis, m_bounds) - std::max(reach, 0.0f),
                     alongMax(axis, m_bounds) + std::max(reach, 0.0f)});
  }
  SkRect bounds() const override { return m_bounds; }

 private:
  SkRect m_bounds;
};

class CircleSilhouette final : public Silhouette {
 public:
  explicit CircleSilhouette(const SkRect& bounds)
      : m_center{bounds.centerX(), bounds.centerY()},
        m_radius(std::min(bounds.width(), bounds.height()) * 0.5f) {}
  void bandSpans(FlowAxis axis, Band band, float margin,
                 std::vector<Span>& spans) override {
    // A disc offset of a circle IS a circle, so the margin is a larger
    // radius and the answer stays one square root a band.
    const float radius = m_radius + margin;
    const float centerAlong = alongOf(axis, m_center);
    const float centerAcross = acrossOf(axis, m_center);
    // Widest chord within the band: at the centre when the band contains
    // it, else at the nearest band edge.
    const float distance =
        acrossGap(band.start, band.end, centerAcross, centerAcross);
    if (distance >= radius) return;
    const float halfChord = std::sqrt(radius * radius - distance * distance);
    spans.push_back({centerAlong - halfChord, centerAlong + halfChord});
  }
  SkRect bounds() const override {
    return SkRect::MakeLTRB(m_center.x - m_radius, m_center.y - m_radius,
                            m_center.x + m_radius, m_center.y + m_radius);
  }

 private:
  glm::vec2 m_center;
  float m_radius = 0;
};

/// THE ANSWER FOR ANYTHING THAT IS PIXELS: a coverage raster of the shape,
/// an exact Euclidean distance field over it, and a run-length scan of
/// everything within the margin of ink.
///
/// An image's coverage has no outline to grow, so "within m of the ink"
/// is the only meaning a standoff has here, and a field answers it
/// deciding nothing. Anything that IS an outline takes the exact answer
/// instead: see PathSilhouette.
class DilatedCoverage {
 public:
  /// Rebuilds when the raster does not already cover this margin. The
  /// margin is taken up to the next whole pixel, so an animating standoff
  /// re-measures a few times rather than every frame.
  void ensure(const SkRect& shapeBounds, float margin,
              const std::function<void(SkCanvas&)>& paint) {
    const float wanted = std::ceil(std::max(margin, 0.0f));
    if (m_builtMargin == wanted && !m_field.empty()) return;
    m_builtMargin = wanted;
    m_field = {};
    m_area = shapeBounds.makeOutset(wanted + 1.0f, wanted + 1.0f);
    if (m_area.isEmpty()) return;
    // ONE RASTER PIXEL A LAYOUT PIXEL, so the staircase is finer than the
    // standoff it is measuring, with a ceiling that keeps a very large
    // shape from asking for a very large raster.
    const float longest = std::max(m_area.width(), m_area.height());
    m_scale = std::min(1.0f, kMaxRaster / std::max(longest, 1.0f));
    const int width = std::max(1, (int)std::ceil(m_area.width() * m_scale));
    const int height = std::max(1, (int)std::ceil(m_area.height() * m_scale));
    const sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeA8(width, height));
    if (!surface) return;
    SkCanvas& canvas = *surface->getCanvas();
    canvas.clear(0);
    canvas.scale(m_scale, m_scale);
    canvas.translate(-m_area.left(), -m_area.top());
    paint(canvas);
    SkPixmap alpha;
    if (!surface->peekPixels(&alpha)) return;
    m_field = image::distanceField(image::coverageMask(alpha, m_threshold));
  }

  void setThreshold(float threshold) {
    if (threshold == m_threshold) return;
    m_threshold = threshold;
    m_field = {};
    m_builtMargin = -1;
  }

  bool ready() const { return !m_field.empty(); }

  /// The runs of the dilated coverage inside the band, in flow coordinates.
  void spans(FlowAxis axis, Band band, float margin,
             std::vector<Span>& out) const {
    if (m_field.empty()) return;
    const float marginPixels = margin * m_scale;
    const bool columns = axis == FlowAxis::kColumns;
    // The band runs across the raster; the answer runs along it.
    const int acrossCount = columns ? m_field.width : m_field.height;
    const int alongCount = columns ? m_field.height : m_field.width;
    const float acrossOrigin = columns ? m_area.left() : m_area.top();
    const float alongOrigin = columns ? m_area.top() : m_area.left();
    int firstBand = (int)std::floor((band.start - acrossOrigin) * m_scale);
    int lastBand = (int)std::ceil((band.end - acrossOrigin) * m_scale);
    firstBand = std::max(firstBand, 0);
    lastBand = std::min(lastBand, acrossCount - 1);
    int runStart = -1;
    for (int along = 0; along < alongCount; ++along) {
      bool covered = false;
      for (int across = firstBand; across <= lastBand && !covered; ++across)
        covered = (columns ? m_field.at(across, along)
                           : m_field.at(along, across)) <= marginPixels;
      if (covered && runStart < 0) runStart = along;
      if (!covered && runStart >= 0) {
        out.push_back({alongOrigin + (float)runStart / m_scale,
                       alongOrigin + (float)along / m_scale});
        runStart = -1;
      }
    }
    if (runStart >= 0)
      out.push_back({alongOrigin + (float)runStart / m_scale,
                     alongOrigin + (float)alongCount / m_scale});
  }

 private:
  static constexpr float kMaxRaster = 2048.0f;
  image::DistanceField m_field;
  SkRect m_area = SkRect::MakeEmpty();
  float m_scale = 1.0f;
  float m_builtMargin = -1;
  float m_threshold = 0.5f;
};

/// A PATH'S SILHOUETTE, AT ANY STANDOFF, AS A PATH.
///
/// The disc offset of a filled path is the union of the fill with its own
/// outline stroked at twice the margin, round join and round cap — which
/// is exactly what a disc rolled around the outline sweeps, and which
/// Skia's path ops answer exactly. So a standoff costs one path op and
/// one flatten, and the band scan that already answers the zero-margin
/// case answers every other one, corners rounded and holes kept.
///
/// The distance field beside this class stays for a coverage silhouette,
/// whose input is pixels: an image has no outline to stroke, and "within
/// m of the ink" is the only meaning available there.
class PathSilhouette final : public Silhouette {
 public:
  explicit PathSilhouette(const SkPath& path) : m_path(path) {
    // An inverse fill means everything the path does not enclose, which as
    // a silhouette is the frame with a hole in it. One meaning is kept:
    // the enclosed region, which is what the band scan reads and what the
    // ink is drawn as.
    switch (m_path.getFillType()) {
      case SkPathFillType::kInverseWinding:
        m_path.setFillType(SkPathFillType::kWinding);
        break;
      case SkPathFillType::kInverseEvenOdd:
        m_path.setFillType(SkPathFillType::kEvenOdd);
        break;
      default:
        break;
    }
    m_evenOdd = m_path.getFillType() == SkPathFillType::kEvenOdd;
    m_bounds = m_path.computeTightBounds();
    flattenInto(m_path, m_contours);
  }

  void bandSpans(FlowAxis axis, Band band, float margin,
                 std::vector<Span>& spans) override {
    if (m_contours.empty()) return;
    const std::vector<std::vector<glm::vec2>>& contours =
        margin > 0 ? dilatedContours(margin) : m_contours;
    if (contours.empty()) return;
    const bool evenOdd = margin > 0 ? m_dilatedEvenOdd : m_evenOdd;
    static thread_local std::vector<std::pair<float, float>> occupied;
    occupied.clear();
    bandOccupancy(contours, evenOdd, axis, band.start, band.end, occupied);
    mergeSpans(occupied);
    for (const auto& [start, end] : occupied) spans.push_back({start, end});
  }

  SkRect bounds() const override { return m_bounds; }

 private:
  // Layout avoidance needs a couple of pixels of fidelity, not rendering
  // accuracy, and every contour is treated as closed: an open sub-path of
  // an exclusion is filled as if its ends were joined, exactly as the fill
  // rule fills it.
  static void flattenInto(const SkPath& path,
                          std::vector<std::vector<glm::vec2>>& contours) {
    constexpr float kFlattenTolerance = 0.5f;
    contours.clear();
    for (geometry::path::Polyline& polyline :
         geometry::path::flatten(path, kFlattenTolerance))
      if (polyline.points.size() >= 3)
        contours.push_back(std::move(polyline.points));
  }

  /// The outline of everything within @p margin of the fill, flattened.
  /// Held for the margin it was built at, so a standoff that does not
  /// change costs nothing after the first band.
  const std::vector<std::vector<glm::vec2>>& dilatedContours(float margin) {
    if (margin == m_dilatedMargin) return m_dilated;
    m_dilatedMargin = margin;

    SkPaint stroke;
    stroke.setStyle(SkPaint::kStroke_Style);
    stroke.setStrokeWidth(margin * 2.0f);
    stroke.setStrokeJoin(SkPaint::kRound_Join);
    stroke.setStrokeCap(SkPaint::kRound_Cap);
    const SkPath rim = skpathutils::FillPathWithPaint(m_path, stroke);
    SkPath grown;
    if (!Op(m_path, rim, SkPathOp::kUnion_SkPathOp, &grown)) {
      m_dilated = m_contours;
      m_dilatedEvenOdd = m_evenOdd;
      return m_dilated;
    }
    m_dilatedEvenOdd = grown.getFillType() == SkPathFillType::kEvenOdd;
    flattenInto(grown, m_dilated);
    return m_dilated;
  }

  SkPath m_path;
  std::vector<std::vector<glm::vec2>> m_contours;
  std::vector<std::vector<glm::vec2>> m_dilated;
  SkRect m_bounds = SkRect::MakeEmpty();
  bool m_evenOdd = false;
  bool m_dilatedEvenOdd = false;
  float m_dilatedMargin = -1;
};

class CoverageSilhouette final : public Silhouette {
 public:
  CoverageSilhouette(sk_sp<SkImage> image, const SkRect& box, float threshold)
      : m_image(std::move(image)), m_box(box) {
    m_dilated.setThreshold(threshold);
  }

  void bandSpans(FlowAxis axis, Band band, float margin,
                 std::vector<Span>& spans) override {
    if (!m_image) return;
    m_dilated.ensure(m_box, margin, [&](SkCanvas& canvas) {
      canvas.drawImageRect(m_image,
                           SkRect::MakeIWH(m_image->width(), m_image->height()),
                           m_box, SkSamplingOptions(SkFilterMode::kLinear),
                           nullptr, SkCanvas::kStrict_SrcRectConstraint);
    });
    m_dilated.spans(axis, band, margin, spans);
  }

  SkRect bounds() const override { return m_box; }

 private:
  sk_sp<SkImage> m_image;
  SkRect m_box;
  DilatedCoverage m_dilated;
};

}  // namespace

namespace silhouette {

std::shared_ptr<Silhouette> rectangle(const SkRect& bounds) {
  return std::make_shared<RectangleSilhouette>(bounds);
}

std::shared_ptr<Silhouette> circle(const SkRect& bounds) {
  return std::make_shared<CircleSilhouette>(bounds);
}

std::shared_ptr<Silhouette> ellipse(const SkRect& bounds) {
  if (std::abs(bounds.width() - bounds.height()) <= kEps) return circle(bounds);
  // A disc offset of an ellipse is not an ellipse, and scaling the axes to
  // fake one over- and under-shoots at different points of the curve. The
  // path answer is the exact one, so an oval that is not round takes it.
  SkPathBuilder oval;
  oval.addOval(bounds);
  return path(oval.detach());
}

std::shared_ptr<Silhouette> path(const SkPath& outline) {
  return std::make_shared<PathSilhouette>(outline);
}

std::shared_ptr<Silhouette> coverage(sk_sp<SkImage> image, const SkRect& box,
                                     float threshold) {
  return std::make_shared<CoverageSilhouette>(std::move(image), box, threshold);
}

}  // namespace silhouette

ExclusionFlow::ExclusionFlow(const SkRect& bounds, FlowAxis axis)
    : m_bounds(bounds), m_axis(axis) {}
ExclusionFlow::~ExclusionFlow() = default;

bool BlockFlow::lineIntervals(const LineRequest& request,
                              std::vector<LineInterval>& intervals) {
  intervals.clear();
  const float lineHeight = request.lineHeight;
  const float top = m_bounds.top() + request.bandStart;
  if (top + lineHeight > m_bounds.bottom() + kEps) return false;
  LineInterval interval;
  interval.origin = {m_bounds.left(), top + request.ascent};
  interval.direction = {1, 0};
  interval.length = m_bounds.width();
  intervals.push_back(interval);
  return true;
}

bool ExclusionFlow::lineIntervals(const LineRequest& request,
                                  std::vector<LineInterval>& intervals) {
  intervals.clear();
  const float lineHeight = request.lineHeight;
  const FlowAxis axis = m_axis;
  const bool columns = axis == FlowAxis::kColumns;

  // The band this line occupies across the stack, and where its pen sits
  // inside it. Lines stack DOWN from the top and put the pen on a baseline
  // `ascent` below the band's near edge; columns advance RIGHT TO LEFT from
  // the right edge and put the pen on the band's central axis, which is
  // what a vertical-shaped glyph centres itself on.
  float bandStart = 0;
  float bandEnd = 0;
  float penAxis = 0;
  if (columns) {
    bandEnd = m_bounds.right() - request.bandStart;
    bandStart = bandEnd - lineHeight;
    if (bandStart < m_bounds.left() - kEps) return false;
    penAxis = bandEnd - lineHeight * 0.5f;
  } else {
    bandStart = m_bounds.top() + request.bandStart;
    bandEnd = bandStart + lineHeight;
    if (bandEnd > m_bounds.bottom() + kEps) return false;
    penAxis = bandStart + request.ascent;
  }

  std::vector<std::pair<float, float>> availableSpans = {
      {alongMin(axis, m_bounds), alongMax(axis, m_bounds)}};

  static thread_local std::vector<Span> occupiedSpans;
  for (const Exclusion& exclusion : m_exclusions) {
    if (!exclusion.shape) continue;
    // Every silhouette answers in ITS OWN SPACE, so rigid motion is two
    // subtractions and a shift and never touches whatever the shape
    // cached: the band arrives moved back by the offset and the spans come
    // out moved forward by it.
    const float offsetAlong =
        alongOf(axis, {exclusion.offset.x(), exclusion.offset.y()});
    const float offsetAcross =
        acrossOf(axis, {exclusion.offset.x(), exclusion.offset.y()});
    const SkRect shapeBounds = exclusion.shape->bounds();
    const float margin = std::max(exclusion.margin, 0.0f);
    if (acrossMax(axis, shapeBounds) + offsetAcross + margin <= bandStart ||
        acrossMin(axis, shapeBounds) + offsetAcross - margin >= bandEnd)
      continue;
    occupiedSpans.clear();
    exclusion.shape->bandSpans(
        axis, Band{bandStart - offsetAcross, bandEnd - offsetAcross}, margin,
        occupiedSpans);
    for (const Span& span : occupiedSpans)
      subtractSpan(availableSpans, span.start + offsetAlong,
                   span.end + offsetAlong);
    if (availableSpans.empty()) break;
  }

  for (const auto& [spanStart, spanEnd] : availableSpans) {
    if (spanEnd - spanStart < m_minimumIntervalWidth) continue;
    LineInterval interval;
    interval.origin =
        columns ? SkPoint{penAxis, spanStart} : SkPoint{spanStart, penAxis};
    interval.direction = columns ? SkVector{0, 1} : SkVector{1, 0};
    interval.length = spanEnd - spanStart;
    intervals.push_back(interval);
  }
  return true;
}

bool VerticalBlockFlow::lineIntervals(const LineRequest& request,
                                      std::vector<LineInterval>& intervals) {
  intervals.clear();
  const float lineHeight = request.lineHeight;
  const float right = m_bounds.right() - request.bandStart;
  if (right - lineHeight < m_bounds.left() - kEps) return false;
  LineInterval interval;
  interval.origin = {right - lineHeight * 0.5f, m_bounds.top()};
  interval.direction = {0, 1};
  interval.length = m_bounds.height();
  intervals.push_back(interval);
  return true;
}

bool LineSetFlow::lineIntervals(const LineRequest& request,
                                std::vector<LineInterval>& intervals) {
  intervals.clear();
  const int index = request.index;
  if (index < 0 || static_cast<size_t>(index) >= m_lines.size()) return false;
  intervals = m_lines[static_cast<size_t>(index)];
  return true;
}

PathFlow::PathFlow(const SkPath& path) { addPath(path); }

void PathFlow::addPath(const SkPath& path) {
  for (geometry::path::Contour& contour : geometry::path::Contour::of(path))
    m_contours.push_back(std::move(contour));
}

bool PathFlow::lineIntervals(const LineRequest& request,
                             std::vector<LineInterval>& intervals) {
  intervals.clear();
  const int index = request.index;
  if (index < 0 || static_cast<size_t>(index) >= m_contours.size())
    return false;
  LineInterval interval;
  interval.contour = m_contours[static_cast<size_t>(index)];
  interval.contourStart = 0;
  interval.length = interval.contour.length();
  intervals.push_back(interval);
  return true;
}

}  // namespace sigil::weave
