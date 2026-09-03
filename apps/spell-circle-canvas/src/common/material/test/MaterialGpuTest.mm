/** @file
 * EVERY BODY THIS LIBRARY SHIPS, COMPILED ON A DEVICE.
 *
 * A recipe's SkSL body is validated one way when it is made into a
 * runtime effect — there the body is a whole program and every name in
 * it is the body's own — and another when a GPU backend inlines that
 * effect into a pipeline's fragment shader under parameters it names
 * itself. A body can pass the first and fail the second, and the second
 * failure is quiet: the draw paints nothing, the next frame tries again,
 * and the only sign is a compiler's complaint on stderr. Every other
 * material suite compiles on a raster surface, so none of them can see
 * it.
 *
 * So this suite stands Graphite up, installs a handler where the failed
 * compiles are reported, and draws one instance of every recipe the kit,
 * the sdf, the field and the ocio features ship, plus a stack per blend
 * and the shading terms, demanding that not one of them reports an
 * error. The control below proves the handler is wired to something: the
 * exact collision the reserved names exist to prevent, built as a raw
 * runtime effect so it reaches the device, must be reported.
 */

#include <sigilcore/hardware/GpuDevice.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Mask.h>
#include <sigilmaterial/kit/Recipes.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/kit/Terms.h>
#include <sigilmaterial/ocio/Ocio.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/gpu/ShaderErrorHandler.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace sigil::material;

namespace {

/** A body that reads no parameter still needs an ABI. */
struct NoParams {
  float unused = 0;
};

/** WHERE A FAILED COMPILE LANDS. Skia's own handler prints the shader and
 *  the errors and returns, so a collector in its place is the whole of
 *  what turns the report into a verdict. */
class ErrorSink final : public skgpu::ShaderErrorHandler {
 public:
  void compileError(const char* /*shader*/, const char* errors,
                    bool /*shaderWasCached*/) override {
    // The errors quote the generated line they are about, which is what
    // names the parameter the body collided with; the whole shader beside
    // them would bury it.
    m_errors += errors ? errors : "";
    m_errors += "\n";
  }

  /** The reports since the last call, and forget them. */
  std::string drain() {
    std::string out = std::move(m_errors);
    m_errors.clear();
    return out;
  }

 private:
  std::string m_errors;
};

ErrorSink& sink() {
  static ErrorSink handler;
  return handler;
}

/** Graphite on an owned device, with the error sink installed BEFORE the
 *  context is built — the options are read once, at creation. */
sigil::skia::GraphiteContext* graphite() {
  static std::unique_ptr<sigil::core::hardware::GpuDevice> device =
      sigil::core::hardware::GpuDevice::createOwned();
  static std::unique_ptr<sigil::skia::GraphiteContext> context = [] {
    sigil::skia::GraphiteContext::reportShaderErrorsTo(&sink());
    return device ? sigil::skia::GraphiteContext::create(*device)
                  : std::unique_ptr<sigil::skia::GraphiteContext>();
  }();
  return context.get();
}

constexpr int kField = 64;

/** @p shader drawn over the whole of a Graphite surface, the recording
 *  inserted and the queue drained, so every pipeline the draw needs has
 *  been built by the time this returns. Every snap is inserted: a
 *  recording snapped and dropped kills the recorder for good. */
void drawOnGpu(sk_sp<SkShader> shader) {
  sigil::skia::GraphiteContext* context = graphite();
  const SkImageInfo info = SkImageInfo::MakeN32Premul(kField, kField);
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(context->recorder(), info);
  ASSERT_TRUE(surface);
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorBLACK);
  SkPaint paint;
  paint.setShader(std::move(shader));
  canvas.drawRect(SkRect::MakeWH(kField, kField), paint);
  if (auto recording = context->recorder()->snap()) {
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    context->context()->insertRecording(insert);
  }
  skgpu::graphite::SubmitInfo submit;
  submit.fSync = skgpu::graphite::SyncToCpu::kYes;
  context->context()->submit(submit);
  context->context()->checkAsyncWorkCompletion();
}

/** @p material drawn through its Paint, and what the device said about
 *  the shader it compiled — empty when it compiled. */
std::string shadeOnGpu(const Material& material) {
  sink().drain();
  skia::Paint paint = skia::Paint::recipe(material);
  skia::PaintFrame frame;
  frame.size = SkSize::Make(kField, kField);
  frame.seconds = 1.25;
  frame.contentScale = 2.0f;
  sk_sp<SkShader> shader = paint.shaderFor(frame);
  if (!shader) return "the material resolved to no shader";
  drawOnGpu(std::move(shader));
  return sink().drain();
}

/** One body calling every term the shading module answers to, so the
 *  module's own text is compiled on the device rather than only the
 *  bodies that happen to prepend it. Every result reaches the return, or
 *  the compiler is free to drop the call that would have failed. */
