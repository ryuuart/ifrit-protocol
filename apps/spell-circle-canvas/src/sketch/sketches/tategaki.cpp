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

#include <shared/VerticalSpecimen.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {

constexpr SkSize kSceneSize = vertical::kSceneSize;

namespace tategaki {
// The chassis: the face, the two style registers and the specimen block,
// plus this plate's inks — it is printed white on a sumi ground.
using namespace vertical;
using namespace vertical::ink;

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;
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
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(2.4);
    Composer& composer = ctx.composer;
    composer.render(describe());
  }

  /** One form, named and shown: a Latin caption over a short column set
   *  the way the caption says. The three together are the whole per-span
   *  vocabulary, side by side at a size where the difference reads. */
  Element specimen(const char* caption, RichText run) {
    namespace tg = tategaki;
    return tg::specimen(
        caption, tg::label(12, tg::kAi, 2),
        text(std::move(run))
            .width(Dim(46.0f))
            .height(Dim(140.0f))
            .writingMode(sigil::weave::WritingMode::kVerticalRL),
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
    auto passage =
        rich(tg::body(tg::kBodySize, tg::kGofun))
            .add(u8"縦組みの文章は、上から下へ、右から左へと流れる。平成")
            .add(u8"31", tg::body(tg::kBodySize, tg::kGofun,
                                  sigil::weave::VerticalForm::kTateChuYoko))
            .add(u8"年")
            .add(u8"12", tg::body(tg::kBodySize, tg::kGofun,
                                  sigil::weave::VerticalForm::kTateChuYoko))
            .add(u8"月、組版の器")
            .add(u8"SigilWeave", tg::body(tg::kBodySize * 0.86f, tg::kAi,
                                          sigil::weave::VerticalForm::kRotated))
            .add(u8"は縦書きに対応した。字は立ち、欧文は寝る。")
            .add(u8"数字は縦中横に組み、二桁のまま読ませる。")
            .add(u8"行は列となり、列は右から左へ積まれてゆく。");

    return box()
        .fill(std::move(ground))
        .child(text(std::move(passage))
                   .absolute()
                   .inset(tg::kW - tg::kColumnBlockRight - tg::kColumnBlockW,
                          92, tg::kColumnBlockRight, 0)
                   .width(Dim(tg::kColumnBlockW))
                   .height(Dim(tg::kColumnBlockH))
                   .writingMode(sigil::weave::WritingMode::kVerticalRL)
                   // The phrase the plate is about, in vermilion — paint only,
                   // so the glyphs are exactly the glyphs the passage shaped.
                   .spanPaint(sel::text(u8"縦組み"),
                              sigil::weave::PaintStyle(tg::kAka.toSkColor()))
                   // One settling entrance, beating cluster by cluster in
                   // READING ORDER: down each column, then right to left.
                   .fx({.effect = fx::rise(30),
                        .stagger = tg::kSettle,
                        .progress = animate(
                            motion::from(0.0f).to(1.0f),
                            {std::chrono::milliseconds((int)tg::kSettleSpan),
                             &ch::easeNone, 180ms})}))
        .child(
            box()
                .absolute()
                .inset(64, 88, 0, 0)
                .column()
                .gap(10)
                .child(text(toU8("\xe7\xb8\xa6\xe7\xb5\x84\xe3\x81\xbf"),
                            tg::body(46, tg::kGofun)))
                .child(box()
                           .width(Dim(120.0f))
                           .height(Dim(1.0f))
                           .fill(Fill::color(tg::kAi)))
                .child(text(toU8("VERTICAL-RL"), tg::label(15, tg::kAi, 4)))
                .child(text(toU8("UTR#50 orientation, 'vert' forms,\n"
                                 "tate-chu-yoko digits, rotated Latin"),
                            tg::label(14, tg::kGofun, 0.5f))
                           .width(Dim(240.0f)))
                .child(box().height(Dim(26.0f)))
                .child(
                    box()
                        .row()
                        .gap(34)
                        .child(specimen(
                            "UPRIGHT",
                            rich(tg::body(28, tg::kGofun,
                                          sigil::weave::VerticalForm::kUpright))
                                .add(u8"字は立つ")))
                        .child(specimen(
                            "ROTATED",
                            rich(tg::body(24, tg::kAi,
                                          sigil::weave::VerticalForm::kRotated))
                                .add(u8"Latin lies")))
                        .child(specimen(
                            "TATE-CHU-YOKO",
                            rich(tg::body(28, tg::kGofun))
                                .add(u8"令和")
                                .add(u8"07",
                                     tg::body(28, tg::kAka,
                                              sigil::weave::VerticalForm::
                                                  kTateChuYoko))
                                .add(u8"年"))))
                .child(box().height(Dim(22.0f)))
                .child(text(toU8("one paragraph \xc2\xb7 one writingMode "
                                 "\xc2\xb7 three forms"),
                            tg::label(13, {0.55f, 0.53f, 0.50f, 1}))
                           .width(Dim(300.0f))))
        .child(text(toU8("cluster-unit entrance staggers DOWN the column, "
                         "columns advance right to left"),
                    tg::label(13, {0.48f, 0.46f, 0.44f, 1}))
                   .absolute()
                   .inset(64, tg::kH - 46, 0, 0));
  }
};

}  // namespace

SIGIL_SKETCH_AS(Tategaki, "tategaki", "Catalog \xc2\xb7 Type",
                "vertical-rl CJK \xe2\x80\x94 three forms, one paragraph")
