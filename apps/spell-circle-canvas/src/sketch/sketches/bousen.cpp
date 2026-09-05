/** @file
 * bousen — the furniture a vertical column carries: the sideline beside
 * it, the alternates the face is asked for, a mark anchored to one phrase,
 * and an entrance that beats column by column.
 */

// 傍線 — everything a column carries once the letters stand: the band that runs
// beside it, the metrics a face keeps for a column and hands over only when
// asked, the anchor a caller hangs off one phrase, and the reading order a
// cascade beats in.
//
// Four things are on the page, and each is one declaration:
//
//   · a red 傍線 down the right of one phrase, and its opposite down the
//     left of another — one Decoration each, on a span, drawn beside the
//     column the way an underline is drawn beneath a line;
//   · a highlight over a third phrase, which in a column covers the column
//     pitch rather than a cap band;
//   · a mark anchored to a fourth, standing in the margin beside the
//     characters it names, because a mark's rect is the union of the
//     advance boxes and those stack DOWN;
//   · a punctuation pair: one column plain, one asking the face for its
//     vertical alternates and kana forms, so what those tags do on the
//     installed face is on the page rather than in a comment.
//
// The cascade is on a strip of its own, beating over unit::Line — one
// COLUMN a beat — because a band and a track do not share a node: a track
// draws its own glyphs in batched buckets and a bucket carries glyphs
// alone. The plate is the settled page.
//
// EDIT THESE FIRST
//   kBodySize            — the passage's own size; the column pitch, and
//                          therefore how wide a highlight is, follows it.
//   kBlockW / kBlockH    — the passage's measure and depth, which is what
//                          decides where its columns break and so which
//                          band crosses a break.
//   kAka / kAi           — the two band inks: vermilion for the right-hand
//                          sideline, indigo for the left-hand one.

#include <shared/VerticalSpecimen.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/kit/Features.h>

#include <utility>

namespace sketch = sigil::sketch;

namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {

constexpr SkSize kSceneSize = vertical::kSceneSize;

namespace bousen {
// The chassis: the face, the two style registers and the specimen block,
// plus this plate's inks — it is printed as ink on unbleached paper.
using namespace vertical;
using namespace vertical::paper;

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;
constexpr float kBodySize = 27;
constexpr float kBlockW = 300;
constexpr float kBlockH = 430;
constexpr float kBlockRight = 60;

/** The same body style asking the face for the metrics and forms it keeps
 *  for a column: punctuation pulled onto the column axis, full-width marks
 *  set to the space their ink needs, small kana cut for a column. What the
 *  installed face answers is what the plate shows. */
inline sigil::weave::TextStyle columnFitted(float size, SkColor4f color) {
  sigil::weave::TextStyle s = body(size, color);
  s.shaping.fontFeatures = {sigil::weave::features::verticalAlternates,
                            sigil::weave::features::proportionalVerticalMetrics,
                            sigil::weave::features::verticalKana};
  return s;
}

/** Latin captions, which stay horizontal — the plate labels itself in the
 *  other writing mode so the two are side by side. */
inline sigil::weave::TextStyle label(float size, SkColor4f color,
                                     float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** A paint that draws its glyphs in @p ink and carries one band beside
 *  the column: `kUnderline` runs down the RIGHT of the column, `kOverline`
 *  down the left, `kHighlight` across the whole pitch. */
inline sigil::weave::PaintStyle banded(SkColor4f ink,
                                       sigil::weave::Decoration::Kind kind,
                                       SkColor4f band, float thickness) {
  sigil::weave::PaintStyle p(ink.toSkColor());
  p.foreground.setAntiAlias(true);
  sigil::weave::Decoration decoration;
  decoration.kind = kind;
  decoration.color = band.toSkColor();
  decoration.thickness = thickness;
  p.addDecoration(decoration);
  return p;
}

/** The strip's entrance, and the ms its master must span to run at those
 *  numbers: one beat a COLUMN, and the strip sets four of them. */
const sigil::motion::Spread kColumnEntrance{.eachMs = 210, .durationMs = 520};
const float kColumnEntranceSpan = kColumnEntrance.spanMs(4);

}  // namespace bousen

struct Bousen final : sketch::Sketch {
  /// After the columns have assembled: the plate is the finished page.

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 2.6,
                             .background = SkColor4f{1, 1, 1, 1}});
    ctx.composer.render(describe());
  }

  /** One band convention, drawn: a short column wearing exactly one
   *  decoration, captioned with the side it stands on. The footer used to
   *  say the three in words; a specimen says them in the same terms the
   *  page above uses. */
  Element bandSpecimen(const char* caption, sigil::weave::Decoration::Kind kind,
                       SkColor4f band, float thickness) {
    namespace bs = bousen;
    sigil::weave::TextStyle style = bs::body(18, bs::kSumi);
    style.paint = bs::banded(bs::kSumi, kind, band, thickness);
    return bs::specimen(
        caption, bs::label(9, bs::kUsu, 0.6f),
        text(u8"\xe5\x82\x8d\xe7\xb7\x9a\xe4\xbe\x8b", style)
            .width(Dim(28.0f))
            .height(Dim(62.0f))
            .writingMode(sigil::weave::WritingMode::kVerticalRL),
        132.0f, 8.0f);
  }

  /** One punctuation column, captioned: the same characters set twice,
   *  once as the face gives them and once as it gives them when asked. */
  Element specimen(const char* caption, const sigil::weave::TextStyle& style) {
    namespace bs = bousen;
    return bs::specimen(
        caption, bs::label(11, bs::kAi, 1.5f),
        text(u8"「あっ」、。", style)
            .width(Dim(42.0f))
            .height(Dim(150.0f))
            .writingMode(sigil::weave::WritingMode::kVerticalRL),
        96.0f, 10.0f);
  }

  Element describe() {
    namespace bs = bousen;
    namespace ch = choreograph;

    Fill ground =
        linearGradient({0, 0}, {0, bs::kH}, {bs::kKinariLift, bs::kKinari});

    auto passage =
        rich(bs::body(bs::kBodySize, bs::kSumi))
            .add(u8"縦組みの本文にも、")
            .add(u8"傍線")
            .add(u8"を引くことができる。線は列の右に立ち、")
            .add(u8"約物")
            .add(u8"は列の心に寄る。")
            .add(u8"小書きの仮名")
            .add(u8"は縦の形に替わり、列は右から左へ組み上がってゆく。");

    return box()
        .fill(std::move(ground))
        .child(
            text(std::move(passage))
                .absolute()
                .inset(bs::kW - bs::kBlockRight - bs::kBlockW, 96,
                       bs::kBlockRight, 0)
                .width(Dim(bs::kBlockW))
                .height(Dim(bs::kBlockH))
                .writingMode(sigil::weave::WritingMode::kVerticalRL)
                // The band the plate is named for: down the RIGHT of the
                // column, the length of the phrase it dresses.
                .spanPaint(
                    sel::text(u8"傍線"),
                    bs::banded(bs::kSumi,
                               sigil::weave::Decoration::Kind::kUnderline,
                               bs::kAka, 2.5f))
                // Its opposite, down the left.
                .spanPaint(sel::text(u8"約物"),
                           bs::banded(bs::kSumi,
                                      sigil::weave::Decoration::Kind::kOverline,
                                      bs::kAi, 2.0f))
                // A highlight covers the column PITCH — there is no cap
                // band across a column to hang one on.
                .spanPaint(
                    sel::text(u8"小書きの仮名"),
                    bs::banded(bs::kSumi,
                               sigil::weave::Decoration::Kind::kHighlight,
                               {bs::kAi.fR, bs::kAi.fG, bs::kAi.fB, 0.13f}, 0))
                // A mark stands in the margin BESIDE the phrase it names:
                // the rect it anchors to is the union of that phrase's
                // advance boxes, and in a column those stack downward, so
                // the note it carries runs down the page beside them.
                .mark(sel::text(u8"列は右から左へ"),
                      box()
                          .key("callout")
                          .left(Dim(-168.0f))
                          .top(pct(0))
                          .width(Dim(168.0f))
                          // THE LEADER. A note standing in the margin is a
                          // note about nothing until something joins it to
                          // the phrase; the rule runs from the text block
                          // to the mark's own left edge, which is the
                          // phrase's edge, so it lands where the anchor is
                          // rather than where a coordinate would have put
                          // it.
                          .child(box()
                                     .key("leader")
                                     .absolute()
                                     .left(Dim(0.0f))
                                     .top(Dim(42.0f))
                                     .width(Dim(168.0f))
                                     .height(Dim(1.0f))
                                     .fill(Fill::color(bs::kAka)))
                          .child(text(rich(bs::label(10, bs::kUsu))
                                          .add(toU8("mark() "),
                                               bs::label(11, bs::kAka, 1))
                                          .add(toU8("\xe2\x80\x94 anchored to "
                                                    "the phrase,\nnot to a "
                                                    "coordinate")))
                                     .width(Dim(150.0f)))))
        // The plate names itself in the other writing mode, so the two
        // stand side by side.
        .child(
            box()
                .absolute()
                .inset(64, 92, 0, 0)
                .column()
                .gap(10)
                .child(text(toU8("\xe5\x82\x8d\xe7\xb7\x9a"),
                            bs::body(44, bs::kSumi)))
                .child(box()
                           .width(Dim(120.0f))
                           .height(Dim(1.0f))
                           .fill(Fill::color(bs::kAka)))
                .child(text(toU8("THE COLUMN'S FURNITURE"),
                            bs::label(13, bs::kAi, 3)))
                .child(text(toU8("a band beside the column, not beneath a\n"
                                 "line \xc2\xb7 a mark on the phrase it names"),
                            bs::label(13, bs::kSumi, 0.4f))
                           .width(Dim(260.0f)))
                .child(box().height(Dim(20.0f)))
                .child(box()
                           .row()
                           .gap(30)
                           .child(specimen("AS THE FACE GIVES IT",
                                           bs::body(26, bs::kSumi)))
                           .child(specimen("valt \xc2\xb7 vpal \xc2\xb7 vkna",
                                           bs::columnFitted(26, bs::kAka))))
                .child(box().height(Dim(14.0f)))
                .child(text(toU8("the pair is one string set twice: the "
                                 "second asks\nthe face for the metrics it "
                                 "keeps for a column"),
                            bs::label(11, bs::kUsu))
                           .width(Dim(300.0f))))
        // The cascade lives on its own strip, and it wears a band. A track
        // draws its glyphs itself, in batched buckets that carry glyphs
        // alone, so the sideline is drawn beside them at the placement the
        // layout left it: it stands still down the whole column while the
        // letters travel into it.
        .child(
            text(u8"列ごとに文字が現れる。右から左へ。", bs::body(21, bs::kAi))
                .absolute()
                .inset(352, 150, 0, 0)
                .width(Dim(120.0f))
                .height(Dim(300.0f))
                .writingMode(sigil::weave::WritingMode::kVerticalRL)
                .spanPaint(
                    sel::text(u8"右から左へ"),
                    bs::banded(bs::kAi,
                               sigil::weave::Decoration::Kind::kUnderline,
                               bs::kAka, 2.0f))
                .fx({.effect = fx::rise(18),
                     .stagger = bs::kColumnEntrance,
                     .over = unit::Line,
                     .progress = animate(motion::from(0.0f).to(1.0f),
                                         {std::chrono::milliseconds(
                                              (int)bs::kColumnEntranceSpan),
                                          &ch::easeNone, 220ms})}))
        .child(text(toU8("\xe2\x86\x91 this strip's entrance beats over\n"
                         "unit::Line \xe2\x80\x94 one COLUMN a beat,\n"
                         "and its band stands at rest"),
                    bs::label(10, bs::kUsu))
                   .absolute()
                   .inset(300, 466, 0, 0)
                   .width(Dim(180.0f)))
        // The three conventions, each on a column of its own, so the page
        // shows them side by side instead of naming them in a footer.
        .child(
            box()
                .absolute()
                .inset(64, 512, 0, 0)
                .row()
                .gap(26)
                .child(bandSpecimen("UNDERLINE \xc2\xb7 RIGHT",
                                    sigil::weave::Decoration::Kind::kUnderline,
                                    bs::kAka, 2.5f))
                .child(bandSpecimen("OVERLINE \xc2\xb7 LEFT",
                                    sigil::weave::Decoration::Kind::kOverline,
                                    bs::kAi, 2.0f))
                .child(bandSpecimen("HIGHLIGHT \xc2\xb7 PITCH",
                                    sigil::weave::Decoration::Kind::kHighlight,
                                    {bs::kAi.fR, bs::kAi.fG, bs::kAi.fB, 0.13f},
                                    0)))
        .child(text(toU8("the entrance beats over COLUMNS \xc2\xb7 a band is "
                         "beside the column, never beneath a line"),
                    bs::label(12, bs::kUsu))
                   .absolute()
                   .inset(64, bs::kH - 44, 0, 0));
  }
};

}  // namespace

SIGIL_SKETCH_AS(Bousen, "bousen", "Catalog \xc2\xb7 Type",
                "vertical columns \xe2\x80\x94 sidelines, alternates, marks")
