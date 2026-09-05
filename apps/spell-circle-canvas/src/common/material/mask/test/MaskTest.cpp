/** @file
 * The mask as an operand: both shapes compile and shade, the fit and the
 * flip land on the uniforms that carry them, and a material that is not
 * a mask comes back unreshaped.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/mask/Mask.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilshaders/MaterialMask.h>

#include <memory>

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
  for (const Material& m :
       {maskMap(map), maskVertexColor(map, 1), maskSlope(map, {0, 1, 0}),
        maskHeight(map, 0, 1)})
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

TEST(MaskShaderTable, HoldsEveryFileTheShaderDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      mask::shaderSources(), SIGIL_MATERIAL_MASK_SHADER_DIR);
}
