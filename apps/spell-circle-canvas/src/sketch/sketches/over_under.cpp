/** @file
 * over_under — local variation is a composition of materials, never a
 * bespoke recipe per pair.
 *
 * `over(base, top, mask, blend)` returns a MATERIAL. The three operands
 * become its children, so the stack compares, animates and resolves as
 * one value, and applying `over` again builds a taller one. `under(m)`
 * steps one down and `stackDepth(m)` counts the steps, which is what a
 * consumer that can only express one material writes down.
 *
 * A MASK is an ordinary material whose red channel, clamped, says how
 * much of the top shows. Two shapes cover every source: a CONSTANT mask
 * is a number, and a SAMPLED one reads its `source` slot as a channel,
 * as a SLOPE (a tangent normal dotted with an axis — moss on the faces
 * that point up), or as a HEIGHT (the value dotted with an axis, no
 * decode — a tide line, dust on the top shelf). Both then FIT: `low` and
 * `high` remap the raw value onto 0..1 and clamp, and `invert` flips it.
 * A slope or a height mask means nothing without a fit, which is why the
 * factories take the range.
 *
 * The base is stone and the top is sheet brass, both generated rather
 * than photographed, so the whole sheet is a function of the numbers in
 * this file.
 *
 * EDIT THESE FIRST
 *   kLow, kHigh — the fit both sampled masks are read through.
 *   kBevel — the shoulder the slope mask's normals are derived from.
 */

// TAGS: Materials/Compositing

#include <include/core/SkSurface.h>
#include <include/effects/SkGradient.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/mask/Mask.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1180};
constexpr float kCell = 240;
constexpr float kPicture = 186;

constexpr float kLow = 0.32f;  // the fit both sampled masks are read through
constexpr float kHigh = 0.70f;
constexpr float kBevel = 26;  // the shoulder the slope normals come from

/** The plate every cell paints: one rounded octagon, so the slope mask's
 *  bevel has real corners to shade and the brass has an edge to catch. */
SkPath plate() {
  return shapes::rounded(shapes::chamfered(30), 8)
      .path({kCell - 36, kPicture - 36})
      .makeTransform(SkMatrix::Translate(18, 18));
}

/** A diagonal black-to-white ramp, baked once — the painted map every
 *  sampled mask on this sheet reads, so the four readings differ only in
 *  how they read it and never in what they were given. */
material::Texture ramp() {
  return material::Texture::produce("over_under.ramp", [] {
    constexpr int kSide = 128;
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSide, kSide));
    const SkPoint ends[2] = {{0, 0}, {kSide, kSide}};
    const SkColor4f stops[2] = {{0, 0, 0, 1}, {1, 1, 1, 1}};
    SkPaint paint;
    paint.setShader(SkShaders::LinearGradient(
        ends, SkGradient({{stops, 2}, {}, SkTileMode::kClamp}, {})));
    surface->getCanvas()->drawPaint(paint);
    return surface->makeImageSnapshot();
  });
}

/** The map placed over the plate's own box, so a mask reading it lines
 *  up with the shape it is masking. */
material::Texture placedRamp() {
  material::Texture map = ramp();
  map.uv(SkMatrix::Scale((kCell - 36) / 128.0f, (kPicture - 36) / 128.0f)
             .postTranslate(18, 18));
  return map;
}

material::Material stone() {
  return material::kit::stone({.hi = {0.62f, 0.63f, 0.66f, 1},
                               .lo = {0.34f, 0.35f, 0.39f, 1},
                               .bedAngle = 18,
                               .bedLength = 44,
                               .speckle = 0.4f,
                               .speckleAlpha = 0.3f});
}

material::Material brass() {
  return material::kit::latten({.from = {18, 18},
                                .to = {kCell - 18, kPicture - 18},
                                .level = 0.55f,
                                .sheen = 0.45f,
                                .patina = 0.12f});
}

sketch::kit::ComparisonCase cell(const char* caseTitle, const char* call,
                                 const std::string& note,
                                 material::Material paint) {
  return {.title = caseTitle,
          .control = call,
          .figure = sketch::kit::well(
              {.width = kCell, .height = kPicture},
              custom(call,
                     [paint = std::move(paint), face = plate()](
                         SkCanvas& canvas, const PaintContext& pc) {
                       material::skia::fill(
                           canvas, face, paint,
                           {.resolution = {pc.size.width(), pc.size.height()}});
                     })),
          .note = note};
}

}  // namespace

