/** @file
 * The material: what a write to a field no body reads says, the bytes it
 * mirrors, equality by value with bindings by identity, which fields
 * carry a binding, the tiers its bindings and children put it in, what
 * resolve injects, and the uniform block's revision.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/advanced/Leaf.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Target.h>

#include <array>
#include <cstring>
#include <functional>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <span>
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

/** A compiler that records how many times it was asked, so a resolve
 *  that should have been memoised can be told from one that ran again. */
int gCompiles = 0;
std::shared_ptr<Program> countingCompiler(std::shared_ptr<const Recipe> r,
                                          Variant v, std::string&) {
  ++gCompiles;
  return std::make_shared<Program>(std::move(r), Target::Slang, v);
}

/** A leaf that is one number: the smallest thing a slot can hold
 *  directly, for the cases about whether a slot holds one at all. */
class NumberLeaf : public Leaf {
 public:
  explicit NumberLeaf(float value) : m_value(value) {}

 protected:
  bool equals(const Leaf& other) const override {
    return static_cast<const NumberLeaf&>(other).m_value == m_value;
  }

 private:
  float m_value = 0;
};

/** Everything a material writes to stderr while @p fn runs. */
std::string captureStderr(const std::function<void()>& fn) {
  testing::internal::CaptureStderr();
  fn();
  return testing::internal::GetCapturedStderr();
}

}  // namespace

TEST(Material, WritingToAFieldNoBodyReadsSaysSo) {
  // A dial that does nothing looks exactly like a wrong value from the
  // call site, and no compiler's reflection can say which it is: the
  // declarations are generated from the parameters whether the body reads
  // them or not. The RECIPE can say, and it is asked at the write.
  auto r = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("half-read")
          .body(Target::Slang, "float4 main() { return uColor; }"));
  Material m(r);
  const std::string said = captureStderr([&] { m.set("uScale", 2.0f); });
  EXPECT_NE(said.find("\"uScale\""), std::string::npos) << said;
  EXPECT_NE(said.find("no body"), std::string::npos) << said;
  // The field the body DOES read is silent, and so is a second write to
  // the one it does not.
  const std::string quiet = captureStderr([&] {
    m.set("uColor", Color{1, 0, 0, 1});
    m.set("uScale", 3.0f);
  });
  EXPECT_EQ(quiet, "") << quiet;
  // …and the value is still written: the report is about the picture,
  // not about the bytes.
  EXPECT_EQ(m.get<float>("uScale"), 3.0f);
  // A NAME INSIDE A LONGER ONE is a different name.
  auto sub = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("substring")
          .body(Target::Slang, "float4 main() { return uScaleFactor; }"));
  Material s(sub);
  EXPECT_NE(captureStderr([&] { s.set("uScale", 1.0f); }).find("uScale"),
            std::string::npos);
  // A recipe with no body at all has nothing to say.
  auto none =
      std::make_shared<const Recipe>(Recipe::of<TwoParameters>("bodiless"));
  Material n(none);
  EXPECT_EQ(captureStderr([&] { n.set("uScale", 1.0f); }), "");
}

TEST(Material, MirrorsParametersAsBytesAndSetsFields) {
  auto r = twoRecipe();
  Material m(r, TwoParameters{2.0f, {0.1f, 0.2f, 0.3f, 1.0f}});
  EXPECT_EQ(m.bytes().size(), sizeof(TwoParameters));
  EXPECT_EQ(m.get<float>("uScale"), 2.0f);
  EXPECT_EQ(m.get<Color>("uColor"), (Color{0.1f, 0.2f, 0.3f, 1.0f}));
  m.set("uScale", 3.0f);
  EXPECT_EQ(m.get<float>("uScale"), 3.0f);
  // A float4 fills a Color field: both are four floats.
  m.set("uColor", glm::vec4{1, 1, 0, 1});
  EXPECT_EQ(m.get<Color>("uColor"), (Color{1, 1, 0, 1}));
  // A mismatch is ignored, not written.
  m.set("uScale", glm::vec2{9, 9});
  EXPECT_EQ(m.get<float>("uScale"), 3.0f);
  m.set("missing", 1.0f);
  Material zero(r);
  EXPECT_EQ(zero.get<float>("uScale"), 0.0f);
  zero.set(TwoParameters{3.0f, {1, 1, 0, 1}});
  EXPECT_TRUE(zero == m);
}

