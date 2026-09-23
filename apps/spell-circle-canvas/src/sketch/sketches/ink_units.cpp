/** @file
 * ONE RAMP, FOUR UNITS. The same unit-square gradient as the ink of the
 * same line of type, laid once across the passage and then restarted on
 * every letter, every word and every line.
 */
// TAGS: Typography/Paint

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <optional>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace paint = sigil::material::skia;
using namespace sigil::compose;
using sigil::weave::Unit;

namespace {

/** The one ramp, authored in the unit square, so each panel differs only
 *  in the box that square lands on. */
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

/** Two lines of two words, inked with the ramp restarting on @p unit — or
 *  once across the passage, where there is none. */
Element passage(std::optional<Unit> unit) {
  return text(u8"EMBER GLASS\nSALT WATER")
      .font(display(40))
      .ink(ramp(), PaintAnchor::OwnBox, unit);
}

Element panel(std::string title, std::string note, Element figure) {
  return sketch::kit::well({.width = 480, .height = 220, .padding = 22})
      .column()
      .gap(12)
      .children({document::eyebrow(std::move(title)), std::move(figure),
                 document::caption(std::move(note))});
}

}  // namespace

struct InkUnits {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1100, 700}, .captureAt = 0.05});
    ctx.composer.render(sketch::kit::page(
        {.title = "A paint that restarts on each unit",
         .subtitle = "One unit-square ramp as the ink of one passage · laid "
                     "once, then afresh on every letter, word and line",
         .footer = "Each unit's box runs across its advances and from its "
                   "cap top down to its baseline, as the passage's does."},
        box().column().gap(20).children(
            {box().row().gap(20).children(
                 {panel("THE PASSAGE", "One ramp across the whole of it.",
                        passage(std::nullopt)),
                  panel("EACH GLYPH", "The ramp afresh on every letter.",
                        passage(Unit::Glyph))}),
             box().row().gap(20).children(
                 {panel("EACH WORD", "The ramp afresh on every word.",
                        passage(Unit::Word)),
                  panel("EACH LINE", "The ramp afresh on every line.",
                        passage(Unit::Line))})})));
  }
};

SIGIL_SKETCH(InkUnits, "Study · Paint",
             "one unit-square ramp as the ink of one passage, laid across "
             "the passage and restarted on every letter, word and line")
