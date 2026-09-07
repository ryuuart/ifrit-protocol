/** @file
 * The material: what a write to a field no body reads says, the bytes it
 * mirrors, equality by value with bindings by identity, the tiers its
 * bindings and children put it in, what resolve injects, and the
 * uniform block's revision.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/Material.h>

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace sigil::material;

namespace {

struct TwoParams {
  float uScale;
  Color uColor;
};

std::shared_ptr<const Recipe> twoRecipe(const char* name = "two") {
  return std::make_shared<const Recipe>(Recipe::of<TwoParams>(name).body(
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
  // declarations are generated from the params whether the body reads
  // them or not. The RECIPE can say, and it is asked at the write.
  auto r = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("half-read")
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
      Recipe::of<TwoParams>("substring")
          .body(Target::Slang, "float4 main() { return uScaleFactor; }"));
  Material s(sub);
  EXPECT_NE(captureStderr([&] { s.set("uScale", 1.0f); }).find("uScale"),
            std::string::npos);
  // A recipe with no body at all has nothing to say.
  auto none = std::make_shared<const Recipe>(Recipe::of<TwoParams>("bodiless"));
  Material n(none);
  EXPECT_EQ(captureStderr([&] { n.set("uScale", 1.0f); }), "");
}

TEST(Material, MirrorsParamsAsBytesAndSetsFields) {
  auto r = twoRecipe();
  Material m(r, TwoParams{2.0f, {0.1f, 0.2f, 0.3f, 1.0f}});
  EXPECT_EQ(m.bytes().size(), sizeof(TwoParams));
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
  zero.set(TwoParams{3.0f, {1, 1, 0, 1}});
  EXPECT_TRUE(zero == m);
}

TEST(Material, EqualityIsByValueWithBindingsByIdentity) {
  auto r = twoRecipe();
  const TwoParams p{1.0f, {0, 0, 0, 1}};
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
  choreograph::Output<float> out{0.5f};
  a.bind("uScale", &out);
  EXPECT_FALSE(a == b);
  b.bind("uScale", &out);
  EXPECT_TRUE(a == b);
  choreograph::Output<float> other{0.5f};
  b.bind("uScale", &other);
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
  // child(name, shared_ptr<const Leaf>{}) is a public door — a slot
  // declared and deliberately left empty — so the two sides of a
  // comparison can disagree about whether a slot holds a leaf at all.
  // Whichever side is empty, the answer is "not equal", read from the
  // pointers rather than from what they point at.
  auto r = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("leafslot")
          .child("uSrc")
          .body(
              Target::SkSL,
              "half4 main(float2 p) { return uSrc.eval(p) * half4(uColor); }"));
  const TwoParams p{1.0f, {1, 1, 1, 1}};
  Material empty(r, p);
  empty.child("uSrc", std::shared_ptr<const Leaf>{});
  Material alsoEmpty(r, p);
  alsoEmpty.child("uSrc", std::shared_ptr<const Leaf>{});
  EXPECT_TRUE(empty == alsoEmpty);

  Material filled(r, p);
  filled.child("uSrc", std::make_shared<const NumberLeaf>(1.0f));
  EXPECT_FALSE(empty == filled);
  EXPECT_FALSE(filled == empty);
}

TEST(Material, TiersFollowBindingsFrameInputsAndChildren) {
  auto plain = twoRecipe();
  Material still(plain, TwoParams{});
  EXPECT_FALSE(still.isAnimated());
  EXPECT_FALSE(still.geometryDependent());

  choreograph::Output<float> out{0.0f};
  Material bound = still;
  bound.bind("uScale", &out);
  EXPECT_TRUE(bound.isAnimated());
  EXPECT_FALSE(bound.geometryDependent());

  auto timed = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("timed").frame(FrameInput::Time));
  EXPECT_TRUE(Material(timed).isAnimated());
  auto sized = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("sized").frame(FrameInput::Resolution));
  EXPECT_FALSE(Material(sized).isAnimated());
  EXPECT_TRUE(Material(sized).geometryDependent());

  auto parentRecipe = std::make_shared<const Recipe>(
      Recipe::of<TwoParams>("parent").child("uA").child("uB"));
  Material parent(parentRecipe);
  EXPECT_FALSE(parent.isAnimated());
  parent.child("uB", Material(sized));
  EXPECT_TRUE(parent.geometryDependent());
  EXPECT_FALSE(parent.isAnimated());
  parent.child("uA", bound);
  EXPECT_TRUE(parent.isAnimated());
  ASSERT_EQ(parent.children().size(), 2u);
  // Slots sit in recipe order however they were filled.
  EXPECT_EQ(parent.children()[0].first, "uA");
  EXPECT_NE(parent.child("uA"), nullptr);
  EXPECT_EQ(parent.child("uZ"), nullptr);
  parent.child("uZ", still);  // undeclared: ignored
  EXPECT_EQ(parent.children().size(), 2u);

  Material same(parentRecipe);
  same.child("uA", bound).child("uB", Material(sized));
  EXPECT_TRUE(parent == same);
  same.child("uB", Material(sized).amount(0.2f));
  EXPECT_FALSE(parent == same);
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
  choreograph::Output<float> out{5.0f};
  auto block = std::make_shared<UniformBlock>(4);
  m.bind("uScale", &out).bind("uTable", block);
  m.bind("uTable", std::make_shared<UniformBlock>(3));  // wrong size: ignored
  EXPECT_TRUE(m.isAnimated());

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
  EXPECT_EQ(at(a, "uTable"), 0.0f);  // the block's zeros, not the params
  EXPECT_EQ(at(a, "uTime"), 1.25f);
  EXPECT_EQ(at(a, "uResolution"), 64.0f);

  // Nothing changed: the same bytes come back, from the memo.
  Material::Resolved b = m.resolve(Target::Slang, frame);
  EXPECT_EQ(a.bytes.data(), b.bytes.data());
  EXPECT_EQ(a.program, b.program);

  // The block wrote without committing — a resolve still reads the current
  // values, because the block is live.
  block->values()[0] = 7.0f;
  Material::Resolved c = m.resolve(Target::Slang, frame);
  EXPECT_EQ(at(c, "uTable"), 7.0f);
  out = 6.0f;
  EXPECT_EQ(at(m.resolve(Target::Slang, frame), "uScale"), 6.0f);

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
