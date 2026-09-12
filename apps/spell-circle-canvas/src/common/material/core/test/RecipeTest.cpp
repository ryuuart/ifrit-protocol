/** @file
 * The recipe: identity is the object and equality the definition, what
 * the layout appends, which slots a body is declared to sample, and the
 * recipe that has no parameters at all.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/Material.h>

#include <memory>
#include <string>
#include <vector>

using namespace sigil::material;

namespace {

struct TwoParameters {
  float uScale;
  Color uColor;
};

std::shared_ptr<const Recipe> twoRecipe(const char* name = "two") {
  return std::make_shared<const Recipe>(Recipe::of<TwoParameters>(name).body(
      Target::SkSL, "half4 main(float2 p) { return half4(uColor * uScale); }"));
}

}  // namespace

TEST(Recipe, IdentityIsTheObjectAndEqualityIsTheDefinition) {
  auto a = twoRecipe();
  auto b = twoRecipe();
  EXPECT_TRUE(*a == *b);
  EXPECT_FALSE(a->id() == b->id());
  EXPECT_EQ(a->id().name, "two");
  EXPECT_EQ(a->id().recipe, a.get());
  auto c = twoRecipe("other");
  EXPECT_FALSE(*a == *c);
  EXPECT_TRUE(a->has(Target::SkSL));
  EXPECT_FALSE(a->has(Target::Slang));
  EXPECT_EQ(a->targets(), std::vector<Target>{Target::SkSL});
}

TEST(Recipe, LayoutAppendsFrameInputsAndDeclarationsListChildren) {
  Recipe r = Recipe::of<TwoParameters>("r");
  r.frame(FrameInput::Resolution).frame(FrameInput::Time).child("uTex");
  EXPECT_TRUE(r.reads(FrameInput::Time));
  EXPECT_FALSE(r.reads(FrameInput::WorldTransform));
  ASSERT_EQ(r.layout().fields.size(), 4u);
  // Enum order, not declaration order.
  EXPECT_EQ(r.layout().fields[2].name, "uTime");
  EXPECT_EQ(r.layout().fields[2].offset, 20u);
  EXPECT_EQ(r.layout().fields[3].name, "uResolution");
  EXPECT_EQ(r.layout().fields[3].offset, 24u);
  EXPECT_EQ(r.layout().byteSize, 32u);
  EXPECT_EQ(r.parameters().byteSize, 20u);
  EXPECT_EQ(r.declarations(Target::SkSL),
            "uniform float uScale;\n"
            "uniform float4 uColor;\n"
            "uniform float uTime;\n"
            "uniform float2 uResolution;\n"
            "uniform shader uTex;\n");
  EXPECT_EQ(r.source(Target::SkSL), "");
  r.body(Target::SkSL, "half4 main(float2 p) { return half4(1); }");
  EXPECT_EQ(r.source(Target::SkSL),
            r.declarations(Target::SkSL) +
                "half4 main(float2 p) { return half4(1); }");
}

TEST(Recipe, ASlotOneTargetsBodyNeverSamplesIsNotDeclaredToIt) {
  Recipe r = Recipe::of<TwoParameters>("split");
  r.child("uRead").child("uUnread");
  // With no body there is nothing to say, so both slots are declared.
  EXPECT_TRUE(r.samples(Target::SkSL, "uUnread"));
  r.body(Target::SkSL, "half4 main(float2 p) { return uRead.eval(p); }");
  r.body(Target::Slang,
         "float4 surface(float2 uv) {"
         " return uUnread.Sample(uv); }");
  // Each target declares the slots its OWN body spells: a declared slot
  // is an image sampler in the compiled program whether anything reads
  // it or not, and a device has few.
  EXPECT_TRUE(r.samples(Target::SkSL, "uRead"));
  EXPECT_FALSE(r.samples(Target::SkSL, "uUnread"));
  EXPECT_FALSE(r.samples(Target::Slang, "uRead"));
  EXPECT_TRUE(r.samples(Target::Slang, "uUnread"));
  EXPECT_NE(r.declarations(Target::SkSL).find("uniform shader uRead;"),
            std::string::npos);
  EXPECT_EQ(r.declarations(Target::SkSL).find("uUnread"), std::string::npos);
  EXPECT_NE(r.declarations(Target::Slang).find("uniform Sampler2D uUnread;"),
            std::string::npos);
  EXPECT_EQ(r.declarations(Target::Slang).find("uRead"), std::string::npos);
  // The slot is still the recipe's — what changed is what each program
  // is told about, not what a material may fill.
  EXPECT_EQ(r.children().size(), 2u);
}

TEST(Recipe, NoParametersIsARecipeOverSlotsAndFrameInputsAlone) {
  struct NoParameters {};
  auto r = std::make_shared<const Recipe>(
      Recipe::of<NoParameters>("bare")
          .frame(FrameInput::Time)
          .child("uSrc")
          .body(Target::SkSL, "half4 main(float2 p) { return uSrc.eval(p); }"));
  EXPECT_TRUE(r->parameters().fields.empty());
  EXPECT_EQ(r->parameters().byteSize, 0u);
  // The frame uniform still lays out, from offset zero.
  ASSERT_EQ(r->layout().fields.size(), 1u);
  EXPECT_EQ(r->layout().fields[0].name, "uTime");
  EXPECT_EQ(r->layout().fields[0].offset, 0u);
  // The slot is declared because this body samples it; one the body never
  // named would be an image sampler nothing reads.
  EXPECT_TRUE(r->samples(Target::SkSL, "uSrc"));
  EXPECT_EQ(r->declarations(Target::SkSL),
            "uniform float uTime;\nuniform shader uSrc;\n");
  // An instance holds no bytes of its own, and resolving still lays out
  // the frame value it declared.
  Material m(r);
  EXPECT_TRUE(m.bytes().empty());
  const Material::Resolved resolved =
      m.resolve(Target::SkSL, FrameData{.seconds = 2.0});
  ASSERT_EQ(resolved.bytes.size(), sizeof(float));
  float seconds = 0.0f;
  std::memcpy(&seconds, resolved.bytes.data(), sizeof(float));
  EXPECT_EQ(seconds, 2.0f);
}
