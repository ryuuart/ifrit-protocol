/** @file
 * The mask as an operand: both shapes compile and shade, the fit and the
 * flip land on the uniforms that carry them, and a material that is not
 * a mask comes back unreshaped.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/mask/Mask.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilshaders/MaterialMask.h>

#include <memory>
#include <string>

#include "ShaderTable.h"

using namespace sigil::material;

namespace {

/** A material that paints rather than covers: no range to move, no
 *  answer to flip. */
struct PaintParams {
  Color uColor;
};

Material paint() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(Recipe::of<PaintParams>("paint").body(
          Target::SkSL, "half4 main(float2 p) { return half4(uColor); }"));
  return Material(recipe, PaintParams{{1, 0, 0, 1}});
}

/** A white 2x2, for a sampled mask to read. */
Texture whiteMap() {
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
  s->getCanvas()->clear(SK_ColorWHITE);
  return Texture::of(s->makeImageSnapshot());
}

}  // namespace

TEST(Mask, ShapesWhatItReads) {
  skia::install();
  const Material half = maskConstant(0.5f);
  EXPECT_TRUE(skia::shader(half, {}));
  EXPECT_FLOAT_EQ(invertMask(half).get<float>("inverted"), 1.0f);
  EXPECT_FLOAT_EQ(invertMask(invertMask(half)).get<float>("inverted"), 0.0f);
  const Material fitted = fitMask(half, 0.25f, 0.75f);
  EXPECT_FLOAT_EQ(fitted.get<float>("low"), 0.25f);
  EXPECT_FLOAT_EQ(fitted.get<float>("high"), 0.75f);

  const Texture map = whiteMap();
  for (const Material& m : {maskMap(map), maskMap(map, 1),
                            maskSlope(map, {0, 1, 0}), maskHeight(map, 0, 1)})
    EXPECT_TRUE(skia::shader(m, {}));
}

TEST(Mask, ReshapingSomethingThatIsNotAMaskChangesNothing) {
  // A material with no range to move and no answer to flip cannot be
  // reshaped, and a stack whose coverage silently stayed as it was looks
  // exactly like a stack whose fit was wrong — so both hand the material
  // straight back, with a report on stderr naming the rule.
  const Material painted = paint();
  EXPECT_EQ(fitMask(painted, 0.25f, 0.75f), painted);
  EXPECT_EQ(invertMask(painted), painted);
}

TEST(Mask, ItPaintsItsCoverageAsGreyAtFullAlpha) {
  // A mask answers a scalar its caller reads out of the red channel, and
  // both bodies spell that answer the same way: the coverage in all three
  // colour channels, alpha 1. Put the coverage in alpha instead and a
  // mask drawn on its own is invisible rather than grey, and the two
  // languages disagree about what one recipe means.
  skia::install();
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  skia::fill(*surface->getCanvas(), SkPath::Rect(SkRect::MakeWH(4, 4)),
             maskConstant(0.5f));

  SkBitmap read;
  read.allocPixels(SkImageInfo::MakeN32Premul(4, 4));
  ASSERT_TRUE(surface->readPixels(read, 0, 0));
  const SkColor got = read.getColor(2, 2);
  EXPECT_EQ(SkColorGetA(got), 255u);
  EXPECT_NEAR(SkColorGetR(got), 128, 2);
  EXPECT_EQ(SkColorGetG(got), SkColorGetR(got));
  EXPECT_EQ(SkColorGetB(got), SkColorGetR(got));

  // The Slang twin is compiled by a device this binary does not have, so
  // what is checked here is that it spells the same answer: the shaped
  // scalar three times over at full alpha, in both bodies of both
  // recipes.
  for (const char* body : {"MaskConstant", "MaskSampled"}) {
    EXPECT_NE(std::string(mask::shaderSource(std::string(body) + ".sksl"))
                  .find("half4(v, v, v, 1.0)"),
              std::string::npos)
        << body;
    EXPECT_NE(std::string(mask::shaderSource(std::string(body) + ".slang"))
                  .find("float4(v, v, v, 1.0)"),
              std::string::npos)
        << body;
  }
}

TEST(MaskShaderTable, HoldsEveryFileTheShaderDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      mask::shaderSources(), SIGIL_MATERIAL_MASK_SHADER_DIR);
}
