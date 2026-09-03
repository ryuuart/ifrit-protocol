/** @file
 * sdf_star — the star silhouette, and the layers one signed distance
 * gets dressed in.
 *
 * A `sdf::Shape` is a silhouette sized by the box MINUS the style's
 * reserved pad. It is built only through its factories: the per-kind
 * parameters are the body's uP0..uP2, they mean something different in
 * each kind, and they are valid only as the factory packs them — the
 * star's `pointiness` is clamped into [2, points], so a raw triple is
 * not a shape and a shape does not change kind.
 *
 * A `sdf::Style` dresses it in one pass, back to front: shadow, glow,
 * fill, border. That is the reason to reach for a distance field rather
 * than a path — four layers, one draw, no stacked saveLayers, and a glow
 * whose falloff is exp(−d / glowRadius) rather than a blurred copy.
 *
 * `pad(style)` is the reserve those layers need INSIDE the box, and
 * `minBoxFor(style, contentPx)` is the box that leaves a stated visible
 * interior after it. Pad is layout reserve and never appearance: a
 * larger pad does not soften a glow, it only stops the box cropping it.
 *
 * EDIT THESE FIRST
 *   kPoints     — how many arms the star has.
 *   kPointiness — m, clamped into [2, points]. It is the one dial the
 *                 arms have: sweep it and watch where the notch between
 *                 two arms lands.
 *   kGlow       — the glow's falloff radius, px.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <include/core/SkPathBuilder.h>

#include <cstdio>
#include <string>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace material = sigil::material;
namespace sdf = sigil::material::sdf;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 640};
constexpr float kCell = 252;
constexpr float kPicture = 196;

constexpr int kPoints = 6;          // arms
constexpr float kPointiness = 2.6f; // m in [2, points]
constexpr float kGlow = 14;         // the glow's falloff radius, px

constexpr SkColor4f kGround{0.06f, 0.06f, 0.075f, 1};
constexpr SkColor4f kCellGround{0.085f, 0.09f, 0.105f, 1};
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

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

SkPath whole() {
  static const SkPath path =
      SkPathBuilder().addRect(SkRect::MakeWH(kCell, kPicture)).detach();
  return path;
}

/** The style every cell starts from: a warm fill under a thin border,
 *  which is the one a glow or a shadow is then added to. */
sdf::Style plain() {
  return {.fill = {0.97f, 0.78f, 0.36f, 1},
          .borderWidth = 2,
          .borderColor = {0.24f, 0.16f, 0.08f, 1}};
}

std::string line(const char* format, auto... args) {
  char buffer[224];
  std::snprintf(buffer, sizeof buffer, format, args...);
  return buffer;
}

Element cell(const char* call, const std::string& note, sdf::Shape shape,
             const sdf::Style& style) {
  return kit::cell(
      voice(), toU8(call), toU8(note),
      custom(call,
             [paint = sdf::material(shape, style)](SkCanvas& canvas,
                                                   const PaintContext& pc) {
               material::skia::fill(
                   canvas, whole(), paint,
                   {.resolution = {pc.size.width(), pc.size.height()}});
             })
          .width(kCell)
          .height(kPicture)
          .clip()
          .fill(Fill::color(kCellGround)));
}

}  // namespace

struct SdfStar final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once
    material::skia::install();  // the SkSL compiler, once per process

    sdf::Style glowing = plain();
    glowing.glowRadius = kGlow;
    glowing.glowColor = {0.98f, 0.52f, 0.20f, 1};

    sdf::Style wide = glowing;
    wide.glowRadius = kGlow * 1.6f;

    sdf::Style dropped = plain();
    dropped.shadowOffset = {0, 9};
    dropped.shadowBlur = 12;
    dropped.shadowColor = {0, 0, 0, 0.8f};

    sdf::Style heavy = plain();
    heavy.borderWidth = 9;
    heavy.borderColor = {0.42f, 0.86f, 0.92f, 1};

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("SDF STAR \xc2\xb7 sdf::star + sdf::Style + "
                           "sdf::pad"),
             .subtitle = toU8("dials \xc2\xb7 the point count (6) \xc2\xb7 "
                              "the pointiness (m in [2, points]) \xc2\xb7 "
                              "the glow radius (14 px, then 22)"),
             .footer = toU8("one draw per cell: shadow, glow, fill and "
                            "border are four layers of one distance, which "
                            "is what a path and four stacked passes would "
                            "have cost four of"),
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
                               {cell("sdf::star(6, 2)",
                                     line("the clamp's lower end \xc2\xb7 "
                                          "the notch between two arms is "
                                          "shallowest here \xc2\xb7 pad "
                                          "%.0f px",
                                          (double)sdf::pad(plain())),
                                     sdf::star(kPoints, 2), plain()),
                                cell("sdf::star(6, 3.4)",
                                     "the notch cuts deeper \xc2\xb7 the "
                                     "shape is REBUILT through the factory, "
                                     "never edited into another kind",
                                     sdf::star(kPoints, 3.4f), plain()),
                                cell("sdf::star(6, 5)",
                                     "\xe2\x80\xa6" "and deeper again, "
                                     "toward the point count the clamp "
                                     "stops at",
                                     sdf::star(kPoints, 5), plain()),
                                cell("sdf::star(12, 3)",
                                     "twice the arms at one m \xc2\xb7 one "
                                     "recipe per KIND, not per parameter",
                                     sdf::star(12, 3), plain())},
                           .gap = 14}),
                      kit::cells(
                          {.cells =
                               {cell("\xe2\x80\xa6" ".glowRadius = 14",
                                     line("exp(\xe2\x88\x92" "d / radius), "
                                          "not a blurred copy \xc2\xb7 pad "
                                          "%.0f px",
                                          (double)sdf::pad(glowing)),
                                     sdf::star(kPoints, kPointiness),
                                     glowing),
                                cell("\xe2\x80\xa6" ".glowRadius = 22",
                                     line("the falloff is the radius and "
                                          "nothing else \xc2\xb7 pad %.0f "
                                          "px, so in a fixed box the "
                                          "silhouette shrinks; minBoxFor("
                                          "style, 120) is %.0f",
                                          (double)sdf::pad(wide),
                                          (double)sdf::minBoxFor(wide, 120)),
                                     sdf::star(kPoints, kPointiness), wide),
                                cell("\xe2\x80\xa6" ".shadowOffset, "
                                     ".shadowBlur",
                                     line("the layer BEHIND the fill "
                                          "\xc2\xb7 pad %.0f px, which is "
                                          "the offset and the blur together",
                                          (double)sdf::pad(dropped)),
                                     sdf::star(kPoints, kPointiness),
                                     dropped),
                                cell("\xe2\x80\xa6" ".borderWidth = 9",
                                     "the border is CENTRED on the edge, so "
                                     "half of it is the pad and half eats "
                                     "the fill",
                                     sdf::star(kPoints, kPointiness), heavy)},
                           .gap = 14})},
                 .column = true,
                 .gap = 18}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(SdfStar, "Kit \xc2\xb7 API",
             "the star silhouette across its pointiness clamp, then the "
             "four layers one distance is dressed in and the pad each of "
             "them reserves")
