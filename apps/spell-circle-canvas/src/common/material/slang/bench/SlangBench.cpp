// What the Slang backend costs: the compile of one program, which is
// paid once per recipe and variant, against the per-draw write into the
// layout that compile reported, which is paid every frame.

#include <benchmark/benchmark.h>
#include <sigilmaterial/slang/SlangCompiler.h>

#include <string>

using namespace sigil::material::slang;

namespace {

constexpr const char* kModule = R"SLANG(
struct VSOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

uniform float4x4 uModel;
uniform float4x4 uViewProjection;
uniform float4 uTint;
uniform float4 uLights[8];

[shader("vertex")]
VSOut vsTest(uint id : SV_VertexID) {
  VSOut out;
  out.position = mul(uViewProjection, mul(uModel, float4(float(id), 0, 0, 1)));
  out.uv = float2(0, 0);
  return out;
}

[shader("fragment")]
float4 fsTest(VSOut input) : SV_Target {
  return uTint * uLights[0];
}
)SLANG";

/** The one program every arm below writes into. */
const Compiled& program() {
  static const Compiled built = [] {
    Compiled out;
    std::string error;
    compileModule(kModule, "vsTest", "fsTest", /*lit=*/false, &out, &error);
    return out;
  }();
  return built;
}

void BM_Compile(benchmark::State& state) {
  for (auto _ : state) {
    Compiled out;
    std::string error;
    benchmark::DoNotOptimize(
        compileModule(kModule, "vsTest", "fsTest", /*lit=*/false, &out, &error));
  }
}
BENCHMARK(BM_Compile);

/** A draw's whole uniform write: two matrices, a colour and eight
 *  emitters — what a lit body actually sets. */
void BM_WriteUniforms(benchmark::State& state) {
  const Compiled& built = program();
  const glm::mat4 m(1.0f);
  const float light[4] = {0, 1, 0, 1};
  for (auto _ : state) {
    Uniforms values(built);
    values.set("uModel", m);
    values.set("uViewProjection", m);
    values.set("uTint", 1, 1, 1, 1);
    for (size_t i = 0; i < 8; ++i) values.setElement("uLights", i, light, 4);
    benchmark::DoNotOptimize(values.bytes().data());
  }
}
BENCHMARK(BM_WriteUniforms);

/** One name looked up in the layout, which is what every set() above
 *  pays before it copies anything. */
void BM_LookUpUniform(benchmark::State& state) {
  const Compiled& built = program();
  for (auto _ : state) benchmark::DoNotOptimize(built.uniform("uTint"));
}
BENCHMARK(BM_LookUpUniform);

}  // namespace
