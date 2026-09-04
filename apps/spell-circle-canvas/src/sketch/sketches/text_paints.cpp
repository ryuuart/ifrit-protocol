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

constexpr SkColor4f kGround{0.06f, 0.06f, 0.075f, 1};
constexpr SkColor4f kCellGround{0.10f, 0.105f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

/** The wordmark's own face: as heavy as the machine has, so the fill has
 *  letterforms to be seen inside. */
weave::TextStyle display() {
  static const sk_sp<SkTypeface> face =
      weave::ports::pickTypeface({"Avenir Next Heavy", "Helvetica Neue Bold",
                                  "Arial Black", "Impact", "sans-serif"});
  return weave::textStyle(
      {.face = face, .size = kSize, .color = kInk, .track = 3.0f});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 8,
          .noteMeasure = kCell};
}

/** The run's box, which is what an animated field is parameterised over.
 *  One rect for all six, so the six differ only in their bodies. */
SkRect run() { return SkRect::MakeWH(1, 1); }

/** One specimen. A second fill, when given, paints a copy of the word
 *  UNDER the first — which is what a transparent field is drawn over. */
Element cell(const char* call, const char* note, paint::Paint fill,
             paint::Paint beneath = {}) {
  Element plate = kit::well({.width = kCell,
                             .height = kPicture,
                             .ground = Fill::color(kCellGround)})
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
  return kit::cell(voice(), toU8(call), toU8(note),
                   std::move(plate).child(std::move(word)));
}

Element field(const char* call, const char* note, material::Material m) {
  return cell(call, note, paint::Paint::recipe(std::move(m)));
}

}  // namespace

struct TextPaints final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // the fields are frozen at kMoment, not at the clock
    material::skia::install();  // the SkSL compiler, once per process

    ctx.composer.render(
        kit::sheet(
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
                            "crosses the capitals at any size"),
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
                     {kit::cells(
                          {.cells =
                               {field("kit::water(bounds, t)",
                                      "rippling blue with fine caustic "
                                      "highlights",
                                      material::kit::water(run(), kMoment)),
                                field("kit::meshGradient(bounds, t)",
                                      "four corners with softly moving "
                                      "control regions",
                                      material::kit::meshGradient(run(),
                                                                  kMoment)),
                                cell("kit::sparkle(bounds, t)",
                                     "a TRANSPARENT field of twinkling "
                                     "points, drawn here over a solid copy "
                                     "of the word \xc2\xb7 on its own it "
                                     "is an overlay",
                                     paint::Paint::recipe(
                                         material::kit::sparkle(run(),
                                                                kMoment)),
                                     paint::Paint::solid(
                                         {0.14f, 0.18f, 0.30f, 1})),
                                field("kit::starNest(bounds, t)",
                                      "a volumetric raymarch \xc2\xb7 the "
                                      "heaviest of the six, since it is a "
                                      "nested loop",
                                      material::kit::starNest(run(), kMoment))},
                           .gap = 14}),
                      kit::cells(
                          {.cells =
                               {field("kit::clouds(bounds, t)",
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
                 .gap = 18}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(TextPaints, "Specimen",
             "one wordmark under the six animated text fields and the two "
             "chrome ramps, all placed by the text metrics rather than by "
             "hand")