TEST(Material, EqualityIsByValueWithBindingsByIdentity) {
  auto r = twoRecipe();
  const TwoParameters p{1.0f, {0, 0, 0, 1}};
  Material a(r, p), b(r, p);
  EXPECT_TRUE(a == b);
  b.set("uScale", 2.0f);
  EXPECT_FALSE(a == b);
  b.set("uScale", 1.0f);
  EXPECT_TRUE(a == b);
  // A different recipe object with the same definition is a different
  // material.
  Material c(twoRecipe(), p);
  EXPECT_FALSE(a == c);
  sigil::motion::Animatable<float> out = sigil::motion::animatable(0.5f);
  a.bind("uScale", out);
  EXPECT_FALSE(a == b);
  b.bind("uScale", out);
  EXPECT_TRUE(a == b);
  sigil::motion::Animatable<float> other = sigil::motion::animatable(0.5f);
  b.bind("uScale", other);
  EXPECT_FALSE(a == b);
  b.unbind("uScale");
  a.unbind("uScale");
  EXPECT_TRUE(a == b);
  a.amount(0.5f);
  EXPECT_FALSE(a == b);
  b.amount(0.5f);
  a.quantizeTime(12);
  EXPECT_FALSE(a == b);
  b.quantizeTime(12);
  a.worldSpace();
  EXPECT_FALSE(a == b);
  b.worldSpace();
  EXPECT_TRUE(a == b);
}

TEST(Material, AnEmptyLeafSlotComparesAgainstAFilledOneWithoutReadingIt) {
  // slot(name, shared_ptr<const Leaf>{}) is a public door — a slot
  // declared and deliberately left empty — so the two sides of a
  // comparison can disagree about whether a slot holds a leaf at all.
  // Whichever side is empty, the answer is "not equal", read from the
  // pointers rather than from what they point at.
  auto r = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("leafslot")
          .slot("uSrc")
          .body(
              Target::SkSL,
              "half4 main(float2 p) { return uSrc.eval(p) * half4(uColor); }"));
  const TwoParameters p{1.0f, {1, 1, 1, 1}};
  Material empty(r, p);
  empty.slot("uSrc", std::shared_ptr<const Leaf>{});
  Material alsoEmpty(r, p);
  alsoEmpty.slot("uSrc", std::shared_ptr<const Leaf>{});
  EXPECT_TRUE(empty == alsoEmpty);

  Material filled(r, p);
  filled.slot("uSrc", std::make_shared<const NumberLeaf>(1.0f));
  EXPECT_FALSE(empty == filled);
  EXPECT_FALSE(filled == empty);
}

TEST(Material, TiersFollowBindingsFrameInputsAndChildren) {
  auto plain = twoRecipe();
  Material still(plain, TwoParameters{});
  EXPECT_FALSE(still.isRunning());
  EXPECT_FALSE(still.geometryDependent());

  sigil::motion::Animatable<float> out = sigil::motion::animatable(0.0f);
  Material bound = still;
  bound.bind("uScale", out);
  EXPECT_TRUE(bound.isRunning());
  EXPECT_FALSE(bound.geometryDependent());

  auto timed = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("timed").frame(FrameInput::Time));
  EXPECT_TRUE(Material(timed).isRunning());
  auto sized = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("sized").frame(FrameInput::Resolution));
  EXPECT_FALSE(Material(sized).isRunning());
  EXPECT_TRUE(Material(sized).geometryDependent());

  auto parentRecipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("parent").slot("uA").slot("uB"));
  Material parent(parentRecipe);
  EXPECT_FALSE(parent.isRunning());
  parent.slot("uB", Material(sized));
  EXPECT_TRUE(parent.geometryDependent());
  EXPECT_FALSE(parent.isRunning());
  parent.slot("uA", bound);
  EXPECT_TRUE(parent.isRunning());
  ASSERT_EQ(parent.slots().size(), 2u);
  // Slots sit in recipe order however they were filled.
  EXPECT_EQ(parent.slots()[0].first, "uA");
  EXPECT_NE(parent.slot("uA"), nullptr);
  EXPECT_EQ(parent.slot("uZ"), nullptr);
  parent.slot("uZ", still);  // undeclared: ignored
  EXPECT_EQ(parent.slots().size(), 2u);

  Material same(parentRecipe);
  same.slot("uA", bound).slot("uB", Material(sized));
  EXPECT_TRUE(parent == same);
  same.slot("uB", Material(sized).amount(0.2f));
  EXPECT_FALSE(parent == same);
}

