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
 * The three rows are the three shelves: `field::` (the whole-box fields),
 * `pattern::` (the tiles, baked once and repeated), and `sdf::` with the
 * unit-space ramps. The library ships no looks; a study carries its own.
 */

// TAGS: Materials/Shaders

#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Texture.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
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
namespace material = sigil::material;
namespace field = sigil::material::field;
namespace sdf = sigil::material::sdf;

using namespace sigil::compose;

namespace {

constexpr float kCell = 170;    // one cell's width, px
constexpr float kSwatch = 100;  // the painted square in it, px

constexpr material::Color kEdge{1, 1, 1, 0.22f};

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious);
  look.captionWhere = kit::Caption::Where::Below;
  look.spacing.marginX = 30;
  look.spacing.marginTop = 26;
  look.spacing.marginBottom = 20;
  look.spacing.captionNoteGap = 3;
  return look;
}

/** The one voice: the recipe's name under the swatch, the call that made
 *  it under that, both ranged left at the cell's width. */
Element swatch(Utf8 name, const char* call, material::Material paint) {
  return sketch::kit::caption(
      kCell, std::move(name), call,
      box()
          .width(kCell)
          .height(kSwatch)
          .fill(std::move(paint))
          .foreground(stroke(1.0f, Fill::color(kEdge))));
}

/** A material's own recipe names the cell — nothing here retypes it. */
Element painted(const char* call, material::Material material) {
  const std::string name = material.recipe().name();
  return swatch(name, call, std::move(material));
}

/** A tile names itself by its generator, since a baked tile has no recipe
 *  of its own: what repeats is an image, sampled through the mapping. */
Element tiled(const char* name, const char* call, material::pattern::Tile tile) {
  return swatch(name, call, material::skia::base(material::skia::paint(material::skia::shader(tile.texture()))));
}

Element row(std::vector<Element> cells) {
  return sketch::kit::cells({.cells = std::move(cells), .gap = 14});
}

}  // namespace

struct StockMaterialsSheet {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // Nothing on the sheet moves: every generator is evaluated from its
    // parameters and the box, and the two that read the clock are pinned
    // by the moment their call names.
    sketch::kit::stage(ctx, {.size = {1150, 1000}, .captureAt = 0.05});

    const std::vector<material::ColorStop> ramp = {
        {0.0f, {0.95f, 0.35f, 0.25f, 1}},
        {0.5f, {0.95f, 0.80f, 0.30f, 1}},
        {1.0f, {0.20f, 0.55f, 0.95f, 1}}};

    // The tile the two content-reading fields are shown over, so the
    // warp has something to displace and the tube something to darken.
    const material::Material under = material::skia::base(material::skia::paint(
        material::skia::shader(material::pattern::checker(14, material::hexColor(0x2b3a54), material::hexColor(0x8fa6c8)).texture())));

    Element fields = row(
        {painted("field::halftoneRamp(9, 1, 3.6, gold)",
                 field::halftoneRamp(9, 1.0f, 3.6f, material::hexColor(0xf2cc4d), 18.0f)),
         painted("field::noise(0.03, 4)", field::noise(0.03f, 4, 3.0f)),
         painted("field::grain(0.35, 4, stretch 2.4)",
                 field::grain(0.35f, 4, 3.0f, 1.0f, 2.4f)),
         swatch(field::rippleRecipe()->name(),
                "field::ripple(7 px, 46 px) over a checker child",
                field::ripple(7.0f, 46.0f, 0.6f)
                    .slot("content", under)),
         painted("field::noise(0.02, 5, turbulence)",
                 field::noise(0.02f, 5, 9.0f, true))});

    Element patterns =
        row({tiled("halftone", "pattern::halftone(11, 3.4, ink)",
                   material::pattern::halftone(11, 3.4f, material::hexColor(0xe8e2d2))),
             tiled("stripes", "pattern::stripes(6, 10, gold).rotate(30)",
                   material::pattern::stripes(6, 10, material::hexColor(0xf2cc4d)).rotate(30)),
             tiled("sequence",
                   "pattern::sequence({{18, navy}, {6, bone}, "
                   "{10, red}})",
                   material::pattern::sequence({{18, material::hexColor(0x1d2b45)},
                                  {6, material::hexColor(0xe8e2d2)},
                                  {10, material::hexColor(0xa33328)}})),
             tiled("checker", "pattern::checker(16, slate, bone)",
                   material::pattern::checker(16, material::hexColor(0x2b3a54), material::hexColor(0xd8dbe2))),
             tiled("gridLines", "pattern::gridLines(20, 1, ash)",
                   material::pattern::gridLines(20, 1.0f, material::hexColor(0x7f88a0))),
             tiled("speckle", "pattern::speckle(120, 34, 1.2, 4.2)",
                   material::pattern::speckle(120, 34, 1.2f, 4.2f,
                                {material::hexColor(0xe8e2d2), material::hexColor(0xf2cc4d)}))});

    Element shapesAndRamps = row(
        {painted("sdf::circle, bordered and glowing",
                 sdf::material(sdf::circle(),
                               {.fill = material::hexColor(0x3389f2),
                                .borderWidth = 3,
                                .borderColor = material::hexColor(0xffffff, 0.9f),
                                .glowRadius = 10,
                                .glowColor = material::hexColor(0x66b3ff, 0.6f)})),
         painted(
             "sdf::roundBox(14), with a shadow",
             sdf::material(sdf::roundBox(14),
                           {.fill = material::hexColor(0xf2593f),
                            .borderWidth = 2,
                            .borderColor = material::hexColor(0xffe6b3, 0.9f),
                            .shadowOffset = {0, 4},
                            .shadowBlur = 8,
                            .shadowColor = material::hexColor(0x000000, 0.55f)})),
         painted("sdf::star(6, 2.6)",
                 sdf::material(sdf::star(6, 2.6f),
                               {.fill = material::hexColor(0xf2cc4d)})),
         swatch(u8"linearUnit", "Paint::linearUnit({0,0}, {1,1}, ramp)",
                sigil::material::linearGradient({0, 0}, {1, 1}, ramp)),
         swatch(
             u8"radialUnit", "Paint::radialUnit({0.5,0.5}, 1, ramp)",
             sigil::material::radialGradient({0.5f, 0.5f}, 1.0f, ramp)),
         swatch(u8"glowUnit", "Paint::glowUnit({0.5,0.5}, 1, ramp)",
                sigil::material::radialGradient(
                    {0.5f, 0.5f}, 1.0f, ramp,
                    {.extent = material::RadialExtent::ClosestSide}))});

    ctx.composer.render(sketch::kit::page(
        {.title = "The material shelf",
         .subtitle = "field · pattern tiles · sdf and the unit ramps",
         .footer = "Recipe names identify live materials; the smaller caption "
                   "shows the parameters behind each swatch."},
        sketch::kit::cells(
            {.cells = {std::move(fields), std::move(patterns),
                       std::move(shapesAndRamps)},
             .column = true,
             .gap = 20})));
  }
};

SIGIL_SKETCH(StockMaterialsSheet, "Start & fixtures",
             "one of every stock SkSL material, painted from its own "
             "recipe — also the split-Skia ctest guard")
