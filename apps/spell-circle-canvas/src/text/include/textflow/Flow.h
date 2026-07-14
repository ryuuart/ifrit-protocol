#pragma once

/** @file
 * The geometry a paragraph flows into. Text is never bound to a rectangle: a
 * "line" is an ordered list of LineIntervals — straight segments in any
 * direction, or spans of an SkPath contour — supplied one line at a time by
 * a FlowGeometry. Ready-made geometries below cover the common cases:
 *   - BlockFlow          a single rectangle.
 *   - ExclusionFlow      a rectangle minus moving shapes (circles, rects, or
 *                        arbitrary/compound SkPaths with their fill rule
 *                        honored).
 *   - VerticalBlockFlow  top-to-bottom CJK columns advancing right to left.
 *   - LineSetFlow        an explicit set of intervals (any origin/direction).
 *   - PathFlow           each SkPath contour becomes a line; glyphs ride the
 *                        tangent via RSXform runs.
 *
 * Implement FlowGeometry yourself for anything else. Pass the chosen
 * geometry to layoutParagraph() in ParagraphLayout.h.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>

#include <absl/container/flat_hash_map.h>

#include <memory>
#include <vector>

namespace textflow {

/// One stretch of pen travel a line of text may occupy. Text is never bound
/// to a rectangle: a "line" is just an ordered list of these, and they can
/// be straight segments in any direction or spans of an SkPath contour.
struct LineInterval {
  /// Straight form: pen starts at `origin` (a baseline point) and travels
  /// along unit vector `direction` for at most `length`.
  SkPoint origin = {0, 0};
  SkVector direction = {1, 0};
  float length = 0;

  /// Path form: when `contour` is set, the pen instead travels the
  /// contour's arc length starting at `contourStart`; glyphs are rotated to
  /// the local tangent (rendered with RSXform runs). `origin`/`direction`
  /// are ignored.
  sk_sp<SkContourMeasure> contour;
  float contourStart = 0;

  /// Contour intervals only: arc length consumed per unit of glyph advance.
  /// Compensates curvature when the glyphs' optical centers ride at a
  /// different radius than the measured baseline contour — e.g. text on the
  /// outside of a small circle reads too loose because the centers sit on a
  /// larger ring than the baseline; advanceScale = rBaseline / rCenter
  /// restores optical spacing. `length`, fitting, and alignment arithmetic
  /// all stay in unscaled advance units (set length = arcLength / scale to
  /// offer the whole contour), only the pen→arc mapping is scaled.
  float advanceScale = 1.0f;
};

/// Supplies the intervals available to each successive line. Implementations
/// are queried per layout pass (they may depend on animated state like
/// moving exclusion shapes); the layout never caches geometry between
/// passes.
class FlowGeometry {
public:
  virtual ~FlowGeometry() = default;

  /** Returns the intervals for line `index` (0-based), given the line's
   * height and the baseline's offset below the line top. Returns false when
   * the geometry is exhausted (no line `index` exists); an empty
   * `intervals` with a true return means "this line has no room, try the
   * next one".
   */
  virtual bool lineIntervals(int index, float lineHeight, float ascent,
                             std::vector<LineInterval> &intervals) = 0;

  /** Returns true when every line yields one interval of the same width
   * (TeX's model — BlockFlow and friends). Knuth-Plass uses this to merge
   * paths that reached the same breakpoint on different line numbers: their
   * futures are identical, so only the best survives and the active list
   * stays bounded by the line width instead of growing with the paragraph.
   */
  virtual bool uniformIntervals() const { return false; }
};

/// Classic paragraph block: horizontal lines filling a rectangle.
class BlockFlow : public FlowGeometry {
public:
  /** Creates horizontal line bands inside `bounds`. */
  explicit BlockFlow(const SkRect &bounds) : m_bounds(bounds) {}
  /** Returns the interval for a horizontal line band when it fits. */
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &intervals) override;
  /** Reports that every produced interval has the block width. */
  bool uniformIntervals() const override { return true; }

private:
  SkRect m_bounds;
};

/// A rectangle with exclusion shapes punched out (CSS float / shape-outside
/// style). Each line band subtracts every intersecting shape's horizontal
/// extent, so lines split into multiple intervals around the shapes. Shapes
/// are cheap to move: geometry is re-evaluated per layout pass.
class ExclusionFlow : public FlowGeometry {
public:
  struct Shape {
    enum Kind { kCircle, kRect, kPath } kind = kRect;
    /// kCircle uses the inscribed circle of `bounds`; kPath ignores
    /// `bounds`.
    SkRect bounds = SkRect::MakeEmpty();
    float padding = 0; ///< extra standoff around the shape