TEST(Material, SurfaceChannelsDeclareTheirTimeAndGeometryInputs) {
  const auto timed = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("surface.time").frame(FrameInput::Time));
  const auto sized = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("surface.size").frame(FrameInput::Resolution));
  for (int channel = 0; channel < 5; ++channel) {
    for (bool live : {false, true}) {
      SurfaceOptions response;
      const Material source(live ? timed : sized);
      switch (channel) {
        case 0:
          response.metallic = source;
          break;
        case 1:
          response.roughness = source;
          break;
        case 2:
          response.occlusion = source;
          break;
        case 3:
          response.normal = source;
          break;
        case 4:
          response.emissionMap = source;
          break;
      }
      const Material material = from(Color{1, 1, 1, 1}).surface(response);
      EXPECT_EQ(material.isRunning(), live) << channel;
      EXPECT_EQ(material.geometryDependent(), !live) << channel;
    }
  }
  const auto direction = sigil::motion::animatable(0.0f);
  const Material ownLight =
      from(Color{1, 1, 1, 1})
          .surface({.lighting = studio({.direction = direction})});
  EXPECT_TRUE(ownLight.isRunning());
  EXPECT_FALSE(
      from(Color{1, 1, 1, 1}).surface({.lighting = studio()}).isRunning());
}

TEST(Material, LightKindsReadOnlyTheirApplicableBindings) {
  for (LightKind kind :
       {LightKind::Directional, LightKind::Point, LightKind::Spot}) {
    for (bool elevation : {false, true}) {
      Light light;
      light.kind = kind;
      const auto angle = sigil::motion::animatable(45.0f);
      if (elevation)
        light.elevation = angle;
      else
        light.direction = angle;
      const bool readsAngle = kind != LightKind::Point;
      EXPECT_EQ(light.isRunning(), readsAngle);
      EXPECT_EQ(Lighting(light).isRunning(), readsAngle);
      EXPECT_EQ(
          from(Color{1, 1, 1, 1}).surface({.lighting = light}).isRunning(),
          readsAngle);
    }
    Light light;
    light.kind = kind;
    light.intensity = sigil::motion::animatable(1.0f);
    EXPECT_TRUE(light.isRunning());
    EXPECT_TRUE(Lighting(light).isRunning());
  }
}

TEST(Material, LightColorConstantsCompareByValueAndLiveColorsByIdentity) {
  const Color red{2, 0, 0, 1};
  const Light constant{.color = red};
  EXPECT_FALSE(constant.isRunning());
  EXPECT_EQ(constant, (Light{.color = red}));
  EXPECT_NE(constant, (Light{.color = Color{0, 0, 2, 1}}));
  auto color = sigil::motion::animatable(red);
  for (LightKind kind :
       {LightKind::Directional, LightKind::Point, LightKind::Spot}) {
    color = red;
    const Light light{.color = color, .kind = kind};
    const Light copied = light;
    const Light separate{.color = sigil::motion::animatable(color.value()),
                         .kind = kind};
    EXPECT_TRUE(light.isRunning());
    EXPECT_TRUE(Lighting(light).isRunning());
    EXPECT_TRUE(
        from(Color{1, 1, 1, 1}).surface({.lighting = light}).isRunning());
    EXPECT_EQ(light, copied);
    EXPECT_NE(light, separate);
    EXPECT_NE(light, (Light{.color = color.value(), .kind = kind}));
    color = Color{0, 0, 2, 1};
    EXPECT_EQ(copied.color.value(), color.value());
    EXPECT_EQ(light, copied);
  }
}

