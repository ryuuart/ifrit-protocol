/** @file
 * The material built up by composition: a colour converts, layers stack
 * in order, a surface and a base are read back, and two materials built
 * the same way compare equal while any difference in the stack does not.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/core/Material.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace sigil::material;

namespace {

/** A base part that takes named inputs of its own, as a procedural graph
 *  does: every write answers a new part carrying the value. */
class InputPart final : public detail::Part {
 public:
  std::map<std::string, std::vector<float>, std::less<>> values;
  std::map<std::string, float, std::less<>> bound;
  bool equals(const Part& other) const override {
    const auto* same = dynamic_cast<const InputPart*>(&other);
    return same && same->values == values && same->bound == bound;
  }
  bool isRunning() const override { return false; }
  bool geometryDependent() const override { return false; }
  std::shared_ptr<const Part> withInput(
      std::string_view name, std::span<const float> floats) const override {
    if (name != "Season" && name != "Tint") return nullptr;
    auto next = std::make_shared<InputPart>(*this);
    next->values[std::string(name)].assign(floats.begin(), floats.end());
    return next;
  }
  std::shared_ptr<const Part> withBinding(
      std::string_view name, sigil::motion::Animatable<float> value)
      const override {
    if (name != "Season") return nullptr;
    auto next = std::make_shared<InputPart>(*this);
    next->bound[std::string(name)] = value.value();
    return next;
  }
};

/** What a generated input struct answers: its set fields as pairs. */
struct SeasonInputs {
  struct Value {
    std::string identifier;
    std::vector<float> values;
  };
  float season = 0;
  std::vector<Value> inputs() const { return {{"Season", {season}}}; }
};

const InputPart& inputsOf(const Material& material) {
  return dynamic_cast<const InputPart&>(*material.source());
}

}  // namespace

TEST(MaterialBuilder, AColourIsAMaterial) {
  const Material ink = Color{1, 0, 0, 1};
  ASSERT_NE(nullptr, ink.color());
  EXPECT_EQ((Color{1, 0, 0, 1}), *ink.color());
  EXPECT_FALSE(ink.hasProgram());
  EXPECT_TRUE(ink.layers().empty());
  EXPECT_EQ(nullptr, ink.surface());
}

TEST(MaterialBuilder, LayersStackBottomFirstAndCompareByValue) {
  const auto build = [](float opacity) {
    return from(Color{0.2f, 0.2f, 0.2f, 1})
        .layer(Color{1, 1, 1, 1}, {.blend = BlendMode::Screen, .opacity = opacity})
        .layer(Color{0, 0, 1, 1},
               {.mask = Mask{.source = Color{0, 0, 0, 0.5f}}})
        .surface({.metallic = 1.0f, .roughness = 0.2f});
  };
  const Material steel = build(0.3f);
  ASSERT_EQ(2u, steel.layers().size());
  EXPECT_EQ(BlendMode::Screen, steel.layers()[0].options.blend);
  EXPECT_TRUE(steel.layers()[1].options.mask.has_value());
  ASSERT_NE(nullptr, steel.surface());
  EXPECT_EQ(1.0f, std::get<float>(steel.surface()->metallic));

  EXPECT_EQ(steel, build(0.3f));
  EXPECT_NE(steel, build(0.4f));
  EXPECT_NE(steel, steel.base());
  EXPECT_EQ(Material(Color{0.2f, 0.2f, 0.2f, 1}), steel.base());
}

TEST(MaterialBuilder, TheDesignatedFormIsTheChain) {
  const Material chained = from(Color{0.5f, 0.5f, 0.5f, 1})
                               .layer(Color{1, 1, 1, 1}, {.opacity = 0.5f})
                               .surface({.roughness = 0.8f});
  const Material designated{{.base = Color{0.5f, 0.5f, 0.5f, 1},
                             .layers = {{Color{1, 1, 1, 1}, {.opacity = 0.5f}}},
                             .surface = SurfaceOptions{.roughness = 0.8f}}};
  EXPECT_EQ(chained, designated);
}

TEST(MaterialBuilder, AProgramOnlyOperationOnAColourIsIgnored) {
  Material flat = Color{1, 1, 1, 1};
  flat.set("amount", 1.0f);
  EXPECT_EQ(Material(Color{1, 1, 1, 1}), flat);
  EXPECT_EQ(nullptr, flat.resolve(Target::SkSL, {}).program);
}

TEST(MaterialBuilder, SetAndBindReachABasePartThatTakesInputs) {
  Material graph(std::shared_ptr<const detail::Part>(
      std::make_shared<const InputPart>()));
  const Material before = graph;
  graph.set("Season", 0.8f).set("Tint", Color{1, 0, 0, 1});
  EXPECT_EQ((std::vector<float>{0.8f}), inputsOf(graph).values.at("Season"));
  EXPECT_EQ(4u, inputsOf(graph).values.at("Tint").size());
  // A copy taken before the writes is untouched, and no longer equal.
  EXPECT_TRUE(inputsOf(before).values.empty());
  EXPECT_FALSE(graph == before);

  graph.set(SeasonInputs{.season = 0.25f});
  EXPECT_EQ((std::vector<float>{0.25f}), inputsOf(graph).values.at("Season"));

  graph.bind("Season", 0.5f);
  EXPECT_FLOAT_EQ(0.5f, inputsOf(graph).bound.at("Season"));
  // An input the part does not take is refused as on any non-program base.
  graph.set("Missing", 1.0f);
  EXPECT_FALSE(inputsOf(graph).values.contains("Missing"));
}
