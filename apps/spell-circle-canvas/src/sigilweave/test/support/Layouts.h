#pragma once

/** @file
 * The layout entry point, a run's exact end position, a circle as one
 * contour, the two-word setting a decoration band is read in, and the
 * readings every test binary takes off a finished layout: which runs
 * placed glyphs, where each line ended, how wide each one is, how many
 * glyphs were placed, and whether every run stayed inside an interval its
 * own band offered.
 *
 * Nothing here calls a GoogleTest assertion, so a benchmark can include it
 * for the same readings without linking a test framework.
 */

#include <include/core/SkPathBuilder.h>
#include <sigilweave/layout/ParagraphLayout.h>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "Faces.h"
#include "Paragraphs.h"

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

/// Glyphs a layout draws: the sum over its runs, placeholders excluded.
inline int64_t glyphCount(const ParagraphLayout& layout) {
  int64_t glyphs = 0;
  for (const PositionedRun& run : layout.runs)
    if (run.shaped) glyphs += (int64_t)run.shaped->glyphs.size();
  return glyphs;
}

/// The runs that placed glyphs, in the order the layout emitted them.
/// A placeholder reserves room without shaping anything, so it is not one.
inline std::vector<const PositionedRun*> wordRuns(
    const ParagraphLayout& layout) {
  std::vector<const PositionedRun*> placed;
  for (const PositionedRun& run : layout.runs)
    if (run.shaped) placed.push_back(&run);
  return placed;
}

/// A single-span paragraph laid out in `flow`, with the paragraph beside
/// it: every index a layout reports is an index into that paragraph, so
/// the two are one answer.
struct LaidOut {
  Paragraph paragraph;
  ParagraphLayout layout;
};

inline LaidOut laidOut(std::u8string_view utf8, float fontSize,
                       FlowGeometry& flow) {
  Paragraph paragraph = makeParagraph(utf8, fontSize);
  ParagraphLayout layout = layoutParagraph(sigil::test::fonts(), paragraph, flow);
  return {std::move(paragraph), std::move(layout)};
}

/// Two words at 32px on one line of a 400x80 block. A band's whole
/// question is what it does at the glue between two words, so this is the
/// setting every claim about one is read in — as geometry in the
/// decoration feature, and as pixels in the paint feature.
inline LaidOut twoWordsOnOneLine() {
  BlockFlow flow(SkRect::MakeWH(400, 80));
  return laidOut(u8"mono nano", 32.0f, flow);
}

/// How far along the pen each line reached, ascending by line index and
/// sized to the layout's line count: the rightmost end of any run on it,
/// which is what a justified or clamped line is measured against.
inline std::vector<float> lineEnds(const ParagraphLayout& layout,
                                   const Paragraph& paragraph) {
  std::vector<float> ends(static_cast<size_t>(std::max(layout.lineCount, 0)),
                          0.0f);
  for (const PositionedRun& run : layout.runs) {
    const size_t line = static_cast<size_t>(run.lineIndex);
    if (line < ends.size())
      ends[line] = std::max(ends[line], runEnd(paragraph, run));
  }
  return ends;
}

/// The extent of each placed line — its rightmost end less its leftmost
/// start — ascending by line index, for the lines that actually carry runs.
inline std::vector<float> lineWidths(const ParagraphLayout& layout,
                                     const Paragraph& paragraph) {
  std::vector<std::pair<int, std::pair<float, float>>> byLine;
  for (const PositionedRun& run : layout.runs) {
    const float left = run.origin.x();
    const float right = runEnd(paragraph, run);
    auto found = std::find_if(
        byLine.begin(), byLine.end(),
        [&](const auto& entry) { return entry.first == run.lineIndex; });
    if (found == byLine.end())
      byLine.emplace_back(run.lineIndex, std::make_pair(left, right));
    else {
      found->second.first = std::min(found->second.first, left);
      found->second.second = std::max(found->second.second, right);
    }
  }
  std::sort(byLine.begin(), byLine.end());
  std::vector<float> widths;
  for (const auto& [line, extent] : byLine)
    widths.push_back(extent.second - extent.first);
  return widths;
}

/// Which axis a flow's pen travels on: a line advances along x, a column
/// down y. The same containment question is asked either way; only the
/// coordinate the spans are read on changes.
enum class PenAxis { kAlongLines, kDownColumns };

/// What reading a layout against its own geometry found.
struct IntervalContainment {
  int runs = 0;          ///< placed runs whose band was read
  int splitBands = 0;    ///< bands that handed back more than one interval
  int outside = 0;       ///< runs sitting inside none of their band's intervals
  int exhausted = 0;     ///< runs on a band the geometry refused to answer for
  int outsideBand = -1;  ///< the band the first stray run was on
  float outsideStart = 0.0f, outsideEnd = 0.0f;  ///< and the span it covered
};

/// Every placed run sits inside one of the intervals its own band offered —
/// the invariant that separates text flowing AROUND a shape from text
/// placed across it. The tolerance is three quarters of a pixel, the same
/// slack a line-edge check allows a rounded advance.
inline IntervalContainment runsStayInsideIntervals(
    FlowGeometry& flow, const ParagraphLayout& layout, float pitch,
    float ascent, PenAxis axis) {
  IntervalContainment found;
  const bool columns = axis == PenAxis::kDownColumns;
  std::vector<LineInterval> intervals;
  for (const PositionedRun& run : layout.runs) {
    if (!run.shaped) continue;
    ++found.runs;
    if (!flow.lineIntervals(run.lineIndex, pitch, ascent, intervals)) {
      ++found.exhausted;
      continue;
    }
    if (intervals.size() > 1) ++found.splitBands;
    const float start = columns ? run.origin.y() : run.origin.x();
    const float end = start + run.shaped->advance;
    bool inside = false;
    for (const LineInterval& interval : intervals) {
      const float open = columns ? interval.origin.y() : interval.origin.x();
      inside = inside ||
               (start >= open - 0.75f && end <= open + interval.length + 0.75f);
    }
    if (inside) continue;
    if (found.outside == 0) {
      found.outsideBand = run.lineIndex;
      found.outsideStart = start;
      found.outsideEnd = end;
    }
    ++found.outside;
  }
  return found;
}

}  // namespace sigil::weave::test