TEST(Material, AFieldSaysWhetherItCarriesABindingAtAll) {
  struct Table {
    float uScale;
    std::array<float, 4> uTable;
  };
  auto r = std::make_shared<const Recipe>(Recipe::of<Table>("bound").body(
      Target::SkSL,
      "half4 main(float2 p) { return half4(uScale * uTable[0]); }"));
  Material m(r);
  EXPECT_FALSE(m.isBound("uScale"));
  EXPECT_FALSE(m.isBound("nothing"));

  // An output, a plain number and a block are all bindings; only the
  // output makes the material move, which is the other question.
  sigil::motion::Animatable<float> out = sigil::motion::animatable(0.5f);
  m.bind("uScale", out);
  EXPECT_TRUE(m.isBound("uScale"));
  EXPECT_FALSE(m.isBound("uTable"));
  m.bind("uScale", 2.0f);
  EXPECT_TRUE(m.isBound("uScale"));
  EXPECT_FALSE(m.isRunning());
  m.unbind("uScale");
  EXPECT_FALSE(m.isBound("uScale"));
  // Bytes are not a binding: what `set()` writes stays a value.
  m.set("uScale", 3.0f);
  EXPECT_FALSE(m.isBound("uScale"));

  m.bind("uTable", std::make_shared<UniformBlock>(4));
  EXPECT_TRUE(m.isBound("uTable"));
  m.bind("uTable", std::shared_ptr<const UniformBlock>{});
  EXPECT_FALSE(m.isBound("uTable"));
  // A binding the material refused is no binding.
  const std::string said = captureStderr([&] { m.bind("uTable", out); });
  EXPECT_NE(said.find("\"uTable\""), std::string::npos) << said;
  EXPECT_FALSE(m.isBound("uTable"));
}

TEST(Material, ColorBindingsReadStraightComponentsAndCheckFieldKinds) {
  struct Parameters {
    Color uColor;
    float uScale;
    glm::vec4 uVector;
    std::array<float, 4> uTable;
  };
  const auto recipe =
      std::make_shared<const Recipe>(Recipe::of<Parameters>("color.binding"));
  Material material(recipe);
  auto color = sigil::motion::animatable(Color{2, .5f, .25f, 1});
  for (const char* name : {"uScale", "uVector", "uTable", "missing"}) {
    const std::string said = captureStderr([&] { material.bind(name, color); });
    EXPECT_NE(said.find(name), std::string::npos) << said;
    EXPECT_FALSE(material.isBound(name));
    EXPECT_FALSE(material.isRunning());
  }
  material.bind("uColor", color);
  EXPECT_TRUE(material.isBound("uColor"));
  EXPECT_TRUE(material.isRunning());
  const Material copied = material;
  Material separate(recipe);
  separate.bind("uColor", sigil::motion::animatable(color.value()));
  EXPECT_NE(material, separate);
  const auto resolved = [&] {
    const auto upload = material.resolve(Target::SkSL, {});
    Color value;
    std::memcpy(&value,
                upload.bytes.data() + recipe->layout().find("uColor")->offset,
                sizeof(value));
    return value;
  };
  EXPECT_EQ(resolved(), color.value());
  color = Color{.25f, 3, .5f, .75f};
  EXPECT_EQ(resolved(), color.value());
  EXPECT_EQ(material, copied);
  material.bind("uColor", Color{.5f, .25f, 2, 1});
  EXPECT_FALSE(material.isRunning());
  EXPECT_EQ(resolved(), (Color{.5f, .25f, 2, 1}));
  material.unbind("uColor");
  EXPECT_FALSE(material.isBound("uColor"));
  EXPECT_EQ(resolved(), (Color{0, 0, 0, 0}));
}

