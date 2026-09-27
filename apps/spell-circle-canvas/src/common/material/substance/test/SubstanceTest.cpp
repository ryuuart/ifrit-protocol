/** @file
 * A Substance archive as a Material: the SDK's sample archives decoded
 * through the hub and described once, cooked at the size they were
 * given, their outputs tagged with the colour space each declares and
 * cooked at the precision each needs, laid into a material's base and
 * surface, and re-cooked apart from the caller when an input is written
 * or followed through the material. Each case skips, rather than fails,
 * without the SDK or its samples; the one that states what a build
 * without the SDK answers skips with it.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImage.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/substance/Substance.h>
#include <sigilmaterial/substance/advanced/Archive.h>
#include <sigilmaterial/substance/advanced/Cook.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace sigil;
using namespace sigil::material;

namespace {

std::string sample(const char* name) {
  return (std::filesystem::path(SIGIL_SUBSTANCE_SDK_DIR) / "assets" / name)
      .string();
}

/** The samples ship with the SDK; without either there is nothing to
 *  cook and the case says why instead of failing. */
#define SKIP_WITHOUT_SAMPLE(name)                                        \
  do {                                                                   \
    if (!sbsar::available()) GTEST_SKIP() << "no Substance SDK";         \
    if (!std::filesystem::exists(sample(name)))                          \
      GTEST_SKIP() << "Substance SDK sample " << sample(name)            \
                   << " not found";                                      \
  } while (0)

sk_sp<SkImage> frameOf(const media::PixelSource& pixels) {
  return pixels.frameAt({}).image;
}

SkColor pixel(const sk_sp<SkImage>& image, int x, int y) {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(image->width(), image->height()));
  image->readPixels(nullptr, bitmap.pixmap(), 0, 0);
  return bitmap.getColor(x, y);
}

int differing(const sk_sp<SkImage>& first, const sk_sp<SkImage>& second) {
  int count = 0;
  for (int y = 4; y < first->height(); y += first->height() / 8)
    for (int x = 4; x < first->width(); x += first->width() / 8)
      count += pixel(first, x, y) != pixel(second, x, y);
  return count;
}

const sbsar::Input* inputNamed(const sbsar::Description& description,
                               std::string_view name) {
  for (const sbsar::Input& input : description.inputs)
    if (input.name == name) return &input;
  return nullptr;
}

const sbsar::Output* outputFor(const sbsar::Description& description,
                               std::string_view usage) {
  for (const sbsar::Output& output : description.outputs)
    if (output.usage == usage) return &output;
  return nullptr;
}

/** The cooked base colour under either spelling a graph may use. */
media::PixelSource baseOf(const Material& material) {
  media::PixelSource base = sbsar::output(material, "baseColor");
  return base ? base : sbsar::output(material, "diffuse");
}

/** What a generated input struct answers, written by hand. */
struct LeavesInputs {
  float hueShift = 0;
  std::vector<sbsar::InputValue> inputs() const {
    return {{"Hue_Shift", hueShift}};
  }
};

}  // namespace

TEST(Substance, ReportsTheEngineItCooksOn) {
  if (!sbsar::available()) GTEST_SKIP() << "no Substance SDK";
  EXPECT_EQ(sbsar::Engine::Cpu, sbsar::engine());
  EXPECT_FALSE(sbsar::engineVersion().empty());
}

TEST(Substance, WithoutTheSdkEveryEntranceAnswersEmpty) {
  if (sbsar::available()) GTEST_SKIP() << "the SDK is present";
  io::Hub hub;
  const Material empty = material::substance(hub, "leaves.sbsar");
  ASSERT_NE(nullptr, empty.color());
  EXPECT_EQ(0.0f, empty.color()->a);
  EXPECT_TRUE(sbsar::describe(hub, "leaves.sbsar").inputs.empty());
  EXPECT_EQ(sbsar::Engine::None, sbsar::engine());
}

TEST(Substance, FindsEveryGraphByTheUrlAndLabelItReports) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  const std::shared_ptr<const sbsar::Archive> archive =
      sbsar::load(hub, sample("Autumn_Leaves.sbsar"));
  ASSERT_TRUE(archive);
  ASSERT_GE(archive->graphCount(), 1u);
  for (size_t index = 0; index < archive->graphCount(); ++index) {
    EXPECT_EQ(index, archive->find(archive->url(index)));
    if (!archive->graph(index).graph.empty())
      EXPECT_TRUE(archive->find(archive->graph(index).graph));
  }
  EXPECT_EQ(0u, archive->find(""));
  EXPECT_FALSE(archive->find("no such graph"));
  // The hub keeps the decode: a second ask is the same archive.
  EXPECT_EQ(archive, sbsar::load(hub, sample("Autumn_Leaves.sbsar")));
}

