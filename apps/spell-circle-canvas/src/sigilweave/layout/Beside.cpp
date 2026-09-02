/** @file
 * The placement a reading takes beside its base: the band it reserves, the
 * one line or column it is set on, and the share of it a broken base
 * carries.
 */

#include "sigilweave/layout/Beside.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/layout/Flow.h"
#include "sigilweave/unicode/Unicode.h"

namespace sigil::weave {

float bandBeside(FontContext& fontContext, const TextStyle& style, float gap) {
  // The reading's OWN strut and nothing about the base: one zero-width
  // unit set in that style is enough to ask the face for its line height,
  // and it shapes nothing.
  Paragraph probe;
  probe.appendText(u8"​", style);
  return probe.strut(fontContext).height + gap;
}

ParagraphLayout layoutBeside(FontContext& fontContext, Paragraph& reading,
                             const Beside& beside) {
  const bool column = beside.writingMode == WritingMode::kVerticalRL;
  reading.setWritingMode(beside.writingMode);
  const float extent = reading.naturalWidth(fontContext);
  if (extent <= 0) return {};
  const float depth = reading.strut(fontContext).height;

  LineInterval interval;
  if (column) {
    // A column reads its furniture on the RIGHT, and the reading runs down
    // beside it on the column's own axis.
    const float centre = (beside.base.top() + beside.base.bottom()) * 0.5f;
    const float across =
        beside.side == Beside::Side::Before
            ? beside.base.right() + beside.gap + depth * 0.5f
            : beside.base.left() - beside.gap - depth * 0.5f;
    interval.origin = {across, centre - extent * 0.5f};
    interval.direction = {0, 1};
  } else {
    const float centre = (beside.base.left() + beside.base.right()) * 0.5f;
    // Above a line the reading stands ON its own baseline, so its own
    // depth is already accounted for by where that baseline sits; below
    // one, the depth is the drop from the base's foot to it.
    const float across = beside.side == Beside::Side::Before
                             ? beside.base.top() - beside.gap
                             : beside.base.bottom() + beside.gap + depth;
    interval.origin = {centre - extent * 0.5f, across};
    interval.direction = {1, 0};
  }
  // One line, wide enough for the whole reading: a reading that wrapped
  // would be a second thing beside the base rather than the one thing it
  // is.
  constexpr float kRoom = 1.0f;
  interval.length = extent + kRoom;
  LineSetFlow flow({{interval}});
  return layoutParagraph(fontContext, reading, flow);
}

WarichuSplit warichuSplit(FontContext& fontContext, Paragraph& note) {
  note.ensureShaped(fontContext);
  const std::vector<Word>& words = note.words();
  WarichuSplit split;
  split.band = note.strut(fontContext).height * 2.0f;
  if (words.empty()) return split;
  split.cutWord = static_cast<uint32_t>(words.size());
  // Every prefix of the note is a candidate first line; the one whose two
  // halves differ least is the cut. A note that offers no interior break
  // is one line, and its advance is its whole natural width.
  float total = 0;
  for (const Word& word : words) total += word.width + word.spaceWidth;
  split.advance = note.naturalWidth(fontContext);
  float running = 0;
  float closest = split.advance;
  for (size_t index = 0; index + 1 < words.size(); ++index) {
    running += words[index].width + words[index].spaceWidth;
    const float first = running - words[index].spaceWidth;
    const float second = total - running;
    const float difference = std::abs(first - second);
    if (difference < closest) {
      closest = difference;
      split.cutWord = static_cast<uint32_t>(index + 1);
      split.advance = std::max(first, second);
    }
  }
  return split;
}

ParagraphLayout layoutWarichu(FontContext& fontContext, Paragraph& note,
                              const SkRect& slot, WritingMode writingMode) {
  note.setWritingMode(writingMode);
  const WarichuSplit split = warichuSplit(fontContext, note);
  const float pitch = note.strut(fontContext).height;
  const float ascent = note.strut(fontContext).ascent;
  if (split.advance <= 0) return {};
  const bool column = writingMode == WritingMode::kVerticalRL;
  // Two lines, each as long as the wider of the two halves, stacked across
  // the slot: down the box in a horizontal setting, and across it from the
  // right in a vertical one, which is the order a column is read in.
  LineSetFlow flow;
  constexpr float kRoom = 1.0f;
  for (int line = 0; line < 2; ++line) {
    LineInterval interval;
    if (column) {
      interval.origin = {slot.right() - pitch * (0.5f + (float)line),
                         slot.top()};
      interval.direction = {0, 1};
    } else {
      interval.origin = {slot.left(),
                         slot.top() + ascent + pitch * (float)line};
      interval.direction = {1, 0};
    }
    interval.length = split.advance + kRoom;
    flow.lines().push_back({interval});
  }
  return layoutParagraph(fontContext, note, flow);
}

std::u16string shareOfReading(std::u16string_view reading, float here,
                              float next) {
  const float total = here + next;
  if (total <= 0 || reading.empty()) return std::u16string(reading);
  const auto length = static_cast<float>(reading.size());
  auto cut = static_cast<size_t>(std::lround(here / total * length));
  cut = std::min(cut, reading.size());
  // THE CUT LANDS ON A GRAPHEME CLUSTER, which is what a reader calls one
  // character: a combining mark, a Hangul syllable, a regional-indicator
  // pair and an emoji sequence are each one thing, and half of any of them
  // is not a reading. The boundary at or before the proportional cut is
  // the one taken, so a reading never gains a character it did not have.
  static thread_local std::vector<uint32_t> clusters;
  unicode::graphemeBoundaries(reading, clusters);
  size_t chosen = 0;
  for (const uint32_t boundary : clusters) {
    if (boundary > cut) break;
    chosen = boundary;
  }
  return std::u16string(reading.substr(0, chosen));
}

}  // namespace sigil::weave
