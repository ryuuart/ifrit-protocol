#include "TestSupport.h"

#include <include/core/SkFontMgr.h>
#include <include/ports/SkFontMgr_mac_ct.h>

#include <absl/container/flat_hash_set.h>

#include <random>

namespace textflow::test {

FontContext &sharedContext() {
  // CoreText font-manager construction enumerates system fonts; do it once.
  static auto *fontContext = new FontContext(SkFontMgr_New_CoreText(nullptr));
  return *fontContext;
}

TextStyle basicStyle(float fontSize) {
  TextStyle style;
  style.shaping.fontSize = fontSize;
  return style;
}

Paragraph makeParagraph(std::u8string_view utf8, float fontSize) {
  Paragraph paragraph;
  paragraph.appendText(utf8, basicStyle(fontSize));
  return paragraph;
}

float runEnd(const Paragraph &paragraph, const PositionedRun &run) {
  if (run.shaped)
    return run.origin.x() + run.shaped->advance;
  return run.origin.x() + paragraph.words()[run.wordIndex].width;
}

bool allGlyphsResolved(const Paragraph &paragraph) {
  for (const Word &word : paragraph.words())
    for (const WordSegment &seg : word.segments)
      for (uint16_t glyph : seg.shaped->glyphs)
        if (glyph == 0)
          return false;
  return true;
}

size_t uniqueClusterCount(const ShapedWord &shapedWord) {
  absl::flat_hash_set<uint32_t> unique(shapedWord.clusters.begin(),
                                       shapedWord.clusters.end());
  return unique.size();
}

std::u8string makePooledText(std::span<const char8_t *const> pool,
                             int wordCount, uint32_t seed) {
  std::mt19937 randomEngine(seed);
  std::u8string text;
  for (int wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
    text += pool[randomEngine() % pool.size()];
    text += ' ';
  }
  return text;
}

} // namespace textflow::test
