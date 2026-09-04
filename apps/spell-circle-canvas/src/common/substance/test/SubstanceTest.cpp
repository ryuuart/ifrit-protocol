/** @file
 * substance_test — the SDK's sample archives loaded, described, rendered
 * at the resolution they were set to, re-rendered after parameter
 * changes, composed through image inputs, and malformed bytes refused.
 * Skips, rather than fails, when the samples are not where the SDK was
 * configured from; the whole target is absent when there is no SDK.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilsubstance/Substance.h>

#include <boost/container/map.hpp>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

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

/** A raster image of one colour, for feeding an image input. */
sk_sp<SkImage> solid(int width, int height, SkColor color) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  bm.eraseColor(color);
  bm.setImmutable();
  return bm.asImage();
}

/** The material sample, loaded, or nullptr with the failure reported. */
std::unique_ptr<substance::Package> leaves() {
  std::string error;
  std::unique_ptr<substance::Package> package =
      substance::Package::load(sampleArchive("Autumn_Leaves.sbsar"), &error);
  EXPECT_TRUE(package) << error;
  return package;
}

/** The graph's base colour under either spelling a package may use. */
sk_sp<SkImage> baseColorOf(const substance::Graph& graph) {
  sk_sp<SkImage> image = graph.output("baseColor");
  return image ? image : graph.output("diffuse");
}

}  // namespace

TEST(Substance, ReportsTheEngineItRendersOn) {
  EXPECT_FALSE(substance::Package::engineVersion().empty());
}

TEST(Substance, FindsEveryGraphByTheUrlAndLabelItReports) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  std::unique_ptr<substance::Package> package = leaves();
  ASSERT_TRUE(package);
  ASSERT_GE(package->graphCount(), 1u);
  for (size_t i = 0; i < package->graphCount(); ++i) {
    substance::Graph& graph = package->graph(i);
    EXPECT_FALSE(graph.url().empty());
    EXPECT_EQ(package->find(graph.url()), &graph);
    // A label need not be authored, and several graphs may share one, so
    // what is promised is that a label leads to a graph.
    if (!graph.label().empty())
      EXPECT_NE(package->find(graph.label()), nullptr) << graph.label();
  }
  EXPECT_EQ(package->find("no such graph"), nullptr);
}

TEST(Substance, DescribesEveryParameterWithItsKindAndArity) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  std::unique_ptr<substance::Package> package = leaves();
  ASSERT_TRUE(package);
  const std::vector<substance::Parameter> params =
      package->graph(0).parameters();
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
}

TEST(Substance, TagsItsOutputsWithTheChannelsTheyFeed) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  std::unique_ptr<substance::Package> package = leaves();
  ASSERT_TRUE(package);
  const std::vector<substance::Output> outputs = package->graph(0).outputs();
  EXPECT_FALSE(outputs.empty());
  bool anyUsage = false;
  for (const substance::Output& o : outputs) anyUsage |= !o.usage.empty();
  EXPECT_TRUE(anyUsage) << "the sample declares channel usages";
}

TEST(Substance, AnswersNoImageBeforeTheFirstRender) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  std::unique_ptr<substance::Package> package = leaves();
  ASSERT_TRUE(package);
  substance::Graph& graph = package->graph(0);
  ASSERT_FALSE(graph.outputs().empty());
  EXPECT_TRUE(graph.outputsByUsage().empty());
  for (const substance::Output& o : graph.outputs())
    EXPECT_EQ(graph.output(o.identifier), nullptr) << o.identifier;
}

TEST(Substance, RendersEveryOutputAtTheResolutionItWasSet) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  std::unique_ptr<substance::Package> package = leaves();
  ASSERT_TRUE(package);
  substance::Graph& graph = package->graph(0);
  ASSERT_TRUE(graph.setResolution(7, 7));  // 128 x 128: fast
  ASSERT_TRUE(graph.render());
  const boost::container::map<std::string, sk_sp<SkImage>> byUsage =
      graph.outputsByUsage();
  ASSERT_FALSE(byUsage.empty());
  sk_sp<SkImage> base;
  for (const auto& [usage, image] : byUsage) {
    ASSERT_TRUE(image) << usage;
    EXPECT_EQ(image->width(), 128) << usage;
    EXPECT_EQ(image->height(), 128) << usage;
    if (usage == "baseColor" || usage == "diffuse") base = image;
  }
  // The by-usage map and output() answer with the same image.
  ASSERT_TRUE(base) << "a material graph has a base colour";
  EXPECT_EQ(baseColorOf(graph), base);

  // A new resolution cooks again, at the size it was given.
  ASSERT_TRUE(graph.setResolution(6, 6));
  ASSERT_TRUE(graph.render());
  const sk_sp<SkImage> again = baseColorOf(graph);
  ASSERT_TRUE(again);
  EXPECT_EQ(again->width(), 64);
}