TEST(Substance, DescribesEveryInputAndThePresetsBesideTheArchive) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  const sbsar::Description leaves =
      sbsar::describe(hub, sample("Autumn_Leaves.sbsar"));
  const sbsar::Input* size = inputNamed(leaves, "$outputsize");
  ASSERT_TRUE(size) << "every graph exposes its output size";
  EXPECT_EQ(sbsar::InputType::Integer2, size->type);
  EXPECT_EQ(2u, size->components());
  const sbsar::Input* hue = inputNamed(leaves, "Hue_Shift");
  ASSERT_TRUE(hue);
  EXPECT_EQ(sbsar::InputType::Float, hue->type);
  EXPECT_FALSE(hue->label.empty());
  ASSERT_EQ(1u, hue->maximum.size());
  EXPECT_GT(hue->maximum[0], hue->minimum[0]);
  const sbsar::Input* leafType = inputNamed(leaves, "LeafType");
  ASSERT_TRUE(leafType);
  EXPECT_FALSE(leafType->choices.empty());
  // The sample's presets stand in a .sbsprs of the same name beside it.
  EXPECT_NE(leaves.presets.end(),
            std::find(leaves.presets.begin(), leaves.presets.end(), "Green"));
  EXPECT_TRUE(sbsar::describe(hub, sample("Autumn_Leaves.sbsar"), "no such")
                  .inputs.empty());
}

TEST(Substance, TagsEachOutputWithTheColourSpaceItDeclares) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  const std::string uri = sample("Autumn_Leaves.sbsar");
  const sbsar::Description leaves = sbsar::describe(hub, uri);
  const sbsar::Output* base = outputFor(leaves, "baseColor");
  if (!base) base = outputFor(leaves, "diffuse");
  const sbsar::Output* normal = outputFor(leaves, "normal");
  ASSERT_TRUE(base && normal);
  EXPECT_EQ(sbsar::Encoding::Srgb, base->encoding);
  EXPECT_EQ(sbsar::Encoding::Raw, normal->encoding);

  const Material material =
      material::substance(hub, uri, {.resolution = 64});
  const sk_sp<SkImage> colour = frameOf(baseOf(material));
  const sk_sp<SkImage> normals = frameOf(sbsar::output(material, "normal"));
  ASSERT_TRUE(colour && normals);
  ASSERT_TRUE(colour->colorSpace());
  EXPECT_TRUE(colour->colorSpace()->isSRGB());
  // Data carries no colour space, so nothing converts it on its way.
  EXPECT_EQ(nullptr, normals->colorSpace());
  // A normal is cooked at 16 bits unless asked otherwise.
  EXPECT_EQ(kR16G16B16A16_unorm_SkColorType, normals->colorType());
}

TEST(Substance, EveryCookReadsTheOneDescriptionTheArchiveBuilt) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  const std::shared_ptr<const sbsar::Archive> archive =
      sbsar::load(hub, sample("Autumn_Leaves.sbsar"));
  ASSERT_TRUE(archive);
  sbsar::CookScheduler cook(archive, 0, {.resolution = 32});
  const sbsar::Description* built = &archive->graph(0);
  EXPECT_EQ(built, &cook.description());
  ASSERT_TRUE(cook.cookNow());
  const float hue[] = {0.4f};
  ASSERT_TRUE(cook.set("Hue_Shift", hue));
  ASSERT_TRUE(cook.cookNow());
  EXPECT_EQ(built, &cook.description());
  EXPECT_EQ(built, &archive->graph(0));
}

TEST(Substance, CooksEveryOutputAtTheResolutionItWasGiven) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  const std::string uri = sample("Autumn_Leaves.sbsar");
  const Material small = material::substance(hub, uri, {.resolution = 128});
  const sk_sp<SkImage> image = frameOf(sbsar::output(small, "normal"));
  ASSERT_TRUE(image);
  EXPECT_EQ(128, image->width());
  EXPECT_EQ(128, image->height());
  // A size that is not a power of two rounds up to one.
  const Material rounded = material::substance(hub, uri, {.resolution = 50});
  const sk_sp<SkImage> up = frameOf(sbsar::output(rounded, "normal"));
  ASSERT_TRUE(up);
  EXPECT_EQ(64, up->width());
}

