#pragma once

/** @file
 * @ingroup geometry
 *
 * The geometry a paragraph flows into. Text is never bound to a rectangle: a
 * "line" is an ordered list of LineIntervals — straight segments in any
 * direction, or spans of an SkPath contour — supplied one line at a time by
 * a FlowGeometry. Ready-made geometries below cover the common cases:
 *   - BlockFlow          a single rectangle.
 *   - ExclusionFlow      a rectangle minus moving shapes (circles, rects, or
 *                        arbitrary/compound SkPaths with their fill rule
 *                        honored), in lines or in columns.
 *   - VerticalBlockFlow  top-to-bottom CJK columns advancing right to left.
 *   - LineSetFlow        an explicit set of intervals (any origin/direction).
 *   - PathFlow           each SkPath contour becomes a line; glyphs ride the
 *                        tangent via RSXform runs.
 *
 * A contour is the geometry library's `geometry::path::Contour` — one sub-path
 * addressed by arc length — so "distance along" and "closed wraps around"
 * mean the same thing here as anywhere else a path is walked.
 *
 * Implement FlowGeometry yourself for anything else. Pass the chosen
 * geometry to layoutParagraph() in ParagraphLayout.h.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "sigilgeometry/path/Contour.h"

namespace sigil::weave {

/// Which way a flow's lines run, and therefore which way its bands stack:
/// horizontal lines stacking down the page, or top-to-bottom columns
/// advancing right to left. It is the writing mode said in the geometry's
/// own terms — a geometry never sees a paragraph — and a geometry that
/// offers both takes one of these.
enum class FlowAxis : uint8_t { kLines, kColumns };

/// One stretch of pen travel a line of text may occupy. Text is never bound
/// to a rectangle: a "line" is just an ordered list of these, and they can
/// be straight segments in any direction or spans of a path contour.
struct LineInterval {
  /// Straight form: pen starts at `origin` (a baseline point) and travels
  /// along unit vector `direction` for at most `length`.
  SkPoint origin = {0, 0};
  SkVector direction = {1, 0};  ///< unit vector of pen travel
  float length = 0;             ///< maximum pen travel, px

  /// Path form: when `contour` is valid, the pen instead travels the
  /// contour's arc length starting at `contourStart`; glyphs are rotated to
  /// the local tangent (rendered with RSXform runs). `origin`/`direction`
  /// are ignored. The contour is the geometry library's: build one with
  /// `geometry::path::Contour::of(path)`; a default-constructed one is "no
  /// contour" and leaves the interval straight.
  geometry::path::Contour contour;
  float contourStart = 0;  ///< arc length where the pen enters the contour

  /// Contour intervals only: WRAP at the contour's ends rather than stop at
  /// them, so the pen may run round the loop forever. A contour the path
  /// flagged closed wraps on its own; this is for one that is closed in
  /// GEOMETRY without being flagged. A 359.9-degree arc is a common
  /// spelling of a ring — losing half a centred caption off it over a tenth
  /// of a degree is not a behaviour anyone wants.
  bool wrapContour = false;

  /// Contour intervals only: arc length consumed per unit of glyph advance.
  /// Compensates curvature when the glyphs' optical centers ride at a
  /// different radius than the measured baseline contour — e.g. text on the
  /// outside of a small circle reads too loose because the centers sit on a
  /// larger ring than the baseline; advanceScale = rBaseline / rCenter
  /// restores optical spacing. `length`, fitting, and alignment arithmetic
  /// all stay in unscaled advance units (set length = arcLength / scale to
  /// offer the whole contour), only the pen→arc mapping is scaled.
  ///
  /// NEGATIVE walks the contour BACKWARDS: the pen still travels forward
  /// through the text, but its arc position decreases and every glyph faces
  /// the other way. That is how a run reads right way up along the lower
  /// half of a ring — the run turns round once, rather than each letter
  /// turning over and reversing the reading order.
  float advanceScale = 1.0f;

  /** Maps a PEN COORDINATE on this interval — travel in advance units from
   * where the pen enters it — to the baseline point it lands on and the
   * unit direction it is turned to.
   *
   * THIS IS THE PLACEMENT THE LAYOUT ITSELF BAKES. It is public so that a
   * caller re-placing a transformed run at draw time reads the same
   * function the blob was built from, and the two can never disagree about
   * where a glyph on a curve belongs. Anchor the glyph's ADVANCE CENTRE at
   * the returned point (the pen coordinate for glyph i is the pen at its
   * start plus half its advance), or accented glyphs drift off the curve.
   *
   * `phase` shifts every glyph along the contour by the same arc length,
   * which is how a marquee runs without laying the paragraph out again. A
   * contour that WRAPS — flagged closed, or opted in through
   * `wrapContour` — takes the phase forever and the pen may sit anywhere;
   * one that does not clamps to its ends. `rotationSteps` snaps the
   * direction to that many directions (0 keeps it exact) — every distinct
   * rotation mints a glyph-atlas strike, so an animated curve that does not
   * snap re-rasterizes every glyph every frame.
   *
   * Returns false when the pen fell OUTSIDE a non-wrapping contour and the
   * result was clamped to its end, so a caller that would rather drop a
   * glyph than pile it on the last point can. A straight interval and a
   * wrapping contour always return true. */
  bool placeAt(float pen, float phase, int rotationSteps, SkPoint* position,
               SkVector* tangent) const;
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
                             std::vector<LineInterval>& intervals) = 0;

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
  explicit BlockFlow(const SkRect& bounds) : m_bounds(bounds) {}
  /** Returns the interval for a horizontal line band when it fits. */
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval>& intervals) override;
  /** Reports that every produced interval has the block width. */
  bool uniformIntervals() const override { return true; }

 private:
  SkRect m_bounds;
};

