#include "sigilweavekit/SampleText.h"

#include "sigilweavekit/Labels.h"

#include <random>
#include <string>

namespace sigil::weave::kit {

sigil::weave::Paragraph mixedScriptFiller(int wordCount, float fontSize,
                                      std::array<SkColor, 3> chunkColors) {
  const char8_t *latin[] = {u8"the",    u8"letters", u8"fall",   u8"away",
                            u8"from",   u8"their",   u8"lines",  u8"and",
                            u8"return", u8"again",   u8"layout", u8"engine",
                            u8"words",  u8"measure", u8"glyph",  u8"cascade",
                            u8"gentle", u8"steady",  u8"rhythm", u8"flowing"};
  const char8_t *cjk[] = {u8"文字", u8"雨",   u8"波紋", u8"字形",
                          u8"빗물", u8"글자", u8"물결", u8"여울",
                          u8"漣漪", u8"文雨", u8"字落", u8"縦横"};
  const sigil::weave::TextStyle styles[3] = {makeStyle(fontSize, chunkColors[0]),
                                         makeStyle(fontSize, chunkColors[1]),
                                         makeStyle(fontSize, chunkColors[2])};
  std::mt19937 randomEngine(23);
  sigil::weave::Paragraph paragraph;
  std::u8string chunk;
  int chunkStyle = 0;
  for (int wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
    if (wordIndex % 5 == 4)
      chunk += cjk[randomEngine() % 12];
    else
      chunk += latin[randomEngine() % 20];
    chunk += ' ';
    if (wordIndex % 120 == 119 || wordIndex + 1 == wordCount) {
      paragraph.appendText(chunk, styles[chunkStyle++ % 3]);
      chunk.clear();
    }
  }
  return paragraph;
}

} // namespace sigil::weave::kit