TEST(Substance, TheMaterialCarriesTheOutputsAsItsBaseAndSurface) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  const Material leaves = material::substance(
      hub, sample("Autumn_Leaves.sbsar"), {.resolution = 32});
  EXPECT_FALSE(leaves.hasProgram());
  ASSERT_EQ(1u, leaves.layers().size()) << "the base colour over the cook";
  // The sample declares an opacity output, which masks the base colour.
  EXPECT_TRUE(leaves.layers()[0].options.mask);
  const SurfaceOptions* surface = leaves.surface();
  ASSERT_TRUE(surface);
  EXPECT_TRUE(surface->normal);
  // The sample declares no roughness, so the channel keeps its number.
  EXPECT_TRUE(std::holds_alternative<float>(surface->roughness));
  // An output the surface has no channel for is not cooked unless asked.
  EXPECT_FALSE(sbsar::output(leaves, "height"));
  const Material withHeight = material::substance(
      hub, sample("Autumn_Leaves.sbsar"),
      {.resolution = 32, .outputs = {{"diffuse"}, {"height"}}});
  EXPECT_TRUE(sbsar::output(withHeight, "height"));
  EXPECT_FALSE(sbsar::output(withHeight, "normal"));
}

TEST(Substance, SetOnTheMaterialCooksApartAndTheNewestValueLands) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  Material leaves = material::substance(hub, sample("Autumn_Leaves.sbsar"),
                                        {.resolution = 64});
  const media::PixelSource base = baseOf(leaves);
  ASSERT_TRUE(base);
  const sk_sp<SkImage> before = frameOf(base);
  const uint64_t landed = base.revision();
  const Material unchanged = leaves;
  leaves.set("Hue_Shift", 0.3f).set("Hue_Shift", 0.5f);
  EXPECT_FALSE(leaves == unchanged);
  sbsar::settle(leaves);
  EXPECT_GT(base.revision(), landed);
  EXPECT_TRUE(leaves.isRunning()) << "a landed cook not yet handed out";
  const sk_sp<SkImage> after = frameOf(base);
  // Settled once every cooked output has handed out what landed.
  frameOf(sbsar::output(leaves, "normal"));
  frameOf(sbsar::output(leaves, "opacity"));
  EXPECT_FALSE(leaves.isRunning());
  EXPECT_GT(differing(before, after), 3);
}

TEST(Substance, BindFollowsAValueThroughTheMaterial) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  Material leaves = material::substance(hub, sample("Autumn_Leaves.sbsar"),
                                        {.resolution = 64});
  const media::PixelSource base = baseOf(leaves);
  ASSERT_TRUE(base);
  const sk_sp<SkImage> before = frameOf(base);
  leaves.bind("Hue_Shift", motion::Animatable<float>(0.5f));
  sbsar::settle(leaves);
  EXPECT_GT(differing(before, frameOf(base)), 3);
}

TEST(Substance, TheGeneratedStructFormWritesItsFields) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  const std::string uri = sample("Autumn_Leaves.sbsar");
  const Material plain = material::substance(hub, uri, {.resolution = 64});
  const Material shifted =
      material::substance(hub, uri, LeavesInputs{.hueShift = 0.5f},
                          {.resolution = 64});
  EXPECT_GT(differing(frameOf(baseOf(plain)),
                      frameOf(baseOf(shifted))),
            3);
  // The keyed form states the same thing.
  const Material keyed = material::substance(
      hub, uri, {.inputs = {{"Hue_Shift", 0.5f}}, .resolution = 64});
  EXPECT_EQ(0, differing(frameOf(baseOf(keyed)),
                         frameOf(baseOf(shifted))));
}

TEST(Substance, APresetBesideTheArchiveIsAppliedByLabel) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  const std::string uri = sample("Autumn_Leaves.sbsar");
  const Material authored = material::substance(hub, uri, {.resolution = 64});
  const Material green =
      material::substance(hub, uri, {.preset = "Green", .resolution = 64});
  EXPECT_GT(differing(frameOf(baseOf(authored)),
                      frameOf(baseOf(green))),
            3);
  sbsar::CookScheduler cook(sbsar::load(hub, uri), 0);
  EXPECT_FALSE(cook.applyPreset("Green")) << "not embedded in the archive";
}

