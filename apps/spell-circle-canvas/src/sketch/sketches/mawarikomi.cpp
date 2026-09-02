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

#include <sigilcompose/shape/Shapes.h>
#include <sigilsketch/canvas/Sketch.h>

#include "VerticalSpecimen.h"

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;

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

struct Mawarikomi final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    ctx.background({1, 1, 1, 1});
    ctx.captureAt(1.0);
    ctx.composer.render(describe());
  }

  /** One clamped column, captioned: the same clamp in two scripts, so the
   *  two forms the marker takes stand next to each other. */
  Element specimen(const char* caption, const char8_t* text8,
                   const sigil::weave::TextStyle& style) {
    namespace mw = mawari;
    return mw::specimen(caption, mw::label(10, mw::kAi, 1.4f),
                        text(text8, style)
                            .width(Dim(46.0f))
                            .height(Dim(216.0f))
                            .writingMode(sigil::weave::WritingMode::kVerticalRL)
                            .maxLines(1)
                            .ellipsis(u8"…"),
                        150.0f, 8.0f);
  }

  Element describe() {
    namespace mw = mawari;

    Material ground = Material::linear(
        {0, 0}, {0, mw::kH}, {{0.0f, mw::kKinariLift}, {1.0f, mw::kKinari}});

    return box()
        .fill(std::move(ground))
        // THE TWO OBSTACLES. Each declares a silhouette, so what the
        // columns subtract is the shape itself and not the box around it:
        // the type reaches into the disc's corners and into the seal's
        // notches.
        .child(box()
                   .key("hinomaru")
                   .absolute()
                   .inset(mw::kBlockLeft + 132, mw::kBlockTop + 96, 0, 0)
                   .width(Dim(mw::kDiscSize))
                   .height(Dim(mw::kDiscSize))
                   .shape(shapes::circle())
                   .fill(Fill::color(mw::kAka)))
        .child(box()
                   .key("in")
                   .absolute()
                   .inset(mw::kBlockLeft + 42, mw::kBlockTop + 316, 0, 0)
                   .width(Dim(mw::kSealSize))
                   .height(Dim(mw::kSealSize))
                   .shape(shapes::star(6))
                   .fill(Fill::color(mw::kAi)))
        // The passage itself. Two exclusions, one declaration each; the
        // margin is the same standoff from either silhouette.
        .child(text(u8"縦組みの文章が障害物に出会うと、その列は頭と足に分かれ"
                    u8"る。文字は列の心に沿って落ちてゆき、形に触れる手前で止"
                    u8"まり、形を過ぎたところからまた続いてゆく。除かれるのは"
                    u8"箱ではなく形そのものだから、丸の四隅にも星の切れ込みに"
                    u8"も字は入り込む。列は右から左へ進み、上と下に分かれたま"
                    u8"ま、次の列へと組み上がってゆく。行に対して働くものは、"
                    u8"四分の一だけ回した列に対しても同じように働く。",
                    mw::body(21, mw::kSumi))
                   .absolute()
                   .inset(mw::kBlockLeft, mw::kBlockTop, 0, 0)
                   .width(Dim(mw::kBlockW))
                   .height(Dim(mw::kBlockH))
                   .writingMode(sigil::weave::WritingMode::kVerticalRL)
                   .flowAround("hinomaru", 11)
                   .flowAround("in", 9)
                   .zIndex(1))
        // The plate names itself in the other writing mode.
        .child(
            box()
                .absolute()
                .inset(64, 84, 0, 0)
                .column()
                .gap(10)
                .child(text(toU8("\xe5\x9b\x9e\xe3\x82\x8a\xe8\xbe\xbc\xe3\x81"
                                 "\xbf"),
                            mw::body(42, mw::kSumi)))
                .child(box()
                           .width(Dim(120.0f))
                           .height(Dim(1.0f))
                           .fill(Fill::color(mw::kAka)))
                .child(text(toU8("THE COLUMN PARTS, AND THE COLUMN STOPS"),
                            mw::label(12, mw::kAi, 2.6f))
                           .width(Dim(268.0f)))
                .child(text(toU8("an exclusion cuts a column exactly as it\n"
                                 "cuts a line \xc2\xb7 a clamped column ends "
                                 "in\na marker at its foot"),
                            mw::label(13, mw::kSumi, 0.4f))
                           .width(Dim(268.0f))))
        // The pair: one clamp in each script, so the marker's two forms
        // are side by side. Both columns hold far more than one column of
        // room, so both are cut.
        .child(box()
                   .absolute()
                   .inset(64, 268, 0, 0)
                   .row()
                   .gap(28)
                   .child(specimen("UPRIGHT \xc2\xb7 THE FACE'S VERT FORM",
                                   u8"一行に収まらぬときは末に印を置く",
                                   mw::body(20, mw::kSumi)))
                   .child(specimen("ROTATED \xc2\xb7 TURNED WITH THE COLUMN",
                                   u8"a Latin column turns a quarter turn and "
                                   u8"so does the marker that cuts it",
                                   mw::label(17, mw::kSumi, 0.2f))))
        .child(text(toU8("both columns are clamped to ONE column and both "
                         "overflow;\nthe cut moved up the column to make room "
                         "for the marker"),
                    mw::label(11, mw::kUsu))
                   .absolute()
                   .inset(64, 520, 0, 0)
                   .width(Dim(300.0f)))
        .child(text(toU8("silhouette \xe2\x86\x92 subtracted as itself  "
                         "\xc2\xb7  a crossed column splits into head and "
                         "foot  \xc2\xb7  the marker takes the form of the "
                         "text it cut"),
                    mw::label(12, mw::kUsu))
                   .absolute()
                   .inset(64, mw::kH - 44, 0, 0));
  }
};

}  // namespace

SIGIL_SKETCH_AS(Mawarikomi, "mawarikomi", "Catalog \xc2\xb7 Type & grid",
                "columns around a silhouette, and a clamped column's marker")