/// A rectangle with exclusion shapes punched out (CSS float / shape-outside
/// style). Each band subtracts every intersecting shape's extent ACROSS the
/// band, so a line — or a column — shortens, or splits into several
/// intervals, around the shapes. Shapes are cheap to move: geometry is
/// re-evaluated per layout pass.
///
/// A COLUMN IS A LINE TURNED A QUARTER TURN, and `FlowAxis` is the whole of
/// the difference: `kColumns` makes each band a top-to-bottom column, the
/// columns advancing right to left from the bounds' right edge, and reads
/// every shape's extent down the column instead of across the line. Pair it
/// with `Paragraph::setWritingMode(WritingMode::kVerticalRL)`, exactly as
/// `VerticalBlockFlow` is paired.
class ExclusionFlow : public FlowGeometry {
 public:
  /// One area text must flow around, in the same coordinate space as the
  /// line bands. Build with the fromCircle/fromRectangle/fromPath factories.
  struct Shape {
    /// Selects which of the geometry fields below are meaningful.
    enum Kind { kCircle, kRect, kPath } kind = kRect;  ///< active geometry form
    /// kCircle uses the inscribed circle of `bounds`; kPath ignores
    /// `bounds`.
    SkRect bounds = SkRect::MakeEmpty();
    float padding = 0;  ///< extra standoff around the shape

    /// kPath: any SkPath — multiple contours, curves, winding or even-odd
    /// fill (holes and concavities stay available to text). The path is
    /// flattened to polygons once and cached by its generation ID, so
    /// translating it through `pathOffset` reuses that flattening; assigning
    /// a rebuilt SkPath changes the generation ID and re-flattens on the next
    /// layout pass.
    SkPath path;
    SkPoint pathOffset = {0, 0};  ///< translation applied to `path` per pass

    /** Creates a circular exclusion inscribed in `bounds`. */
    [[nodiscard]] static Shape fromCircle(const SkRect& bounds,
                                          float padding = 0) {
      return {kCircle, bounds, padding, {}, {0, 0}};
    }
    /** Creates an axis-aligned rectangular exclusion. */
    [[nodiscard]] static Shape fromRectangle(const SkRect& bounds,
                                             float padding = 0) {
      return {kRect, bounds, padding, {}, {0, 0}};
    }
    /** Creates an exclusion from an arbitrary filled SkPath. */
    [[nodiscard]] static Shape fromPath(const SkPath& path, float padding = 0) {
      return {kPath, SkRect::MakeEmpty(), padding, path, {0, 0}};
    }
  };

