#pragma once

/** @file
 * SigilCompose text painter — THE SEAM THE KERNEL DRAWS DRESSED TYPE
 * THROUGH: every operation the composer asks of text that is not simply
 * resting on its own straight baseline, as a value a text verb installs
 * on a description. The kernel holds the paragraph, lays it out and draws
 * it at rest by itself; the typography feature implements this and
 * registers itself as the engine. The vocabulary the operations are
 * spelled in — the selector, the unit, the beat, the reading, the path —
 * is that feature's, under <sigilcompose/typography/>, and is only named
 * here.
 */

#include <sigilcore/comparable/Erased.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/layout/PositionedRun.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/PaintStyle.h>
#include <sigilweave/style/TextStyle.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::weave {
class FontContext;
}  // namespace sigil::weave

class SkCanvas;
struct SkSize;

namespace sigil::compose {

struct PaintContext;
// The typography vocabulary the seam is spelled in, defined under
// <sigilcompose/typography/>: one unit as the layout placed it, one beat
// of a cascade, a reading beside the type, and a run's path baseline.
struct TextUnit;
struct Beat;
struct Annotation;
struct TextPath;
namespace detail {
struct Instance;
}  // namespace detail

namespace detail {
/** ONE `weave::rich()` RUN THAT WAS WRITTEN UNDER A STYLE NAME, and the text it
 *  occupies — what `sel::style` resolves against.
 *
 *  The name is tied to the run's TEXT rather than to the style span it
 *  produced, and that is the whole reason the answer holds up. Spans are
 *  cut and merged by every `spanPaint` and `spanStyle` the leaf declares,
 *  so a span index is a number about the paragraph's current normal form;
 *  a run's extent is a fact about the content that only new content
 *  changes. Re-registering the name against a different style, or a restyle
 *  slicing across the run, leaves this untouched.
 *
 *  Built as the runs are appended, in declaration order. Empty for every
 *  content form that carries no names. */
struct NamedRun {
  std::string name;
  sigil::weave::CharRange chars;
};

/** WHERE A LEAF STANDS IN ITS STORY — what makes `weave::sel::line` address the
 *  story and `sel::inFrame` address one frame of it.
 *
 *  A story's words, characters, sentences and named runs are the story's
 *  already: every frame of a chain builds the whole story's paragraph and
 *  resumes at a word, so those numbers never were the frame's. The LINE is
 *  the one address that was, and the offset is what turns it back. A leaf
 *  that is not a frame carries a zero offset and its own key, so the
 *  ordinary case is the general one with nothing subtracted. */
struct TextScope {
  uint32_t lineOffset = 0;    ///< story line index of this leaf's line 0
  uint32_t storyLines = 0;    ///< lines the whole chain placed; 0 if not one
  bool inChain = false;       ///< this leaf is one frame of several
  std::string_view frameKey;  ///< this leaf's key, for sel::inFrame
};
}  // namespace detail

/** WHAT THE KERNEL ASKS OF DRESSED TYPE — every operation the composer
 *  needs from text that is not simply resting on its own straight baseline:
 *  a run carrying fx() tracks, riding a path, anchoring marks, or restyled
 *  by selector. The kernel holds the paragraph, lays it out and draws it at
 *  rest by itself; everything below is answered by the value a text verb
 *  installs on the description (`fx()`, `onPath()`, `mark()`, `spanStyle()`,
 *  `spanPaint()`, `variationDrive()`). A text node carrying none of those
 *  has no painter, and the kernel then draws its paragraph at rest, resolves
 *  no marks and restyles nothing — the same picture a painter would draw for
 *  a description with nothing to dress.
 *
 *  The instance handed in is the kernel's retained node for the text; the
 *  painter reads its paragraph and layout and keeps its own engine state on
 *  it. */
class TextPainterOps {
 public:
  virtual ~TextPainterOps() = default;
  /** THE GLYPH DRAW for dressed text: the rest pose comes from the baseline
   *  — level on a plain run, on the curve and turned to it on a path run —
   *  and every fx() track's deviation applies on top of it. @p override is
   *  the glyph-paint override textFill()/textStroke() ask for, or null;
   *  @p onPath is null for text with no baseline path; @p size is the
   *  node's box; @p ctx is the node's paint context. */
  virtual void paint(detail::Instance& inst, SkCanvas& canvas,
                     const sigil::weave::PaintStyle* override,
                     const TextPath* onPath, SkSize size,
                     const PaintContext& ctx) const = 0;
  /** WHERE EACH mark() ANCHORS: refills the instance's mark rects from the
   *  layout the letters are drawn from, one rect per anchor. */
  virtual void marks(detail::Instance& inst) const = 0;
  /** WHERE THE UNITS A SELECTOR ADDRESSES LANDED, one entry each, read off
   *  the same layout the letters are drawn from — the query behind
   *  `Composer::units`. */
  virtual std::vector<TextUnit> units(detail::Instance& inst,
                                      const sigil::weave::Selector& selector,
                                      sigil::weave::Unit unit) const = 0;
  /** LAYS OUT EVERY READING this text carries against the layout its
   *  letters are drawn from, and leaves the results on the instance for
   *  the kernel to draw. */
  virtual void annotations(detail::Instance& inst) const = 0;
  /** THE BAND THIS TEXT'S RESERVING ANNOTATIONS NEED, from their own
   *  metrics alone — asked BEFORE the text is laid out, which is what
   *  makes a reservation a layout input rather than a cycle. */
  virtual sigil::weave::ReservedBand reservedBand(
      detail::Instance& inst,
      std::span<const Annotation> annotations) const = 0;
  /** WHICH TEXT A SELECTOR ADDRESSES, as UTF-16 ranges — sorted, merged,
   *  non-overlapping. `weave::sel::line` reads @p lines, or @p columns where
   * the passage is vertical; `sel::style` reads @p named. */
  virtual std::vector<sigil::weave::CharRange> ranges(
      const sigil::weave::Selector& selector,
      sigil::weave::Paragraph& paragraph, sigil::weave::FontContext& fonts,
      std::span<const sigil::weave::LineMetrics> lines,
      std::span<const sigil::weave::ColumnMetrics> columns,
      std::span<const detail::NamedRun> named,
      detail::TextScope scope) const = 0;
  /** Whether a restyle to @p style over @p ranges can be carried as
   *  draw-time axis tracks instead of re-shaping the text it covers: the
   *  style must differ from every covered span's only in variable-font
   *  axes, drop none the text was shaped with, and every axis it moves must
   *  be advance-invariant on that span's face. On success @p axes holds one
   *  (tag, coordinate) per axis that actually changes.
   *
   *  @p paintCarried is the text whose PAINT an earlier declaration owns.
   *  A span lying wholly inside it is compared on its other dimensions
   *  alone, because the fold writes no paint at all: the colour standing
   *  there is the one that is meant to stand, so a difference between it
   *  and @p style's own paint is not a reshape. */
  virtual bool foldable(
      detail::Instance& inst, const sigil::weave::TextStyle& style,
      std::span<const sigil::weave::CharRange> ranges,
      const sigil::weave::Paragraph& paragraph,
      std::span<const sigil::weave::CharRange> paintCarried,
      std::vector<std::pair<std::string, float>>& axes) const = 0;
  /** THE SCHEDULE ONE TRACK IS RUNNING, resolved against the layout the
   *  last draw produced; rects in the node's own space. */
  virtual std::vector<Beat> beats(detail::Instance& inst,
                                  size_t trackIndex) const = 0;
  /** THE SAME SCHEDULE'S WHOLE VIRTUAL SPAN in ms; 0 wherever beats()
   *  answers empty. */
  virtual float cascadeSpanMs(detail::Instance& inst,
                              size_t trackIndex) const = 0;
};

/** The painter as a description carries it: a comparable value, excluded
 *  from structural equality because it is the same engine on every text
 *  that has one. */
using TextPainter = core::Erased<TextPainterOps>;

namespace detail {
/** THE ENGINE WITHOUT A DESCRIPTION TO CARRY IT. A text leaf installs the
 *  painter when it dresses its type, and one that dresses nothing has
 *  none — which is right for drawing, and wrong for a query that asks
 *  where a plain passage's words landed. The typography tier registers
 *  itself here as it is linked in, and the read-back queries fall through
 *  to it. Null in a program that links the kernel without that tier, where
 *  those queries answer empty, as an unknown key does. */
void registerTextEngine(const TextPainterOps* engine);
[[nodiscard]] const TextPainterOps* registeredTextEngine();
}  // namespace detail

}  // namespace sigil::compose
