/** @file
 * material_skia_bench — the shader a material becomes per frame: resolve
 * plus the builder upload and makeShader, by uniform count, against the
 * same SkSL filled by hand. Run a Release build; Debug numbers say
 * nothing.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkM44.h>
#include <include/core/SkString.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <memory>

using namespace sigil::material;

namespace {

struct P2 {
  float uScale;
  Color uColor;
};
struct P16 {
  float a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p;
};

constexpr const char* kBody2 =
    "half4 main(float2 p) { return half4(uColor * uScale); }";
constexpr const char* kBody16 =
    "half4 main(float2 p) { return half4(a + b + c + d + e + f + g + h + i + "
    "j + k + l + m + n + o + p); }";

template <class P>
void BM_Shader_Live(benchmark::State& state, const char* body) {
  skia::install();
  auto recipe = std::make_shared<const Recipe>(
      Recipe::of<P>("bench").frame(FrameInput::Time).body(Target::SkSL, body));
  Material m(recipe);
  FrameData frame;
  for ([[maybe_unused]] auto iteration : state) {
    frame.seconds += 1.0 / 60.0;
    sk_sp<SkShader> s = skia::shader(m, frame);
    benchmark::DoNotOptimize(s.get());
  }
}

void BM_Shader_ByHand2(benchmark::State& state) {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform float uScale;\nuniform float4 uColor;\nuniform float uTime;\n" +
      std::string(kBody2)));
  float t = 0;
  for ([[maybe_unused]] auto iteration : state) {
    t += 1.0f / 60.0f;
    SkRuntimeShaderBuilder b(effect);
    b.uniform("uScale") = 1.0f;
    b.uniform("uColor") = SkV4{1, 1, 1, 1};
    b.uniform("uTime") = t;
    sk_sp<SkShader> s = b.makeShader();
    benchmark::DoNotOptimize(s.get());
  }
}

}  // namespace

BENCHMARK_CAPTURE(BM_Shader_Live<P2>, 2, kBody2)->Name("BM_Shader_Live/2");
BENCHMARK_CAPTURE(BM_Shader_Live<P16>, 16, kBody16)->Name("BM_Shader_Live/16");
BENCHMARK(BM_Shader_ByHand2);

BENCHMARK_MAIN();
