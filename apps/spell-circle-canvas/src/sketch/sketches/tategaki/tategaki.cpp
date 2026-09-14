/** @file
 * tategaki — vertical-rl CJK: three forms in one paragraph, with the
 * upright, rotated and substituted runs set against each other.
 */

// 縦組み — a vertical-RL CJK plate, the one place the library's writing mode
// is visible as type rather than as geometry.
//
// One paragraph carries all three vertical forms at once, because that is
// what makes the mode legible: ideographs standing upright in their 'vert'
// shapes, a Latin word lying on its side down the column, and two-digit
// numbers set 縦中横 — shaped across and stood upright in the column, which
// is how a date reads in vertical prose.
//
// Over that, the rest of the text surface asked to work down the page: a
// spanPaint highlight on a named phrase, and one settling entrance beating
// cluster by cluster in reading order — down each column, then right to
// left across them.
//
// Ruby and kenten are deliberately absent. Each is a few lines over the
// placed runs of a finished layout rather than a library feature, and the
// shapes they take differ enough per passage that a verb would fit none of
// them.
//
// EDIT THESE FIRST
//   kBodySize                 — the passage's size. The three specimen
//                               columns are set from it, so the whole
//                               page rescales off this one number.
//   kColumnBlockW / ...H      — the main column block's measure and its
//                               depth: how many columns the passage
//                               breaks into, and how far each one runs.
//   the stagger's amountMs    — how long the settling entrance takes to
//                               reach the last cluster. The declared
//                               moment stands after it, so raising it
//                               past 2.4 s puts the plate mid-entrance.

// TAGS: Typography/CJK

#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/query/Selector.h>

#include "VerticalSpecimen.h"

namespace sketch = sigil::sketch;

namespace motion = sigil::motion;

using namespace sigil::compose;
namespace weave = sigil::weave;
using namespace std::chrono_literals;

