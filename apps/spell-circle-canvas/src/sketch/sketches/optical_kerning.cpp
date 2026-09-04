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

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 254;
constexpr float kPicture = 200;

constexpr float kSize = 40;  // the size the deltas are measured at
const char* kHeadline = "WAVY. To AVA";
const char* kPairs[6] = {"AV", "VA", "To", "Y.", "WA", "av"};

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};
constexpr SkColor4f kTable{0.95f, 0.44f, 0.32f, 0.80f};
constexpr SkColor4f kOptical{0.40f, 0.76f, 0.98f, 0.80f};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

/** The headline's register. `optical` is the whole difference between the
 *  two settings on this sheet. */
weave::TextStyle display(float size, SkColor4f color, bool optical) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"Helvetica Neue", "Helvetica", "Arial", "sans-serif"});
  weave::TextStyle style =
      weave::textStyle({.face = face, .size = size, .color = color});
  style.shaping.opticalKerning = optical;
  return style;
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

Element cell(const char* call, const char* note, Element body) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   kit::well({.width = kCell,
                              .height = kPicture,
                              .ground = Fill::color(kCellGround),
                              .padding = 12})
                       .child(std::move(body)));
}

}  // namespace

struct OpticalKerning final : sketch::Sketch {
  std::string rows[7];

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    // THE DELTAS ARE MEASURED: each pair is set twice at the headline's
    // own size and the difference of the two advances is the answer. A
    // pair the face already kerns has little left to give.
    const auto advance = [&](const char* text8, bool optical) {
      return ctx
          .measure(
              box().child(text(toU8(text8), display(kSize, kFigure, optical))))
          .width();
    };
    for (int i = 0; i < 6; ++i)
      rows[i] =
          kit::format("%-3s %+6.2f px", kPairs[i],
                      advance(kPairs[i], true) - advance(kPairs[i], false));
    rows[6] = kit::format("%-3s %+6.2f px", "the line",
                          advance(kHeadline, true) - advance(kHeadline, false));

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("OPTICAL KERNING \xc2\xb7 "
                           "ShapingStyle::opticalKerning"),
             .subtitle = toU8("dials \xc2\xb7 the size (40 px, which is the "
                              "size the deltas are for) \xc2\xb7 the pairs "
                              "measured \xc2\xb7 the face, whose own even "
                              "pair is the reference"),
             .footer = toU8("the face's table is switched OFF while this is "
                            "on, because the two are answers to the same "
                            "question and a page takes one of them \xe2\x80"
                            "\x94 and the reference is the face's own even "
                            "pair, so a loose face stays loose"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells = {plain(), optical(), both(), table()}, .gap = 14}))
            .absolute()
            .inset(0));
  }

  Element headline(SkColor4f colour, bool optical) {
    return text(toU8(kHeadline), display(kSize, colour, optical))
        .width(Dim(kCell - 24));
  }

  Element plain() {
    return cell("opticalKerning = false",
                "the face's own kerning table \xc2\xb7 a designer's pairs, "
                "and the setting every other cell is read against",
                headline(kFigure, false));
  }

  Element optical() {
    return cell("opticalKerning = true",
                "every pair measured instead \xc2\xb7 the outlines are read "
                "for the narrowest distance between them and closed to the "
                "face's own even pair",
                headline(kFigure, true));
  }

  /** The two settings over one another: where they disagree is where the
   *  measured answer and the designer's differ. */
  Element both() {
    return cell("both, superimposed",
                "the table in warm under the measured answer in cool "
                "\xc2\xb7 the letters drift apart along the line, because "
                "every pair's delta accumulates into the next",
                box()
                    .absolute()
                    .inset(0)
                    .child(headline(kTable, false).absolute().inset(0))
                    .child(headline(kOptical, true).absolute().inset(0)));
  }

  Element table() {
    Element column = box().column().gap(7);
    for (const std::string& row : rows)
      column.child(text(toU8(row), mono(11, kFigure)));
    return cell("measured pair deltas",
                "each pair set twice and the two advances subtracted "
                "\xc2\xb7 negative closes the pair up, and the last row is "
                "the whole line",
                std::move(column));
  }
};

SIGIL_SKETCH(OpticalKerning, "Kit \xc2\xb7 API",
             "one headline under the face's kerning table and under the "
             "measured answer, superimposed, with the per-pair deltas "
             "measured and printed")