struct OverUnder {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const material::Material mixed =
        material::over(stone(), brass(), material::maskMap(placedRamp()));
    const material::Material twice = material::over(
        mixed, material::kit::board({.paint = {0.10f, 0.11f, 0.13f, 1}}),
        material::maskConstant(0.35f), material::Blend::Multiply);

    ctx.composer.render(sketch::kit::page(
        {.title = "A surface over another surface",
         .subtitle =
             "Follow the inputs, then change the mask, then change the blend",
         .footer = "A mask is a material read as a scalar. Its source, fit and "
                   "inversion remain explicit."},
        box().column().gap(28).children(
            {box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(20)
                 .children(
                     {box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "THE TWO SURFACES", .note = ""}),
                           sketch::kit::comparison(
                               {.cases =
                                    {cell("BASE · STONE", "kit::stone(…)",
                                          "Stone supplies the base.", stone()),
                                     cell("TOP · BRASS", "kit::latten(…)",
                                          "Brass supplies the top.", brass())},
                                .measure = 500,
                                .gap = 20})}),
                      box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "HOW MUCH TOP IS VISIBLE?",
                                .note = ""}),
                           sketch::kit::comparison(
                               {.cases =
                                    {cell("A CONSTANT",
                                          "over(…, maskConstant(0.35))",
                                          "Uniform 35% brass across the plate.",
                                          material::over(
                                              stone(), brass(),
                                              material::maskConstant(0.35f))),
                                     cell("A SAMPLED RAMP",
                                          "over(…, maskMap(ramp))",
                                          "The diagonal ramp sets local "
                                          "coverage.",
                                          mixed)},
                                .measure = 500,
                                .gap = 20})})}),
             sketch::kit::sectionHeader(
                 {.label = "READ AND REMAP THE MASK",
                  .note = "The surface pair stays fixed. Only the "
                          "interpretation of the source changes."}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("INVERT", "invert(maskMap(ramp))",
                            "Flip the same ramp’s answer.",
                            material::over(
                                stone(), brass(),
                                material::invertMask(
                                    material::maskMap(placedRamp())))),
                       cell("FIT A RANGE", "fit(maskMap(ramp), 0.32, 0.70)",
                            "Only the input range 0.32–0.70 spans the "
                            "transition.",
                            material::over(stone(), brass(),
                                           material::fitMask(
                                               material::maskMap(placedRamp()),
                                               kLow, kHigh))),
                       cell("READ AS HEIGHT", "maskHeight(ramp, 0.32, 0.70)",
                            "Read the ramp as height, without decoding a "
                            "tangent normal.",
                            material::over(
                                stone(), brass(),
                                material::maskHeight(placedRamp(), kLow,
                                                     kHigh, {0, 1, 0}))),
                       cell("READ A NORMAL",
                            "maskSlope(bevelNormals(plate, 26))",
                            "The bevel normal decides which shoulder faces up.",
                            material::over(stone(), brass(),
                                           material::maskSlope(
                                               material::bevelNormals(plate(),
                                                                      kBevel),
                                               {0, -1, 0}, 0.05f, 0.55f)))},
                  .measure = 1020,
                  .gap = 20}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(20)
                 .children(
                     {box().column().gap(16).children(
                          {sketch::kit::sectionHeader(
                               {.label = "CHANGE THE COMPOSITING LAW",
                                .note = ""}),
                           sketch::kit::comparison(
                               {.cases = {cell("ADDITIVE BLEND",
                                               "over(…, Blend::Add)",
                                               "Add brass rather than mixing "
                                               "towards it.",
                                               material::over(
                                                   stone(), brass(),
                                                   material::maskMap(
                                                       placedRamp()),
                                                   material::Blend::Add)),
                                          cell("A STACK OVER A STACK",
                                               "over(over(…), …, Multiply)",
                                               "Multiply another masked layer "
                                               "over the existing stack.",
                                               twice)},
                                .measure = 500,
                                .gap = 20})}),
                      box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "ONE VALUE, THREE CHILDREN",
                                .note = ""}),
                           document::caption(
                               "A masked stack holds a base, a top and a "
                               "mask. Applying the operation again creates "
                               "another layer; reading under() walks back "
                               "through that structure.")
                               .width(500),
                           document::caption(
                               kit::formatted(
                                   "The final specimen has stack depth %d.",
                                   material::stackDepth(twice)))
                               .width(500)})})})));
  }
};

SIGIL_SKETCH(OverUnder, "Kit · API",
             "brass stacked over stone through every kind of mask the kit "
             "ships, then through each blend, and finally a stack over a "
             "stack")