namespace {

constexpr SkSize kSceneSize = vertical::kSceneSize;

namespace tategaki {
// The chassis: the face, the two style registers and the specimen block,
// plus this plate's inks — it is printed white on a sumi ground.
using namespace vertical;
using namespace vertical::ink;

constexpr float kH = kSceneSize.fHeight;
constexpr float kBodySize = 30;
constexpr float kColumnBlockW = 420;
constexpr float kColumnBlockH = 436;
constexpr float kColumnBlockRight = 56;

/** The settling entrance: an AMOUNT-mode cascade, so the whole spread is
 *  1100 ms however many clusters the passage breaks into, and its span is
 *  the same number for every count past one. */
const sigil::motion::Spread kSettle{.amountMs = 1100, .durationMs = 520};
const float kSettleSpan = kSettle.spanMs(2);

}  // namespace tategaki

struct Tategaki final : sketch::Sketch {
  /// After the cascade has settled: the plate is the finished setting, not
  /// a frame of its entrance.

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 2.4,
                             .background = SkColor4f{0, 0, 0, 1}});
    ctx.composer.render(describe());
  }

  /** One form, named and shown: a Latin caption over a short column set
   *  the way the caption says. The three together are the whole per-span
   *  vocabulary, side by side at a size where the difference reads. The
   *  column's runs inherit @p style, so a run says only the form it takes. */
  Element specimen(const char* caption, weave::RichText run,
                   weave::Type style) {
    namespace tg = tategaki;
    return tg::specimen(
        caption, tg::labelType(12, tg::kAi, 2),
        text(std::move(run))
            .font(std::move(style))
            .width(46.0f)
            .height(140.0f)
            .block({.writingMode = sigil::weave::WritingMode::kVerticalRL}),
        0.0f, 12.0f);
  }

  Element describe() {
    namespace tg = tategaki;
    namespace ch = choreograph;

    Fill ground =
        linearGradient({0, 0}, {0, tg::kH}, {tg::kSumiLift, tg::kSumi});

    // All three vertical forms in one passage. Only the two numbers and the
    // Latin word name a form; everything else takes UTR#50's, which is what
    // stands the ideographs upright and turns the Latin on its side by
    // itself.
    // The passage inherits the body register from its leaf, so a run says
    // only the form it takes, and the Latin word its size and colour too.
    auto passage =
        weave::rich()
            .add(u8"縦組みの文章は、上から下へ、右から左へと流れる。平成")
            .add(u8"31",
                 weave::Type{.verticalForm =
                                 sigil::weave::VerticalForm::kTateChuYoko})
            .add(u8"年")
            .add(u8"12",
                 weave::Type{.verticalForm =
                                 sigil::weave::VerticalForm::kTateChuYoko})
            .add(u8"月、組版の器")
            .add(u8"SigilWeave",
                 weave::Type{
                     .size = tg::kBodySize * 0.86f,
                     .color = tg::kAi,
                     .verticalForm = sigil::weave::VerticalForm::kRotated})
            .add(u8"は縦書きに対応した。字は立ち、欧文は寝る。")
            .add(u8"数字は縦中横に組み、二桁のまま読ませる。")
            .add(u8"行は列となり、列は右から左へ積まれてゆく。");

    // The plate is printed shell white on sumi: every run that names no
    // colour is set in it.
    return box()
        .fill(std::move(ground))
        .ink(tg::kGofun)
        .children(
            {text(std::move(passage))
                 .font(tg::bodyType(tg::kBodySize))
                 .right(tg::kColumnBlockRight)
                 .top(92)
                 .width(tg::kColumnBlockW)
                 .height(tg::kColumnBlockH)
                 .block({.writingMode = sigil::weave::WritingMode::kVerticalRL})
                 // The phrase the plate is about, in vermilion — paint only,
                 // so the glyphs are exactly the glyphs the passage shaped.
                 .spanPaint(weave::selectors::text(u8"縦組み"),
                            sigil::weave::PaintStyle(tg::kAka.toSkColor()))
                 // One settling entrance, beating cluster by cluster in
                 // READING ORDER: down each column, then right to left.
                 .fx({.effect = fx::rise(30),
                      .stagger = tg::kSettle,
                      .progress = animate(
                          motion::from(0.0f).to(1.0f),
                          {std::chrono::milliseconds((int)tg::kSettleSpan),
                           &ch::easeNone, 180ms})}),
             box()
                 .absolute()
                 .inset(64, 88, 0, 0)
                 .column()
                 .gap(10)
                 .children(
                     {text("縦組み").font(tg::bodyType(46)),
                      kit::line({.length = Dimension(120),
                                 .fill = Fill::color(tg::kAi)}),
                      text("VERTICAL-RL").font(tg::labelType(15, tg::kAi, 4)),
                      text("UTR#50 orientation, 'vert' forms,\n"
                           "tate-chu-yoko digits, rotated Latin")
                          .font(tg::labelType(14, 0.5f))
                          .width(240.0f),
                      box().height(26.0f),
                      box().row().gap(34).children(
                          {specimen(
                               "UPRIGHT", weave::rich().add(u8"字は立つ"),
                               tg::bodyType(
                                   28, sigil::weave::VerticalForm::kUpright)),
                           specimen("ROTATED",
                                    weave::rich().add(u8"Latin lies"),
                                    tg::bodyType(
                                        24, tg::kAi,
                                        sigil::weave::VerticalForm::kRotated)),
                           specimen(
                               "TATE-CHU-YOKO",
                               weave::rich()
                                   .add(u8"令和")
                                   .add(u8"07",
                                        weave::Type{
                                            .color = tg::kAka,
                                            .verticalForm = sigil::weave::
                                                VerticalForm::kTateChuYoko})
                                   .add(u8"年"),
                               tg::bodyType(28))}),
                      box().height(22.0f),
                      text("one paragraph · one writingMode "
                           "· three forms")
                          .font(tg::labelType(13, {0.55f, 0.53f, 0.50f, 1}))
                          .width(300.0f)}),
             text("cluster-unit entrance staggers DOWN the column, "
                  "columns advance right to left")
                 .font(tg::labelType(13, {0.48f, 0.46f, 0.44f, 1}))
                 .left(64)
                 .bottom(46)});
  }
};

}  // namespace

SIGIL_SKETCH_AS(Tategaki, "tategaki", "Catalog · Type",
                "vertical-rl CJK — three forms, one paragraph")