TEST(Material, PixelSamplingTransformChangesUploadAndGeometryWithoutMotion) {
  registerCompiler(Target::Slang, countingCompiler);
  struct Nothing {};
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<Nothing>("material.pixel-sampling")
          .frame(FrameInput::LocalToSample)
          .body(Target::Slang,
                "float4 surface(float2 p) { return float4("
                "uLocalToSample[0].xy, 0, 1); }"));
  const Material material(recipe);
  EXPECT_TRUE(material.geometryDependent());
  EXPECT_FALSE(material.isRunning());
  const Material parent =
      from(Color{0, 0, 0, 1}).layer(material, {.opacity = .5f});
  EXPECT_TRUE(parent.geometryDependent());
  EXPECT_FALSE(parent.isRunning());
  FrameData frame;
  const auto matrix = [&](const Material::Resolved& resolved) {
    std::array<float, 9> values{};
    EXPECT_EQ(resolved.bytes.size(), values.size() * sizeof(float));
    if (resolved.bytes.size() != sizeof(values)) return values;
    std::memcpy(values.data(), resolved.bytes.data(), sizeof(values));
    return values;
  };
  const Material::Resolved identity = material.resolve(Target::Slang, frame);
  ASSERT_TRUE(identity.program);
  const std::array expectedIdentity{1.f, 0.f, 0.f, 0.f, 1.f,
                                    0.f, 0.f, 0.f, 1.f};
  EXPECT_EQ(matrix(identity), expectedIdentity);
  const std::byte* held = identity.bytes.data();
  EXPECT_EQ(material.resolve(Target::Slang, frame).bytes.data(), held);
  frame.localToSample = glm::mat3{0, .25f, 0, -.125f, 0, 0, 8, 12, 1};
  const Material::Resolved rotated = material.resolve(Target::Slang, frame);
  const std::array expectedRotated{0.f, .25f, 0.f,  -.125f, 0.f,
                                   0.f, 8.f,  12.f, 1.f};
  EXPECT_EQ(matrix(rotated), expectedRotated);
  EXPECT_EQ(rotated.program, identity.program);
  EXPECT_EQ(material.resolve(Target::Slang, frame).bytes.data(),
            rotated.bytes.data());
  const glm::vec3 offset = frame.localToSample * glm::vec3{2, 0, 0};
  EXPECT_EQ(offset, (glm::vec3{0, .5f, 0}));
}

TEST(Material, ResolveSamplesBindingsInjectsFrameAndMemoises) {
  ProgramCache::shared().registerCompiler(Target::Slang, countingCompiler);
  struct P {
    float uScale;
    std::array<float, 4> uTable;
  };
  auto r = std::make_shared<const Recipe>(Recipe::of<P>("live")
                                              .frame(FrameInput::Time)
                                              .frame(FrameInput::Resolution)
                                              .body(Target::Slang, "x"));
  Material m(r, P{1.0f, {1, 2, 3, 4}});
  sigil::motion::Animatable<float> out = sigil::motion::animatable(5.0f);
  auto block = std::make_shared<UniformBlock>(4);
  m.bind("uScale", out).bind("uTable", block);
  m.bind("uTable", std::make_shared<UniformBlock>(3));  // wrong size: ignored
  EXPECT_TRUE(m.isRunning());

  FrameData frame;
  frame.seconds = 1.25;
  frame.resolution = {64, 32};
  gCompiles = 0;
  Material::Resolved a = m.resolve(Target::Slang, frame);
  ASSERT_NE(a.program, nullptr);
  ASSERT_EQ(a.bytes.size(), r->layout().byteSize);
  const auto at = [&](const Material::Resolved& res, const char* name) {
    float v;
    std::memcpy(&v, res.bytes.data() + r->layout().find(name)->offset,
                sizeof v);
    return v;
  };
  EXPECT_EQ(at(a, "uScale"), 5.0f);
  EXPECT_EQ(at(a, "uTable"), 0.0f);  // the block's zeros, not the parameters
  EXPECT_EQ(at(a, "uTime"), 1.25f);
  EXPECT_EQ(at(a, "uResolution"), 64.0f);

  // Nothing changed: the same bytes come back, from the memo.
  Material::Resolved b = m.resolve(Target::Slang, frame);
  EXPECT_EQ(a.bytes.data(), b.bytes.data());
  EXPECT_EQ(a.program, b.program);

  // Draft edits stay hidden while scalar and frame inputs update.
  block->values()[0] = 7.0f;
  Material::Resolved c = m.resolve(Target::Slang, frame);
  EXPECT_EQ(at(c, "uTable"), 0.0f);
  EXPECT_EQ(c.bytes.data(), b.bytes.data());
  out = 6.0f;
  Material::Resolved scalarChanged = m.resolve(Target::Slang, frame);
  EXPECT_EQ(at(scalarChanged, "uScale"), 6.0f);
  EXPECT_EQ(at(scalarChanged, "uTable"), 0.0f);
  frame.seconds = 1.75;
  frame.resolution = {32, 16};
  Material::Resolved frameChanged = m.resolve(Target::Slang, frame);
  EXPECT_EQ(at(frameChanged, "uTime"), 1.75f);
  EXPECT_EQ(at(frameChanged, "uResolution"), 32.0f);
  EXPECT_EQ(at(frameChanged, "uTable"), 0.0f);
  block->commit();
  EXPECT_EQ(at(m.resolve(Target::Slang, frame), "uTable"), 7.0f);

  // Quantised time snaps to the step, so within one step the memo holds.
  m.quantizeTime(4.0f);
  frame.seconds = 1.30;
  const std::byte* first = m.resolve(Target::Slang, frame).bytes.data();
  frame.seconds = 1.45;
  EXPECT_EQ(m.resolve(Target::Slang, frame).bytes.data(), first);
  EXPECT_EQ(at(m.resolve(Target::Slang, frame), "uTime"), 1.25f);
  frame.seconds = 1.5;
  EXPECT_EQ(at(m.resolve(Target::Slang, frame), "uTime"), 1.5f);

  // No SkSL body: null program, bytes still resolved.
  Material::Resolved none = m.resolve(Target::SkSL, frame);
  EXPECT_EQ(none.program, nullptr);
  EXPECT_EQ(none.bytes.size(), r->layout().byteSize);
}

