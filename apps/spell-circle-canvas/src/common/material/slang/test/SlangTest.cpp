// The Slang backend: a module compiles, its layout is the compiler's own
// answer rather than a guess, a draw's bytes land at those offsets, the
// two modules every session carries are importable, and the kit's grained
// recipes compile as the surface a device renderer asks them for.

#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/slang/SlangCompiler.h>

#include <cstring>
#include <string>

#include <gtest/gtest.h>

using namespace sigil::material::slang;

namespace {

/** A whole module: one uniform buffer, one sampled slot, and the two
 *  stages that read them. */
constexpr const char* kModule = R"SLANG(
struct VSOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

uniform float4x4 uModel;
uniform float4 uTint;
uniform float4 uPoints[3];
uniform Texture2D uMap;
uniform SamplerState uMapSampler;

[shader("vertex")]
VSOut vsTest(uint id : SV_VertexID) {
  VSOut out;
  out.position = mul(uModel, float4(float(id), 0, 0, 1));
  out.uv = float2(0, 0);
  return out;
}

[shader("fragment")]
float4 fsTest(VSOut input) : SV_Target {
  return uTint * uPoints[1] * uMap.Sample(uMapSampler, input.uv);
}
)SLANG";

}  // namespace

TEST(MaterialSlang, AModuleCompilesToTwoStagesAndALayout) {
  Compiled built;
  std::string error;
  ASSERT_TRUE(compileModule(kModule, "vsTest", "fsTest", /*lit=*/false, &built,
                            &error))
      << error;
  EXPECT_FALSE(built.empty());
  EXPECT_FALSE(built.fragment.empty());
  // SPIR-V's magic word, so what came back is words and not bytes read
  // sideways.
  EXPECT_EQ(built.vertex.front(), 0x07230203u);

  const UniformSlot* tint = built.uniform("uTint");
  ASSERT_NE(tint, nullptr);
  EXPECT_EQ(tint->bytes, 16u);
  EXPECT_EQ(tint->count, 1u);

  // A matrix is rows at a stride, not sixteen contiguous floats.
  const UniformSlot* model = built.uniform("uModel");
  ASSERT_NE(model, nullptr);
  EXPECT_EQ(model->count, 4u);
  EXPECT_GT(model->stride, 0u);

  // …and an array is elements at one.
  const UniformSlot* points = built.uniform("uPoints");
  ASSERT_NE(points, nullptr);
  EXPECT_EQ(points->count, 3u);
  EXPECT_GT(points->stride, 0u);

  EXPECT_GE(built.uniformBytes, model->offset + model->bytes);
  // A sampled slot carries no bytes, so it is a texture and not a
  // uniform.
  EXPECT_EQ(built.uniform("uMap"), nullptr);
  ASSERT_FALSE(built.textures.empty());
  EXPECT_NE(std::find(built.textures.begin(), built.textures.end(), "uMap"),
            built.textures.end());
}

TEST(MaterialSlang, AUniformNoBodyReadsIsNotInTheLayout) {
  Compiled built;
  std::string error;
  ASSERT_TRUE(compileModule(kModule, "vsTest", "fsTest", /*lit=*/false, &built,
                            &error))
      << error;
  // Nothing declared it, so nothing can be written for it — and writing
  // to a name the program does not carry is a no-op rather than a fault.
  EXPECT_EQ(built.uniform("uNeverDeclared"), nullptr);
  Uniforms values(built);
  values.set("uNeverDeclared", 1, 2, 3, 4);
  EXPECT_EQ(values.bytes().size(), built.uniformBytes);
}

TEST(MaterialSlang, ADrawsBytesLandAtTheReportedOffsets) {
  Compiled built;
  std::string error;
  ASSERT_TRUE(compileModule(kModule, "vsTest", "fsTest", /*lit=*/false, &built,
                            &error))
      << error;

  Uniforms values(built);
  values.set("uTint", 0.25f, 0.5f, 0.75f, 1.0f);
  const UniformSlot* tint = built.uniform("uTint");
  ASSERT_NE(tint, nullptr);
  float read[4] = {0, 0, 0, 0};
  std::memcpy(read, values.bytes().data() + tint->offset, sizeof(read));
  EXPECT_FLOAT_EQ(read[0], 0.25f);
  EXPECT_FLOAT_EQ(read[3], 1.0f);

  // An element of an array goes at the element's stride, not four floats
  // in.
  const float second[4] = {9, 8, 7, 6};
  values.setElement("uPoints", 1, second, 4);
  const UniformSlot* points = built.uniform("uPoints");
  ASSERT_NE(points, nullptr);
  std::memcpy(read, values.bytes().data() + points->offset + points->stride,
              sizeof(read));
  EXPECT_FLOAT_EQ(read[0], 9.0f);
  // …and an index past the array writes nothing.
  values.setElement("uPoints", 99, second, 4);
  EXPECT_EQ(values.bytes().size(), built.uniformBytes);
}