    /// kPath: any SkPath — multiple contours, curves, winding or even-odd
    /// fill (holes and concavities stay available to text). The path is
    /// flattened once and cached by its generation ID, so animate it by
    /// setting `pathOffset` (free) rather than rebuilding the SkPath every
    /// frame (a re-flatten per frame — still cheap, but not free).
    SkPath path;
    SkPoint pathOffset = {0, 0};

    /** Creates a circular exclusion inscribed in `bounds`. */
    [[nodiscard]] static Shape fromCircle(const SkRect &bounds,
                                          float padding = 0) {
      return {kCircle, bounds, padding, {}, {0, 0}};
    }
    /** Creates an axis-aligned rectangular exclusion. */
    [[nodiscard]] static Shape fromRectangle(const SkRect &bounds,
                                             float padding = 0) {
      return {kRect, bounds, padding, {}, {0, 0}};
    }
    /** Creates an exclusion from an arbitrary filled SkPath. */
    [[nodiscard]] static Shape fromPath(const SkPath &path, float padding = 0) {
      return {kPath, SkRect::MakeEmpty(), padding, path, {0, 0}};
    }
  };

  /** Creates horizontal line bands in `bounds`, minus configured shapes. */
  explicit ExclusionFlow(const SkRect &bounds);
  /** Destroys private flattened-path cache entries. */
  ~ExclusionFlow() override;

  /** Returns the mutable list of shapes subtracted from each line band. */
  std::vector<Shape> &shapes() { return m_shapes; }
  /** Returns the outer layout bounds. */
  const SkRect &bounds() const { return m_bounds; }

  /** Drops exclusion-created slivers (intervals narrower than
   * `minimumWidth`) that would otherwise appear between shapes.
   */
  void setMinIntervalWidth(float minimumWidth) {
    m_minIntervalWidth = minimumWidth;
  }

  /** Produces the remaining horizontal intervals for a line band. */
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &intervals) override;

private:
  struct FlatPath; ///< flattened-polygon cache entry (Flow.cpp)
  const FlatPath &flattenedPathFor(const SkPath &path);

  SkRect m_bounds;
  std::vector<Shape> m_shapes;
  float m_minIntervalWidth = 8;
  /// unique_ptr values: FlatPath addresses stay stable across rehashes.
  absl::flat_hash_map<uint32_t, std::unique_ptr<FlatPath>> m_flattenedPathCache;
};

/// Vertical-RL block (CJK book layout): each "line" is a top-to-bottom
/// column, columns advancing right to left. `lineHeight` is the column
/// pitch; the interval origin sits on the column's central axis, which is
/// what vertical-shaped glyphs centre themselves on (`ascent` is unused).
/// Pair with Paragraph::setWritingMode(WritingMode::kVerticalRL).
class VerticalBlockFlow : public FlowGeometry {
public:
  /** Creates top-to-bottom columns advancing right-to-left in `bounds`. */
  explicit VerticalBlockFlow(const SkRect &bounds) : m_bounds(bounds) {}
  /** Returns the interval for one vertical column when it fits. */
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &intervals) override;
  /** Reports that every produced column has the block height. */
  bool uniformIntervals() const override { return true; }

private:
  SkRect m_bounds;
};

/// Fully explicit geometry: the caller supplies every line's intervals —
/// arbitrary positions, directions, and counts (the Pretext demos).
class LineSetFlow : public FlowGeometry {
public:
  /** Creates an initially empty explicit geometry. */
  LineSetFlow() = default;
  /** Takes ownership of every caller-specified line interval. */
  explicit LineSetFlow(std::vector<std::vector<LineInterval>> lines)
      : m_lines(std::move(lines)) {}

  /** Returns the mutable explicit line collection. */
  std::vector<std::vector<LineInterval>> &lines() { return m_lines; }

  /** Copies the requested explicit line into `intervals`. */
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &intervals) override;

private:
  std::vector<std::vector<LineInterval>> m_lines;
};

/// Each contour of each path becomes one line; glyphs follow the curve.
class PathFlow : public FlowGeometry {
public:
  /** Measures every contour of `path` as a separate line. */
  explicit PathFlow(const SkPath &path);
  /** Appends every contour of another path as additional lines. */
  void addPath(const SkPath &path);

  /** Returns the measured contour interval at `index`. */
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &intervals) override;

private:
  std::vector<sk_sp<SkContourMeasure>> m_contours;
};

} // namespace textflow
