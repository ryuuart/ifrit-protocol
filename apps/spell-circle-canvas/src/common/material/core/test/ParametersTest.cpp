/** @file
 * The parameter struct as a layout: reflection names, counts and offsets in
 * declaration order, the walk over the fields, the same layout computed
 * from a field list no C++ type stands behind, and the declarations each
 * target's compiler reads.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/core/Target.h>

#include <array>
#include <cstddef>
#include <string>

using namespace sigil::material;

namespace {

struct TwoParameters {
  float uScale;
  Color uColor;
};

struct EveryKind {
  float f;
  glm::vec2 v2;
  glm::vec4 v4;
  std::array<float, 3> arr;
  Color c;
};

}  // namespace

TEST(Parameters,
     ReflectionNamesEveryFieldInDeclarationOrderAndFindsThemByName) {
  EXPECT_EQ(fieldCount<TwoParameters>(), 2u);
  EXPECT_EQ(fieldCount<EveryKind>(), 5u);
  EXPECT_EQ((fieldName<0, TwoParameters>()), "uScale");
  EXPECT_EQ((fieldName<1, TwoParameters>()), "uColor");
  const Schema& s = schema<EveryKind>();
  ASSERT_EQ(s.fields.size(), 5u);
  EXPECT_EQ(s.fields[0].name, "f");
  EXPECT_EQ(s.fields[3].name, "arr");
  EXPECT_EQ(s.find("arr"), &s.fields[3]);
  EXPECT_EQ(s.find("nope"), nullptr);
}

TEST(Parameters, TheSchemaIsTheParametersStructsOwnLayout) {
  // A material's bytes ARE its parameter struct, so what a renderer writes
  // a uniform at is where the field stands in the struct — whatever the
  // compiler chose to put it. The kinds are this library's reading of
  // the C++ types beside them.
  const Schema& s = schema<EveryKind>();
  ASSERT_EQ(s.fields.size(), 5u);
  EXPECT_EQ(s.fields[0].kind, ParameterType::Float);
  EXPECT_EQ(s.fields[0].offset, offsetof(EveryKind, f));
  EXPECT_EQ(s.fields[1].kind, ParameterType::Vec2);
  EXPECT_EQ(s.fields[1].offset, offsetof(EveryKind, v2));
  EXPECT_EQ(s.fields[2].kind, ParameterType::Vec4);
  EXPECT_EQ(s.fields[2].offset, offsetof(EveryKind, v4));
  EXPECT_EQ(s.fields[3].kind, ParameterType::FloatArray);
  EXPECT_EQ(s.fields[3].floats, 3u);
  EXPECT_EQ(s.fields[3].offset, offsetof(EveryKind, arr));
  EXPECT_EQ(s.fields[4].kind, ParameterType::Color);
  EXPECT_EQ(s.fields[4].offset, offsetof(EveryKind, c));
  EXPECT_EQ(s.byteSize, sizeof(EveryKind));
}

TEST(Parameters, TheFieldWalkVisitsEveryFieldInDeclarationOrder) {
  const EveryKind p{
      1.5f, {2, 3}, {4, 5, 6, 7}, {8, 9, 10}, {0.1f, 0.2f, 0.3f, 1}};
  std::string names;
  forEachField(p, [&](std::string_view name, const auto& value) {
    names += name;
    names += ' ';
    (void)value;
  });
  EXPECT_EQ(names, "f v2 v4 arr c ");
}

TEST(Parameters, APackedFieldListLaysOutTheWayTheStructOfThoseFieldsDoes) {
  // The door for an ABI no C++ type stands behind. It is the SAME layout
  // the compiler gives the struct, which is what lets a definition
  // assembled at run time upload through a body written for the struct.
  const Schema packed = packedSchema({
      {.name = "f", .kind = ParameterType::Float},
      {.name = "v2", .kind = ParameterType::Vec2},
      {.name = "v4", .kind = ParameterType::Vec4},
      {.name = "arr", .kind = ParameterType::FloatArray, .floats = 3},
      {.name = "c", .kind = ParameterType::Color},
  });
  EXPECT_TRUE(packed == schema<EveryKind>());
  // A float count the caller spelled for a kind that states its own is
  // the kind's.
  const Schema matrix = packedSchema({
      {.name = "uWorld", .kind = ParameterType::Mat3, .floats = 2},
      {.name = "uScale", .kind = ParameterType::Float, .floats = 4},
  });
  ASSERT_EQ(matrix.fields.size(), 2u);
  EXPECT_EQ(matrix.fields[0].floats, 9u);
  EXPECT_EQ(matrix.fields[1].offset, 9 * sizeof(float));
  EXPECT_EQ(matrix.byteSize, 10 * sizeof(float));
  EXPECT_EQ(packedSchema({}).byteSize, 0u);
}

TEST(Parameters, APackedFieldListLeavesOutARepeatedNameAndAnEmptyArray) {
  // Both would make a layout nothing downstream can be asked: two fields
  // of one name are one uniform to `find()` and two declarations to a
  // compiler, and an array of no floats declares `float name[0]`.
  testing::internal::CaptureStderr();
  const Schema packed = packedSchema({
      {.name = "uTone", .kind = ParameterType::Color},
      {.name = "uTone", .kind = ParameterType::Float},
      {.name = "uBars", .kind = ParameterType::FloatArray},
      {.name = "uScale", .kind = ParameterType::Float},
  });
  const std::string said = testing::internal::GetCapturedStderr();
  EXPECT_NE(said.find("\"uTone\""), std::string::npos) << said;
  EXPECT_NE(said.find("\"uBars\""), std::string::npos) << said;
  ASSERT_EQ(packed.fields.size(), 2u);
  EXPECT_EQ(packed.fields[0].name, "uTone");
  EXPECT_EQ(packed.fields[0].kind, ParameterType::Color);
  EXPECT_EQ(packed.fields[1].name, "uScale");
  // What is left out takes no bytes with it: the layout is the fields
  // that came back.
  EXPECT_EQ(packed.fields[1].offset, 4 * sizeof(float));
  EXPECT_EQ(packed.byteSize, 5 * sizeof(float));
}

TEST(Parameters, EachTargetSpellsTheDeclarationsItsCompilerReads) {
  const std::string sksl = declare<EveryKind>(Target::SkSL);
  EXPECT_EQ(sksl,
            "uniform float f;\n"
            "uniform float2 v2;\n"
            "uniform float4 v4;\n"
            "uniform float arr[3];\n"
            "uniform float4 c;\n");
  // Slang spells these vector types the same way SkSL does.
  EXPECT_EQ(declare<EveryKind>(Target::Slang), sksl);
}
