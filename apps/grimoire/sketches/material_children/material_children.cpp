/** @file
 * material_children — a material filling another material's slot: the
 * child slot an SkSL program samples, and the three slots `over()` stacks.
 * Local variation is a composition of materials, never a bespoke recipe
 * per pair.
 *
 * A SHADER'S CHILD. `palette.sksl` beside this file has TWO sources:
 * `uIndex`, whose red BYTE is a palette index rather than a picture, and
 * `uPalette`, the 16-entry table that index selects from — a rule ("look
 * this number up over there") that SkSL expresses and nothing else in the
 * library does, which is what a child slot is for. Every panel is the
 * SAME index chart, the indices 0..15 in reading order; only the child,
 * or the `uShade` uniform that moves the index along the table, changes.
 * The live panel is data: update() changes which table is current and
 * re-describes, and because children ride the prune signature, only the
 * panel whose child changed repaints. Both sources are sampled NEAREST:
 * an index read linearly is a blend of two unrelated palette entries.
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
 *   kShade — the `uShade` uniform on the fourth palette panel: at 6 the
 *     last cells flatten onto the table's final entry.
 *   kSwapEvery — seconds between table swaps on the live panel.
 *   kLow, kHigh — the fit both sampled masks are read through.
 *   kBevel — the shoulder the slope mask's normals are derived from.
 */

// TAGS: Materials/Compositing, Materials/Shaders

#include <include/core/SkBitmap.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkGradient.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilmaterial/advanced/Combine.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/mask/Mask.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Bevel.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "Board.h"
#include "Latten.h"
#include "Stone.h"

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1500};
constexpr float kCell = 240;
constexpr float kPicture = 186;

constexpr float kLow = 0.32f;  // the fit both sampled masks are read through
constexpr float kHigh = 0.70f;
constexpr float kBevel = 26;  // the shoulder the slope normals come from

constexpr float kShade = 6.0f;      // uShade on the fourth palette panel
constexpr double kSwapEvery = 0.8;  // seconds per table on the live panel
constexpr int kChart = 4;           // the index chart is kChart × kChart
constexpr float kPanel = 188;
constexpr material::Color kFrame{0.20f, 0.24f, 0.32f, 1};

/** A one-row table of colours. No colour space, like every compose
 *  surface: the byte written here is the byte the shader reads. */
sk_sp<SkImage> table(const std::vector<SkColor>& entries) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels((int)entries.size(), 1);
  for (size_t entry = 0; entry < entries.size(); ++entry)
    *bitmap.getAddr32((int)entry, 0) = SkPreMultiplyColor(entries[entry]);
  bitmap.setImmutable();
  return bitmap.asImage();
}

/** THE INDEX CHART — kChart × kChart cells whose red byte is 0..15 in
 *  reading order. */
sk_sp<SkImage> indexChart() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kChart, kChart);
  for (int row = 0; row < kChart; ++row)
    for (int column = 0; column < kChart; ++column)
      *bitmap.getAddr32(column, row) =
          SkPreMultiplyColor(SkColorSetARGB(255, row * kChart + column, 0, 0));
  bitmap.setImmutable();
  return bitmap.asImage();
}

/** The palettes, in the order the live panel cycles them. */
enum Palette : size_t { Grey, Fire, Ice, PaletteCount };

/** EVERY TABLE THE PALETTE ROW DRAWS WITH, held for the sketch's life:
 *  an image material compares by image POINTER, so minting a fresh image
 *  per describe would make every panel unequal to every other and defeat
 *  every prune. The sketch holds them rather than a function-local
 *  static, because this file is compiled into a library a reload
 *  unloads. */
struct Tables {
  sk_sp<SkImage> index = indexChart();
  std::array<sk_sp<SkImage>, PaletteCount> palettes{
      [] {
        std::vector<SkColor> ramp;
        for (int entry = 0; entry < 16; ++entry)
          ramp.push_back(
              SkColorSetARGB(255, 17 * entry, 17 * entry, 17 * entry));
        return table(ramp);
      }(),
      table({0xff100005, 0xff2a0410, 0xff450a16, 0xff62111a, 0xff7f1a1c,
             0xff9c261b, 0xffb8351a, 0xffd04718, 0xffe25c17, 0xffee7419,
             0xfff58f26, 0xfff9a840, 0xfffcc063, 0xfffdd68e, 0xfffee8bd,
             0xffffffff}),
      table({0xff03060f, 0xff071228, 0xff0b1f42, 0xff102c5c, 0xff143a76,
             0xff17498f, 0xff1a59a7, 0xff1f6bbc, 0xff2a7fcd, 0xff3d93da,
             0xff56a7e4, 0xff74baec, 0xff96cdf2, 0xffbadff7, 0xffdceffb,
             0xffffffff})};
  /** `palette.sksl`, read in setup. */
  material::Material effect = material::Color{0, 0, 0, 0};
};

