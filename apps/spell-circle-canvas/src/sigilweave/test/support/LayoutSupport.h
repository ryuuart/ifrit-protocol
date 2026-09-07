#pragma once

/** @file
 * Support for weave_layout_test: the font context, paragraph helpers, the
 * layout API and run positions, the breaker a claim about breaking is
 * held to both of, and the three readings the paragraph controls are
 * checked through — where the baselines landed, where each line starts,
 * and the two-block fixture a block rule needs a boundary in.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "Faces.h"
#include "Layouts.h"
#include "Paragraphs.h"

namespace sigil::weave::test {

/// A claim about how text is broken into lines is a claim about both
/// breakers, so it is written once and parameterized on which one decides.
/// A file states its own suite over this base and instantiates it with
/// `bothBreakers`, so a failure line names the breaker that broke.
class BrokenBothWays : public ::testing::TestWithParam<LineBreakStrategy> {
 protected:
  LineBreakStrategy breaker() const { return GetParam(); }
};

/// The rows a `BrokenBothWays` suite is instantiated over.
inline auto bothBreakers() {
  return ::testing::Values(LineBreakStrategy::kGreedy,
                           LineBreakStrategy::kKnuthPlass);
}

/// The row name for a breaker parameter.
inline std::string breakerName(
    const ::testing::TestParamInfo<LineBreakStrategy>& info) {
  return info.param == LineBreakStrategy::kGreedy ? "Greedy" : "KnuthPlass";
}

/// Ascending distinct baselines of the placed lines.
inline std::vector<float> baselines(const ParagraphLayout& layout) {
  std::vector<float> found;
  for (const PositionedRun& run : layout.runs) {
    if (run.transformed) continue;
    if (std::find(found.begin(), found.end(), run.origin.y()) == found.end())
      found.push_back(run.origin.y());
  }
  std::sort(found.begin(), found.end());
  return found;
}

/// Leftmost run origin on each line, ascending by line index.
inline std::vector<float> lineStarts(const ParagraphLayout& layout) {
  std::vector<std::pair<int, float>> byLine;
  for (const PositionedRun& run : layout.runs) {
    auto found = std::find_if(byLine.begin(), byLine.end(),
                              [&](const std::pair<int, float>& entry) {
                                return entry.first == run.lineIndex;
                              });
    if (found == byLine.end())
      byLine.emplace_back(run.lineIndex, run.origin.x());
    else
      found->second = std::min(found->second, run.origin.x());
  }
  std::sort(byLine.begin(), byLine.end());
  std::vector<float> starts;
  for (const auto& [line, start] : byLine) starts.push_back(start);
  return starts;
}

/// Two blocks, the second after a hard break.
inline Paragraph twoBlocks() {
  return makeParagraph(
      u8"The first block runs on for several words so that it wraps.\n"
      u8"The second block does the same and wraps as well.",
      16.0f);
}

}  // namespace sigil::weave::test
