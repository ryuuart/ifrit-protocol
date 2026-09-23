/** @file
 * WHICH BOX A PAINT IS STRETCHED OVER. One ramp, four readings: the ink
 * on a subtree over each box, and a fill stretched over the canvas
 * running through a row of cards as though they were windows cut in it.
 */
// TAGS: Typography/Paint

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace paint = sigil::material::skia;
using namespace sigil::compose;

namespace {

constexpr float kCard = 150.0f;
constexpr int kCards = 3;

/** The one ramp every panel here is painted with, authored in the unit
 *  square so that every PaintBox is a statement about the BOX and never
 *  about the ramp. */
paint::Paint ramp() {
  return paint::Paint::linearUnit({0, 0}, {1, 0},
                                  {{0.0f, {0.96f, 0.29f, 0.24f, 1}},
                                   {0.5f, {0.98f, 0.76f, 0.19f, 1}},
                                   {1.0f, {0.16f, 0.45f, 0.93f, 1}}});
}

sigil::weave::Type display(float size) {
  return {.face = sigil::weave::ports::face(
              {"Avenir Next Heavy", "Helvetica Neue Bold", "Arial Black"}),
          .size = size,
          .track = size * 0.02f};
}

/** Three words under one box that states the ink, so what changes
 *  between the panels is the box the ramp is stretched over alone. */
Element subtree(PaintBox over) {
  const auto word = [](std::u8string_view letters) {
    return text(letters).font(display(46));
  };
  return box()
      .row()
      .gap(14)
      .ink(ramp(), over)
      .children({word(u8"ONE"), word(u8"TWO"), word(u8"SIX")});
}

Element panel(std::string title, std::string note, Element figure) {
  return sketch::kit::well({.width = 340, .height = 190, .padding = 22})
      .column()
      .gap(14)
      .children({document::eyebrow(std::move(title)), std::move(figure),
                 document::caption(std::move(note))});
}

/** A row of cards, each filled with the SAME paint stretched over the
 *  canvas: what a card shows is decided by where it stands. */
Element cards() {
  Element row = box().row().gap(16);
  std::vector<Element> children;
  for (int i = 0; i < kCards; ++i)
    children.push_back(box().width(kCard).height(96).borderRadius({10}).fill(
        ramp(), PaintBox::Canvas));
  return row.children(std::move(children));
}

}  // namespace

struct PaintAnchors {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1100, 760}, .captureAt = 0.05});
    ctx.composer.render(sketch::kit::page(
        {.title = "The box a paint is stretched over",
         .subtitle = "One unit-square ramp · the ink on a subtree under each "
                     "anchor, and one canvas-anchored fill through three cards",
         .footer = "A text leaf's own box is its text-metric box, which is "
                   "why the own-box row repeats the whole ramp per word."},
        box().column().gap(26).children(
            {box().row().gap(20).children(
                 {panel("INK · OWN BOX",
                        "Each word maps the ramp onto its own metrics.",
                        subtree(PaintBox::Element)),
                  panel("INK · DECLARING BOX",
                        "One ramp across the row; each word its own slice.",
                        subtree(PaintBox::Subtree)),
                  panel("INK · CANVAS BOX",
                        "The field belongs to the canvas, not to the row.",
                        subtree(PaintBox::Canvas))}),
             document::eyebrow("FILL · CANVAS BOX"), cards(),
             document::caption(
                 "Three cards, one paint: the gaps read as though the "
                 "cards were windows cut in a single gradient.")})));
  }
};

SIGIL_SKETCH(PaintAnchors, "Study · Paint",
             "one unit-square ramp stretched over every paint box — the ink "
             "on a subtree and one fill over the canvas across three cards")
