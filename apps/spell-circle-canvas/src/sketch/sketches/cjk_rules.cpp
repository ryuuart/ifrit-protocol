/** @file
 * Japanese composition at three boundaries: a house prohibition, a
 * punctuation mark at the measure, and the white inside bracket pairs.
 * Locale tailoring already forbids common punctuation starts. The extra
 * prohibition here deliberately adds an ideograph so its effect is visible.
 */
// TAGS: Typography/CJK

#include <sigilcompose/core/Core.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/kit/LineTables.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
constexpr SkColor4f kInk{0.88f, 0.89f, 0.92f, 1};
constexpr float kSize = 22;

weave::Type mincho(float size = kSize) {
  return {.face = weave::ports::face(
              {"Hiragino Mincho ProN", "Yu Mincho", "Noto Serif CJK JP"}),
          .size = size,
          .color = kInk,
          .track = 0,
          .language = "ja"};
}

weave::MojikumiTable brackets() {
  weave::MojikumiTable table;
  table.members[static_cast<size_t>(weave::MojikumiClass::kOpening)] = u"「（";
  table.members[static_cast<size_t>(weave::MojikumiClass::kClosing)] = u"）」";
  table.room[static_cast<size_t>(weave::MojikumiClass::kClosing)]
            [static_cast<size_t>(weave::MojikumiClass::kOpening)] = -0.5f;
  return table;
}

Element column(const char8_t* copy, float depth) {
  return text(copy).font(mincho()).width(112).height(depth).block(
      {.writingMode = weave::WritingMode::kVerticalRL,
       .lineBreakLocale = "ja"});
}

Element paired(Element before, Element after, float depth) {
  const auto& look = sketch::kit::theme();
  const auto sample = [&](const char* label, Element body) {
    return box().column().gap(16).width(132).children(
        {text(label).styleClass("captionNote"),
         box().width(132).height(156).children(
             {box().absolute().left(8).top(depth).width(116).height(1).fill(
                  Fill::color(look.palette.figure)),
              std::move(body).absolute().left(10).top(0)})});
  };
  return sketch::kit::well({.width = 328, .height = 226, .padding = 20})
      .children({box().row().gap(24).children(
          {sample("REFERENCE", std::move(before)),
           sample("WITH THE RULE", std::move(after))})});
}

Element tracking(bool tightened) {
  return sketch::kit::well({.width = 501, .height = 104, .padding = 20})
      .children({text(u8"文字の間に流れる白い空間")
                     .font(mincho(26))
                     .width(461)
                     .block({.lineBreakLocale = "ja",
                             .mojikumi = brackets(),
                             .tsume = tightened ? -0.12f : 0.0f})});
}
}  // namespace

struct CjkRules {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1100, 810}, .captureAt = 0.05});
    weave::KinsokuTable house = weave::kit::kinsoku::japanese();
    house.notLineStart += u"組";
    ctx.composer.render(sketch::kit::page(
        {.title = "At the edge of a Japanese line",
         .subtitle = "Paired vertical settings at 22 px · gold rules mark the "
                     "same measure in each pair",
         .footer = "Japanese locale tailoring comes first. A house table adds "
                   "prohibitions; hanging and spacing tables change the room "
                   "around marks."},
        box().column().gap(30).children(
            {sketch::kit::comparison(
                 {.cases =
                      {{.title = "KEEP A WORD OPENING TOGETHER",
                        .control = "House rule: 組 may not start a column",
                        .figure = paired(column(u8"日本語と組版を学ぶ。", 88),
                                         column(u8"日本語と組版を学ぶ。", 88)
                                             .block({.kinsoku = house}),
                                         88),
                        .note =
                            "The house rule carries the preceding character "
                            "forward so 組 cannot open the column."},
                       {.title = "LET THE FULL STOP HANG",
                        .control = "End aligned · hanging::japanese()",
                        .figure = paired(
                            column(u8"文字を組む。", 132)
                                .block(
                                    {.alignment = weave::TextAlignment::kEnd}),
                            column(u8"文字を組む。", 132)
                                .block({.alignment = weave::TextAlignment::kEnd,
                                        .hanging =
                                            weave::kit::hanging::japanese()}),
                            132),
                        .note = "Both sentences align to the gold edge. With "
                                "hanging, the full stop sits beyond it."},
                       {.title = "CLOSE THE BRACKET GAP",
                        .control = "Closing → opening: −0.5 em",
                        .figure =
                            paired(column(u8"「組版」「余白」「行間」", 132),
                                   column(u8"「組版」「余白」「行間」", 132)
                                       .block({.mojikumi = brackets()}),
                                   132),
                        .note =
                            "Two half-empty bracket cells share less white. "
                            "The marks and their size stay unchanged."}},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::comparison(
                 {.cases = {{.title = "FULL-WIDTH SPACING",
                             .control = "tsume = 0",
                             .figure = tracking(false),
                             .note = "A horizontal reference at 26 px."},
                            {.title = "A TIGHTER TEXTURE",
                             .control = "tsume = −0.12 em",
                             .figure = tracking(true),
                             .note = "Unclassified full-width gaps close in "
                                     "addition to bracket spacing."}},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(CjkRules, "Kit · API",
             "paired Japanese settings that expose a house prohibition, "
             "hanging punctuation, bracket spacing and full-width tracking")
