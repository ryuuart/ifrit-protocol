/** @file
 * The params struct as a layout: reflection names, counts and offsets in
 * declaration order, the walk over the fields, and the declarations each
 * target's compiler reads.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/Material.h>

#include <array>
#include <cstddef>
#include <string>

using namespace sigil::material;

namespace {

struct TwoParams {
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

TEST(Params, ReflectionNamesEveryFieldInDeclarationOrderAndFindsThemByName) {
  EXPECT_EQ(fieldCount<TwoParams>(), 2u);
  EXPECT_EQ(fieldCount<EveryKind>(), 5u);
  EXPECT_EQ((fieldName<0, TwoParams>()), "uScale");
  EXPECT_EQ((fieldName<1, TwoParams>()), "uColor");
  const Schema& s = schema<EveryKind>();
  ASSERT_EQ(s.fields.size(), 5u);
  EXPECT_EQ(s.fields[0].name, "f");
  EXPECT_EQ(s.fields[3].name, "arr");
  EXPECT_EQ(s.find("arr"), &s.fields[3]);
  EXPECT_EQ(s.find("nope"), nullptr);
}

TEST(Params, TheSchemaIsTheParamsStructsOwnLayout) {
  // A material's bytes ARE its params struct, so what a renderer writes
  // a uniform at is where the field stands in the struct — whatever the
  // compiler chose to put it. The kinds are this library's reading of
  // the C++ types beside them.
  const Schema& s = schema<EveryKind>();
  ASSERT_EQ(s.fields.size(), 5u);
  EXPECT_EQ(s.fields[0].kind, Kind::Float);
  EXPECT_EQ(s.fields[0].offset, offsetof(EveryKind, f));
  EXPECT_EQ(s.fields[1].kind, Kind::Vec2);
  EXPECT_EQ(s.fields[1].offset, offsetof(EveryKind, v2));
  EXPECT_EQ(s.fields[2].kind, Kind::Vec4);
  EXPECT_EQ(s.fields[2].offset, offsetof(EveryKind, v4));
  EXPECT_EQ(s.fields[3].kind, Kind::FloatArray);
  EXPECT_EQ(s.fields[3].floats, 3u);
  EXPECT_EQ(s.fields[3].offset, offsetof(EveryKind, arr));
  EXPECT_EQ(s.fields[4].kind, Kind::Color);
  EXPECT_EQ(s.fields[4].offset, offsetof(EveryKind, c));
  EXPECT_EQ(s.byteSize, sizeof(EveryKind));
}

TEST(Params, TheFieldWalkVisitsEveryFieldInDeclarationOrder) {
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

TEST(Params, EachTargetSpellsTheDeclarationsItsCompilerReads) {
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