/** @p image sampled NEAREST through @p placement, as a material. */
material::Material nearest(const sk_sp<SkImage>& image, SkMatrix placement) {
  return material::skia::base(material::skia::image(
      image, material::Repeat::Pad, material::Repeat::Pad, placement,
      SkSamplingOptions(SkFilterMode::kNearest)));
}

/** ONE EFFECT, TWO CHILDREN, ONE UNIFORM — compiled to one shader. */
material::Material paletted(const Tables& tables, const sk_sp<SkImage>& palette,
                            float shade) {
  return material::Material(tables.effect)
      .set("uShade", shade)
      .slot("uIndex", nearest(tables.index, SkMatrix::Scale(kPanel / kChart,
                                                            kPanel / kChart)))
      .slot("uPalette", nearest(palette, SkMatrix::I()));
}

/** A palette panel: the chart through @p palette, the table's sixteen
 *  entries as a strip under it. */
sketch::kit::ComparisonCase lookup(const char* caseTitle, const char* call,
                                   const char* note, const Tables& tables,
                                   const sk_sp<SkImage>& palette, float shade,
                                   std::string key) {
  return {.title = caseTitle,
          .control = call,
          .figure = box().column().gap(6).children(
              {box()
                   .key(std::move(key))
                   .width(kPanel)
                   .height(kPanel)
                   .fill(paletted(tables, palette, shade))
                   .stroke(stroke(1.0f, Fill::color(kFrame))),
               box().width(kPanel).height(14).fill(
                   nearest(palette, SkMatrix::Scale(kPanel / 16.0f, 14.0f)))}),
          .note = note};
}

/** The plate every cell paints: one rounded octagon, so the slope mask's
 *  bevel has real corners to shade and the brass has an edge to catch. */
SkPath plate() {
  return sigil::geometry::path::toSk(shapes::rounded(shapes::chamfered(30), 8)
                                         .outline({kCell - 36, kPicture - 36}))
      .makeTransform(SkMatrix::Translate(18, 18));
}

/** A diagonal black-to-white ramp, baked once — the painted map every
 *  sampled mask on this sheet reads, so the four readings differ only in
 *  how they read it and never in what they were given. */
material::Texture ramp() {
  return material::Texture(
      sigil::media::PixelSource::produce("material_children.ramp", [] {
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
      }));
}

/** The map placed over the plate's own box, so a mask reading it lines
 *  up with the shape it is masking. */
material::Texture placedRamp() {
  material::Texture map = ramp();
  map.uv(material::skia::toMatrix(
      SkMatrix::Scale((kCell - 36) / 128.0f, (kPicture - 36) / 128.0f)
          .postTranslate(18, 18)));
  return map;
}

material::Material stone() {
  return material_children::stone({.hi = {0.62f, 0.63f, 0.66f, 1},
                                   .lo = {0.34f, 0.35f, 0.39f, 1},
                                   .bedAngle = 18,
                                   .bedLength = 44,
                                   .speckle = 0.4f,
                                   .speckleAlpha = 0.3f});
}

material::Material brass() {
  return material_children::latten({.from = {18, 18},
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
                         sigil::draw::Pen& pen, const PaintContext& context) {
                       SkCanvas& canvas = *pen.canvas();
                       material::skia::fill(
                           canvas, face, paint,
                           {.resolution = {context.size.x, context.size.y}});
                     })),
          .note = note};
}

}  // namespace

struct MaterialChildren {
  int live = 0;
  Tables tables;

  void setup(sketch::Scene2DContext& ctx) {
    struct PaletteParameters {
      float uShade = 0;
    };
    tables.effect = material::shader(
        ctx.assets.hub(), ctx.local("palette.sksl"), PaletteParameters{},
        {.textures = {{"uIndex", {}}, {"uPalette", {}}}});
    // the live panel is on the fire table here
    sketch::kit::stage(
        ctx,
        {.theme = sketch::kit::featureTheme(sketch::kit::Density::Spacious),
         .size = kCanvas,
         .captureAt = 1.0});
    ctx.composer.render(describe());
  }

  void update(double elapsed, sketch::Scene2DContext& ctx) {
    // Derived from `elapsed`, not accumulated: a still at a declared time
    // is then the same still every run.
    const int want = (int)(elapsed / kSwapEvery);
    if (want == live) return;
    live = want;
    ctx.composer.render(describe());
  }