  /** Creates line bands — or columns — in `bounds`, minus configured
   * shapes. */
  explicit ExclusionFlow(const SkRect& bounds,
                         FlowAxis axis = FlowAxis::kLines);
  /** Destroys private flattened-path cache entries. */
  ~ExclusionFlow() override;

  /** Returns the mutable list of shapes subtracted from each band. */
  std::vector<Shape>& shapes() { return m_shapes; }
  /** Returns the outer layout bounds. */
  const SkRect& bounds() const { return m_bounds; }
  /** Returns whether the bands are lines or columns. */
  FlowAxis axis() const { return m_axis; }

  /** Drops exclusion-created slivers (intervals shorter than
   * `minimumWidth` of pen travel, in px) that would otherwise appear
   * between shapes. Defaults to 8 px.
   */
  void setMinIntervalWidth(float minimumWidth) {
    m_minIntervalWidth = minimumWidth;
  }

  /** Produces the remaining intervals of one line band or column. */
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval>& intervals) override;

 private:
  struct FlatPath;   ///< flattened-polygon cache entry (Flow.cpp)
  struct PathCache;  ///< private container type; keeps hash-map deps in
                     ///< Flow.cpp
  const FlatPath& flattenedPathFor(const SkPath& path);

  SkRect m_bounds;
  FlowAxis m_axis = FlowAxis::kLines;
  std::vector<Shape> m_shapes;
  float m_minIntervalWidth = 8;
  std::unique_ptr<PathCache> m_pathCache;
};

/// Vertical-RL block (CJK book layout): each "line" is a top-to-bottom
/// column, columns advancing right to left. `lineHeight` is the column
/// pitch; the interval origin sits on the column's central axis, which is
/// what vertical-shaped glyphs centre themselves on (`ascent` is unused).
/// Pair with Paragraph::setWritingMode(WritingMode::kVerticalRL).
class VerticalBlockFlow : public FlowGeometry {
 public:
  /** Creates top-to-bottom columns advancing right-to-left in `bounds`. */
  explicit VerticalBlockFlow(const SkRect& bounds) : m_bounds(bounds) {}
  /** Returns the interval for one vertical column when it fits. */
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval>& intervals) override;
  /** Reports that every produced column has the block height. */
  bool uniformIntervals() const override { return true; }

 private:
  SkRect m_bounds;
};

/// Fully explicit geometry: the caller supplies every line's intervals —
/// arbitrary positions, directions, and counts. Use it when the text should
/// land on shapes the block geometries cannot express: scattered labels,
/// hand-placed captions, one interval per animated slot.
class LineSetFlow : public FlowGeometry {
 public:
  /** Creates an initially empty explicit geometry. */
  LineSetFlow() = default;
  /** Takes ownership of every caller-specified line interval. */
  explicit LineSetFlow(std::vector<std::vector<LineInterval>> lines)
      : m_lines(std::move(lines)) {}

  /** Returns the mutable explicit line collection. */
  std::vector<std::vector<LineInterval>>& lines() { return m_lines; }

  /** Copies the requested explicit line into `intervals`. */
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval>& intervals) override;

 private:
  std::vector<std::vector<LineInterval>> m_lines;
};

/// Each contour of each path becomes one line; glyphs follow the curve.
class PathFlow : public FlowGeometry {
 public:
  /** Measures every contour of `path` as a separate line. */
  explicit PathFlow(const SkPath& path);
  /** Appends every contour of another path as additional lines. */
  void addPath(const SkPath& path);

  /** Returns the measured contour interval at `index`. */
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval>& intervals) override;

 private:
  std::vector<geometry::path::Contour> m_contours;
};

}  // namespace sigil::weave
