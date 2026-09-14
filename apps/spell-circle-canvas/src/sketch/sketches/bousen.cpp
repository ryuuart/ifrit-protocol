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
// The cascade is on a strip of its own, beating over weave::Unit::Line — one
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

// TAGS: Typography/CJK

#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/kit/Features.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/query/Selector.h>

#include <utility>

#include "tategaki/VerticalSpecimen.h"

namespace sketch = sigil::sketch;

namespace motion = sigil::motion;

using namespace sigil::compose;
namespace weave = sigil::weave;
using namespace std::chrono_literals;

namespace {

constexpr SkSize kSceneSize = vertical::kSceneSize;

namespace bousen {
// The chassis: the face, the two style registers and the specimen block,
// plus this plate's inks — it is printed as ink on unbleached paper.
using namespace vertical;
using namespace vertical::paper;

constexpr float kH = kSceneSize.fHeight;
constexpr float kBodySize = 27;
constexpr float kBlockW = 300;
constexpr float kBlockH = 430;
constexpr float kBlockRight = 60;

/** A COLUMN: the one block every vertical leaf on this plate is set in. */
const weave::Block kColumn{.writingMode = weave::WritingMode::kVerticalRL};

/** The same body style asking the face for the metrics and forms it keeps
 *  for a column: punctuation pulled onto the column axis, full-width marks
 *  set to the space their ink needs, small kana cut for a column. What the
 *  installed face answers is what the plate shows. */
inline weave::TextStyle columnFitted(float size, SkColor4f color) {
  weave::TextStyle s = body(size, color);
  s.shaping.fontFeatures = {weave::features::verticalAlternates,
                            weave::features::proportionalVerticalMetrics,
                            weave::features::verticalKana};
  return s;
}

/** The wash a highlight covers a column's pitch with: the left-hand
 *  band's own indigo, thin enough to read the letters through. */
const SkColor4f kAiWash{kAi.fR, kAi.fG, kAi.fB, 0.13f};

/** A paint that draws its glyphs in @p ink and carries one band beside
 *  the column: `kUnderline` runs down the RIGHT of the column, `kOverline`
 *  down the left, `kHighlight` across the whole pitch. */
inline weave::PaintStyle banded(SkColor4f ink, weave::Decoration::Kind kind,
                                SkColor4f band, float thickness) {
  weave::PaintStyle p(ink.toSkColor());
  p.foreground.setAntiAlias(true);
  weave::Decoration decoration;
  decoration.kind = kind;
  decoration.color = band.toSkColor();
  decoration.thickness = thickness;
  p.addDecoration(decoration);
  return p;
}

/** The strip's entrance, and the ms its master must span to run at those
 *  numbers: one beat a COLUMN, and the strip sets four of them. */
const motion::Spread kColumnEntrance{.eachMs = 210, .durationMs = 520};
const float kColumnEntranceSpan = kColumnEntrance.spanMs(4);

}  // namespace bousen

struct Bousen {
  /// After the columns have assembled: the plate is the finished page.

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 2.6,
                             .background = SkColor4f{1, 1, 1, 1}});
    ctx.composer.render(describe());
  }

  /** One band convention, drawn: a short column wearing exactly one
   *  decoration, captioned with the side it stands on. The footer used to
   *  say the three in words; a specimen says them in the same terms the
   *  page above uses. */
  Element bandSpecimen(const char* caption, weave::Decoration::Kind kind,
                       SkColor4f band, float thickness) {
    namespace bs = bousen;
    weave::TextStyle style = bs::body(18, bs::kSumi);
    style.paint = bs::banded(bs::kSumi, kind, band, thickness);
    return bs::specimen(
        caption, bs::labelType(9, bs::kUsu, 0.6f),
        text(u8"傍線例", style).width(28.0f).height(62.0f).block(bs::kColumn),
        132.0f, 8.0f);
  }

  /** One punctuation column, captioned: the same characters set twice,
   *  once as the face gives them and once as it gives them when asked. */
  Element specimen(const char* caption, const weave::TextStyle& style) {
    namespace bs = bousen;
    return bs::specimen(caption, bs::labelType(11, bs::kAi, 1.5f),
                        text(u8"「あっ」、。", style)
                            .width(42.0f)
                            .height(150.0f)
                            .block(bs::kColumn),
                        96.0f, 10.0f);
  }

  Element describe() {
    namespace bs = bousen;
    namespace ch = choreograph;

    Fill ground =
        linearGradient({0, 0}, {0, bs::kH}, {bs::kKinariLift, bs::kKinari});

    auto passage =
        weave::rich(bs::body(bs::kBodySize, bs::kSumi))
            .add(u8"縦組みの本文にも、")
            .add(u8"傍線")
            .add(u8"を引くことができる。線は列の右に立ち、")
            .add(u8"約物")
            .add(u8"は列の心に寄る。")
            .add(u8"小書きの仮名")
            .add(u8"は縦の形に替わり、列は右から左へ組み上がってゆく。");

    // The Latin the plate labels itself in, stated once: every caption and
    // note under here inherits it, and names only what differs.
    return box()
        .fill(std::move(ground))
        .font({.size = 10})
        .ink(bs::kUsu)
        .children(
            {text(std::move(passage))
                 .absolute()
                 .right(bs::kBlockRight)
                 .top(96)
                 .width(bs::kBlockW)
                 .height(bs::kBlockH)
                 .block(bs::kColumn)
                 // The band the plate is named for: down the RIGHT of the
                 // column, the length of the phrase it dresses.
                 .spanPaint(
                     weave::selectors::text(u8"傍線"),
                     bs::banded(bs::kSumi, weave::Decoration::Kind::kUnderline,
                                bs::kAka, 2.5f))
                 // Its opposite, down the left.
                 .spanPaint(
                     weave::selectors::text(u8"約物"),
                     bs::banded(bs::kSumi, weave::Decoration::Kind::kOverline,
                                bs::kAi, 2.0f))
                 // A highlight covers the column PITCH — there is no cap
                 // band across a column to hang one on.
                 .spanPaint(
                     weave::selectors::text(u8"小書きの仮名"),
                     bs::banded(bs::kSumi, weave::Decoration::Kind::kHighlight,
                                bs::kAiWash, 0))
                 // A mark stands in the margin BESIDE the phrase it names:
                 // the rect it anchors to is the union of that phrase's
                 // advance boxes, and in a column those stack downward, so
                 // the note it carries runs down the page beside them.
                 .mark(
                     weave::selectors::text(u8"列は右から左へ"),
                     box()
                         .key("callout")
                         // A mark is a child of the column and inherits
                         // its writing mode; the note is Latin and reads
                         // across, so the callout says so.
                         .block(
                             {.writingMode = weave::WritingMode::kHorizontal})
                         .left(-168.0f)
                         .top(pct(0))
                         .width(168.0f)
                         // THE LEADER. A note standing in the margin is a
                         // note about nothing until something joins it to
                         // the phrase; the rule runs from the text block
                         // to the mark's own left edge, which is the
                         // phrase's edge, so it lands where the anchor is
                         // rather than where a coordinate would have put
                         // it.
                         .children({kit::at(box().key("leader").absolute().fill(
                                                Fill::color(bs::kAka)),
                                            0.0f, 42.0f, 168.0f, 1.0f),
                                    text(weave::rich()
                                             .add("mark() ",
                                                  weave::Type{.size = 11,
                                                              .color = bs::kAka,
                                                              .track = 1})
                                             .add("— anchored to "
                                                  "the phrase,\nnot to a "
                                                  "coordinate"))
                                        .width(150.0f)})),
             // The plate names itself in the other writing mode, so the two
             // stand side by side.
             box()
                 .absolute()
                 .inset(64, 92, 0, 0)
                 .column()
                 .gap(10)
                 .children(
                     {text("傍線", bs::body(44, bs::kSumi)),
                      kit::line({.length = Dimension(120),
                                 .fill = Fill::color(bs::kAka)}),
                      text("THE COLUMN'S FURNITURE")
                          .font({.size = 13, .color = bs::kAi, .track = 3}),
                      text("a band beside the column, not beneath a\n"
                           "line · a mark on the phrase it names")
                          .font({.size = 13, .color = bs::kSumi, .track = 0.4f})
                          .width(260.0f),
                      box().height(20.0f),
                      box().row().gap(30).children(
                          {specimen("AS THE FACE GIVES IT",
                                    bs::body(26, bs::kSumi)),
                           specimen("valt · vpal · vkna",
                                    bs::columnFitted(26, bs::kAka))}),
                      box().height(14.0f),
                      text("the pair is one string set twice: the "
                           "second asks\nthe face for the metrics it "
                           "keeps for a column")
                          .font({.size = 11})
                          .width(300.0f)}),
             // The cascade lives on its own strip, and it wears a band. A track
             // draws its glyphs itself, in batched buckets that carry glyphs
             // alone, so the sideline is drawn beside them at the placement the
             // layout left it: it stands still down the whole column while the
             // letters travel into it.
             text(u8"列ごとに文字が現れる。右から左へ。", bs::body(21, bs::kAi))
                 .absolute()
                 .inset(352, 150, 0, 0)
                 .width(120.0f)
                 .height(300.0f)
                 .block(bs::kColumn)
                 .spanPaint(
                     weave::selectors::text(u8"右から左へ"),
                     bs::banded(bs::kAi, weave::Decoration::Kind::kUnderline,
                                bs::kAka, 2.0f))
                 .fx({.effect = fx::rise(18),
                      .stagger = bs::kColumnEntrance,
                      .unit = weave::Unit::Line,
                      .progress = animate(motion::from(0.0f).to(1.0f),
                                          {std::chrono::milliseconds(
                                               (int)bs::kColumnEntranceSpan),
                                           &ch::easeNone, 220ms})}),
             text("↑ this strip's entrance beats over\n"
                  "weave::Unit::Line — one COLUMN a beat,\n"
                  "and its band stands at rest")
                 .absolute()
                 .inset(300, 466, 0, 0)
                 .width(180.0f),
             // The three conventions, each on a column of its own, so the page
             // shows them side by side instead of naming them in a footer.
             box()
                 .absolute()
                 .inset(64, 512, 0, 0)
                 .row()
                 .gap(26)
                 .children({bandSpecimen("UNDERLINE · RIGHT",
                                         weave::Decoration::Kind::kUnderline,
                                         bs::kAka, 2.5f),
                            bandSpecimen("OVERLINE · LEFT",
                                         weave::Decoration::Kind::kOverline,
                                         bs::kAi, 2.0f),
                            bandSpecimen("HIGHLIGHT · PITCH",
                                         weave::Decoration::Kind::kHighlight,
                                         bs::kAiWash, 0)}),
             text("the entrance beats over COLUMNS · a band is "
                  "beside the column, never beneath a line")
                 .font({.size = 12})
                 .absolute()
                 .inset(64, bs::kH - 44, 0, 0)});
  }
};

}  // namespace

SIGIL_SKETCH_AS(Bousen, "bousen", "Catalog · Type",
                "vertical columns — sidelines, alternates, marks")
