/** @file
 * frame_inputs — the values a body reads that no author sets, and the
 * two doors a caller opens for the ones that are its own.
 *
 * A recipe DECLARES which per-frame values its body reads —
 * `frame(FrameInput::Resolution)`, `ContentScale`, `WorldTransform`,
 * `Time` — and only those are uploaded. A material that declares none is
 * a pure function of its parameters and can be cached across frames;
 * declaring Time or ContentScale makes it animated, and declaring
 * Resolution or the world transform makes it geometry-dependent.
 *
 * A `UniformBlock` is the caller's side of the same seam: a revisioned
 * float buffer behind a live ARRAY uniform, for per-frame data no scalar
 * can carry. Own it beside the model, write `values()`, `commit()`. It
 * compares by IDENTITY, so a block recreated every describe reads as a
 * new binding and re-patches its node.
 *
 * `withRecipe` is the third door: THE SAME INSTANCE over a second
 * definition of the same params layout. Values, bindings and children
 * carry over and the two compile and cache apart, which is the point —
 * one program per specialization rather than one per draw.
 *
 * EDIT THESE FIRST
 *   kBars  — how many bars the table holds. It is a constant in the body
 *            and the array's length, so the two cannot drift.
 *   kGain  — the height every bar is multiplied by.
 */

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/core/UniformBlock.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <array>
#include <cmath>
#include <memory>
#include <string>

namespace sketch = sigil::sketch;
namespace material = sigil::material;

using namespace sigil::compose;
using material::Color;
using material::FrameInput;
using material::Recipe;
using material::Target;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 646};
constexpr float kCell = 341;
constexpr float kPicture = 196;

constexpr int kBars = 12;      // the table's length, and a constant in the body
constexpr float kGain = 0.9f;  // every bar's height multiplier

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.cellGround = {0.09f, 0.095f, 0.11f, 1};
  look.type.captionLabel = {.size = 11, .mono = true};
  look.type.captionNote = {.size = 10.5f, .track = 0.2f};
  return look;
}

/** The ABI: a float, a colour and a table. Packed floats with float
 *  alignment, which is what lets the struct's memory image BE the
 *  upload. */
struct BarsParams {
  float uGain;
  Color uTint;
  std::array<float, kBars> uBars;
};

/** The body every cell but the last two runs: bars off the table, a
 *  hairline between them one DEVICE pixel wide (which is what makes the
 *  content scale visible), and the world translation read as a phase. */
constexpr char kBarsBody[] = R"(
half4 main(float2 p) {
  float2 uv = p / uResolution;
  float2 origin = float2(uWorld[2][0], uWorld[2][1]);
  float x = fract(uv.x + origin.x / max(uResolution.x, 1.0));
  float h = 0.0;
  for (int i = 0; i < 12; ++i) {
    float lo = float(i) / 12.0;
    float hi = float(i + 1) / 12.0;
    h += uBars[i] * step(lo, x) * step(x, hi);
  }
  h *= uGain;
  float cell = uResolution.x / 12.0;
  float toEdge = abs(mod(p.x, cell) - cell * 0.5);
  float hair = step(cell * 0.5 - 1.0 / uContentScale, toEdge);
  float bar = step(1.0 - h, uv.y);
  float3 c = mix(float3(0.09, 0.10, 0.13), uTint.rgb, bar);
  c = mix(c, float3(0.92, 0.93, 0.97), hair * 0.85);
  return half4(half3(c), 1.0);
}
)";

/** THE SPECIALIZATION: the same ABI, a body that spends the table on
 *  discs rather than bars. Same values, same bindings, its own program. */
constexpr char kDotsBody[] = R"(
half4 main(float2 p) {
  float2 uv = p / uResolution;
  float3 c = float3(0.09, 0.10, 0.13);
  float cell = uResolution.x / 12.0;
  for (int i = 0; i < 12; ++i) {
    float2 at = float2((float(i) + 0.5) * cell,
                       uResolution.y * (1.0 - uBars[i] * uGain * 0.86 - 0.07));
    float d = distance(p, at);
    c = mix(c, uTint.rgb, smoothstep(cell * 0.32, cell * 0.32 - 2.0, d));
  }
  c = mix(c, float3(0.92, 0.93, 0.97),
          step(uResolution.y - 1.5 / uContentScale, p.y));
  return half4(half3(c), 1.0);
}
)";

/** A body that reads NEITHER the table nor the gain: the flat tint
 *  alone. Whatever the compiler drops, the upload skips — every value
 *  the material writes to such a field, a constant or a whole bound
 *  table, reaches nothing. */
constexpr char kFlatBody[] = R"(
half4 main(float2 p) {
  float2 uv = p / uResolution;
  float3 c = mix(float3(0.09, 0.10, 0.13), uTint.rgb, uv.y);
  return half4(half3(c), 1.0);
}
)";

