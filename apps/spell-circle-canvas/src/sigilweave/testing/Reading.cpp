#include "sigilweave/testing/Reading.h"

#include <sigilweave/choreograph/PlacedGlyph.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <utility>

namespace sigil::weave::testing {

namespace {

/// The hyphen a word broken at a line's end draws, which is the word's
/// own shaped hyphen rather than one of its segments.
bool drawsHyphen(const Paragraph& paragraph, const PositionedRun& run) {
  if (!run.shaped || run.wordIndex >= paragraph.words().size()) return false;
  const Word& word = paragraph.words()[run.wordIndex];
  return word.hyphenGlyph && word.hyphenGlyph.get() == run.shaped;
}

/// A run that draws its word — a segment of it, or the slot of an inline
/// object — rather than glyphs the layout made for itself: an overflow
/// marker, a tab leader, an initial letter's cut.
bool drawsItsWord(const Paragraph& paragraph, const PositionedRun& run) {
  if (run.wordIndex >= paragraph.words().size()) return false;
  if (run.placeholderIndex >= 0) return true;
  if (!run.shaped) return false;
  for (const WordSegment& segment : paragraph.words()[run.wordIndex].segments())
    if (segment.shaped.get() == run.shaped) return true;
  return false;
}

/// Where a run starts and ends along the pen: x across a line, y down a
/// column.
std::pair<float, float> penSpan(const PositionedRun& run, bool columns) {
  const float start = columns ? run.origin.y() : run.origin.x();
  return {start, start + run.advance};
}

}  // namespace

Reading read(const Passage& passage) {
  const Paragraph& paragraph = passage.paragraph;
  const ParagraphLayout& layout = passage.layout;
  const std::vector<Word>& words = paragraph.words();
  const bool columns = paragraph.writingMode() == WritingMode::kVerticalRL;

  Reading reading;
  reading.lineCount = layout.lineCount;
  reading.overflowed = layout.overflowed();
  reading.ellipsized = layout.ellipsized;

  std::map<int, std::vector<const PositionedRun*>> runsByLine;
  for (const PositionedRun& run : layout.runs) {
    RunReading& runReading = reading.runs.emplace_back();
    runReading.wordIndex = run.wordIndex;
    runReading.lineIndex = run.lineIndex;
    runReading.origin = run.origin;
    runReading.advance = run.advance;
    runReading.glyphCount =
        run.shaped ? static_cast<uint32_t>(run.shaped->glyphs.size()) : 0u;
    runReading.transformed = run.transformed;
    runReading.hyphen = drawsHyphen(paragraph, run);
    runReading.placeholderIndex = run.placeholderIndex;
    runReading.fit = run.fit;
    runsByLine[run.lineIndex].push_back(&run);
  }

  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& placed) {
    reading.glyphs.push_back({.glyph = placed.glyph,
                              .rest = placed.rest,
                              .advance = placed.advance,
                              .textIndex = placed.textIndex,
                              .wordIndex = placed.wordIndex,
                              .lineIndex = placed.lineIndex,
                              .transformed = placed.transformed,
                              .tangent = placed.tangent});
  });

  for (uint32_t wordIndex = 0; wordIndex < words.size(); ++wordIndex)
    if (words[wordIndex].hyphenBreak)
      reading.hyphenationPoints.push_back(words[wordIndex].whitespaceEnd);

  std::map<int, SkRect> boxes;
  for (const LineMetrics& metrics : layout.lineMetrics(paragraph))
    boxes[metrics.lineIndex] = metrics.rect();
  for (const ColumnMetrics& metrics : layout.columnMetrics(paragraph))
    boxes[metrics.lineIndex] = metrics.rect();

  std::map<int, std::vector<LineScore>> scoresByLine;
  for (const LineScore& score : layout.lineScores)
    scoresByLine[score.lineIndex].push_back(score);

  for (const auto& [lineIndex, runs] : runsByLine) {
    LineReading& line = reading.lines.emplace_back();
    line.lineIndex = lineIndex;
    if (auto box = boxes.find(lineIndex); box != boxes.end())
      line.box = box->second;
    if (auto scores = scoresByLine.find(lineIndex);
        scores != scoresByLine.end())
      line.scores = scores->second;

    std::map<uint32_t, std::pair<float, float>> wordSpans;
    bool turned = false;
    bool fitRead = false;
    bool textRead = false;
    for (const PositionedRun* run : runs) {
      turned = turned || run->transformed;
      if (line.measure == 0 && run->intervalIndex >= 0 &&
          static_cast<size_t>(run->intervalIndex) < layout.intervals.size())
        line.measure =
            layout.intervals[static_cast<size_t>(run->intervalIndex)].length;
      if (!fitRead && run->shaped) {
        line.fit = run->fit;
        fitRead = true;
      }
      const bool hyphen = drawsHyphen(paragraph, *run);
      if (hyphen) {
        line.endsInHyphen = true;
        reading.hyphensTaken.push_back(words[run->wordIndex].whitespaceEnd);
      }
      if (!hyphen && !drawsItsWord(paragraph, *run)) continue;
      const Word& word = words[run->wordIndex];
      if (!hyphen) {
        line.textBegin = textRead ? std::min(line.textBegin, word.textBegin)
                                  : word.textBegin;
        line.textEnd = textRead ? std::max(line.textEnd, word.whitespaceEnd)
                                : word.whitespaceEnd;
        textRead = true;
      }
      if (run->transformed) continue;
      const auto [start, end] = penSpan(*run, columns);
      auto found = wordSpans.find(run->wordIndex);
      if (found == wordSpans.end())
        wordSpans.emplace(run->wordIndex, std::make_pair(start, end));
      else
        found->second = {std::min(found->second.first, start),
                         std::max(found->second.second, end)};
    }

    std::vector<std::pair<float, float>> spans;
    for (const auto& [wordIndex, span] : wordSpans) spans.push_back(span);
    std::sort(spans.begin(), spans.end());
    if (!turned && !spans.empty()) {
      float end = spans.front().second;
      for (const auto& span : spans) end = std::max(end, span.second);
      line.extent = end - spans.front().first;
      for (size_t index = 0; index + 1 < spans.size(); ++index)
        line.gaps.push_back(spans[index + 1].first - spans[index].second);
    }
  }

  std::sort(reading.hyphensTaken.begin(), reading.hyphensTaken.end());
  return reading;
}

}  // namespace sigil::weave::testing