TEST(Substance, SetsAnInputItHasAndRefusesOneItDoesNot) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  sbsar::CookScheduler cook(sbsar::load(hub, sample("Autumn_Leaves.sbsar")), 0);
  const float hue[] = {0.25f};
  EXPECT_TRUE(cook.set("Hue_Shift", hue));
  EXPECT_EQ(std::vector<float>{0.25f}, cook.get("Hue_Shift"));
  EXPECT_FALSE(cook.set("no_such_input", hue));
  EXPECT_FALSE(cook.set("$outputsize", hue)) << "takes two numbers";
  cook.reset();
  const sbsar::Input* described = inputNamed(cook.description(), "Hue_Shift");
  ASSERT_TRUE(described);
  EXPECT_EQ(described->defaultValue, cook.get("Hue_Shift"));
  EXPECT_TRUE(cook.isHeavyDuty("$randomseed") ||
              !cook.isHeavyDuty("Hue_Shift"));
  // Through the material, an input the graph lacks is refused, not written.
  Material leaves = material::substance(hub, sample("Autumn_Leaves.sbsar"),
                                        {.resolution = 32});
  const Material before = leaves;
  leaves.set("no_such_input", 1.0f);
  EXPECT_TRUE(leaves == before);
}

TEST(Substance, TheNormalFormatInputSelectsTheGreenConventionItReportsBack) {
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  io::Hub hub;
  sbsar::CookScheduler cook(sbsar::load(hub, sample("Autumn_Leaves.sbsar")), 0);
  EXPECT_TRUE(cook.normalsAreDirectX()) << "the engine's default";
  const float openGl[] = {1.0f};
  ASSERT_TRUE(cook.set("$normalformat", openGl));
  EXPECT_FALSE(cook.normalsAreDirectX());
}

TEST(Substance, RefusesWhatIsNotAnArchive) {
  if (!sbsar::available()) GTEST_SKIP() << "no Substance SDK";
  const char junk[] = "this is not an archive";
  EXPECT_FALSE(sbsar::Archive::decode(std::as_bytes(std::span(junk))));
  io::Hub hub;
  const Material missing = material::substance(hub, "/nonexistent/x.sbsar");
  ASSERT_NE(nullptr, missing.color());
  EXPECT_EQ(0.0f, missing.color()->a);
}

TEST(Substance, GraphsComposeThroughImageInputs) {
  // The SDK's second sample is a FILTER: it takes a diffuse and a height
  // image and answers a lit diffuse. Feed it the leaves graph's own
  // outputs, and the result differs from the filter cooked on nothing.
  SKIP_WITHOUT_SAMPLE("Autumn_Leaves.sbsar");
  SKIP_WITHOUT_SAMPLE("Post_Illumination.sbsar");
  io::Hub hub;
  const Material leaves = material::substance(
      hub, sample("Autumn_Leaves.sbsar"),
      {.resolution = 128, .outputs = {{"diffuse"}, {"baseColor"}, {"height"}}});
  media::PixelSource diffuse = sbsar::output(leaves, "diffuse");
  if (!diffuse) diffuse = sbsar::output(leaves, "baseColor");
  const media::PixelSource height = sbsar::output(leaves, "height");
  ASSERT_TRUE(diffuse && height);

  sbsar::CookScheduler post(sbsar::load(hub, sample("Post_Illumination.sbsar")),
                            0, {.resolution = 128});
  std::vector<std::string> imageInputs;
  for (const sbsar::Input& input : post.description().inputs)
    if (input.type == sbsar::InputType::Image) imageInputs.push_back(input.name);
  ASSERT_GE(imageInputs.size(), 2u) << "the filter takes two images";
  ASSERT_TRUE(post.cookNow());
  const media::PixelSource lit = post.output(post.cooked().front());
  const sk_sp<SkImage> empty = frameOf(lit);
  ASSERT_TRUE(empty);
  ASSERT_TRUE(post.setImage(imageInputs[0], diffuse));
  ASSERT_TRUE(post.setImage(imageInputs[1], height));
  ASSERT_TRUE(post.cookNow());
  const sk_sp<SkImage> fed = frameOf(lit);
  ASSERT_TRUE(fed);
  EXPECT_EQ(128, fed->width());
  EXPECT_GT(differing(empty, fed), 3);
  // A numeric input refuses an image; an empty source resets.
  EXPECT_FALSE(post.setImage("$outputsize", diffuse));
  EXPECT_TRUE(post.setImage(imageInputs[0], media::PixelSource{}));
}
