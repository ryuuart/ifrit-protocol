/** @file
 * horizontal_flow — a line meeting a silhouette, and an ornamented initial
 * opening a paragraph.
 *
 * The left passage crosses a star in the middle of its measure. Each
 * horizontal band receives the intervals on both sides of the silhouette,
 * then becomes a full line again below it. The right passage begins with a
 * caller-built ORNAMENT — an element with a key and a silhouette — and the
 * paragraph flows around that key, so the same line-flow rule follows its
 * outline. (A plain letter needs none of this: `initialLetter` sizes and
 * seats one from the block's own pitch.)
 *
 * EDIT THESE FIRST
 *   kShapeSize — how much of the left passage the central shape interrupts.
 *   kDropWidth / kDropHeight — how many opening lines the ornament occupies.
 *   kWrapMargin — the clearance between either silhouette and the type.
 */

#include <sigilcompose/core/Core.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1080, 620};
constexpr float kShapeSize = 152;
constexpr float kDropWidth = 96;
constexpr float kDropHeight = 118;
constexpr float kWrapMargin = 9;

constexpr SkColor4f kPaper{0.94f, 0.91f, 0.84f, 1};
constexpr SkColor4f kPanel{0.975f, 0.955f, 0.91f, 1};
constexpr SkColor4f kInk{0.12f, 0.11f, 0.10f, 1};
constexpr SkColor4f kQuiet{0.42f, 0.37f, 0.31f, 1};
constexpr SkColor4f kCinnabar{0.67f, 0.16f, 0.11f, 1};
constexpr SkColor4f kGold{0.78f, 0.55f, 0.16f, 1};

weave::TextStyle sans(float size, SkColor4f color, float track = 0) {
  const sk_sp<SkTypeface> face = weave::ports::face(
      {"Avenir Next", "Helvetica Neue", "DejaVu Sans", "sans-serif"});
  return weave::textStyle(
      {.face = face, .size = size, .color = color, .track = track});
}

weave::TextStyle serif(float size, SkColor4f color, float track = 0) {
  const sk_sp<SkTypeface> face = weave::ports::face(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  return weave::textStyle(
      {.face = face, .size = size, .color = color, .track = track});
}

Element caption(const char* title, const char* note) {
  return box()
      .column()
      .gap(5)
      .child(text(toU8(title), sans(11, kCinnabar, 1.8f)))
      .child(text(toU8(note), sans(11, kQuiet)).width(430));
}

Element shapePassage() {
  Element medallion =
      box()
          .key("central-star")
          .absolute()
          .left(139)
          .top(74)
          .width(kShapeSize)
          .height(kShapeSize)
          .shape(shapes::star(10, 0.62f, 0.08f))
          .fill(Fill::color(kGold))
          .child(
              text(u8"FLOW", sans(14, kInk, 1.2f)).absolute().left(49).top(66));

  Element passage =
      text(
          u8"A horizontal line begins at the left edge of its measure. "
          u8"When it reaches the ornament, the available band becomes two "
          u8"intervals: words fill the room on the left and continue in "
          u8"the room on the right. Near each point the interval changes "
          u8"with the silhouette instead of following its box. Once the "
          u8"shape has passed, the paragraph recovers its full measure and "
          u8"continues without a special text mode.",
          serif(15, kInk))
          .key("shape-passage")
          .width(430)
          .flowAround("central-star", kWrapMargin)
          .zIndex(1);

  return box()
      .width(430)
      .height(350)
      .child(std::move(medallion))
      .child(std::move(passage));
}

Element droppedPassage() {
  Element ornament =
      box()
          .width(kDropWidth)
          .height(kDropHeight)
          .shape(shapes::rounded(shapes::star(8, 0.58f, 0.12f), 5))
          .fill(Fill::color(kCinnabar))
          .child(text(u8"H", serif(50, kPaper)).absolute().left(29).top(29));
  ornament.key("illuminated-h").absolute().left(Dim(0.0f)).top(Dim(0.0f));

  return box()
      .width(430)
      .height(350)
      .child(std::move(ornament))
      .child(text(u8"orizontal setting needs no drop-cap mechanism when the "
                  u8"initial is an ornament. The ornament is an element with "
                  u8"a key and a silhouette, while this paragraph is an "
                  u8"ordinary text leaf flowing around that key. The opening "
                  u8"lines take the changing room beside the points; the "
                  u8"later lines return to the whole measure. A photograph, "
                  u8"seal, flourish, or illustrated letter uses exactly the "
                  u8"same relationship.",
                  serif(15, kInk))
                 .key("drop-passage")
                 .width(430)
                 .flowAround("illuminated-h", kWrapMargin));
}

Element panel(float left, const char* title, const char* note, Element body) {
  return box()
      .absolute()
      .left(left)
      .top(126)
      .width(476)
      .height(438)
      .padding(22)
      .column()
      .gap(18)
      .fill(Fill::color(kPanel))
      .child(caption(title, note))
      .child(std::move(body));
}

}  // namespace

struct HorizontalFlow final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx,
                       {.size = SkSize::Make(kCanvas.width(), kCanvas.height()),
                        .captureAt = 0.05,
                        .background = kPaper});

    ctx.composer.render(
        box()
            .fill(Fill::color(kPaper))
            .child(text(u8"HORIZONTAL TEXT FLOW", sans(24, kInk, 3.2f))
                       .absolute()
                       .left(42)
                       .top(34))
            .child(text(u8"one exclusion rule · a shape in the measure · an "
                        u8"ornament at the opening",
                        sans(13, kQuiet, 0.5f))
                       .absolute()
                       .left(43)
                       .top(76))
            .child(panel(42, "FLOW AROUND A SHAPE",
                         "the line divides left and right, then becomes whole",
                         shapePassage()))
            .child(panel(562, "ORNAMENTED DROP CAP",
                         "a caller-built element supplies the painted outline",
                         droppedPassage())));
  }
};

SIGIL_SKETCH(HorizontalFlow, "Catalog \xc2\xb7 Type",
             "horizontal text flowing around a central silhouette and an "
             "ornamented drop cap")
