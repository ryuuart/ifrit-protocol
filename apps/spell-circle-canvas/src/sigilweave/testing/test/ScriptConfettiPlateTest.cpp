/** @file
 * Two thousand tokens across a dozen writing systems scattered as rotated
 * confetti — Arabic joins, Devanagari conjuncts, emoji ZWJ sequences —
 * every token resolved through per-codepoint fallback and one shape
 * cache.
 */

#include <gtest/gtest.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/testing/Passage.h>

#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <utility>

#include "support/Plates.h"

using namespace sigil::weave;
using namespace sigil::weave::test;
namespace weave = sigil::weave;

TEST(WeavePlates, ScriptConfettiDrawsItsBaseline) {
  FontContext& fonts = sigil::test::fonts();
  const char8_t* tokens[] = {
      u8"حرف",  u8"كلمة", u8"अक्षर",  u8"शब्द",   u8"אות",   u8"מילה", u8"ตัวอักษร",
      u8"字",   u8"글",   u8"λόγος", u8"буква", u8"🎉",    u8"👍🏽", u8"文字",
      u8"ঢাকা", u8"கடல்",  u8"ᚱᚢᚾ",   u8"ainm",  u8"słowo", u8"λέξη"};
  std::mt19937 randomEngine(77);  // NOLINT(bugprone-random-generator-seed): a
                                  // fixed seed keeps the plate reproducible
  Paragraph paragraph;
  std::u8string text;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    text += tokens[randomEngine() % 20];
    text += ' ';
  }
  paragraph.appendText(text, plateStyle(15));
  const uint32_t textLength = static_cast<uint32_t>(paragraph.text().size());
  for (uint32_t textOffset = 0; textOffset + 40 < textLength; textOffset += 40)
    paragraph.setPaint(textOffset, textOffset + 20,
                       PaintStyle{(textOffset / 40) % 3 == 0   ? kAccent
                                  : (textOffset / 40) % 3 == 1 ? kBlue
                                                               : kInk});

  LineSetFlow flow;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    const float angle = static_cast<float>(randomEngine() % 628) * 0.01f;
    flow.lines().push_back(
        {LineInterval{{20.0f + static_cast<float>(randomEngine() % 1360),
                       20.0f + static_cast<float>(randomEngine() % 860)},
                      {std::cos(angle), std::sin(angle)},
                      60}});
  }

  const weave::testing::Plate plate({1400, 900}, kPaper);
  plate.draw(weave::testing::lay(fonts, std::move(paragraph), flow));
  expectPlate(plate, "babel");
}
