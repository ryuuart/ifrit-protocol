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

#include <include/core/SkSurface.h>
#include <include/effects/SkGradient.h>
#include <sigilcompose/core/Core.h>
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
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 622};
constexpr float kCell = 200;
constexpr float kPicture = 176;

constexpr float kLow = 0.32f;  // the fit both sampled masks are read through
constexpr float kHigh = 0.70f;
constexpr float kBevel = 26;  // the shoulder the slope normals come from

/** The plate every cell paints: one rounded octagon, so the slope mask's
 *  bevel has real corners to shade and the brass has an edge to catch. */
SkPath plate() {
  static const SkPath path = shapes::rounded(shapes::chamfered(30), 8)
                                 .path({kCell - 36, kPicture - 36})
                                 .makeTransform(SkMatrix::Translate(18, 18));
  return path;
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

Element cell(const char* call, const std::string& note,
             material::Material paint) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well(
          {.width = kCell, .height = kPicture},
          custom(call, [paint = std::move(paint)](SkCanvas& canvas,
                                                  const PaintContext& pc) {
            material::skia::fill(
                canvas, plate(), paint,
                {.resolution = {pc.size.width(), pc.size.height()}});
          })));
}

/** The page's own prose, held apart from the tree so the tree reads as a
 *  tree. */
constexpr const char* kTitle =
    "OVER AND UNDER \xc2\xb7 over(base, top, mask, blend) and the mask family";
constexpr const char* kSubtitle =
    "dials \xc2\xb7 the mask kind \xc2\xb7 the fit (0.32 to 0.70) \xc2\xb7 the "
    "blend \xc2\xb7 the bevel the slope normals come from (26 px)";
constexpr const char* kFooter =
    "maskVertexColor reads a painted colour lane the same way maskMap reads "
    "an image \xe2\x80\x94 the renderer supplies the texture, and everything "
    "after it on this sheet is the same fit and the same invert";

/** Row one: the two sources, the two mask shapes, and the invert. */
Element sources(const material::Material& mixed) {
  return kit::cells(
      {.cells = {cell("kit::stone(\xe2\x80\xa6)",
                      "the BASE: a generated bed, grain and speckle, no "
                      "texture at all",
                      stone()),
                 cell("kit::latten(\xe2\x80\xa6)",
                      "the TOP: sheet brass, one colour and a ladder of "
                      "lights under a sheen",
                      brass()),
                 cell("over(\xe2\x80\xa6, maskConstant(0.35))",
                      "a mask that is the same everywhere \xc2\xb7 the whole "
                      "plate at 35% brass",
                      material::over(stone(), brass(),
                                     material::maskConstant(0.35f))),
                 cell("over(\xe2\x80\xa6, maskMap(ramp))",
                      "one CHANNEL of a painted map \xc2\xb7 its own uv "
                      "placement decides where each texel lands",
                      mixed),
                 cell("invert(maskMap(ramp))",
                      "the same map with its answer flipped \xc2\xb7 brass "
                      "where the ramp is dark",
                      material::over(stone(), brass(),
                                     material::invertMask(
                                         material::maskMap(placedRamp()))))},
       .gap = 12});
}

/** Row two: the fit, the two derived readings, a blend, and a stack over
 *  a stack. */
Element readings(const material::Material& twice) {
  return kit::cells(
      {.cells = {cell("fit(maskMap(ramp), 0.32, 0.70)",
                      "the raw range that maps onto 0..1 moved \xc2\xb7 the "
                      "transition narrows to that band",
                      material::over(
                          stone(), brass(),
                          material::fitMask(material::maskMap(placedRamp()),
                                            kLow, kHigh))),
                 cell(
                     "maskSlope(bevelNormals(plate, 26))",
                     "dot(N, up) fitted \xc2\xb7 brass on the shoulder that "
                     "faces the light, stone on the one that turns away",
                     material::over(stone(), brass(),
                                    material::maskSlope(
                                        material::bevelNormals(plate(), kBevel),
                                        {0, -1, 0}, 0.05f, 0.55f))),
                 cell("maskHeight(ramp, 0.32, 0.70)",
                      "the same map read with NO tangent decode \xc2\xb7 a "
                      "value dotted with an axis, which is what a tide line is",
                      material::over(stone(), brass(),
                                     material::maskHeight(placedRamp(), kLow,
                                                          kHigh, {0, 1, 0}))),
                 cell("over(\xe2\x80\xa6, Blend::Add)",
                      "the top ADDS, scaled by the mask \xc2\xb7 one recipe "
                      "per blend, so no body carries a branch",
                      material::over(stone(), brass(),
                                     material::maskMap(placedRamp()),
                                     material::Blend::Add)),
                 cell("over(over(\xe2\x80\xa6), \xe2\x80\xa6, Multiply)",
                      kit::formatted("a stack over a stack \xc2\xb7 stackDepth "
                                     "%d, and under() walks back down to the "
                                     "stone",
                                     material::stackDepth(twice)),
                      twice)},
       .gap = 12});
}

}  // namespace

struct OverUnder final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    material::skia::install();  // the SkSL compiler, once per process

    const material::Material mixed =
        material::over(stone(), brass(), material::maskMap(placedRamp()));
    const material::Material twice = material::over(
        mixed, material::kit::board({.paint = {0.10f, 0.11f, 0.13f, 1}}),
        material::maskConstant(0.35f), material::Blend::Multiply);

    ctx.composer.render(sketch::kit::page(
        {.title = toU8(kTitle),
         .subtitle = toU8(kSubtitle),
         .footer = toU8(kFooter)},
        kit::cells({.cells = {sources(mixed), readings(twice)},
                    .column = true,
                    .gap = 16})));
  }
};

SIGIL_SKETCH(OverUnder, "Kit \xc2\xb7 API",
             "brass stacked over stone through every kind of mask the kit "
             "ships, then through each blend, and finally a stack over a "
             "stack")
