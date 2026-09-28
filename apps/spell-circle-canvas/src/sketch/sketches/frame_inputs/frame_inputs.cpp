/** @file
 * A twelve-value uniform buffer drawn through one parameter layout.
 * The upper comparison changes only content scale or world translation.
 * The lower changes the data binding, replaces the bar recipe with dots,
 * and selects a body that never reads the table. UniformBlock identity is
 * stable; its committed revision makes new contents available at resolve.
 */

// TAGS: Materials/Shaders

#include <sigildraw/Pen.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace material = sigil::material;

using namespace sigil::compose;
using material::Color;
using material::FrameInput;
using material::Recipe;
using material::Target;

namespace {

constexpr SkSize kCanvas = {1100, 810};
constexpr float kCell = 324;
constexpr float kPicture = 182;

constexpr int kBars = 12;      // the table's length, and a constant in the body
constexpr float kGain = 0.9f;  // every bar's height multiplier

/** The ABI: a float, a colour and a table. Packed floats with float
 *  alignment, which is what lets the struct's memory image BE the
 *  upload. */
struct BarsParameters {
  float uGain;
  Color uTint;
  std::array<float, kBars> uBars;
};

std::shared_ptr<const Recipe> make(const char* name, std::string body) {
  return std::make_shared<const Recipe>(
      Recipe::of<BarsParameters>(name)
          .frame(FrameInput::Resolution)
          .frame(FrameInput::ContentScale)
          .frame(FrameInput::WorldTransform)
          .body(Target::SkSL, std::move(body)));
}

/** THE THREE RECIPES THIS SHEET COVERS ITS CELLS WITH. Each is BUILT
 *  where it is asked for, which is once: this sheet is complete at setup
 *  and never described again. A memo in a function-local static would
 *  hold a recipe in a dylib a hot reload unloads, and hold it for the
 *  process rather than for the sketch. */
std::shared_ptr<const Recipe> barsRecipe(std::string body) {
  return make("cover.bars", std::move(body));
}
std::shared_ptr<const Recipe> dotsRecipe(std::string body) {
  return make("cover.dots", std::move(body));
}
std::shared_ptr<const Recipe> flatRecipe(std::string body) {
  return make("cover.flat", std::move(body));
}

/** The cell's whole face, as the path a material is filled through. */
SkPath whole() {
  return SkPathBuilder().addRect(SkRect::MakeWH(kCell, kPicture)).detach();
}

sketch::kit::ComparisonCase example(const char* title, const char* control,
                                    const char* note, material::Material m,
                                    float contentScale,
                                    glm::mat3 world = glm::mat3(1)) {
  return {.title = title,
          .control = control,
          .figure = sketch::kit::well(
              {.width = kCell, .height = kPicture},
              custom(title,
                     [m = std::move(m), contentScale, world, face = whole()](sigil::draw::Pen& pen, const PaintContext& pc) {
                       SkCanvas& canvas = *pen.canvas();
                       material::skia::fill(
                           canvas, face, m,
                           {.resolution = {pc.size.x, pc.size.y},
                            .contentScale = contentScale,
                            .world = world});
                     })),
          .note = note};
}

}  // namespace

struct FrameInputs {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    // Each body is a recipe's rather than a whole program, so it is read as
    // text from the file beside this one and handed to its recipe.
    const auto body = [&](const char* name) {
      return ctx.assets.hub().text(ctx.local(name)).value_or(std::string());
    };

    // The caller's table, owned beside the model rather than in the
    // describe: a block re-made each frame would compare unequal and
    // re-patch its node every time.
    auto spectrum = std::make_shared<material::UniformBlock>(kBars);
    for (int i = 0; i < kBars; ++i)
      spectrum->values()[(size_t)i] =
          0.25f + 0.7f * std::abs(std::sin(0.9f * (float)i));
    spectrum->commit();

    auto second = std::make_shared<material::UniformBlock>(kBars);
    for (int i = 0; i < kBars; ++i)
      second->values()[(size_t)i] = 0.15f + 0.8f * (float)i / (float)kBars;
    second->commit();

    const BarsParameters stock{
        .uGain = kGain, .uTint = {0.42f, 0.80f, 0.92f, 1}, .uBars = {}};

    // ONE recipe pointer for the two materials that share a body: a
    // recipe is compared by pointer wherever a material is.
    const std::shared_ptr<const Recipe> cover = barsRecipe(body("bars.sksl"));
    material::Material bars(cover, stock);
    bars.bind("uBars", spectrum);

    material::Material ramped(cover, stock);
    ramped.bind("uBars", second);
    ramped.set("uTint", Color{0.96f, 0.68f, 0.34f, 1});

    Element frame = sketch::kit::comparison(
        {.cases =
             {example(
                  "REFERENCE", "contentScale = 1",
                  "One-pixel dividers; the live table supplies twelve heights.",
                  bars, 1),
              example("DENSITY", "contentScale = 3",
                      "Only scale changes. The divider is now one third of a "
                      "layout unit.",
                      bars, 3),
              example("PLACEMENT", "world translation x = 142",
                      "Only the world transform changes. Its translation "
                      "shifts the phase.",
                      bars, 1, glm::mat3(1, 0, 0, 0, 1, 0, 142, 0, 1))},
         .measure = 1020,
         .gap = 24});
    Element recipe = sketch::kit::comparison(
        {.cases =
             {example(
                  "CHANGE THE DATA", "second UniformBlock + tint",
                  "The same bar recipe reads a different twelve-value buffer.",
                  ramped, 1),
              example("CHANGE THE PROGRAM", "withRecipe(dotsRecipe())",
                      "The original data and tint carry over to a body that "
                      "draws discs.",
                      bars.withRecipe(dotsRecipe(body("dots.sksl"))), 1),
              example("LEAVE VALUES UNUSED", "withRecipe(flatRecipe())",
                      "This body reads the tint. The bound table contributes "
                      "no pixels.",
                      bars.withRecipe(flatRecipe(body("flat.sksl"))), 1)},
         .measure = 1020,
         .gap = 24});
    ctx.composer.render(sketch::kit::page(
        {.title = "What changes a shader's output?",
         .subtitle = "A twelve-value table and one parameter layout, varied "
                     "through frame inputs, data bindings and recipe bodies.",
         .footer = "UniformBlock is owned beside the model. Its identity stays "
                   "stable while values and revision change."},
        box().column().gap(26).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  SAME MATERIAL, DIFFERENT FRAME"}),
             std::move(frame),
             sketch::kit::sectionHeader(
                 {.label = "02  CHANGE ONE PART OF THE MATERIAL"}),
             std::move(recipe)})));
  }
};

SIGIL_SKETCH(FrameInputs, "Kit · API",
             "one recipe reading the resolution, the content scale and the "
             "world transform, driven by a caller-owned block and worn on "
             "three definitions of one ABI")
