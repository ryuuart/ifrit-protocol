#pragma once

/** @file
 * TextState — what dressed type keeps on a node between frames: the run
 * broken across a baseline's contours, the per-glyph selection a selector
 * answers, and the axis tracks a restyle carries instead of re-shaping.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkSize.h>
#include <sigilweave/layout/ParagraphLayout.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "ComposeInternal.h"

namespace sigil::compose::detail {

/** THE TEXT ENGINE'S STATE ON A NODE — what dressed type keeps between
 *  frames beyond the paragraph and the flow layout the kernel measures by.
 *  Held by the instance through one pointer and created the first time an
 *  engine operation touches the node, so text drawn at rest costs none of
 *  it. Written and read by the text painter a description carries; the
 *  kernel itself reads one field, the folded axis tracks, because they are
 *  tracks the painter draws and volatility counts. */
struct TextState {
  // onPath(): the run broken across the baseline's contours, and the
  // geometry it was broken across. A SECOND layout beside `textLayout`
  // rather than a replacement for it — `textLayout` is still the node's
  // MEASURE, the run laid straight, and the box it measures is what the
  // baseline is resolved against. Rebuilt when the content, the box or the
  // baseline value changes; the `at` phase is applied at PAINT and never
  // touches it.
  sigil::weave::ParagraphLayout pathLayout;
  std::vector<sigil::weave::LineInterval> pathIntervals;
  float pathTotalLength = 0;   // every contour's arc length together
  float pathRestAt = 0;        // the `at` the layout's entry point baked in
  SkPoint pathCentroid{0, 0};  // Orient::Radial's centre
  bool pathValid = false;
  uint32_t pathRev = ~0u;            // contentRev the path layout belongs to
  SkSize pathSize = {-1, -1};        // the box the baseline resolved against
  std::optional<TextPath> pathSpec;  // the value it was built from

  // ---- fx() selection, resolved once per (content, layout, selector) -------
  //
  // A selector answers one byte per glyph, and answering it can mean an ICU
  // regular expression over the whole paragraph. That is a per-EDIT cost,
  // not a per-frame one: the masks below are rebuilt when the text changes,
  // when the layout reflows (a line selector moves with the break), or when
  // the description's selectors themselves change.
  std::vector<sigil::weave::Selector> selectionKeys;
  std::vector<std::vector<uint8_t>> selectionMasks;
  uint32_t selectionRev = ~0u;
  // BOTH MEASURES LAYOUT KEYS ON: a line selector moves with the break,
  // and a vertical or depth-bounded passage breaks on its height exactly
  // as a horizontal one breaks on its width.
  float selectionWidth = -1.0f;
  float selectionHeight = -1.0f;
  // spanStyle() restyles that differ from the text they cover ONLY in
  // advance-invariant variable-font axes, carried as tracks instead of
  // re-shaping: the paragraph keeps the glyphs and pen positions it shaped,
  // and the coordinate reaches the glyphs at draw time exactly as a driven
  // axis does. Decided against the materialized paragraph, which is why the
  // list lives here and not on the description, and rebuilt with it. Drawn
  // AFTER the description's own tracks, so the painter's selection and
  // track lists are the description's tracks followed by these.
  std::vector<Track> spanAxisTracks;

  // ---- what an ink-only repaint needs ----------------------------------
  //
  // The ranges each spanPaint()/spanStyle() restyle resolved to when the
  // paragraph was materialised, in declaration order, and which of them
  // were folded into axis tracks rather than applied. An inheriting leaf
  // whose ink changes sets the new colour on its inherited ranges in place
  // and then replays these paints over them, so the restyles stand exactly
  // as materialisation left them without a line being broken again.
  std::vector<std::vector<sigil::weave::CharRange>> restyleRanges;
  std::vector<bool> restyleFolded;
};

}  // namespace sigil::compose::detail
