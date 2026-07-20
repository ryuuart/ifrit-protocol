// Scene G(a) — 2000 tokens across a dozen writing systems scattered as
// rotated confetti: Arabic joins, Devanagari conjuncts, emoji ZWJ sequences
// and all — every token resolved through per-codepoint fallback and the one
// shape cache.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <cstdio>
#include <random>
#include <string>

using namespace sigil::weave;

void sceneBabel(FontContext &fontContext,
                const std::filesystem::path &outputDirectory) {
  const char8_t *tokens[] = {
      u8"حرف",  u8"كلمة", u8"अक्षर",  u8"शब्द",   u8"אות",   u8"מילה", u8"ตัวอักษร",
      u8"字",   u8"글",   u8"λόγος", u8"буква", u8"🎉",    u8"👍🏽", u8"文字",
      u8"ঢাকা", u8"கடல்",  u8"ᚱᚢᚾ",   u8"ainm",  u8"słowo", u8"λέξη"};
  std::mt19937 randomEngine(77);
  Paragraph paragraph;
  std::u8string text;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    text += tokens[randomEngine() % 20];
    text += ' ';
  }
  paragraph.appendText(text, style(15));
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

  const auto coldStartTime = Clock::now();
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  const auto coldEndTime = Clock::now();
  ParagraphLayout warm = layoutParagraph(fontContext, paragraph, flow);
  const auto warmEndTime = Clock::now();

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1400, 900));
  surface->getCanvas()->clear(kPaper);
  layout.draw(surface->getCanvas(), paragraph);
  writePng(surface.get(), outputDirectory / "babel.png");
  std::printf("Scene G — babel confetti: %zu tokens, %zu runs, cold %.1f us, "
              "warm %.1f us\n",
              paragraph.words().size(), layout.runs.size(),
              toMicroseconds(coldEndTime - coldStartTime),
              toMicroseconds(warmEndTime - coldEndTime));
}