std::shared_ptr<const Recipe> make(const char* name, const char* body) {
  return std::make_shared<const Recipe>(Recipe::of<BarsParams>(name)
                                            .frame(FrameInput::Resolution)
                                            .frame(FrameInput::ContentScale)
                                            .frame(FrameInput::WorldTransform)
                                            .body(Target::SkSL, body));
}

const std::shared_ptr<const Recipe>& barsRecipe() {
  static const std::shared_ptr<const Recipe> r = make("cover.bars", kBarsBody);
  return r;
}
const std::shared_ptr<const Recipe>& dotsRecipe() {
  static const std::shared_ptr<const Recipe> r = make("cover.dots", kDotsBody);
  return r;
}
const std::shared_ptr<const Recipe>& flatRecipe() {
  static const std::shared_ptr<const Recipe> r = make("cover.flat", kFlatBody);
  return r;
}

SkPath whole() {
  static const SkPath path =
      SkPathBuilder().addRect(SkRect::MakeWH(kCell, kPicture)).detach();
  return path;
}

Element cell(const char* call, const std::string& note, material::Material m,
             float contentScale, glm::mat3 world = glm::mat3(1.0f)) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well(
          {.width = kCell, .height = kPicture},
          custom(call, [m = std::move(m), contentScale, world](
                           SkCanvas& canvas, const PaintContext& pc) {
            material::skia::fill(
                canvas, whole(), m,
                {.resolution = {pc.size.width(), pc.size.height()},
                 .contentScale = contentScale,
                 .world = world});
          })));
}

}  // namespace

struct FrameInputs final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    material::skia::install();  // the SkSL compiler, once per process

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

    const BarsParams stock{
        .uGain = kGain, .uTint = {0.42f, 0.80f, 0.92f, 1}, .uBars = {}};

    material::Material bars(barsRecipe(), stock);
    bars.bind("uBars", spectrum);

    material::Material ramped(barsRecipe(), stock);
    ramped.bind("uBars", second);
    ramped.set("uTint", Color{0.96f, 0.68f, 0.34f, 1});

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("FRAME INPUTS \xc2\xb7 Recipe::frame + "
                       "UniformBlock + Material::withRecipe"),
         .subtitle = toU8("dials \xc2\xb7 the content scale (1, then 3) "
                          "\xc2\xb7 the world translation \xc2\xb7 the "
                          "block's twelve floats \xc2\xb7 the recipe the "
                          "instance is worn on"),
         .footer = toU8("what a compiler KEEPS is what the upload "
                        "fills: a field a body never reads reaches "
                        "nothing, and the program cache names the "
                        "recipe and every field the compiler dropped "
                        "once per target")},
        kit::cells(
            {.cells =
                 {kit::cells(
                      {.cells =
                           {cell("bind(\"uBars\", block) \xc2\xb7 "
                                 "contentScale 1",
                                 kit::formatted("twelve floats read LIVE at every "
                                             "resolve \xc2\xb7 the hairlines "
                                             "are 1 / uContentScale wide, so "
                                             "here they are 1 px"),
                                 bars, 1.0f),
                            cell("\xe2\x80\xa6"
                                 " contentScale 3",
                                 "the same material and the same block "
                                 "\xc2\xb7 only the frame value moved, "
                                 "and the hairlines thinned to a third",
                                 bars, 3.0f),
                            cell("a second block, a second tint",
                                 "the block compares by IDENTITY, so "
                                 "this is a different binding \xc2\xb7 "
                                 "its values never enter the prune "
                                 "comparison",
                                 ramped, 1.0f)},
                       .gap = 14}),
                  kit::cells(
                      {.cells =
                           {cell("frame(WorldTransform) \xc2\xb7 uWorld "
                                 "translated",
                                 "the body reads column 2 of uWorld as "
                                 "its phase \xc2\xb7 identity outside a "
                                 "composite, so it degrades to the "
                                 "node's own space",
                                 bars,
                                 1.0f,
                                 glm::mat3(1, 0, 0, 0, 1, 0, 142, 0, 1)),
                            cell("withRecipe(dotsRecipe())",
                                 "THE SAME INSTANCE over a second "
                                 "definition of one params layout "
                                 "\xc2\xb7 the values, the binding and "
                                 "the tint all carried over",
                                 bars.withRecipe(dotsRecipe()), 1.0f),
                            cell("withRecipe(flatRecipe())",
                                 "a body that reads neither uBars nor "
                                 "uGain \xc2\xb7 the third definition "
                                 "of one ABI, and the table it is still "
                                 "bound to reaches nothing",
                                 bars.withRecipe(flatRecipe()), 1.0f)},
                       .gap =
                           14})},
             .column = true,
             .gap = 18})));
  }
};

SIGIL_SKETCH(FrameInputs, "Kit \xc2\xb7 API",
             "one recipe reading the resolution, the content scale and the "
             "world transform, driven by a caller-owned block and worn on "
             "three definitions of one ABI")
