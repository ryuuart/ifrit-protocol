/** @file
 * optical_kerning — setting every pair as tight as the face's own even
 * pair, by measuring the letters instead of reading a table.
 *
 * A face's kerning is a DESIGNER'S TABLE of pairs. Optical kerning is the
 * answer when there is none, or when a line mixes faces that never met:
 * each adjacent pair's outlines are measured for the narrowest distance
 * between them, and the pair is closed — or opened — until that distance
 * is the one the face's own reference pair leaves. The face's table is
 * switched OFF while this is on, because the two are answers to the same
 * question and a page takes one of them.
 *
 * IT IS AN APPROXIMATION, and worth knowing how. A designer kerns by
 * judging the white between two letters as an AREA and as a rhythm; this
 * measures a distance in bands. A pair a designer would have opened for
 * legibility, and a pair whose white is wide but shallow, both come out
 * tighter here.
 *
 * What the library does NOT decide is how tight type should be: the
 * reference is the face's own even pair, so a loose face stays loose. And
 * it reaches BETWEEN THE LETTERS OF ONE WORD — two words are separated by
 * a space, whose own width is the setting's to spend.
 *
 * The deltas are measured rather than asserted: each pair is set twice
 * and the difference of the two advances is what the last cell prints.
 *
 * EDIT THESE FIRST
 *   kHeadline — the line both settings are shown on.
 *   kSize — the size it is set at, which is the size the deltas are for.
 *   kPairs — the pairs the table measures.
 */

// TAGS: Typography/Lettering

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 620};
constexpr float kComparison = 660;
constexpr float kReading = 332;

constexpr float kSize = 40;  // the size the deltas are measured at
const char* kHeadline = "WAVY. To AVA";
const char* kPairs[6] = {"AV", "VA", "To", "Y.", "WA", "av"};

constexpr SkColor4f kTable{0.95f, 0.44f, 0.32f, 0.80f};
constexpr SkColor4f kOptical{0.40f, 0.76f, 0.98f, 0.80f};

sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.type.captionLabel = {.size = 11, .track = 0.8f, .mono = true};
  look.spacing.rowGap = 8;
  look.captionWhere = kit::Caption::Where::Above;
  return look;
}

/** The headline's register. `optical` is the whole difference between the
 *  two settings on this sheet. */
weave::TextStyle display(float size, SkColor4f color, bool optical) {
  const sk_sp<SkTypeface> face = weave::ports::face(
      {"Helvetica Neue", "Helvetica", "Arial", "sans-serif"});
  weave::TextStyle style =
      weave::textStyle({.face = face, .size = size, .color = color});
  style.shaping.opticalKerning = optical;
  return style;
}

const sketch::kit::Cell kSpecimen{
    .plate = {.width = kComparison,
              .height = 86,
              .padding = 20,
              .content = sketch::kit::Well::Content{.across = Align::Start}}};

}  // namespace

struct OpticalKerning {
  std::vector<sketch::kit::Reading> pairs;
  std::string lineDelta;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // THE DELTAS ARE MEASURED: each pair is set twice at the headline's
    // own size and the difference of the two advances is the answer. A
    // pair the face already kerns has little left to give.
    const SkColor4f figure = sketch::kit::theme().palette.figure;
    const auto advance = [&](const char* text8, bool optical) {
      return ctx
          .measure(
              box().children({text(text8, display(kSize, figure, optical))}))
          .width();
    };
    pairs.clear();
    for (const char* pair : kPairs)
      pairs.push_back(
          {.name = pair,
           .value = kit::formatted(
               "%+.2f px", advance(pair, true) - advance(pair, false))});
    lineDelta = kit::formatted(
        "%+.2f", advance(kHeadline, true) - advance(kHeadline, false));

    ctx.composer.render(sketch::kit::page(
        {.title = "Optical kerning",
         .subtitle = "One line at 40 px · the designer's table and the "
                     "measured alternative",
         .footer = "Optical kerning replaces the face's table. Negative "
                   "deltas close a pair; positive deltas open it."},
        kit::cells(
            {.cells = {kit::cells({.cells = {sketch::kit::cell(
                                                 kSpecimen, "FONT TABLE",
                                                 "opticalKerning = false",
                                                 headline(figure, false)),
                                             sketch::kit::cell(
                                                 kSpecimen, "OPTICAL FIT",
                                                 "opticalKerning = true",
                                                 headline(figure, true)),
                                             both()},
                                   .column = true,
                                   .gap = 18}),
                       table()},
             .gap = 28})));
  }

  Element headline(SkColor4f colour, bool optical) {
    return text(kHeadline, display(kSize, colour, optical))
        .width(kComparison - 40);
  }

  /** The two settings over one another: where they disagree is where the
   *  measured answer and the designer's differ. */
  Element both() {
    return sketch::kit::cell(
        kSpecimen, "SUPERIMPOSED", "Warm: font table · cool: optical fit",
        box()
            .width(kComparison - 40)
            .height(50)
            .children({headline(kTable, false).absolute().inset(0),
                       headline(kOptical, true).absolute().inset(0)}));
  }

  Element table() {
    const sketch::kit::Theme& sheet = sketch::kit::theme();
    return sketch::kit::caption(
        kReading, "MEASURED DIFFERENCE",
        "Optical advance minus font-table advance",
        sketch::kit::well({.width = kReading, .height = 368, .padding = 22})
            .column()
            .gap(24)
            .children(
                {sketch::kit::readout(pairs,
                                      {.measure = kReading - 44, .ruled = true})
                     .shrink(0),
                 box().column().gap(4).children(
                     {text("WHOLE LINE").styleClass("eyebrow"),
                      text(lineDelta)
                          .font(sheet.font({.size = 42, .mono = true}))
                          .ink(sheet.palette.figure),
                      text("px in total advance").styleClass("captionNote")}),
                 text("The reference is the face's own even pair. "
                      "Each difference accumulates along the line.")
                     .width(kReading - 44)
                     .styleClass("captionNote")}));
  }
};

SIGIL_SKETCH(OpticalKerning, "Kit · API",
             "one headline under the face's kerning table and under the "
             "measured answer, superimposed, with the per-pair deltas "
             "measured and printed")