TEST(Material, ResolvedUploadsRetainBytesAcrossFramesAndMaterialLifetime) {
  registerCompiler(Target::Slang, countingCompiler);
  struct Parameters {
    float uScale;
    std::array<float, 2> uTable;
  };
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<Parameters>("material.owned-upload")
          .frame(FrameInput::Time)
          .body(Target::Slang, "uScale uTable uTime"));
  const auto at = [&](const Material::Resolved& upload, const char* name) {
    float value = 0;
    std::memcpy(&value,
                upload.bytes.data() + recipe->layout().find(name)->offset,
                sizeof(value));
    return value;
  };
  Material::Resolved retained;
  {
    sigil::motion::Animatable<float> scale = sigil::motion::animatable(2.f);
    auto table = std::make_shared<UniformBlock>(2);
    table->values()[0] = 3;
    table->commit();
    Material material(recipe, Parameters{1, {0, 0}});
    material.bind("uScale", scale).bind("uTable", table);
    retained = material.resolve(Target::Slang, {.seconds = 1});
    const std::vector<std::byte> original(retained.bytes.begin(),
                                          retained.bytes.end());
    EXPECT_EQ(material.resolve(Target::Slang, {.seconds = 1}).bytes.data(),
              retained.bytes.data());
    std::array<Material::Resolved, 3> later;
    for (size_t i = 0; i < later.size(); ++i) {
      scale = 4.f + static_cast<float>(i);
      table->values()[0] = 8.f + static_cast<float>(i);
      table->commit();
      later[i] = material.resolve(Target::Slang, {.seconds = 2.0 + i});
      EXPECT_EQ(at(later[i], "uScale"), 4.f + i);
      EXPECT_EQ(at(later[i], "uTable"), 8.f + i);
      EXPECT_EQ(at(later[i], "uTime"), 2.f + i);
    }
    EXPECT_EQ(
        std::vector<std::byte>(retained.bytes.begin(), retained.bytes.end()),
        original);
    for (size_t i = 0; i < later.size(); ++i)
      EXPECT_EQ(at(later[i], "uTime"), 2.f + i);
  }
  ASSERT_TRUE(retained.program);
  EXPECT_EQ(at(retained, "uScale"), 2.f);
  EXPECT_EQ(at(retained, "uTable"), 3.f);
  EXPECT_EQ(at(retained, "uTime"), 1.f);
  const Material::Resolved copy = retained;
  retained = {};
  EXPECT_EQ(at(copy, "uTime"), 1.f);
}

