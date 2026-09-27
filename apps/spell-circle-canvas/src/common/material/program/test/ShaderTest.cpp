/** @file
 * A shader as a material: it paints what its body returns from the
 * parameter struct's values; one source is one definition, so a
 * re-described material prunes and a differing source does not; a bound
 * field makes the shader's pass live while a layer over it stays still
 * and nothing compiles again; a texture is sampled by the name its body
 * reads; a file is read through a hub; and the tier-one headers reach no
 * header of the program model.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <sigilio/advanced/Places.h>
#include <sigilio/hub/Hub.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmotion/values/Animatable.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "ScratchDir.h"

using namespace sigil::material;
using sigil::test::ScratchDir;

namespace {

struct Tint {
  float amount = 0.5f;
};

struct Flat {
  Color ink = {1, 0, 0, 1};
};

constexpr std::string_view kFlat = R"(
half4 main(float2 p) { return half4(ink.rgb * ink.a, ink.a); }
)";

/** @p shader drawn over a box of @p size pixels, read back. */
SkBitmap drawn(const sk_sp<SkShader>& shader, int size = 4) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size, true);
  SkCanvas canvas(bitmap);
  SkPaint fill;
  fill.setShader(shader);
  canvas.drawRect(SkRect::MakeWH((float)size, (float)size), fill);
  return bitmap;
}

FrameData frameOf(int size) {
  FrameData frame;
  frame.resolution = {(float)size, (float)size};
  return frame;
}

}  // namespace

TEST(MaterialShader, PaintsWhatItsBodyReturnsFromTheParameters) {
  const Material blue = shader(kFlat, Flat{{0, 0, 1, 1}});
  ASSERT_TRUE(blue.hasProgram());
  const SkBitmap bitmap = drawn(skia::shader(blue, frameOf(4)));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(2, 2));
}

TEST(MaterialShader, OneSourceIsOneDefinition) {
  const Material first = shader(kFlat, Flat{});
  EXPECT_EQ(first, shader(kFlat, Flat{}))
      << "the same source describes the same shader, so the two prune";
  EXPECT_EQ(&first.recipe(), &shader(kFlat, Flat{}).recipe());
  EXPECT_NE(first, shader(kFlat, Flat{{0, 1, 0, 1}}))
      << "other values are another instance";
  const std::string other = std::string(kFlat) + "\n// another text\n";
  EXPECT_NE(first, shader(other, Flat{}))
      << "a differing source is another definition";
  // With no key the name is derived from the text, the same every run.
  EXPECT_EQ(first.recipe().name(), shader(kFlat, Flat{}).recipe().name());
  EXPECT_NE(first.recipe().name(), shader(other, Flat{}).recipe().name());
  EXPECT_EQ("flat", shader(kFlat, Flat{}, {.key = "flat"}).recipe().name());
}

TEST(MaterialShader, ReadsTheFrameInputsItsBodySpells) {
  const Material timed = shader(
      "half4 main(float2 p) { return half4(amount, uTime, 0, 1); }", Tint{});
  EXPECT_TRUE(timed.recipe().reads(FrameInput::Time));
  EXPECT_FALSE(timed.recipe().reads(FrameInput::Resolution));
  EXPECT_TRUE(timed.isRunning());
  EXPECT_FALSE(shader(kFlat, Flat{}).isRunning());
}

TEST(MaterialShader, ABoundFieldMakesItsPassLiveAndOnlyItsPass) {
  struct Level {
    float level = 0;
  };
  sigil::motion::Animatable<float> dial = sigil::motion::animatable(0.0f);
  Material base = shader(
      "half4 main(float2 p) { return half4(level, 0, 0, 1); }", Level{});
  base.bind("level", dial);
  const Material still = shader(kFlat, Flat{{0, 0, 1, 0.5f}});
  const Material dressed =
      from(base).layer(still, {.blend = BlendMode::Screen, .opacity = 0.5f});

  EXPECT_TRUE(skia::paint(dressed).isRunning());
  EXPECT_FALSE(skia::paint(still).isRunning())
      << "the layer over the bound shader does not move";

  dial = 0.0f;
  const SkBitmap dark = drawn(skia::shader(skia::paint(dressed), frameOf(4)));
  const size_t programs = ProgramCache::shared().size();
  dial = 1.0f;
  const SkBitmap bright = drawn(skia::shader(skia::paint(dressed), frameOf(4)));
  EXPECT_GT(SkColorGetR(bright.getColor(2, 2)),
            SkColorGetR(dark.getColor(2, 2)) + 200);
  EXPECT_EQ(programs, ProgramCache::shared().size())
      << "a moved field uploads new bytes and compiles nothing";
}

TEST(MaterialShader, SamplesATextureByTheNameItsBodyReads) {
  SkBitmap red;
  red.allocN32Pixels(2, 2, true);
  red.eraseColor(SK_ColorRED);
  red.setImmutable();
  const Material sampled =
      shader("half4 main(float2 p) { return picture.eval(p); }",
             {.sampling = Sampling::Nearest,
              .textures = {{"picture", red.asImage()}}});
  ASSERT_NE(nullptr, sampled.leaf("picture"));
  EXPECT_EQ(SK_ColorRED,
            drawn(skia::shader(sampled, frameOf(2)), 2).getColor(1, 1));
}

TEST(MaterialShader, AFileIsReadThroughAHub) {
  const ScratchDir scratch("material_shader");
  scratch.write("tint.sksl",
                "half4 main(float2 xy) { return half4(amount, 0, uTime, 1); }\n");
  sigil::io::Hub hub;
  sigil::io::mount(hub, "res://", scratch.path);

  const Material first = shader(hub, "res://tint.sksl", Tint{0.25f});
  ASSERT_TRUE(first.hasProgram());
  EXPECT_EQ(0.25f, first.get<float>("amount"));
  EXPECT_TRUE(first.recipe().reads(FrameInput::Time));
  EXPECT_EQ(first, shader(hub, "res://tint.sksl", Tint{0.25f}));

  EXPECT_FALSE(shader(hub, "res://absent.sksl", Tint{}).hasProgram());
}

namespace {

/** Every header of this library @p header reaches through its own
 *  includes, spelled as it is included. */
std::set<std::string> reached(const std::string& header) {
  const std::filesystem::path root = SIGIL_MATERIAL_INCLUDE_DIR;
  const std::regex include(R"(#include\s*<(sigilmaterial/[^>]+)>)");
  std::set<std::string> seen;
  std::vector<std::string> pending{header};
  while (!pending.empty()) {
    const std::string spelled = pending.back();
    pending.pop_back();
    if (!seen.insert(spelled).second) continue;
    std::ifstream file(root / spelled);
    EXPECT_TRUE(file.good()) << spelled << " is not under " << root;
    std::string line;
    std::smatch match;
    while (std::getline(file, line))
      if (std::regex_search(line, match, include))
        pending.push_back(match[1].str());
  }
  return seen;
}

}  // namespace

TEST(MaterialTier, TheTierOneHeadersReachNoHeaderOfTheProgramModel) {
  for (const std::string header :
       {"sigilmaterial/core/Material.h", "sigilmaterial/program/Shader.h"}) {
    const std::set<std::string> headers = reached(header);
    EXPECT_GT(headers.size(), 2u) << header << " was not read";
    for (const std::string& spelled : headers)
      EXPECT_FALSE(spelled.starts_with("sigilmaterial/advanced/"))
          << header << " reaches " << spelled;
  }
}
