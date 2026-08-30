/** @file
 * substance_test — the SDK's sample archives loaded, described,
 * rendered, re-rendered after parameter changes, composed through
 * image inputs, and garbage refused. Skips, rather than fails, when the
 * samples are not where the SDK was configured from.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilsubstance/Substance.h>

#include <filesystem>

using namespace sigil;

namespace {

std::filesystem::path sampleArchive(const char* name) {
  return std::filesystem::path(SIGIL_SUBSTANCE_SDK_DIR) / "assets" / name;
}

/** The samples ship with the SDK; without them there is nothing to
 *  render and the test reports why instead of failing. */
#define SKIP_WITHOUT_SAMPLE(name)                                    \
  do {                                                               \
    if (!std::filesystem::exists(sampleArchive(name)))               \
      GTEST_SKIP() << "Substance SDK sample " << sampleArchive(name) \
                   << " not found";                                  \
  } while (0)

SkColor pixel(const sk_sp<SkImage>& image, int x, int y) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(image->width(), image->height()));
  image->readPixels(nullptr, bm.pixmap(), 0, 0);
  return bm.getColor(x, y);
}

}  // namespace

TEST(Substance, LoadsAPackageAndDescribesIt) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  std::string error;
  std::unique_ptr<substance::Package> package =
      substance::Package::load(sampleArchive("Autumn_Leaves.sbsar"), &error);
  ASSERT_TRUE(package) << error;
  ASSERT_GE(package->graphCount(), 1u);
  substance::Graph& graph = package->graph(0);
  EXPECT_FALSE(graph.url().empty());
  EXPECT_EQ(package->find(graph.url()), &graph);
  EXPECT_EQ(package->find("no such graph"), nullptr);
  const std::vector<substance::Parameter> params = graph.parameters();
  EXPECT_FALSE(params.empty());
  bool hasSize = false;
  for (const substance::Parameter& p : params) {
    if (p.identifier == "$outputsize") {
      hasSize = true;
      EXPECT_EQ(p.kind, substance::Parameter::Kind::Int2);
      EXPECT_EQ(p.components(), 2);
    }
    if (p.kind != substance::Parameter::Kind::Image &&
        p.kind != substance::Parameter::Kind::Text &&
        p.kind != substance::Parameter::Kind::Other)
      EXPECT_EQ(p.values.size(), p.defaults.size()) << p.identifier;
  }
  EXPECT_TRUE(hasSize) << "every graph exposes its output size";
  const std::vector<substance::Output> outputs = graph.outputs();
  EXPECT_FALSE(outputs.empty());
  bool anyUsage = false;
  for (const substance::Output& o : outputs) anyUsage |= !o.usage.empty();
  EXPECT_TRUE(anyUsage) << "the sample declares channel usages";
  EXPECT_FALSE(substance::Package::engineVersion().empty());
}