Material termsMaterial() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParams>("terms.everyTerm")
          .body(Target::SkSL,
                kit::termsSource(Target::SkSL) + R"(
      half4 main(float2 xy) {
        float3 n = normalize(float3(0.2, 0.3, 1.0));
        float3 l = normalize(float3(xy.x + 0.5, xy.y + 0.5, 1.0));
        float3 v = float3(0.0, 0.0, 1.0);
        float3 f0 = specularColor(float3(0.9, 0.8, 0.4), 0.7);
        float3 radiance = float3(0.6, 0.7, 0.9);
        float acc = atan2P(l.y, l.x) + acosP(n.z)
                  + roughnessLevel(0.3, 6.0)
                  + lambert(n, l) + blinn(n, l, v, 32.0)
                  + occlusion(0.7, 0.8) + luminance(radiance);
        float2 uv = equirectUv(l);
        float3 dir = equirectDirection(uv);
        float2 ab = environmentBrdf(0.3, max(dot(n, v), 0.0));
        float3 lit = fresnel(f0, max(dot(n, v), 0.0))
                   + fresnelRough(f0, max(dot(n, v), 0.0), 0.3)
                   + environmentSpecular(radiance, f0, 0.3, max(dot(n, v), 0.0))
                   + environmentReflection(radiance, 0.5)
                   + refraction(-v, n, 0.66)
                   + attenuate(radiance, float3(0.1, 0.02, 0.05), 2.0)
                   + emission(radiance, 0.5, float3(1.0, 1.0, 1.0))
                   + dir + float3(ab.x, ab.y, acc);
        return half4(half3(toneMap(lit, 1.0)), 1.0);
      }
    )"));
  return Material(recipe, NoParams{});
}

/** Every material this library can hand a backend, named. */
std::vector<std::pair<std::string, Material>> everyMaterial() {
  std::vector<std::pair<std::string, Material>> all;
  const auto add = [&all](std::vector<Material> some) {
    for (Material& m : some) {
      std::string name = m.recipe().name();
      all.emplace_back(std::move(name), std::move(m));
    }
  };
  add(kit::everyRecipe());
  add(sdf::everyRecipe());
  add(field::everyRecipe());
  add({termsMaterial()});

  // The stacks: one per blend, over operands the kit supplies. A stack is
  // a material like any other and its operands are its children, so the
  // body compiled is the combinator's over three sampled slots.
  kit::SurfaceParams red;
  red.baseColor = {1, 0.2f, 0.1f, 1};
  kit::SurfaceParams blue;
  blue.baseColor = {0.1f, 0.3f, 1, 1};
  for (const Blend blend : {Blend::Mix, Blend::Add, Blend::Multiply})
    add({over(kit::unlit(red), kit::unlit(blue), kit::maskConstant(0.5f),
              blend)});

  if (ocio::available()) add({ocio::exponent(2.2f)});
  return all;
}

}  // namespace

#define REQUIRE_GPU()                \
  if (!graphite()) {                 \
    GTEST_SKIP() << "no GPU device"; \
  }

// THE SWEEP. Every recipe the library ships, drawn on the device through
// the same Paint a consumer draws it through. A body that compiles as its
// own SkSL program and not once Graphite has inlined it fails here and
// nowhere else.
TEST(MaterialGpu, EveryRecipeCompilesOnTheDevice) {
  REQUIRE_GPU();
  skia::install();
  const std::vector<std::pair<std::string, Material>> all = everyMaterial();
  // A count, so a list that quietly stopped enumerating cannot pass.
  ASSERT_GE(all.size(), 30u) << "the enumeration lost recipes";
  for (const auto& [name, material] : all) {
    const std::string reported = shadeOnGpu(material);
    if (reported.empty()) continue;
    // The first failure is the only trustworthy one: a pass Graphite drops
    // for want of a pipeline leaves the recorder replaying out of order,
    // and every recipe after it reports that instead of itself.
    ADD_FAILURE() << name << " did not compile on the device:\n" << reported;
    break;
  }
}

// THE CONTROL, twice over. Graphite inlines a runtime effect's body into
// its fragment shader under parameters of its own naming — `pos` for the
// coordinates — and discards the name the body's own main declared. So a
// body declaring a local `pos` redeclares that parameter: it makes a
// perfectly good SkRuntimeEffect, because there the body is the whole
// program, and the device rejects it. This proves both halves: that the
// device really does reject it and the sink really does hear, and that
// the material compiler refuses the body before it can get that far.
TEST(MaterialGpu, ABodyDeclaringAReservedNameIsCaughtBothWays) {
  REQUIRE_GPU();
  skia::install();
  constexpr char kCollides[] = R"(
    half4 main(float2 xy) {
      float2 pos = xy * 0.5;
      return half4(half2(pos), 0.0, 1.0);
    }
  )";

  // Raw: past the material compiler, onto the device.
  auto [effect, message] = SkRuntimeEffect::MakeForShader(SkString(kCollides));
  ASSERT_TRUE(effect) << "the body is valid SkSL on its own: " << message.c_str();
  sink().drain();
  drawOnGpu(effect->makeShader(SkData::MakeEmpty(), {}));
  EXPECT_FALSE(sink().drain().empty())
      << "the device accepted a body that redeclares its own parameter — "
         "either Graphite stopped naming it `pos` or the error sink is not "
         "installed";

  // Through the library: refused at compile, so it never reaches a device.
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParams>("gpu.collides").body(Target::SkSL, kCollides));
  EXPECT_FALSE(skia::shader(Material(recipe, NoParams{}), {}))
      << "the material compiler let a reserved parameter name through";
}
