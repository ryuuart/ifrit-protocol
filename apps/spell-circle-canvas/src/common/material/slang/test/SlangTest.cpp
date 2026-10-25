// The Slang backend: a module compiles, its layout is the compiler's own
// answer rather than a guess, a draw's bytes land at those offsets, the
// two modules every session carries are importable, and a recipe's Slang
// body compiles as the surface a device renderer asks it for.

#include <gtest/gtest.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/slang/SlangCompiler.h>
#include <sigilshaders/MaterialSlang.h>
#include <unistd.h>

#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>

#include "ShaderTable.h"

using namespace sigil::material::slang;

namespace {

/** The compile written into @p out, its error into @p error: the shape the
 *  cases below read a program through. */
bool compileInto(std::string_view source, std::string_view vertexEntry,
                 std::string_view fragmentEntry, bool lit, Compiled* out,
                 std::string* error) {
  auto compiled = compileModule(source, vertexEntry, fragmentEntry, lit);
  if (!compiled) {
    *error = compiled.error().message;
    return false;
  }
  *out = std::move(*compiled);
  return true;
}

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
  ASSERT_TRUE(
      compileInto(kModule, "vsTest", "fsTest", /*lit=*/false, &built, &error))
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
  ASSERT_TRUE(
      compileInto(kModule, "vsTest", "fsTest", /*lit=*/false, &built, &error))
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
  ASSERT_TRUE(
      compileInto(kModule, "vsTest", "fsTest", /*lit=*/false, &built, &error))
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
  ASSERT_TRUE(
      compileInto(kModule, "vsTest", "fsTest", /*lit=*/false, &built, &error))
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
  EXPECT_TRUE(
      compileInto(source, "vsTest", "fsTest", /*lit=*/false, &built, &error))
      << error;
}

TEST(MaterialSlang, ABodyThatDoesNotCompileSaysWhy) {
  const auto refused =
      compileModule("this is not Slang", "vsTest", "fsTest", /*lit=*/false);
  ASSERT_FALSE(refused);
  EXPECT_FALSE(refused.error().message.empty());
}

TEST(MaterialSlang, ADiagnosticSaysNothingAboutWhatWasCompiledBefore) {
  // A module is loaded under a name no other module shares, and the
  // compiler quotes that name back at every source location it reports.
  // What a caller reads is about the source it handed over, so the same
  // body read twice — with a compile in between — says the same thing.
  Compiled built;
  std::string first;
  std::string second;
  EXPECT_FALSE(compileInto("this is not Slang", "vsTest", "fsTest",
                           /*lit=*/false, &built, &first));
  EXPECT_TRUE(
      compileInto(kModule, "vsTest", "fsTest", /*lit=*/false, &built, &second))
      << second;
  EXPECT_FALSE(compileInto("this is not Slang", "vsTest", "fsTest",
                           /*lit=*/false, &built, &second));
  EXPECT_EQ(first, second);
  for (size_t at = first.find("SigilProgram"); at != std::string::npos;
       at = first.find("SigilProgram", at + 1)) {
    const size_t after = at + std::strlen("SigilProgram");
    EXPECT_FALSE(after < first.size() &&
                 std::isdigit((unsigned char)first[after]))
        << first;
  }
}

TEST(MaterialSlang, AMissingEntryPointIsNamed) {
  const auto refused =
      compileModule(kModule, "vsTest", "fsNoSuchStage", /*lit=*/false);
  ASSERT_FALSE(refused);
  EXPECT_NE(refused.error().message.find("fsNoSuchStage"), std::string::npos)
      << refused.error().message;
}

// ---------------------------------------------------------------------------
// A recipe's Slang body compiles as a surface.

namespace {

/** A body answers `float4 surface(float2 uv)`; the scaffold a renderer
 *  hands the compiler declares the two stages that read it, beside the
 *  uniforms the recipe generates. */
constexpr const char* kSurfaceScaffold = R"SLANG(
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

}  // namespace

namespace {

struct SurfaceStandIn {
  float seed = 0;
};

}  // namespace

TEST(MaterialSlang, ARecipesSlangBodyCompilesAsASurface) {
  using sigil::material::FrameInput;
  using sigil::material::Recipe;
  using sigil::material::Target;
  Recipe recipe =
      Recipe::of<SurfaceStandIn>("slang.test.surface")
          .body(Target::Slang,
                "float4 surface(float2 uv) {\n"
                "  return float4(frac(uv * uResolution / 64.0), seed, 1.0);\n"
                "}\n")
          .frame(FrameInput::Resolution);
  Compiled built;
  std::string error;
  const std::string source = recipe.source(Target::Slang) + kSurfaceScaffold;
  ASSERT_TRUE(
      compileInto(source, "vsTest", "fsTest", /*lit=*/false, &built, &error))
      << error;
  // Every parameter the body reads is in the layout, and a body that reads
  // the frame's resolution has it there too.
  EXPECT_NE(built.uniform("seed"), nullptr);
  EXPECT_NE(built.uniform("uResolution"), nullptr);
}

// ---- the embedded shader table --------------------------------------------

TEST(MaterialSlang, TheShaderTableHoldsEveryFileTheDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      shaderSources(), SIGIL_MATERIAL_SLANG_SHADER_DIR);
}

TEST(MaterialSlang, AStoreKeepsOneProgramPerTextItCompiled) {
  // A named store is written after a compile, once per distinct text: the
  // same text again is the process's own copy, and a text that differs
  // by a comment is a program of its own.
  const std::filesystem::path store =
      std::filesystem::temp_directory_path() /
      ("sigil-material-slang-store-" + std::to_string(::getpid()));
  std::filesystem::remove_all(store);
  useProgramStore(store);
  const std::string first = std::string(kModule) + "\n// kept once\n";
  const std::string second = std::string(kModule) + "\n// kept twice\n";
  Compiled a;
  Compiled again;
  Compiled b;
  std::string error;
  ASSERT_TRUE(compileInto(first, "vsTest", "fsTest", false, &a, &error))
      << error;
  ASSERT_TRUE(compileInto(first, "vsTest", "fsTest", false, &again, &error))
      << error;
  ASSERT_TRUE(compileInto(second, "vsTest", "fsTest", false, &b, &error))
      << error;
  useProgramStore({});
  EXPECT_EQ(again.vertex, a.vertex);
  EXPECT_EQ(again.fragment, a.fragment);
  EXPECT_EQ(again.textures, a.textures);
  ASSERT_NE(again.uniform("uTint"), nullptr);
  EXPECT_EQ(again.uniform("uTint")->offset, a.uniform("uTint")->offset);
  size_t kept = 0;
  for (const auto& entry : std::filesystem::directory_iterator(store))
    kept += entry.path().extension() == ".program" ? 1 : 0;
  EXPECT_EQ(kept, 2u);
  std::filesystem::remove_all(store);
}
