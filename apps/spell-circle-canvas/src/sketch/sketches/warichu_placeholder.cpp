/** @file
 * A long aside becomes a balanced pair of short lines inside one inline
 * slot. The split is measured in the note's own face and size; its wider
 * line determines the advance and its two-line depth determines the band.
 */
// TAGS: Typography/Paragraph, Typography/CJK

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/Beside.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/unicode/Unicode.h>

#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
constexpr SkColor4f kSlot{0.19f, 0.21f, 0.26f, 1};
constexpr const char8_t* kNote =
    u8"a small interruption that stays within the line";

struct Note {
  weave::Type type;
  weave::WarichuSplit split;
  std::u8string words, first, second;
  float oneLine = 0;

  void measure(sketch::SketchContext& ctx, std::u8string copy,
               weave::Type voice, bool vertical = false) {
    words = std::move(copy);
    type = std::move(voice);
    weave::Paragraph paragraph =
        weave::ParagraphBuilder(weave::textStyle(type)).addText(words).build();
    if (vertical) paragraph.setWritingMode(weave::WritingMode::kVerticalRL);
    split = weave::warichuSplit(*ctx.fonts, paragraph);
    const auto& utf16 = paragraph.text();
    const uint32_t cut = split.cutWord < paragraph.words().size()
                             ? paragraph.words()[split.cutWord].textBegin
                             : static_cast<uint32_t>(utf16.size());
    first = weave::unicode::toUtf8(std::u16string_view(utf16).substr(0, cut));
    second = weave::unicode::toUtf8(std::u16string_view(utf16).substr(cut));
    oneLine = ctx.measure(box().children({text(words).font(type)})).width();
  }

  Element lines(bool vertical = false) const {
    const float half = split.band * 0.5f;
    const auto line = [&](const std::u8string& copy, float at) {
      Element leaf = text(copy).font(type).absolute();
      return vertical
                 ? kit::at(std::move(leaf), at, 0, half, split.advance)
                       .block({.writingMode = weave::WritingMode::kVerticalRL})
                 : std::move(leaf.left(0).top(at).width(split.advance));
    };
    return box().children(
        {line(first, vertical ? half : 0), line(second, vertical ? 0 : half)});
  }
};

Element horizontal(const Note& note, bool balanced) {
  const SkSize extent = balanced ? SkSize{note.split.advance, note.split.band}
                                 : SkSize{note.oneLine, 19};
  return sketch::kit::well({.width = 501, .height = 190, .padding = 24})
      .children(
          {text(
               weave::rich()
                   .add(u8"An aside ")
                   .slot("note", extent, 5)
                   .add(
                       u8" can interrupt a sentence without leaving the line."))
               .font({.face = sketch::kit::houseFace(sketch::kit::Voice::Book),
                      .size = 23,
                      .track = 0})
               .width(453)
               .children(
                   {box()
                        .key("note")
                        .fill(Fill::color(kSlot))
                        .children({balanced
                                       ? note.lines()
                                       : text(note.words).font(note.type)})})});
}
}  // namespace

struct WarichuPlaceholder {
  Note latin, japanese;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1100, 825}, .captureAt = 0.05});
    const auto& sheet = sketch::kit::theme();
    latin.measure(ctx, kNote,
                  {.face = sketch::kit::houseFace(sketch::kit::Voice::Book),
                   .size = 13,
                   .color = sheet.palette.figure,
                   .track = 0});
    japanese.measure(
        ctx, u8"小さな文字で二行に組む",
        {.face = weave::ports::face(
             {"Hiragino Mincho ProN", "Yu Mincho", "Noto Serif CJK JP"}),
         .size = 13,
         .color = sheet.palette.figure,
         .track = 0,
         .language = "ja"},
        true);
    Element construction =
        sketch::kit::well({.width = 328, .height = 234, .padding = 22})
            .column()
            .gap(22)
            .children({text("THE TWO LINES").styleClass("eyebrow"),
                       box()
                           .width(latin.split.advance)
                           .height(latin.split.band)
                           .fill(Fill::color(kSlot))
                           .children({latin.lines()}),
                       text("The wider line sets the advance.\nThe pair shares "
                            "one unbreakable slot.")
                           .width(284)
                           .styleClass("captionNote")});
    Element vertical =
        sketch::kit::well({.width = 328, .height = 234, .padding = 22})
            .children(
                {text(weave::rich()
                          .add(u8"割注は")
                          .slot("note",
                                {japanese.split.advance, japanese.split.band})
                          .add(u8"本文の途中に置く。"))
                     .font({.face = japanese.type.face,
                            .size = 24,
                            .track = 0,
                            .language = "ja"})
                     .width(284)
                     .height(190)
                     .block({.writingMode = weave::WritingMode::kVerticalRL})
                     .children({box()
                                    .key("note")
                                    .fill(Fill::color(kSlot))
                                    .children({japanese.lines(true)})})});
    Element reading =
        sketch::kit::well({.width = 328, .height = 234, .padding = 22})
            .column()
            .gap(20)
            .children(
                {sketch::kit::readout(
                     {{.name = "Single-line advance",
                       .value = kit::formatted("%.1f px", latin.oneLine)},
                      {.name = "Balanced advance",
                       .value = kit::formatted("%.1f px", latin.split.advance)},
                      {.name = "Two-line band",
                       .value = kit::formatted("%.1f px", latin.split.band)},
                      {.name = "Cut word",
                       .value = kit::formatted("%u", latin.split.cutWord)}},
                     {.measure = 284, .ruled = true}),
                 text("Measured at 13 px. The note keeps its own size; the "
                      "base is 23 px.")
                     .width(284)
                     .styleClass("captionNote")});
    ctx.composer.render(sketch::kit::page(
        {.title = "An aside inside the line",
         .subtitle = "Warichu · one note, first in a single line and then "
                     "balanced into two",
         .footer =
             "warichuSplit chooses the break with the closest advances. The "
             "slot's band enters the paragraph's strut before wrapping."},
        box().column().gap(30).children(
            {sketch::kit::comparison(
                 {.cases = {{.title = "ONE LONG INTERRUPTION",
                             .control = "The note stays on one line",
                             .figure = horizontal(latin, false),
                             .note = "The wide slot spends most of the "
                                     "sentence's measure."},
                            {.title = "TWO BALANCED LINES",
                             .control = "The same note · warichuSplit",
                             .figure = horizontal(latin, true),
                             .note = "A shorter advance leaves room for the "
                                     "surrounding text."}},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::comparison(
                 {.cases = {{.title = "HOW THE SLOT IS BUILT",
                             .figure = std::move(construction)},
                            {.title = "IN A VERTICAL BASE",
                             .figure = std::move(vertical)},
                            {.title = "THE MEASURED ANSWER",
                             .figure = std::move(reading)}},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(WarichuPlaceholder, "Kit · API",
             "a measured inline note before and after balancing, its two-line "
             "construction, and a Japanese vertical setting")
