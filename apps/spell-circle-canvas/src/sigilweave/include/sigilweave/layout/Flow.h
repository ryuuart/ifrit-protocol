#pragma once

/** @file
 * @ingroup weave-geometry
 *
 * The geometry a paragraph flows into. Text is never bound to a rectangle:
 * a "line" is an ordered list of LineIntervals — straight segments in any
 * direction, or spans of an SkPath contour — supplied one line at a time
 * by a FlowGeometry, of which BlockFlow, ExclusionFlow, VerticalBlockFlow,
 * LineSetFlow and PathFlow are the ready-made ones. Implement the
 * interface for anything else, and pass the geometry to layoutParagraph().
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "sigilgeometry/path/Contour.h"

class SkImage;

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

  /// Contour intervals only: arc length consumed per unit of glyph
  /// advance, which compensates curvature when the glyphs' optical centres
  /// ride at a different radius than the measured baseline contour. Only
  /// the pen-to-arc mapping is scaled; `length`, fitting and alignment
  /// stay in unscaled advance units. NEGATIVE walks the contour backwards,
  /// which is how a run reads right way up along a ring's lower half.
  float advanceScale = 1.0f;

  /** Maps a PEN COORDINATE — travel in advance units from where the pen
   * enters this interval — to the baseline point it lands on and its unit
   * direction. @p phase shifts along the contour by an arc length;
   * @p rotationSteps snaps the direction, 0 keeping it exact. False when
   * the pen fell OUTSIDE a non-wrapping contour and was clamped to its end.
   * @trap Anchor the glyph's ADVANCE CENTRE there, or accents drift. */
  bool placeAt(float pen, float phase, int rotationSteps, SkPoint* position,
               SkVector* tangent) const;
};

/// ONE BAND ASKED OF A GEOMETRY, and everything about it the band's number
/// alone does not say. `bandStart` is why it is a value rather than three
/// arguments: bands stack at distances the LAYOUT accumulates, not at
/// `index · lineHeight`, as soon as a text's blocks lead differently. The
/// block context is for a geometry that wants it — a frame grid, a well
/// cut for one block, a drop cap's notch — and every stock one ignores it.
struct LineRequest {
  int index = 0;         ///< 0-based band ordinal, ascending without gaps
  float bandStart = 0;   ///< the band's near edge, along the stacking axis,
                         ///< from the flow's own start edge
  float lineHeight = 0;  ///< the band's thickness across that axis
  float ascent = 0;      ///< the baseline's offset below the near edge
  int blockIndex = 0;    ///< which block of the text is being set here
  int lineInBlock = 0;   ///< the band's 0-based place inside that block
};

/// Supplies the intervals available to each successive line. Implementations
/// are queried per layout pass (they may depend on animated state like
/// moving exclusion shapes); the layout never caches geometry between
/// passes.
class FlowGeometry {
 public:
  virtual ~FlowGeometry() = default;

  /** Returns the intervals available in `request`'s band. Returns false when
   * the geometry is exhausted (the band lies past its end); an empty
   * `intervals` with a true return means "this band has no room, try the
   * next one".
   */
  virtual bool lineIntervals(const LineRequest& request,
                             std::vector<LineInterval>& intervals) = 0;

  /** Sugar for a caller with no block model: bands stacked at
   * `index · lineHeight`, which is where a passage of one pitch puts them.
   */
  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval>& intervals) {
    return lineIntervals(
        LineRequest{index, static_cast<float>(index) * lineHeight, lineHeight,
                    ascent, 0, index},
        intervals);
  }

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
  using FlowGeometry::lineIntervals;
  /** Returns the interval for a horizontal line band when it fits. */
  bool lineIntervals(const LineRequest& request,
                     std::vector<LineInterval>& intervals) override;
  /** Reports that every produced interval has the block width. */
  bool uniformIntervals() const override { return true; }

 private:
  SkRect m_bounds;
};

/// ONE STRETCH OF A BAND a flow shape occupies, measured along the flow's
/// own axis — the units a line's pen travels in, and a column's.
struct Span {
  float start = 0;
  float end = 0;
};

/// ONE BAND, measured ACROSS the flow: where a line's own depth begins and
/// ends, which is where a column's width begins and ends when the flow is
/// turned a quarter turn.
struct Band {
  float start = 0;
  float end = 0;
};

/// A SHAPE TEXT STANDS OFF. One question: which stretches of a band
/// this shape occupies, along the flow axis — the same shape of answer for
/// a rectangle, a photograph's alpha and anything a caller writes, which
/// is why there is no kind to switch on. THE MARGIN IS A DISC AND NOT A
/// SQUARE, and every implementation owes that meaning. A flow shape
/// caches what answering costs it, so one belongs to one flow at a time.
class FlowShape {
 public:
  virtual ~FlowShape() = default;
  /** Appends the stretches of @p band that this shape occupies, dilated by
   * @p margin, which is never negative. The spans need not be sorted or
   * disjoint; the flow merges them. */
  virtual void bandSpans(FlowAxis axis, Band band, float margin,
                         std::vector<Span>& spans) = 0;
  /** The shape's own extent, margin excluded. */
  virtual SkRect bounds() const = 0;
};

