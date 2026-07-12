#pragma once

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>

#include <absl/container/flat_hash_map.h>

#include <memory>
#include <vector>

namespace textflow {

// One stretch of pen travel a line of text may occupy. Text is never bound
// to a rectangle: a "line" is just an ordered list of these, and they can be
// straight segments in any direction or spans of an SkPath contour.
struct LineInterval {
  // Straight form: pen starts at `origin` (a baseline point) and travels
  // along unit vector `dir` for at most `length`.
  SkPoint origin = {0, 0};
  SkVector dir = {1, 0};
  float length = 0;

  // Path form: when `contour` is set, the pen instead travels the contour's
  // arc length starting at `contourStart`; glyphs are rotated to the local
  // tangent (rendered with RSXform runs). `origin`/`dir` are ignored.
  sk_sp<SkContourMeasure> contour;
  float contourStart = 0;

  // Contour intervals only: arc length consumed per unit of glyph advance.
  // Compensates curvature when the glyphs' optical centers ride at a
  // different radius than the measured baseline contour — e.g. text on the
  // outside of a small circle reads too loose because the centers sit on a
  // larger ring than the baseline; advanceScale = rBaseline / rCenter
  // restores optical spacing. `length`, fitting, and alignment arithmetic
  // all stay in unscaled advance units (set length = arcLength / scale to
  // offer the whole contour), only the pen→arc mapping is scaled.
  float advanceScale = 1.0f;
};

// Supplies the intervals available to each successive line. Implementations
// are queried per layout pass (they may depend on animated state like moving
// exclusion shapes); the layout never caches geometry between passes.
class FlowGeometry {
public:
  virtual ~FlowGeometry() = default;

  // Intervals for line `index` (0-based), given the line's height and the
  // baseline's offset below the line top. Returns false when the geometry is
  // exhausted (no line `index` exists); an empty `out` with a true return
  // means "this line has no room, try the next one".
  virtual bool lineIntervals(int index, float lineHeight, float ascent,
                             std::vector<LineInterval> &out) = 0;
};

// Classic paragraph block: horizontal lines filling a rectangle.
class BlockFlow : public FlowGeometry {
public:
  explicit BlockFlow(const SkRect &rect) : m_rect(rect) {}
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &out) override;

private:
  SkRect m_rect;
};

// A rectangle with exclusion shapes punched out (CSS float / shape-outside
// style). Each line band subtracts every intersecting shape's horizontal
// extent, so lines split into multiple intervals around the shapes. Shapes
// are cheap to move: geometry is re-evaluated per layout pass.
class ExclusionFlow : public FlowGeometry {
public:
  struct Shape {
    enum Kind { kCircle, kRect, kPath } kind = kRect;
    // kCircle uses the inscribed circle of `bounds`; kPath ignores `bounds`.
    SkRect bounds = SkRect::MakeEmpty();
    float padding = 0; // extra standoff around the shape

    // kPath: any SkPath — multiple contours, curves, winding or even-odd
    // fill (holes and concavities stay available to text). The path is
    // flattened once and cached by its generation ID, so animate it by
    // setting `pathOffset` (free) rather than rebuilding the SkPath every
    // frame (a re-flatten per frame — still cheap, but not free).
    SkPath path;
    SkPoint pathOffset = {0, 0};

    static Shape Circle(const SkRect &bounds, float padding = 0) {
      return {kCircle, bounds, padding, {}, {0, 0}};
    }
    static Shape Rect(const SkRect &bounds, float padding = 0) {
      return {kRect, bounds, padding, {}, {0, 0}};
    }
    static Shape Path(const SkPath &path, float padding = 0) {
      return {kPath, SkRect::MakeEmpty(), padding, path, {0, 0}};
    }
  };

  // Defined out of line: FlatPath is incomplete here.
  explicit ExclusionFlow(const SkRect &rect);
  ~ExclusionFlow() override;

  std::vector<Shape> &shapes() { return m_shapes; }
  const SkRect &rect() const { return m_rect; }

  // Intervals narrower than this are dropped (slivers between shapes).
  void setMinIntervalWidth(float w) { m_minIntervalWidth = w; }

  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &out) override;

private:
  struct FlatPath; // flattened-polygon cache entry (Flow.cpp)
  const FlatPath &flattened(const SkPath &path);

  SkRect m_rect;
  std::vector<Shape> m_shapes;
  float m_minIntervalWidth = 8;
  // unique_ptr values: FlatPath addresses stay stable across rehashes.
  absl::flat_hash_map<uint32_t, std::unique_ptr<FlatPath>> m_flatCache;
};

// Vertical-RL block (CJK book layout): each "line" is a top-to-bottom
// column, columns advancing right to left. `lineHeight` is the column
// pitch; the interval origin sits on the column's central axis, which is
// what vertical-shaped glyphs centre themselves on (`ascent` is unused).
// Pair with Paragraph::setWritingMode(WritingMode::kVerticalRL).
class VerticalBlockFlow : public FlowGeometry {
public:
  explicit VerticalBlockFlow(const SkRect &rect) : m_rect(rect) {}
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &out) override;

private:
  SkRect m_rect;
};

// Fully explicit geometry: the caller supplies every line's intervals —
// arbitrary positions, directions, and counts (the Pretext demos).
class LineSetFlow : public FlowGeometry {
public:
  LineSetFlow() = default;
  explicit LineSetFlow(std::vector<std::vector<LineInterval>> lines)
      : m_lines(std::move(lines)) {}

  std::vector<std::vector<LineInterval>> &lines() { return m_lines; }

  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &out) override;

private:
  std::vector<std::vector<LineInterval>> m_lines;
};

// Each contour of each path becomes one line; glyphs follow the curve.
class PathFlow : public FlowGeometry {
public:
  explicit PathFlow(const SkPath &path);
  void addPath(const SkPath &path);

  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &out) override;

private:
  std::vector<sk_sp<SkContourMeasure>> m_contours;
};

} // namespace textflow
