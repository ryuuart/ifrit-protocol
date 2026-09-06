/** @file
 * The stock silhouettes an exclusion flow subtracts: a rectangle and a
 * circle answered analytically, a filled path answered off its flattened
 * outline — and, at a standoff, off the outline of everything within the
 * margin of it, which Skia's path ops give exactly — and an image answered
 * off its own alpha, at a standoff off a distance field, since pixels have
 * no outline to grow. The margin is a disc either way and never a square.
 */

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
#include <utility>
#include <vector>

#include "BandScan.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"
#include "sigilimage/field/DistanceField.h"
#include "sigilweave/layout/Flow.h"

namespace sigil::weave {

using detail::acrossMax;
using detail::acrossMin;
using detail::acrossOf;
using detail::alongMax;
using detail::alongMin;
using detail::alongOf;
using detail::bandOccupancy;
using detail::kBandEpsilon;
using detail::mergeSpans;

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
  if (std::abs(bounds.width() - bounds.height()) <= kBandEpsilon) return circle(bounds);
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

}  // namespace sigil::weave
