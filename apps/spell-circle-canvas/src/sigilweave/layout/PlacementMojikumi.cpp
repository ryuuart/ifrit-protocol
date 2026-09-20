/** @file
 * THE ROOM A JAPANESE SETTING PUTS AFTER EACH WORD: the mojikumi table's
 * fraction of an em between the two classes that meet across a break
 * opportunity, less whatever tsume closes between two plain full-width
 * characters. Resolved once per layout, then read by both breakers and
 * by placement out of one array.
 */

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "ParagraphLayoutInternal.h"
#include "sigilweave/paragraph/Paragraph.h"

namespace sigil::weave {

namespace detail {

void resolveMojikumi(const Paragraph& paragraph,
                     const ParagraphLayoutOptions& options,
                     std::vector<float>& room) {
  room.clear();
  const bool tsume = options.tsume != 0;
  if (options.mojikumi.empty() && !tsume) return;
  const std::vector<Word>& words = paragraph.words();
  const std::u16string_view text = paragraph.text();
  room.assign(words.size(), 0.0f);
  // The em the fraction is of is the SPAN's, not the shaped word's: the
  // room a gap needs is settled before anything is shaped, so a text that
  // overflows its frame pays for the gaps it sets and not for the rest.
  size_t spanCursor = 0;
  const auto emAt = [&](uint32_t offset) {
    const std::vector<StyleSpan>& spans = paragraph.spans();
    if (spans.empty()) return 0.0f;
    while (spanCursor + 1 < spans.size() && spans[spanCursor].end <= offset)
      ++spanCursor;
    return spans[spanCursor].style.shaping.fontSize;
  };
  // The class of the character either side of each gap. A gap between two
  // words is where two full-width characters meet across a break
  // opportunity, which in a text set in full-width characters is nearly
  // every gap it has.
  const auto classAt = [&](uint32_t offset) {
    if (offset >= text.size()) return MojikumiClass::kOther;
    const MojikumiClass named = options.mojikumi.classOf(text[offset]);
    if (named != MojikumiClass::kOther) return named;
    size_t cursor = offset;
    return unicode::isFullWidth(unicode::decodeAt(text, cursor))
               ? MojikumiClass::kIdeograph
               : MojikumiClass::kOther;
  };
  for (size_t index = 0; index + 1 < words.size(); ++index) {
    const Word& word = words[index];
    // Only a gap with nothing in it is the table's to set: a word that
    // ends in whitespace has its own glue, and the face set it.
    if (word.whitespaceEnd != word.textEnd || word.textEnd == 0) continue;
    const MojikumiClass before = classAt(word.textEnd - 1);
    const MojikumiClass after = classAt(words[index + 1].textBegin);
    if (before == MojikumiClass::kOther || after == MojikumiClass::kOther)
      continue;
    float fraction =
        options.mojikumi
            .room[static_cast<size_t>(before)][static_cast<size_t>(after)];
    // TSUME closes the gap after a full-width character the table gives no
    // class of its own — the plain ones, whose side bearings are the
    // face's own even spacing rather than a punctuation mark's air.
    if (tsume && before == MojikumiClass::kIdeograph &&
        after == MojikumiClass::kIdeograph)
      fraction -= options.tsume;
    if (fraction == 0) continue;
    room[index] = fraction * emAt(word.textBegin);
  }
}

}  // namespace detail

}  // namespace sigil::weave
