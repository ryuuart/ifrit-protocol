/** @file
 * stock_materials — one of every stock SkSL material, painted.
 *
 * THE GUARD. A sketch dylib carries its OWN copy of Skia (vcpkg builds
 * Skia with hidden visibility, so a sketch dylib links libskia.a directly
 * rather than resolving its symbols from the host). That means
 * `SkRuntimeEffect::MakeForShader` allocates the SkSL AST inside the
 * SKETCH's Skia image, while `SkRuntimeEffect::getRPProgram` and the SkSL
 * inliner run inside the HOST's. Virtual dispatch across that boundary
 * faults on pointer authentication.
 *
 * Shallow shaders never reach the deep inliner path, so the failure looks
 * arbitrary — until you notice the ones that crash are exactly the ones
 * whose SkSL has nested or repeated helper calls. The rule that falls out:
 *
 *     every stock SkSL material must keep main() monolithic.
 *
 * So every stock generator is PAINTED here, not merely constructed:
 * compiling an effect is not what crashes, running it is. Add a helper
 * function to a body in SigilMaterial and reloading this sheet segfaults.
 *
 * The sheet is therefore a census, and it is read as one: each cell is
 * captioned with the recipe's OWN name (`Material::recipe().name()`), so
 * a renamed or a re-pointed recipe changes the caption without anyone
 * retyping it, and a generator that ships without arriving here has no
 * cell.
 *
 * The five rows are the five shelves: `field::` (the whole-box fields),
 * `pattern::` (the tiles, baked once and repeated), `kit::` grained (the
 * four quarried surfaces and the girih panel), `sdf::` with the unit-space
 * ramps, and `kit::` text paints.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/kit/Patterns.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/pattern/Tile.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace mat = sigil::material;
namespace mskia = sigil::material::skia;
namespace field = sigil::material::field;
namespace ptn = sigil::material::pattern;
namespace sdf = sigil::material::sdf;
namespace mkit = sigil::material::kit;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr float kCell = 170;    // one cell's width, px
constexpr float kSwatch = 100;  // the painted square in it, px

constexpr SkColor4f kEdge{1, 1, 1, 0.22f};

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.ground = {0.05f, 0.05f, 0.07f, 1};
  look.palette.ink = {0.88f, 0.90f, 0.94f, 1};
  look.palette.ash = {0.56f, 0.58f, 0.65f, 1};
  look.palette.rule = {0.18f, 0.19f, 0.23f, 1};
  look.type.title = {.size = 15, .track = 2.2f};
  look.type.subtitle = {.size = 11, .track = 0.7f};
  look.type.footer = {.size = 10, .track = 0.3f};
  look.type.captionLabel = {.size = 11, .track = 0.5f};
  look.type.captionNote = {.size = 9.5f, .track = 0.2f};
  look.captionWhere = kit::Caption::Where::Below;
  look.spacing.marginX = 30;
  look.spacing.marginTop = 26;
  look.spacing.marginBottom = 20;
  look.spacing.captionNoteGap = 3;
  return look;
}

/** The one voice: the recipe's name under the swatch, the call that made
 *  it under that, both ranged left at the cell's width. */
Element swatch(std::u8string name, const char* call, mskia::Paint paint) {
  return sketch::kit::caption(
      kCell, std::move(name), toU8(call),
      box()
          .width(kCell)
          .height(kSwatch)
          .fill(std::move(paint))
          .foreground(stroke(1.0f, Fill::color(kEdge))));
}

/** A material's own recipe names the cell — nothing here retypes it. */
Element painted(const char* call, mat::Material material) {
  std::u8string name = toU8(material.recipe().name());
  return swatch(std::move(name), call,
                mskia::Paint::recipe(std::move(material)));
}

/** A tile names itself by its generator, since a baked tile has no recipe
 *  of its own: what repeats is an image, sampled through the mapping. */
Element tiled(const char* name, const char* call, ptn::Tile tile) {
  return swatch(toU8(name), call,
                mskia::Paint::shader(tile.texture().shader()));
}

