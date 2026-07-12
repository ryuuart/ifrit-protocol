#pragma once

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>

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
    enum Kind { kCircle, kRect } kind = kRect;
    // kCircle uses the inscribed circle of `bounds`.
    SkRect bounds = SkRect::MakeEmpty();
    float padding = 0; // extra standoff around the shape
  };

  explicit ExclusionFlow(const SkRect &rect) : m_rect(rect) {}

  std::vector<Shape> &shapes() { return m_shapes; }
  const SkRect &rect() const { return m_rect; }

  // Intervals narrower than this are dropped (slivers between shapes).
  void setMinIntervalWidth(float w) { m_minIntervalWidth = w; }

  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &out) override;

private:
  SkRect m_rect;
  std::vector<Shape> m_shapes;
  float m_minIntervalWidth = 8;
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
