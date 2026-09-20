#pragma once

/** @file
 * @ingroup weave-layout
 *
 * What a layout pass leaves behind, run by run: a PositionedRun is one draw
 * call — a shared word blob at an origin, or a fully positioned RSXform
 * blob — together with where in the paragraph and on the geometry it came
 * from. LineMetrics and ColumnMetrics are the per-line and per-column bands
 * derived from those runs on demand, for selection bands, line backgrounds
 * and hit-testing.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTextBlob.h>

#include <cstdint>

#include "sigilweave/fonts/Shaper.h"

namespace sigil::weave {

/** HOW A JUSTIFIED LINE RESPACED THE GLYPHS OF A RUN: extra advance
 *  after every glyph, and a horizontal scale on the glyphs themselves.
 *  The identity is neither, which every run of a line fitted on its word
 *  gaps alone carries, and it is baked into the run's blob.
 *  @trap A caller that reads the glyphs BACK must apply it, or it reads
 *  the shaper's positions instead of the ones the line was set at. */
struct GlyphFit {
  float letterSpacing = 0;  ///< px added after each glyph
  float glyphScale = 1.0f;  ///< horizontal scale on the glyphs
  [[nodiscard]] bool plain() const {
    return letterSpacing == 0 && glyphScale == 1.0f;
  }
  /** The advance a shaped run of @p glyphCount glyphs takes under this
   *  fit, from the advance the shaper gave it. */
  [[nodiscard]] float advanceOf(float shapedAdvance, size_t glyphCount) const {
    return shapedAdvance * glyphScale +
           letterSpacing * static_cast<float>(glyphCount);
  }
  bool operator==(const GlyphFit&) const = default;
};

/// One draw call: a shared word blob translated to `origin`, or a fully
/// positioned RSXform blob drawn at the canvas origin. A placeholder run
/// carries no blob at all, just the flow position where the caller draws
/// its inline object. A RUN BORROWS ITS GLYPHS AND OWNS NOTHING BUT ITS
/// PLACEMENT: it is valid exactly as long as BOTH the paragraph it was
/// set from and the layout holding it are alive, and copying it out of
/// the layout does not extend that.
struct PositionedRun {
  sk_sp<SkTextBlob> blob;  ///< null for placeholder runs
  /// Glyph source (batched drawing, choreography) — BORROWED, see above.
  /// Null on a placeholder run.
  const ShapedWord* shaped = nullptr;
  SkPoint origin = {0, 0};    ///< draw position; already baked into
                              ///< transformed blobs
  uint32_t styleIndex = 0;    ///< paint lookup into Paragraph::spans()
  uint32_t wordIndex = 0;     ///< which Word produced this run
  int lineIndex = 0;          ///< 0-based flow line the run landed on
  bool transformed = false;   ///< RSXform blob (positions baked into the blob)
  int placeholderIndex = -1;  ///< \>= 0: index into Paragraph::placeholders()
  /// Which flow interval this run landed on — an index into
  /// ParagraphLayout::intervals. With `penOffset` it is the whole of what a
  /// caller needs to re-place a transformed run at draw time: the geometry
  /// it was placed on, and where along that geometry its pen started.
  int intervalIndex = -1;
  float penOffset = 0;  ///< pen travel at the run's start, in advance units
  /// THE ADVANCE THIS RUN TOOK WHERE IT LANDED, which is the shaped
  /// advance except on a justified line that spent letter spacing or a
  /// glyph scale: those are the line's answer and not the face's, so the
  /// shaped word cannot report them and anything measuring a line reads
  /// them from here.
  float advance = 0;
  /// What the line's fit did to this run's glyphs — identity on every line
  /// fitted on its word gaps alone.
  GlyphFit fit;
};

/// Geometry of one laid-out line, derived on demand from its placed runs
/// (ParagraphLayout::lineMetrics). The extent is the advance extent of what
/// actually landed — selection bands, line backgrounds, and line hit-testing
/// live here — not the flow interval's full measure (query the FlowGeometry
/// itself for raw interval geometry).
struct LineMetrics {
  int lineIndex = 0;       ///< matches PositionedRun::lineIndex
  float baseline = 0;      ///< baseline y shared by the line's runs
  float ascent = 0;        ///< tallest ascent above the baseline (positive)
  float descent = 0;       ///< deepest descent below the baseline (positive)
  float left = 0;          ///< leftmost run origin
  float right = 0;         ///< rightmost run end (advance extent)
  uint32_t textBegin = 0;  ///< first UTF-16 unit placed on the line
  uint32_t textEnd = 0;    ///< one past the last unit, trailing glue included

  /** Returns the line's bounding band (ascent above to descent below). */
  [[nodiscard]] SkRect rect() const {
    return SkRect::MakeLTRB(left, baseline - ascent, right, baseline + descent);
  }
};

/// Geometry of one laid-out COLUMN of a vertical paragraph — the
/// counterpart of LineMetrics, derived on demand from the placed runs. A
/// column has no baseline: its reading axis is y and every form centres
/// itself ACROSS the column's central axis, so the band is that axis plus
/// the flow's own column pitch.
struct ColumnMetrics {
  int lineIndex = 0;       ///< matches PositionedRun::lineIndex
  float axis = 0;          ///< the column's central axis, x
  float pitch = 0;         ///< the column band's width (the flow's line pitch)
  float top = 0;           ///< first placed pen position down the column
  float bottom = 0;        ///< one past the last, trailing glue excluded
  uint32_t textBegin = 0;  ///< first UTF-16 unit placed in the column
  uint32_t textEnd = 0;    ///< one past the last unit, trailing glue included

  /** Returns the column's bounding band (half the pitch either side). */
  [[nodiscard]] SkRect rect() const {
    return SkRect::MakeLTRB(axis - pitch * 0.5f, top, axis + pitch * 0.5f,
                            bottom);
  }
};

}  // namespace sigil::weave