Element row(std::vector<Element> cells) {
  return kit::cells({.cells = std::move(cells), .gap = 14});
}

}  // namespace

struct StockMaterialsSheet final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = {1150, 900}});
    // Nothing on the sheet moves: every generator is evaluated from its
    // parameters and the box, and the two that read the clock are pinned
    // by the moment their call names.
    ctx.captureAt(0.05);

    const std::vector<mskia::Stop> ramp = {{0.0f, {0.95f, 0.35f, 0.25f, 1}},
                                           {0.5f, {0.95f, 0.80f, 0.30f, 1}},
                                           {1.0f, {0.20f, 0.55f, 0.95f, 1}}};
    const SkRect swatchBox = SkRect::MakeWH(kCell, kSwatch);

    // The tile the two content-reading fields are shown over, so the
    // warp has something to displace and the tube something to darken.
    const mskia::Paint under = mskia::Paint::shader(
        ptn::checker(14, mat::rgb(0x2b3a54), mat::rgb(0x8fa6c8))
            .texture()
            .shader());

    Element fields = row(
        {painted("field::halftoneRamp(9, 1, 3.6, gold)",
                 field::halftoneRamp(9, 1.0f, 3.6f, mat::rgb(0xf2cc4d), 18.0f)),
         painted("field::noise(0.03, 4)", field::noise(0.03f, 4, 3.0f)),
         painted("field::grain(0.35, 4, stretch 2.4)",
                 field::grain(0.35f, 4, 3.0f, 1.0f, 2.4f)),
         swatch(toU8(field::rippleRecipe()->name()),
                "field::ripple(7 px, 46 px) over a checker child",
                mskia::Paint::recipe(field::ripple(7.0f, 46.0f, 0.6f))
                    .child("content", under)),
         swatch(toU8(field::crtOverlayRecipe()->name()),
                "field::crtOverlay(4 px) laid over the same checker",
                mskia::Paint::blend({{under, SkBlendMode::kSrc},
                                     {mskia::Paint::recipe(field::crtOverlay()),
                                      SkBlendMode::kSrcOver}})),
         painted("field::noise(0.02, 5, turbulence)",
                 field::noise(0.02f, 5, 9.0f, true))});

    Element patterns =
        row({tiled("halftone", "pattern::halftone(11, 3.4, ink)",
                   ptn::halftone(11, 3.4f, mat::rgb(0xe8e2d2))),
             tiled("stripes", "pattern::stripes(6, 10, gold).rotate(30)",
                   ptn::stripes(6, 10, mat::rgb(0xf2cc4d)).rotate(30)),
             tiled("sequence",
                   "pattern::sequence({{18, navy}, {6, bone}, "
                   "{10, red}})",
                   ptn::sequence({{18, mat::rgb(0x1d2b45)},
                                  {6, mat::rgb(0xe8e2d2)},
                                  {10, mat::rgb(0xa33328)}})),
             tiled("checker", "pattern::checker(16, slate, bone)",
                   ptn::checker(16, mat::rgb(0x2b3a54), mat::rgb(0xd8dbe2))),
             tiled("gridLines", "pattern::gridLines(20, 1, ash)",
                   ptn::gridLines(20, 1.0f, mat::rgb(0x7f88a0))),
             tiled("speckle", "pattern::speckle(120, 34, 1.2, 4.2)",
                   ptn::speckle(120, 34, 1.2f, 4.2f,
                                {mat::rgb(0xe8e2d2), mat::rgb(0xf2cc4d)}))});

    Element grained =
        row({painted("kit::stone({.bedAngle = 24, .bedLength = 46})",
                     mkit::stone({.bedAngle = 24, .bedLength = 46, .seed = 3})),
             painted("kit::timber({.span = 90, .figure = 0.5})",
                     mkit::timber({.span = 90, .figure = 0.5f, .seed = 5})),
             painted("kit::latten({.level = 0.6, .sheen = 0.5})",
                     mkit::latten({.level = 0.6f, .sheen = 0.5f, .seed = 7})),
             painted("kit::board({.tooth = 0.4, .wear = 0.3})",
                     mkit::board({.tooth = 0.4f, .wear = 0.3f, .seed = 11})),
             tiled("girih8", "kit::girih8(30, fezPalette(), 1.6, 45\xc2\xb0)",
                   mkit::girih8(30, mkit::fezPalette(), 1.6f, 45.0f)),
             tiled("girih8 \xc2\xb7 nasrid",
                   "kit::girih8(30, nasridPalette(), 1.6, 62\xc2\xb0)",
                   mkit::girih8(30, mkit::nasridPalette(), 1.6f, 62.0f))});

    Element shapesAndRamps =
        row({painted("sdf::circle, bordered and glowing",
                     sdf::material(sdf::circle(),
                                   {.fill = mat::rgb(0x3389f2),
                                    .borderWidth = 3,
                                    .borderColor = mat::rgb(0xffffff, 0.9f),
                                    .glowRadius = 10,
                                    .glowColor = mat::rgb(0x66b3ff, 0.6f)})),
             painted("sdf::roundBox(14), with a shadow",
                     sdf::material(sdf::roundBox(14),
                                   {.fill = mat::rgb(0xf2593f),
                                    .borderWidth = 2,
                                    .borderColor = mat::rgb(0xffe6b3, 0.9f),
                                    .shadowOffset = {0, 4},
                                    .shadowBlur = 8,
                                    .shadowColor = mat::rgb(0x000000, 0.55f)})),
             painted("sdf::star(6, 2.6)",
                     sdf::material(sdf::star(6, 2.6f),
                                   {.fill = mat::rgb(0xf2cc4d)})),
             swatch(u8"linearUnit", "Paint::linearUnit({0,0}, {1,1}, ramp)",
                    mskia::Paint::linearUnit({0, 0}, {1, 1}, ramp)),
             swatch(u8"radialUnit", "Paint::radialUnit({0.5,0.5}, 1, ramp)",
                    mskia::Paint::radialUnit({0.5f, 0.5f}, 1.0f, ramp)),
             swatch(u8"glowUnit", "Paint::glowUnit({0.5,0.5}, 1, ramp)",
                    mskia::Paint::glowUnit({0.5f, 0.5f}, 1.0f, ramp))});

    Element textPaints = row(
        {painted("kit::water(bounds, 1.4 s)", mkit::water(swatchBox, 1.4f)),
         painted("kit::meshGradient(bounds, 1.4 s)",
                 mkit::meshGradient(swatchBox, 1.4f)),
         painted("kit::sparkle(bounds, 1.4 s)", mkit::sparkle(swatchBox, 1.4f)),
         painted("kit::starNest(bounds, 1.4 s)",
                 mkit::starNest(swatchBox, 1.4f)),
         painted("kit::clouds(bounds, 1.4 s)", mkit::clouds(swatchBox, 1.4f)),
         painted("kit::tunnel(bounds, 1.4 s)", mkit::tunnel(swatchBox, 1.4f))});

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("STOCK MATERIALS \xc2\xb7 every generator, "
                       "painted once"),
         .subtitle = toU8("field \xc2\xb7 pattern tiles \xc2\xb7 the "
                          "grained kit and girih \xc2\xb7 sdf and the "
                          "unit ramps \xc2\xb7 the text paints"),
         .footer = toU8("each caption is the recipe's own name; running "
                        "the effect is what crosses the split-Skia "
                        "image boundary, so every cell is PAINTED")},
        kit::cells({.cells = {std::move(fields), std::move(patterns),
                              std::move(grained), std::move(shapesAndRamps),
                              std::move(textPaints)},
                    .column = true,
                    .gap = 20})));
  }
};

SIGIL_SKETCH(StockMaterialsSheet, "Start & fixtures",
             "one of every stock SkSL material, painted from its own "
             "recipe \xe2\x80\x94 also the split-Skia ctest guard")