  Element describe() const {
    // The theme is bound where the tree is DESCRIBED: this sketch
    // describes again whenever the live panel changes table.
    const material::Material mixed =
        material::over(stone(), brass(), material::maskMap(placedRamp()));
    const material::Material twice = material::over(
        mixed, material_children::board({.paint = {0.10f, 0.11f, 0.13f, 1}}),
        material::maskConstant(0.35f), material::BlendMode::Multiply);

    return sketch::kit::page(
        {.title = "A material in another material's slot",
         .subtitle = "A palette looked up through a shader's child, then a "
                     "surface over another surface: the inputs, the mask, "
                     "the blend",
         .footer = "A mask is a material read as a scalar. Its source, fit and "
                   "inversion remain explicit."},
        box().column().gap(28).children(
            {sketch::kit::section(
                 {.label = "A SHADER'S CHILD",
                  .note = "The chart stores indices; the strip under each "
                          "result is the table it was read through."},
                 sketch::kit::comparison(
                     {.cases =
                          {lookup("GREY", "slot(\"uPalette\", grey)",
                                  "Indices 0\u201315 through a grey ramp.",
                                  tables, tables.palettes[Grey], 0, "grey"),
                           lookup("FIRE", "slot(\"uPalette\", fire)",
                                  "The same indices through the fire table.",
                                  tables, tables.palettes[Fire], 0, "fire"),
                           lookup("ICE", "slot(\"uPalette\", ice)",
                                  "The same indices through the ice table.",
                                  tables, tables.palettes[Ice], 0, "ice"),
                           lookup("SHADE +6", "set(\"uShade\", 6)",
                                  "Six added to the index, clamped at entry "
                                  "15.",
                                  tables, tables.palettes[Ice], kShade,
                                  "shade"),
                           lookup("LIVE TABLE", "the table update() swaps",
                                  "The table changes every 0.8 seconds.",
                                  tables,
                                  tables.palettes[(size_t)live % PaletteCount],
                                  0, "live")},
                      .measure = 1020,
                      .gap = 20})),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(20)
                 .children(
                     {sketch::kit::section(
                          {.label = "THE TWO SURFACES", .note = ""},
                          sketch::kit::comparison(
                              {.cases =
                                   {cell("BASE · STONE", "stone(…)",
                                         "Stone supplies the base.", stone()),
                                    cell("TOP · BRASS", "latten(…)",
                                         "Brass supplies the top.", brass())},
                               .measure = 500,
                               .gap = 20})),
                      sketch::kit::section(
                          {.label = "HOW MUCH TOP IS VISIBLE?", .note = ""},
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
                               .gap = 20}))}),
             sketch::kit::section(
                 {.label = "READ AND REMAP THE MASK",
                  .note = "The surface pair stays fixed. Only the "
                          "interpretation of the source changes."},
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
                                material::over(
                                    stone(), brass(),
                                    material::fitMask(
                                        material::maskMap(placedRamp()), kLow,
                                        kHigh))),
                           cell("READ AS HEIGHT",
                                "maskHeight(ramp, 0.32, 0.70)",
                                "Read the ramp as height, without decoding a "
                                "tangent normal.",
                                material::over(stone(), brass(),
                                               material::maskHeight(
                                                   placedRamp(), kLow, kHigh,
                                                   {0, 1, 0}))),
                           cell("READ A NORMAL",
                                "maskSlope(bevelNormals(plate, 26))",
                                "The bevel normal decides which shoulder faces "
                                "up.",
                                material::over(stone(), brass(),
                                               material::maskSlope(
                                                   material::skia::bevelNormals(
                                                       plate(), kBevel),
                                                   {0, -1, 0}, 0.05f, 0.55f)))},
                      .measure = 1020,
                      .gap = 20})),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(20)
                 .children(
                     {sketch::kit::section(
                          {.label = "CHANGE THE COMPOSITING LAW", .note = ""},
                          sketch::kit::comparison(
                              {.cases =
                                   {cell("ADDITIVE BLEND",
                                         "over(…, Blend::Add)",
                                         "Add brass rather than mixing "
                                         "towards it.",
                                         material::over(
                                             stone(),
                                             brass(),
                                             material::maskMap(placedRamp()),
                                             material::BlendMode::PlusLighter)),
                                    cell("A STACK OVER A STACK",
                                         "over(over(…), …, Multiply)",
                                         "Multiply another masked layer "
                                         "over the existing stack.",
                                         twice)},
                               .measure = 500,
                               .gap = 20})),
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
                               .width(500)})})}));
  }
};

SIGIL_SKETCH(MaterialChildren, "Kit · API",
             "a material in another's slot — an index chart looked up "
             "through swapped palette tables in an SkSL child slot, then "
             "brass stacked over stone through every kind of mask, each "
             "blend, and a stack over a stack")
