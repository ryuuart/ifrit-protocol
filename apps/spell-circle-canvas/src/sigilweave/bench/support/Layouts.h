#pragma once
/** @file
 * The glyph count of a finished layout, for every benchmark that reports
 * its work per glyph placed.
 */

#include <sigilweave/layout/ParagraphLayout.h>

#include <cstdint>

namespace sigil::weave::bench {

/** Glyphs a layout draws: the sum over its runs, placeholders excluded. */
inline int64_t glyphCount(const ParagraphLayout& layout) {
  int64_t glyphs = 0;
  for (const PositionedRun& run : layout.runs)
    if (run.shaped) glyphs += (int64_t)run.shaped->glyphs.size();
  return glyphs;
}

}  // namespace sigil::weave::bench
