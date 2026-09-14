/** @file
 * mawarikomi — a column meeting an obstacle: the passage that parts around
 * a silhouette, and the clamped column that ends in a marker.
 */

// 回り込み — the two things a column does when it runs out of room.
//
// It PARTS. An exclusion cuts a column exactly as it cuts a line: the
// column it crosses hands back a head above the shape and a foot below it,
// and the type falls down the head, skips the shape, and picks the foot up
// again. A silhouette is subtracted as itself, so the disc on this page
// takes back the corners a box would have eaten, and the notched seal
// beside it takes back its notches.
//
// It STOPS. A clamped column reports the text it could not take, and the
// marker that says so lands at the column's FOOT, measured against the
// column's length so the cut moves up to make room for it — the same trade
// a clamped line makes at its end. The marker is set the way the text it
// cut was set: the Japanese column ends in an upright marker, the face's
// own vertical form, and the Latin column beside it — which rotates a
// quarter turn to run down the page — ends in a marker turned with it.
// The pair is on the page rather than in this comment.
//
// The plate is a settled page: nothing here moves.
//
// EDIT THESE FIRST
//   kBlockLeft / kBlockTop    — where the passage stands on the page.
//   kBlockW / kBlockH         — its measure and its depth, which is what
//                               decides whether the clamped column has
//                               anything left to report.
//   kDiscSize / kSealSize     — the two obstacles. Grow either past the
//                               block's own measure and the column it
//                               crosses hands back no foot at all, which
//                               is the interesting edge of the parting.

// TAGS: Typography/Paragraph, Typography/CJK

#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>

#include "tategaki/VerticalSpecimen.h"

namespace sketch = sigil::sketch;

namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kSceneSize = vertical::kSceneSize;

namespace mawari {
// The chassis: the face, the two style registers and the specimen block,
// plus this plate's inks — it is printed as ink on unbleached paper.
using namespace vertical;
using namespace vertical::paper;

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

// The passage and the two shapes it parts around, in one coordinate frame
// so the page can be read off these six numbers.
constexpr float kBlockLeft = 380, kBlockTop = 88;
constexpr float kBlockW = 460, kBlockH = 470;
constexpr float kDiscSize = 168, kSealSize = 104;

}  // namespace mawari

struct Mawarikomi {
  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 1.0,
                             .background = SkColor4f{1, 1, 1, 1}});
    ctx.composer.render(describe());
  }

  /** One clamped column, captioned: the same clamp in two scripts, so the
   *  two forms the marker takes stand next to each other. */
  Element specimen(const char* caption, const char8_t* text8,
                   const sigil::weave::Type& style) {
    namespace mw = mawari;
    return mw::specimen(
        caption, mw::labelType(10, mw::kAi, 1.4f),
        text(text8)
            .font(style)
            .width(46.0f)
            .height(216.0f)
            .block({.writingMode = sigil::weave::WritingMode::kVerticalRL})
            .maxLines(1)
            .ellipsis(u8"…"),
        150.0f, 8.0f);
  }

  Element describe() {
    namespace mw = mawari;

    Fill ground =
        linearGradient({0, 0}, {0, mw::kH}, {mw::kKinariLift, mw::kKinari});

    // The plate is printed as ink on unbleached paper: every run that names
    // no colour is set in the sumi.
    return box()
        .fill(std::move(ground))
        .ink(mw::kSumi)
        // THE TWO OBSTACLES. Each declares a silhouette, so what the
        // columns subtract is the shape itself and not the box around it:
        // the type reaches into the disc's corners and into the seal's
        // notches.
        .children(
            {kit::at(mw::kBlockLeft + 132, mw::kBlockTop + 96, mw::kDiscSize,
                     mw::kDiscSize)
                 .key("hinomaru")
                 .shape(shapes::circle())
                 .fill(Fill::color(mw::kAka)),
             kit::at(mw::kBlockLeft + 42, mw::kBlockTop + 316, mw::kSealSize,
                     mw::kSealSize)
                 .key("in")
                 .shape(shapes::star(6))
                 .fill(Fill::color(mw::kAi)),
             // The passage itself. Two exclusions, one declaration each; the
             // margin is the same standoff from either silhouette.
             kit::at(
                 text(u8"縦組みの文章が障害物に出会うと、その列は頭と足に分かれ"
                      u8"る。文字は列の心に沿って落ちてゆき、形に触れる手前で止"
                      u8"まり、形を過ぎたところからまた続いてゆく。除かれるのは"
                      u8"箱ではなく形そのものだから、丸の四隅にも星の切れ込みに"
                      u8"も字は入り込む。列は右から左へ進み、上と下に分かれたま"
                      u8"ま、次の列へと組み上がってゆく。行に対して働くものは、"
                      u8"四分の一だけ回した列に対しても同じように働く。")
                     .font(mw::bodyType(21)),
                 mw::kBlockLeft, mw::kBlockTop, mw::kBlockW, mw::kBlockH)
                 .block({.writingMode = sigil::weave::WritingMode::kVerticalRL})
                 .flowAround("hinomaru", 11)
                 .flowAround("in", 9)
                 .zIndex(1),
             // The plate names itself in the other writing mode.
             box().at({64, 84}).column().gap(10).children(
                 {text("回り込み").font(mw::bodyType(42)),
                  kit::line({.length = Dimension(120),
                             .fill = Fill::color(mw::kAka)}),
                  text("THE COLUMN PARTS, AND THE COLUMN STOPS")
                      .font(mw::labelType(12, mw::kAi, 2.6f))
                      .width(268.0f),
                  text("an exclusion cuts a column exactly as it\n"
                       "cuts a line · a clamped column ends "
                       "in\na marker at its foot")
                      .font(mw::labelType(13, 0.4f))
                      .width(268.0f)}),
             // The pair: one clamp in each script, so the marker's two forms
             // are side by side. Both columns hold far more than one column of
             // room, so both are cut.
             box().at({64, 268}).row().gap(28).children(
                 {specimen("UPRIGHT · THE FACE'S VERT FORM",
                           u8"一行に収まらぬときは末に印を置く",
                           mw::bodyType(20)),
                  specimen("ROTATED · TURNED WITH THE COLUMN",
                           u8"a Latin column turns a quarter turn and "
                           u8"so does the marker that cuts it",
                           mw::labelType(17, 0.2f))}),
             text("both columns are clamped to ONE column and both "
                  "overflow;\nthe cut moved up the column to make room "
                  "for the marker")
                 .font(mw::labelType(11, mw::kUsu))
                 .at({64, 520})
                 .width(300.0f),
             text("silhouette → subtracted as itself  "
                  "·  a crossed column splits into head and "
                  "foot  ·  the marker takes the form of the "
                  "text it cut")
                 .font(mw::labelType(12, mw::kUsu))
                 .at({64, mw::kH - 44})});
  }
};

}  // namespace

SIGIL_SKETCH_AS(Mawarikomi, "mawarikomi", "Catalog · Type",
                "columns around a silhouette, and a clamped column's marker")