TEST(Substance, TheNormalFormatInputSelectsTheGreenConventionItReportsBack) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  std::unique_ptr<substance::Package> package = leaves();
  ASSERT_TRUE(package);
  substance::Graph& graph = package->graph(0);
  EXPECT_TRUE(graph.normalsAreDirectX()) << "the engine's default";
  ASSERT_TRUE(graph.set("$normalformat", 1.0f));
  EXPECT_FALSE(graph.normalsAreDirectX());
}

TEST(Substance, SetsAParameterItHasAndRefusesOneItDoesNot) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  std::unique_ptr<substance::Package> package = leaves();
  ASSERT_TRUE(package);
  substance::Graph& graph = package->graph(0);

  // The first float slider that is not the size, moved to its far end.
  const std::vector<substance::Parameter> params = graph.parameters();
  const substance::Parameter* knob = nullptr;
  for (const substance::Parameter& p : params)
    if (p.kind == substance::Parameter::Kind::Float &&
        p.identifier != "$outputsize" && p.maximum.size() == 1 &&
        p.maximum[0] > p.minimum[0]) {
      knob = &p;
      break;
    }
  if (knob) {
    const float far = knob->values[0] == knob->maximum[0] ? knob->minimum[0]
                                                          : knob->maximum[0];
    EXPECT_TRUE(graph.set(knob->identifier, far)) << knob->identifier;
  }

  // A wrong identifier and a wrong arity are refused, not applied.
  EXPECT_FALSE(graph.set("no_such_parameter", 1.0f));
  EXPECT_FALSE(graph.set("$outputsize", {1.0f}));
}

TEST(Substance, ResetReturnsEveryParameterToItsAuthoredValue) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  std::unique_ptr<substance::Package> package = leaves();
  ASSERT_TRUE(package);
  substance::Graph& graph = package->graph(0);
  ASSERT_TRUE(graph.setResolution(6, 6));
  graph.reset();
  for (const substance::Parameter& p : graph.parameters())
    if (p.kind == substance::Parameter::Kind::Float)
      EXPECT_EQ(p.values, p.defaults) << p.identifier;
}

TEST(Substance, RefusesBytesThatAreNotAnArchive) {
  std::string error;
  const char junk[] = "this is not an archive";
  EXPECT_FALSE(substance::Package::load(junk, sizeof(junk), &error));
  EXPECT_FALSE(error.empty());
}

TEST(Substance, RefusesAFileThatIsNotThere) {
  std::string error;
  EXPECT_FALSE(substance::Package::load(
      std::filesystem::path("/nonexistent/x.sbsar"), &error));
  EXPECT_FALSE(error.empty());
}

TEST(Substance, TakesAnImageInputWhoseSizeIsNotTheGraphsOwn) {
  // An image input carries its own dimensions in; nothing about it has to
  // match the resolution the graph renders at.
  SKIP_WITHOUT_SAMPLE("Post_Illumination.sbsar");
  std::string error;
  std::unique_ptr<substance::Package> filter = substance::Package::load(
      sampleArchive("Post_Illumination.sbsar"), &error);
  ASSERT_TRUE(filter) << error;
  substance::Graph& post = filter->graph(0);
  std::string imageInput;
  for (const substance::Parameter& p : post.parameters())
    if (p.kind == substance::Parameter::Kind::Image) {
      imageInput = p.identifier;
      break;
    }
  ASSERT_FALSE(imageInput.empty()) << "the filter takes an image";
  ASSERT_TRUE(post.setResolution(7, 7));  // 128 x 128
  EXPECT_TRUE(post.setImage(imageInput, solid(16, 16, SK_ColorGREEN)));
  EXPECT_TRUE(post.render());
}

TEST(Substance, GraphsComposeThroughImageInputs) {
  // The SDK's second sample is a FILTER: it takes a diffuse and a
  // height image and returns a lit diffuse. Feed it the leaves graph's
  // own outputs — one package's render is another's input — and the
  // result differs from the filter run on nothing.
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  SKIP_WITHOUT_SAMPLE("Post_Illumination.sbsar");
  std::unique_ptr<substance::Package> source = leaves();
  ASSERT_TRUE(source);
  substance::Graph& leafGraph = source->graph(0);
  ASSERT_TRUE(leafGraph.setResolution(7, 7));
  ASSERT_TRUE(leafGraph.render());
  sk_sp<SkImage> diffuse = leafGraph.output("diffuse");
  sk_sp<SkImage> height = leafGraph.output("height");
  ASSERT_TRUE(diffuse && height);

  std::string error;
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
