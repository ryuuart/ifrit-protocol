#include "TestSupport.h"

#include <include/core/SkFontMgr.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <absl/container/flat_hash_set.h>

#include <random>

namespace sigil::weave::test {

FontContext &sharedContext() {
  // systemFontManager() shares one enumerated font set process-wide.
  static auto *fontContext = new FontContext(ports::systemFontManager());
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

} // namespace sigil::weave::test
