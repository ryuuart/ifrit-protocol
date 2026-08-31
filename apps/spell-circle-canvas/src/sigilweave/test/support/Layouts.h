#pragma once

/** @file
 * The layout entry point, a run's exact end position, and a circle as one
 * contour, for every test binary that places paragraphs and checks where
 * their runs land.
 */

#include <include/core/SkPathBuilder.h>
#include <sigilweave/layout/ParagraphLayout.h>

#include <utility>
#include <vector>

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

/// A closed circle as one contour, plus its length.
inline std::pair<sigil::geometry::path::Contour, float> circleContour(
    float radius) {
  SkPathBuilder builder;
  builder.addCircle(0, 0, radius);
  std::vector<sigil::geometry::path::Contour> contours =
      sigil::geometry::path::Contour::of(builder.detach());
  if (contours.empty()) return {sigil::geometry::path::Contour{}, 0.0f};
  return {contours.front(), contours.front().length()};
}

}  // namespace sigil::weave::test
