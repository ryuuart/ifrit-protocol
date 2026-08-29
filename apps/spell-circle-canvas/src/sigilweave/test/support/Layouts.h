#pragma once

/** @file
 * The layout entry point and a run's exact end position, for every test
 * binary that places paragraphs and checks where their runs land.
 */

#include <sigilweave/ParagraphLayout.h>

namespace sigil::weave::test {

/// Exact pen x where a run ends. Blob ink bounds are conservative
/// (font-bounds based), so line-edge checks use shaped advances instead.
/// Each run is one word *segment* (multi-segment words emit several runs,
/// each offset by its own advanceOffset), so use the segment's shaped
/// advance.
inline float runEnd(const Paragraph& paragraph, const PositionedRun& run) {
  if (run.shaped) return run.origin.x() + run.shaped->advance;
  return run.origin.x() + paragraph.words()[run.wordIndex].width;
}

}  // namespace sigil::weave::test
