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

// TAGS: Materials/Shaders

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace sdf = sigil::material::sdf;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1000};
constexpr float kCell = 240;
constexpr float kPicture = 224;

constexpr int kPoints = 6;           // arms
constexpr float kPointiness = 2.6f;  // m in [2, points]
constexpr float kGlow = 14;          // the glow's falloff radius, px

SkPath whole() {
  return SkPathBuilder().addRect(SkRect::MakeWH(kCell, kPicture)).detach();
}

/** The style every cell starts from: a warm fill under a thin border,
 *  which is the one a glow or a shadow is then added to. */
sdf::Style plain() {
  return {.fill = {0.97f, 0.78f, 0.36f, 1},
          .borderWidth = 2,
          .borderColor = {0.24f, 0.16f, 0.08f, 1}};
}

sketch::kit::ComparisonCase cell(const char* caseTitle, const char* call,
                                 const std::string& note, sdf::Shape shape,
                                 const sdf::Style& style) {
  return {.title = caseTitle,
          .control = call,
          .figure = sketch::kit::well(
              {.width = kCell, .height = kPicture},
              custom(call,
                     [paint = sdf::material(shape, style), face = whole()](
                         SkCanvas& canvas, const PaintContext& pc) {
                       material::skia::fill(
                           canvas, face, paint,
                           {.resolution = {pc.size.width(), pc.size.height()}});
                     })),
          .note = note};
}

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.06f, 0.06f, 0.075f, 1};
  look.palette.cellGround = {0.085f, 0.09f, 0.105f, 1};
  return look;
}

}  // namespace

struct SdfStar {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

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

    ctx.composer.render(sketch::kit::page(
        {.title = "One distance, several layers",
         .subtitle = "Change the silhouette first; then compare shadow, glow, "
                     "fill and border",
         .footer = "Padding protects the effect inside the box. Increasing it "
                   "reduces the visible interior at a fixed extent."},
        box().column().gap(24).children(
            {sketch::kit::sectionHeader(
                 {.label = "THE SILHOUETTE",
                  .note = "Pointiness 2 → 3.4 → 5 · then change the number of "
                          "arms"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell(
                           "SHALLOW", "sdf::star(6, 2)",
                           "Six arms; the shallow end of the pointiness range.",
                           sdf::star(kPoints, 2), plain()),
                       cell("DEEPER", "sdf::star(6, 3.4)",
                            "Keep the point count; deepen the notches.",
                            sdf::star(kPoints, 3.4f), plain()),
                       cell("DEEPEST", "sdf::star(6, 5)",
                            "The notches approach the point-count limit.",
                            sdf::star(kPoints, 5), plain()),
                       cell("TWELVE POINTS", "sdf::star(12, 3)",
                            "Twelve arms with pointiness held at three.",
                            sdf::star(12, 3), plain())},
                  .measure = 1020,
                  .gap = 20}),
             sketch::kit::sectionHeader(
                 {.label = "FOUR WAYS TO DRESS ONE DISTANCE",
                  .note = "All four below use six points and pointiness 2.6."}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("GLOW · 14 PX",
                            "…"
                            ".glowRadius = 14",
                            "An exponential halo around the distance field.",
                            sdf::star(kPoints, kPointiness), glowing),
                       cell("GLOW · 22.4 PX",
                            "…"
                            ".glowRadius = 22",
                            "A wider falloff also reserves more space inside "
                            "the box.",
                            sdf::star(kPoints, kPointiness), wide),
                       cell("DROP SHADOW",
                            "…"
                            ".shadowOffset, "
                            ".shadowBlur",
                            "Offset and blur reserve room behind the "
                            "silhouette.",
                            sdf::star(kPoints, kPointiness), dropped),
                       cell("BORDER · 9 PX",
                            "…"
                            ".borderWidth = 9",
                            "The centred border uses space inside and outside "
                            "the edge.",
                            sdf::star(kPoints, kPointiness), heavy)},
                  .measure = 1020,
                  .gap = 20}),
             sketch::kit::sectionHeader(
                 {.label = "LAYOUT RESERVE IS VISIBLE", .note = ""}),
             text(kit::formatted(
                      "Reserved padding   glow %.0f px   /   wide glow %.0f px "
                      "  /   shadow %.0f px\nA 120 px interior with the wide "
                      "glow needs a %.0f px box.",
                      (double)sdf::pad(glowing), (double)sdf::pad(wide),
                      (double)sdf::pad(dropped),
                      (double)sdf::minBoxFor(wide, 120)))
                 .width(1020)
                 .styleClass("captionNote")})));
  }
};

SIGIL_SKETCH(SdfStar, "Kit · API",
             "the star silhouette across its pointiness clamp, then the "
             "four layers one distance is dressed in and the pad each of "
             "them reserves")