TEST(MaterialSlang, AMatrixIsWrittenRowByRow) {
  Compiled built;
  std::string error;
  ASSERT_TRUE(compileModule(kModule, "vsTest", "fsTest", /*lit=*/false, &built,
                            &error))
      << error;
  glm::mat4 m(1.0f);
  m[3][0] = 5.0f;  // the translation in x, column-major as glm holds it
  Uniforms values(built);
  values.set("uModel", m);
  const UniformSlot* slot = built.uniform("uModel");
  ASSERT_NE(slot, nullptr);
  // The shader reads rows, so what was written is the transpose: row 0,
  // element 3.
  float row0[4];
  std::memcpy(row0, values.bytes().data() + slot->offset, sizeof(row0));
  EXPECT_FLOAT_EQ(row0[3], 5.0f);
}

TEST(MaterialSlang, EverySessionCarriesPortableAndShading) {
  // Both are loaded by name, so this resolves with nothing on disk.
  const std::string source = std::string(R"SLANG(
import Portable;
import Shading;

struct VSOut { float4 position : SV_Position; };

[shader("vertex")]
VSOut vsTest(uint id : SV_VertexID) {
  VSOut out;
  out.position = float4(sqrtP(float(id)), 0, 0, 1);
  return out;
}

[shader("fragment")]
float4 fsTest(VSOut input) : SV_Target {
  return float4(lambert(float3(0, 0, 1), float3(0, 0, 1)), 0, 0, 1);
}
)SLANG");
  Compiled built;
  std::string error;
  EXPECT_TRUE(compileModule(source, "vsTest", "fsTest", /*lit=*/false, &built,
                            &error))
      << error;
}

TEST(MaterialSlang, ABodyThatDoesNotCompileSaysWhy) {
  Compiled built;
  std::string error;
  EXPECT_FALSE(compileModule("this is not Slang", "vsTest", "fsTest",
                             /*lit=*/false, &built, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_TRUE(built.empty());
}

TEST(MaterialSlang, AMissingEntryPointIsNamed) {
  Compiled built;
  std::string error;
  EXPECT_FALSE(compileModule(kModule, "vsTest", "fsNoSuchStage",
                             /*lit=*/false, &built, &error));
  EXPECT_NE(error.find("fsNoSuchStage"), std::string::npos) << error;
}

// ---------------------------------------------------------------------------
// The kit's grained recipes carry a Slang body, and it compiles.

TEST(MaterialSlang, EveryGrainedRecipeCompilesAsASurface) {
  // A body answers `float4 surface(float2 uv)`; the scaffold a renderer
  // hands the compiler declares the uniforms the recipe generates and the
  // two stages that read the surface.
  constexpr const char* kScaffold = R"SLANG(
struct VSOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };

[shader("vertex")]
VSOut vsTest(uint id : SV_VertexID) {
  VSOut out;
  out.position = float4(float(id), 0, 0, 1);
  out.uv = float2(0.5, 0.5);
  return out;
}

[shader("fragment")]
float4 fsTest(VSOut input) : SV_Target {
  return surface(input.uv);
}
)SLANG";
  namespace kit = sigil::material::kit;
  using sigil::material::Recipe;
  using sigil::material::Target;
  for (const Recipe* recipe :
       {kit::stoneRecipe().get(), kit::timberRecipe().get(),
        kit::lattenRecipe().get(), kit::boardRecipe().get()}) {
    Compiled built;
    std::string error;
    const std::string source = recipe->source(Target::Slang) + kScaffold;
    EXPECT_TRUE(compileModule(source, "vsTest", "fsTest", /*lit=*/false,
                              &built, &error))
        << recipe->name() << ": " << error;
    // Every parameter the body reads is in the layout.
    EXPECT_NE(built.uniform("seed"), nullptr) << recipe->name();
  }
}