/// ONE AREA TEXT FLOWS AROUND: a shape, how far the text stands off it,
/// and where it has moved to since. `offset` is rigid motion and costs the
/// shape nothing — the marquee, the drifting figure, the parallax
/// photograph — where a rebuilt shape re-answers from scratch.
struct Exclusion {
  std::shared_ptr<FlowShape> shape;
  float margin = 0;         ///< the standoff, px, as a disc; never negative,
                            ///< and a negative one is read as none
  SkPoint offset = {0, 0};  ///< translation applied per layout pass
};

/// The stock flow shapes. A caller with a shape none of these describes
/// implements FlowShape itself and stands beside them.
namespace flowshape {

/** An axis-aligned rectangle. Its margin rounds the corners, exactly as a
 * disc offset does. */
[[nodiscard]] std::shared_ptr<FlowShape> rectangle(const SkRect& bounds);
/** The circle INSCRIBED in `bounds`, answered analytically: one square root
 * a band, and the margin is simply a larger radius. */
[[nodiscard]] std::shared_ptr<FlowShape> circle(const SkRect& bounds);
/** The oval inscribed in `bounds` — the circle above when it is round, and
 * otherwise the oval's own path, because a disc offset of an ellipse is not
 * an ellipse and only the path answer stays exact. */
[[nodiscard]] std::shared_ptr<FlowShape> ellipse(const SkRect& bounds);
/** Any filled SkPath — several contours, curves, winding or even-odd fill,
 * so holes and concavities stay available to text. Flattened once and
 * kept, and read exactly at any margin. An inverse fill type is read as
 * its own non-inverse self: a flow shape is the region the path encloses.
 */
[[nodiscard]] std::shared_ptr<FlowShape> path(const SkPath& path);
/** AN IMAGE'S OWN ALPHA, resolved inside @p box in flow coordinates: a
 * pixel is inside where its alpha is greater than @p threshold, a fraction
 * of full opacity, so a soft edge admits words further in as the tolerance
 * rises. A new frame re-thresholds and re-measures. */
[[nodiscard]] std::shared_ptr<FlowShape> coverage(sk_sp<SkImage> image,
                                                  const SkRect& box,
                                                  float threshold = 0.5f);

}  // namespace flowshape

/// A rectangle with exclusions punched out (CSS float / shape-outside
/// style). Each band subtracts every intersecting flow shape's extent
/// ACROSS the band, so a line — or a column — shortens, or splits into
/// several intervals, around them. Exclusions are cheap to move: geometry
/// is re-evaluated per layout pass. A COLUMN IS A LINE TURNED A QUARTER
/// TURN, and `FlowAxis` is the whole of the difference; pair `kColumns`
/// with `Paragraph::setWritingMode`, as `VerticalBlockFlow` is paired.
class ExclusionFlow : public FlowGeometry {
 public:
  /** Creates line bands — or columns — in `bounds`, minus configured
   * exclusions. */
  explicit ExclusionFlow(const SkRect& bounds,
                         FlowAxis axis = FlowAxis::kLines);
  ~ExclusionFlow() override;

  /** Returns the mutable list of exclusions subtracted from each band. */
  std::vector<Exclusion>& exclusions() { return m_exclusions; }
  /** Returns the outer layout bounds. */
  const SkRect& bounds() const { return m_bounds; }
  /** Returns whether the bands are lines or columns. */
  FlowAxis axis() const { return m_axis; }

  /** Drops exclusion-created slivers (intervals shorter than
   * `minimumWidth` of pen travel, in px) that would otherwise appear
   * between shapes. Defaults to 8 px.
   */
  void setMinimumIntervalWidth(float minimumWidth) {
    m_minimumIntervalWidth = minimumWidth;
  }

  using FlowGeometry::lineIntervals;
  /** Produces the remaining intervals of one line band or column. */
  bool lineIntervals(const LineRequest& request,
                     std::vector<LineInterval>& intervals) override;

 private:
  SkRect m_bounds;
  FlowAxis m_axis = FlowAxis::kLines;
  std::vector<Exclusion> m_exclusions;
  float m_minimumIntervalWidth = 8;
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
  using FlowGeometry::lineIntervals;
  /** Returns the interval for one vertical column when it fits. */
  bool lineIntervals(const LineRequest& request,
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

  using FlowGeometry::lineIntervals;
  /** Copies the requested explicit line into `intervals`. */
  bool lineIntervals(const LineRequest& request,
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

  using FlowGeometry::lineIntervals;
  /** Returns the measured contour interval at `request.index`. */
  bool lineIntervals(const LineRequest& request,
                     std::vector<LineInterval>& intervals) override;

 private:
  std::vector<geometry::path::Contour> m_contours;
};

}  // namespace sigil::weave
