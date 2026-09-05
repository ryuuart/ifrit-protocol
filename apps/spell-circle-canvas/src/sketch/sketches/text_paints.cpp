/** @file
 * text_paints — one word, eight fills, and the coordinate space that
 * makes them work at any size.
 *
 * `textFill` paints the GLYPHS with a material mapped to TEXT-METRIC
 * space: the material's unit square lands with x across the widest line
 * and y from the first line's CAP TOP — real cap height, off the face's
 * metrics — to the last line's baseline. That is what makes a chrome
 * wordmark work: author the ramp once in [0, 1] and its horizon crosses
 * the capitals whatever the font size, with no hand-positioned
 * gradients. It supersedes the style's foreground paint and combines
 * with `fx()`, so a letter in flight is painted exactly as a resting one
 * is.
 *
 * The six animated fields share one ABI — the run's origin and extent,
 * the clock, and a slow two-axis drift derived from it — so the params
 * are built by one call and the six differ only in their bodies. They
 * are held at one moment here; bind the clock and they run.
 *
 * The two chrome ramps are not fields at all: they are stop lists in
 * unit space, handed straight to the same verb.
 *
 * EDIT THESE FIRST
 *   kWord    — the word. Capitals, because the mapping is cap height.
 *   kSize    — the type size, px. Change it and nothing else moves.
 *   kMoment  — the second every animated field is frozen at.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Gloss.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace material = sigil::material;
namespace paint = sigil::material::skia;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 596};
constexpr float kCell = 252;
constexpr float kPicture = 168;

constexpr const char* kWord = "SIGIL";
constexpr float kSize = 56;      // the type size, px
constexpr float kMoment = 6.4f;  // the second every field is frozen at

constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.ground = {0.06f, 0.06f, 0.075f, 1};
  look.palette.cellGround = {0.10f, 0.105f, 0.125f, 1};
  look.spacing.captionGap = 8;
  return look;
}

/** The wordmark's own face: as heavy as the machine has, so the fill has
 *  letterforms to be seen inside. */
weave::TextStyle display() {
  const sk_sp<SkTypeface> face =
      weave::ports::face({"Avenir Next Heavy", "Helvetica Neue Bold",
                          "Arial Black", "Impact", "sans-serif"});
  return weave::textStyle(
      {.face = face, .size = kSize, .color = kInk, .track = 3.0f});
}

/** The run's box, which is what an animated field is parameterised over.
 *  One rect for all six, so the six differ only in their bodies. */
SkRect run() { return SkRect::MakeWH(1, 1); }

/** One specimen. A second fill, when given, paints a copy of the word
 *  UNDER the first — which is what a transparent field is drawn over. */
Element cell(const char* call, const char* note, paint::Paint fill,
             paint::Paint beneath = {}) {
  Element plate = sketch::kit::well({.width = kCell, .height = kPicture})
                      .alignItems(Align::Center)
                      .justify(Justify::Center);
  Element word = text(toU8(kWord), display()).textFill(std::move(fill));
  if (beneath.isSolid() || beneath.asShader())
    plate.child(
        box()
            .absolute()
            .inset(0)
            .alignItems(Align::Center)
            .justify(Justify::Center)
            .child(text(toU8(kWord), display()).textFill(std::move(beneath))));
  return sketch::kit::caption(kCell, toU8(call), toU8(note),
                              std::move(plate).child(std::move(word)));
}

Element field(const char* call, const char* note, material::Material m) {
  return cell(call, note, paint::Paint::recipe(std::move(m)));
}

}  // namespace

struct TextPaints final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    // the fields are frozen at kMoment, not at the clock
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    material::skia::install();  // the SkSL compiler, once per process

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("TEXT PAINTS \xc2\xb7 Element::textFill over "
                       "kit::water, meshGradient, sparkle, starNest, "
                       "clouds, tunnel"),
         .subtitle = toU8("dials \xc2\xb7 the paint \xc2\xb7 the type "
                          "size (56 px \xe2\x80\x94 change it and the "
                          "fills do not move) \xc2\xb7 the moment "
                          "(6.4 s)"),
         .footer = toU8("the material's unit square lands with x across "
                        "the widest line and y from cap top to "
                        "baseline, so a ramp authored once in [0, 1] "
                        "crosses the capitals at any size")},
        kit::cells(
            {.cells =
                 {kit::cells(
                      {.cells =
                           {field("kit::water(bounds, t)",
                                  "rippling blue with fine caustic "
                                  "highlights",
                                  material::kit::water(run(), kMoment)),
                            field("kit::meshGradient(bounds, t)",
                                  "four corners with softly moving "
                                  "control regions",
                                  material::kit::meshGradient(run(), kMoment)),
                            cell("kit::sparkle(bounds, t)",
                                 "a TRANSPARENT field of twinkling "
                                 "points, drawn here over a solid copy "
                                 "of the word \xc2\xb7 on its own it "
                                 "is an overlay",
                                 paint::Paint::recipe(
                                     material::kit::sparkle(run(), kMoment)),
                                 paint::Paint::solid({0.14f, 0.18f, 0.30f, 1})),
                            field("kit::starNest(bounds, t)",
                                  "a volumetric raymarch \xc2\xb7 the "
                                  "heaviest of the six, since it is a "
                                  "nested loop",
                                  material::kit::starNest(run(), kMoment))},
                       .gap = 14}),
                  kit::cells(
                      {.cells = {field("kit::clouds(bounds, t)",
                                       "layered ridged and fbm noise "
                                       "drifting on the shared motion "
                                       "vector",
                                       material::kit::clouds(run(), kMoment)),
                                 field("kit::tunnel(bounds, t)",
                                       "an endless kaleidoscope falling "
                                       "away \xc2\xb7 the same ABI, a very "
                                       "different body",
                                       material::kit::tunnel(run(), kMoment)),
                                 cell("kit::sunsetChromeType()",
                                      "not a field at all \xc2\xb7 a stop "
                                      "list in UNIT space, so the hard "
                                      "horizon lands at half cap height",
                                      kit::sunsetChromeType()),
                                 cell("kit::silverChromeType()",
                                      "the same construction, colder "
                                      "\xc2\xb7 one ramp, and the metrics do "
                                      "the placing",
                                      kit::silverChromeType())},
                       .gap = 14})},
             .column = true,
             .gap = 18})));
  }
};

SIGIL_SKETCH(TextPaints, "Specimen",
             "one wordmark under the six animated text fields and the two "
             "chrome ramps, all placed by the text metrics rather than by "
             "hand")