TEST(Material, CopiedResolveStateSeparatesParametersVariantsAndRecipes) {
  registerCompiler(Target::Slang, countingCompiler);
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("material.resolve-copies")
          .body(Target::Slang, "uScale uColor"));
  const Material original(recipe, TwoParameters{2, {1, 0, 0, 1}});
  Material copy = original;
  const Material::Resolved first = original.resolve(Target::Slang, {});
  EXPECT_EQ(copy.resolve(Target::Slang, {}).bytes.data(), first.bytes.data());
  copy.set("uScale", 7.f);
  copy.set("uColor", Color{0, 1, 0, 1});
  const Material::Resolved edited = copy.resolve(Target::Slang, {});
  const auto parameters = [](const Material::Resolved& upload) {
    TwoParameters values{};
    std::memcpy(&values, upload.bytes.data(), sizeof(values));
    return values;
  };
  EXPECT_EQ(parameters(edited).uScale, 7.f);
  EXPECT_EQ(parameters(edited).uColor, (Color{0, 1, 0, 1}));
  EXPECT_EQ(parameters(first).uScale, 2.f);
  EXPECT_EQ(parameters(first).uColor, (Color{1, 0, 0, 1}));
  EXPECT_EQ(original.resolve(Target::Slang, {}).bytes.data(),
            first.bytes.data());
  EXPECT_EQ(edited.program, first.program);

  const Material::Resolved variant = original.resolve(Target::Slang, {}, {3});
  ASSERT_TRUE(variant.program);
  EXPECT_NE(variant.program, first.program);
  const auto replacement = std::make_shared<const Recipe>(
      Recipe::of<TwoParameters>("material.resolve-specialization")
          .frame(FrameInput::Time)
          .body(Target::Slang, "uColor uScale uTime"));
  const Material specialized = original.withRecipe(replacement);
  const Material::Resolved changed =
      specialized.resolve(Target::Slang, {.seconds = 6});
  ASSERT_TRUE(changed.program);
  EXPECT_NE(changed.program, first.program);
  EXPECT_EQ(changed.program->recipe().name(), replacement->name());
  EXPECT_EQ(changed.bytes.size(), replacement->layout().byteSize);
  EXPECT_EQ(first.bytes.size(), recipe->layout().byteSize);
  EXPECT_EQ(parameters(changed).uScale, 2.f);
  EXPECT_EQ(original.resolve(Target::Slang, {}).program, first.program);
}

TEST(UniformBlock, RevisionAdvancesOnCommitOnly) {
  UniformBlock block(3);
  EXPECT_EQ(block.size(), 3u);
  EXPECT_EQ(block.revision(), 0u);
  block.values()[1] = 2.0f;
  EXPECT_EQ(block.revision(), 0u);
  block.commit();
  EXPECT_EQ(block.revision(), 1u);
  block.commit();
  EXPECT_EQ(block.revision(), 2u);
  const UniformBlock& ro = block;
  EXPECT_EQ(ro.values()[1], 2.0f);
  EXPECT_EQ(ro.values()[0], 0.0f);
}

TEST(UniformBlock, HeldDraftSpansPublishOnlyOnCommit) {
  UniformBlock block(3);
  const std::span<float> draft = block.values();
  const std::span<const float> published = block.committedValues();
  EXPECT_EQ(published[1], 0.0f);
  draft[1] = 2.0f;
  EXPECT_EQ(published[1], 0.0f);
  block.commit();
  EXPECT_EQ(published[1], 2.0f);
  EXPECT_EQ(block.values().data(), draft.data());
  EXPECT_EQ(block.committedValues().data(), published.data());

  draft[1] = 9.0f;
  const UniformBlock& readOnly = block;
  EXPECT_EQ(readOnly.values()[1], 9.0f);
  EXPECT_EQ(readOnly.committedValues()[1], 2.0f);
  block.commit();
  EXPECT_EQ(published[1], 9.0f);
  EXPECT_EQ(block.values().data(), draft.data());
  EXPECT_EQ(block.committedValues().data(), published.data());
}
