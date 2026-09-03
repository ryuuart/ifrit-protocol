/** @file
 * exact_tangent — the rotation ladder under curved lettering, and the one
 * switch that lifts it.
 *
 * A run `onPath` is shaped once and then every glyph is placed by arc
 * length and turned to its tangent. That turn is SNAPPED by default, and
 * the reason is that every distinct rotation is both a batch bucket and a
 * glyph-atlas strike: a curve whose glyphs turned continuously would mint
 * a fresh strike per letter per frame.
 *
 * How fine the ladder is depends on the RENDERED SIZE. One step turns a
 * glyph by 2π/N, sweeping its far edge — take that as the half-em —
 * through (px/2)·2π/N pixels, so the ladder is cut at sixteen steps per
 * pixel of em and the sweep is about a fifth of a pixel at every size.
 * Both ends are clamped, at 64 steps and at 2048, and the ceiling is what
 * bounds the strike population at all. It binds from 128 px of em upward,
 * where a step's sweep starts to pass the quarter-pixel grid a moving
 * run's origins sit on again.
 *
 * So `exactTangent` changes nothing a reader can see at label sizes, and
 * that is the point. The last cell superimposes the two runs in two
 * colours at a size where the ceiling has bound for some time, and they
 * still land within a fraction of a pixel of each other — the cool run
 * covers the warm one and no fringe appears. Set the switch for STATIC
 * artwork set larger still, where the steps do show and nothing is paying
 * per frame.
 *
 * EDIT THESE FIRST
 *   kLabelSize — the size the ladder is invisible at.
 *   kDisplaySize, kDetailSize — display size, and the detail the two
 *     runs are superimposed at.
 *   kTurns — the spiral's turns, which is how tight the baseline is.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Curves.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 200;
constexpr float kPicture = 176;

constexpr float kLabelSize = 15;    // where the ladder is invisible
constexpr float kDisplaySize = 74;  // display size, still on the ladder
constexpr float kDetailSize = 260;  // one letter, cropped to a detail
constexpr float kTurns = 3.2f;      // the spiral's turns

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};
constexpr SkColor4f kSnapped{0.95f, 0.44f, 0.32f, 0.75f};
constexpr SkColor4f kExact{0.40f, 0.76f, 0.98f, 0.75f};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

weave::TextStyle inscription(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

/** A run on the tight spiral. The baseline resolves against the TEXT
 *  node's own box, so the leaf carries the plate's dimensions. */
Element run(const char* word, float size, SkColor4f colour, bool exact,
            float inset = 16) {
  return text(toU8(word), inscription(size, colour))
      .absolute()
      .inset(inset)
      .onPath({.path = shapes::spiral(kTurns),
               .align = TextPath::Align::Center,
               .at = 0.42f,
               .exactTangent = exact});
}

/** A run on the inscribed circle — the large-size cells. `offset` rides
 *  the type inside the baseline so a big face stays on the plate. */
Element arcRun(const char* word, float size, SkColor4f colour, bool exact,
               float at = 0.30f, float offset = -22, float inset = 14) {
  return text(toU8(word), inscription(size, colour))
      .absolute()
      .inset(inset)
      .onPath({.path = shapes::circle(),
               .at = at,
               .align = TextPath::Align::Center,
               .offset = offset,
               .exactTangent = exact});
}

Element plate(Element body) {
  return box()
      .width(Dim(kCell))
      .height(Dim(kPicture))
      .clip()
      .fill(Fill::color(kCellGround))
      .child(std::move(body));
}

Element cell(const char* call, const char* note, Element body) {
  return kit::cell(voice(), toU8(call), toU8(note), plate(std::move(body)));
}

}  // namespace

struct ExactTangent final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("THE TANGENT LADDER \xc2\xb7 "
                           "TextPath::exactTangent on a tight spiral"),
             .subtitle = toU8("dials \xc2\xb7 the size (15 px, then 74, then 260) "
                              "\xc2\xb7 the spiral's turns (3.2) \xc2\xb7 "
                              "exactTangent \xc2\xb7 how far off the "
                              "baseline the type rides"),
             .footer = toU8("the ladder is sixteen steps per pixel of em, "
                            "clamped between 64 and 2048 \xe2\x80\x94 so a "
                            "step sweeps a glyph's far edge about a fifth "
                            "of a pixel at every size until the ceiling "
                            "binds, which is why the switch is for artwork "
                            "set large and static"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {cell("onPath({spiral(3.2), at = 0.42})",
                           "the baseline every cell uses \xc2\xb7 one run "
                           "shaped once and placed by arc length, at label "
                           "size with the ladder ON",
                           run("a tight spiral carries its whole run",
                               kLabelSize, kFigure, false)),
                      cell("\xe2\x80\xa6" ".exactTangent = true",
                           "the same run with the ladder lifted \xc2\xb7 at "
                           "this size the two are the same picture, which "
                           "is what the default is for",
                           run("a tight spiral carries its whole run",
                               kLabelSize, kFigure, true)),
                      cell("74 px \xc2\xb7 exactTangent = false",
                           "display size on a circle \xc2\xb7 still on "
                           "the sixteen-steps-per-pixel ladder, so a step "
                           "sweeps about a fifth of a pixel here too",
                           arcRun("Ravello", kDisplaySize, kFigure,
                                  false)),
                      cell("74 px \xc2\xb7 exactTangent = true",
                           "the same letters turned to their exact "
                           "tangents \xc2\xb7 one strike per letter per "
                           "distinct angle, which a static plate can afford",
                           arcRun("Ravello", kDisplaySize, kFigure,
                                  true)),
                      cell("260 px, both at once",
                           "snapped in warm under exact in cool, cropped "
                           "to a detail \xc2\xb7 no fringe: the two land "
                           "within a fraction of a pixel, which is the "
                           "ladder doing its job",
                           box()
                               .absolute()
                               .inset(0)
                               .clip()
                               .child(arcRun("Ra", kDetailSize, kSnapped,
                                             false, 0.26f, -86, 4))
                               .child(arcRun("Ra", kDetailSize, kExact, true,
                                             0.26f, -86, 4)))},
                 .gap = 12}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(ExactTangent, "Kit \xc2\xb7 API",
             "curved lettering with the rotation ladder on and lifted, at "
             "label size where it cannot be seen and at display size "
             "magnified until it can")
