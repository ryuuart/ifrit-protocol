// Scene B — Knuth-Plass justified paragraph with live word updates while the
// paragraph stays justified.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <cstdio>
#include <string>

using namespace textflow;

void sceneKnuthPlass(FontContext &fontContext, int frames,
                     const std::filesystem::path &outputDirectory) {
  std::printf("Scene B — Knuth-Plass justified paragraph, words updating "
              "live\n");

  ParagraphBuilder builder(style(16.5f));
  builder.addText(
      u8"In olden times, when wishing still helped one, there lived a king "
      "whose daughters were all beautiful; and the youngest was so beautiful "
      "that the sun itself, which has seen so much, was astonished whenever "
      "it shone in her face. Close by the king's castle lay a great dark "
      "forest, and under an old lime-tree in the forest was a well, and when "
      "the day was very warm, the king's child went out into the forest and "
      "sat down by the side of the ");
  builder.pushStyle(style(16.5f, kAccent)).addText(u8"cool").popStyle();
  builder.addText(u8" fountain; and when she was bored she took a golden ball, "
                  "and threw it up on high and caught it; and this ball was "
                  "her favorite plaything. ");
  builder.pushStyle(style(16.5f, kBlue, "ja"))
      .addText(u8"グリム童話は日本語でも読まれ、")
      .popStyle();
  builder.addText(u8"the tale crosses scripts without breaking its measure.");
  Paragraph paragraph = builder.build();

  BlockFlow flow(SkRect::MakeXYWH(50, 40, 560, 640));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 27;

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(660, 720));

  const char8_t *synonyms[] = {u8"cool", u8"cold", u8"calm", u8"deep"};
  const char8_t *ballWords[] = {u8"golden", u8"silver", u8"copper",
                                u8"crystal"};

  TimingStats layoutTime;
  const uint64_t shapeCallsBefore = fontContext.stats().shapeCalls;

  for (int frame = 0; frame < frames; ++frame) {
    // Word updates while the paragraph stays justified.
    if (frame > 0 && frame % 20 == 0) {
      const std::u16string &text = paragraph.text();
      for (const char8_t *candidate : synonyms) {
        std::u16string needle(candidate, candidate + 4);
        const size_t textOffset = text.find(needle);
        if (textOffset != std::u16string::npos) {
          paragraph.replaceText(static_cast<uint32_t>(textOffset),
                                static_cast<uint32_t>(textOffset + 4),
                                synonyms[(frame / 20) % 4]);
          break;
        }
      }
    }
    if (frame > 0 && frame % 30 == 0) {
      for (const char8_t *candidate : ballWords) {
        std::u16string needle;
        for (const char8_t *character = candidate; *character; ++character)
          needle.push_back(static_cast<char16_t>(*character));
        const size_t textOffset = paragraph.text().find(needle);
        if (textOffset != std::u16string::npos) {
          paragraph.replaceText(
              static_cast<uint32_t>(textOffset),
              static_cast<uint32_t>(textOffset + needle.size()),
              ballWords[(frame / 30) % 4]);
          break;
        }
      }
    }

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto layoutEndTime = Clock::now();
    layoutTime.add(toMicroseconds(layoutEndTime - layoutStartTime));

    if (frame % 60 == 0 || frame == frames - 1) {
      surface->getCanvas()->clear(kPaper);
      layout.draw(surface->getCanvas(), paragraph);
      char name[64];
      std::snprintf(name, sizeof name, "knuthplass_f%03d.png", frame);
      writePng(surface.get(), outputDirectory / name);
    }
  }

  layoutTime.report("layout (KP justify per frame)");
  std::printf("  hb_shape calls during animation: %llu (edits only)\n\n",
              static_cast<unsigned long long>(fontContext.stats().shapeCalls -
                                              shapeCallsBefore));
}