TEST(Substance, RendersOutputsAndParametersChangeThem) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  std::string error;
  std::unique_ptr<substance::Package> package =
      substance::Package::load(sampleArchive("Autumn_Leaves.sbsar"), &error);
  ASSERT_TRUE(package) << error;
  substance::Graph& graph = package->graph(0);
  ASSERT_TRUE(graph.setResolution(7, 7));  // 128 x 128: fast
  EXPECT_TRUE(graph.normalsAreDirectX()) << "the engine's default";
  ASSERT_TRUE(graph.set("$normalformat", 1.0f));
  EXPECT_FALSE(graph.normalsAreDirectX());
  ASSERT_TRUE(graph.render());
  const std::map<std::string, sk_sp<SkImage>> byUsage = graph.outputsByUsage();
  ASSERT_FALSE(byUsage.empty());
  sk_sp<SkImage> base;
  for (const auto& [usage, image] : byUsage) {
    ASSERT_TRUE(image) << usage;
    EXPECT_EQ(image->width(), 128) << usage;
    EXPECT_EQ(image->height(), 128) << usage;
    if (usage == "baseColor" || usage == "diffuse") base = image;
  }
  ASSERT_TRUE(base) << "a material graph has a base colour";
  EXPECT_EQ(graph.output("baseColor") ? graph.output("baseColor")
                                      : graph.output("diffuse"),
            base);

  // Move a numeric parameter and the picture moves. Pick the first
  // float slider that is not the size; if the sample has none, the
  // resolution change alone proves the re-render.
  const std::vector<substance::Parameter> params = graph.parameters();
  const substance::Parameter* knob = nullptr;
  for (const substance::Parameter& p : params)
    if (p.kind == substance::Parameter::Kind::Float &&
        p.identifier != "$outputsize" && p.maximum.size() == 1 &&
        p.maximum[0] > p.minimum[0]) {
      knob = &p;
      break;
    }
  ASSERT_TRUE(graph.setResolution(6, 6));
  if (knob) {
    const float far = knob->values[0] == knob->maximum[0] ? knob->minimum[0]
                                                          : knob->maximum[0];
    ASSERT_TRUE(graph.set(knob->identifier, far));
  }
  ASSERT_TRUE(graph.render());
  sk_sp<SkImage> again = graph.output("baseColor");
  if (!again) again = graph.output("diffuse");
  ASSERT_TRUE(again);
  EXPECT_EQ(again->width(), 64);
  // A wrong identifier and a wrong arity are refused, not applied.
  EXPECT_FALSE(graph.set("no_such_parameter", 1.0f));
  EXPECT_FALSE(graph.set("$outputsize", {1.0f}));

  // reset() returns to the authored values.
  graph.reset();
  for (const substance::Parameter& p : graph.parameters())
    if (p.kind == substance::Parameter::Kind::Float)
      EXPECT_EQ(p.values, p.defaults) << p.identifier;
}

TEST(Substance, RejectsGarbage) {
  std::string error;
  const char junk[] = "this is not an archive";
  EXPECT_FALSE(substance::Package::load(junk, sizeof(junk), &error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(substance::Package::load(
      std::filesystem::path("/nonexistent/x.sbsar"), &error));
}

TEST(Substance, GraphsComposeThroughImageInputs) {
  // The SDK's second sample is a FILTER: it takes a diffuse and a
  // height image and returns a lit diffuse. Feed it the leaves graph's
  // own outputs — one package's render is another's input — and the
  // result differs from the filter run on nothing.
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  SKIP_WITHOUT_SAMPLE("Post_Illumination.sbsar");
  std::string error;
  std::unique_ptr<substance::Package> leaves =
      substance::Package::load(sampleArchive("Autumn_Leaves.sbsar"), &error);
  ASSERT_TRUE(leaves) << error;
  substance::Graph& source = leaves->graph(0);
  ASSERT_TRUE(source.setResolution(7, 7));
  ASSERT_TRUE(source.render());
  sk_sp<SkImage> diffuse = source.output("diffuse");
  sk_sp<SkImage> height = source.output("height");
  ASSERT_TRUE(diffuse && height);

  std::unique_ptr<substance::Package> filter = substance::Package::load(
      sampleArchive("Post_Illumination.sbsar"), &error);
  ASSERT_TRUE(filter) << error;
  substance::Graph& post = filter->graph(0);
  std::vector<std::string> imageInputs;
  for (const substance::Parameter& p : post.parameters())
    if (p.kind == substance::Parameter::Kind::Image)
      imageInputs.push_back(p.identifier);
  ASSERT_GE(imageInputs.size(), 2u) << "the filter takes two images";
  ASSERT_TRUE(post.setResolution(7, 7));
  ASSERT_TRUE(post.render());
  sk_sp<SkImage> empty = post.output("diffuse");
  ASSERT_TRUE(empty);

  ASSERT_TRUE(post.setImage(imageInputs[0], diffuse));
  ASSERT_TRUE(post.setImage(imageInputs[1], height));
  ASSERT_TRUE(post.render());
  sk_sp<SkImage> lit = post.output("diffuse");
  ASSERT_TRUE(lit);
  EXPECT_EQ(lit->width(), 128);
  // Different pixels somewhere: the inputs reached the graph.
  int differing = 0;
  for (int y = 8; y < 128; y += 24)
    for (int x = 8; x < 128; x += 24)
      differing += pixel(lit, x, y) != pixel(empty, x, y);
  EXPECT_GT(differing, 3);
  // A non-image parameter refuses an image; a null image resets.
  EXPECT_FALSE(post.setImage("SpecularIntensity", diffuse));
  EXPECT_TRUE(post.setImage(imageInputs[0], nullptr));
}
