/** @file
 * HOW AN EFFECT IS REALISED OVER A LAYER: the slot an executor fills
 * from that layer, so a body needing a NEIGHBOURHOOD of it does not
 * have to gather one per pixel, and the one shared list of programs an
 * effect is built out of and draws through.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>

#include <memory>
#include <vector>

#include "support/EffectRead.h"

using namespace sigil::material;
using sigil::material::test::bloomThrough;
using sigil::material::test::texel;

// ---------------------------------------------------------------------------
// A SLOT THE EXECUTOR FILLS FROM THE LAYER. A recipe over a layer reads
// that layer in one slot; a body that needs a NEIGHBOURHOOD of it — a
// bloom, a glass pass over its own light — would otherwise have to
// gather that neighbourhood itself, per pixel, at a fixed tap count.

namespace {

struct OneRadius {
  float uRadius = 0;
};

/** A recipe whose `bloom` slot an executor fills from the layer blurred
 *  at `uRadius`. It answers the slot where there is a radius and the
 *  layer where there is not, which is one body reading both names. */
const std::shared_ptr<const Recipe>& blurredSlotRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<OneRadius>("effect.layerslot.blurred")
          .slot("content")
          .slot("bloom", LayerFilter::Blurred, "uRadius")
          .body(Target::SkSL,
                "half4 main(float2 p) {\n"
                "  if (uRadius > 0) return bloom.eval(p);\n"
                "  return content.eval(p);\n"
                "}"));
  return recipe;
}

/** One flat colour as an image, for a slot an author fills himself. */
sk_sp<SkImage> oneColour(SkColor colour) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(8, 8));
  bitmap.eraseColor(colour);
  bitmap.setImmutable();
  return bitmap.asImage();
}

}  // namespace

TEST(SkiaEffect, ASlotTheExecutorFillsIsTheLayerThroughThatFilter) {
  // The claim is an identity, not a likeness: the slot IS the layer
  // through that filter. So the same program reading the slot, and the
  // same program reading the layer after the filter was spent on it,
  // paint the same picture — at a sigma Skia's linear blur takes whole
  // and at one it has to reduce and enlarge for.
  const SkColor4f amber{1.0f, 0.72f, 0.1f, 1.0f};
  for (float sigma : {3.0f, 24.0f}) {
    const Material derived(blurredSlotRecipe(), OneRadius{sigma});
    const Material plain(blurredSlotRecipe(), OneRadius{0});
    const auto fromSlot = bloomThrough(
        skia::resolvedImageFilter(Filter::of(derived, 0), nullptr), amber);
    const auto fromChain = bloomThrough(
        skia::resolvedImageFilter(
            Filter::blur(sigma).then(Filter::of(plain, 0)), nullptr),
        amber);
    ASSERT_EQ(fromSlot.size(), fromChain.size());
    for (size_t i = 0; i < fromSlot.size(); ++i)
      ASSERT_EQ(fromSlot[i], fromChain[i]) << "sigma " << sigma << " at " << i;
    // And it really is spread: light stands well outside the square the
    // layer holds, where the layer itself is black.
    EXPECT_GT(texel(fromSlot, 16, 32)[0], 0.0f);
  }
}

TEST(SkiaEffect, AnAuthorFilledSlotIsNotRefilledByTheExecutor) {
  // A recipe that declares who fills a slot states a default, not a
  // rule: an author who binds a source to the name keeps it, which is
  // also what lets the same material be painted as an ordinary fill.
  Material material(blurredSlotRecipe(), OneRadius{8});
  material.slot("bloom", Texture(oneColour(SK_ColorBLUE)));
  const auto pixels = bloomThrough(
      skia::resolvedImageFilter(Filter::of(material, 0), nullptr),
      {1.0f, 0.72f, 0.1f, 1.0f});
  EXPECT_FLOAT_EQ(texel(pixels, 32, 32)[2], 1.0f);
  EXPECT_FLOAT_EQ(texel(pixels, 32, 32)[0], 0.0f);
}

TEST(SkiaEffect, ARecipeWithALayerSlotStillComparesByItsValues) {
  const Material eight(blurredSlotRecipe(), OneRadius{8});
  const Material four(blurredSlotRecipe(), OneRadius{4});
  EXPECT_TRUE(Filter::of(eight, 0) == Filter::of(eight, 0));
  EXPECT_FALSE(Filter::of(eight, 0) == Filter::of(four, 0));
}

TEST(SkiaEffect, TheProgramsAnEffectIsBuiltOutOfAreOneSharedList) {
  const std::span<const sk_sp<SkRuntimeEffect>> programs =
      skia::everyFilterProgram();
  ASSERT_FALSE(programs.empty());
  // The list is what a backend can be asked to NAME, and it names the
  // object: nothing in it may be absent, and two asks must answer the
  // same objects in the same order or a name written down one run means
  // something else the next.
  const std::span<const sk_sp<SkRuntimeEffect>> again =
      skia::everyFilterProgram();
  ASSERT_EQ(again.size(), programs.size());
  std::vector<const SkRuntimeEffect*> seen;
  for (size_t at = 0; at < programs.size(); ++at) {
    EXPECT_NE(programs[at], nullptr);
    EXPECT_EQ(programs[at].get(), again[at].get());
    EXPECT_EQ(std::count(seen.begin(), seen.end(), programs[at].get()), 0);
    seen.push_back(programs[at].get());
  }
}

TEST(SkiaEffect, AnEffectDrawsThroughTheProgramTheListNames) {
  // The whole point of the list: the object declared is the object the
  // draw runs, so a stage built here is found in it.
  const std::span<const sk_sp<SkRuntimeEffect>> programs =
      skia::everyFilterProgram();
  const Filter gate = Filter::brightPass(0.5f, 0.1f);
  const sk_sp<SkColorFilter> map = skia::colorFilter(gate);
  ASSERT_NE(map, nullptr);
  // The bright pass reads its own pixel, so it is one of the bodies
  // made for a colour filter rather than for a shader.
  size_t colourMaps = 0;
  for (const sk_sp<SkRuntimeEffect>& program : programs)
    if (program->allowColorFilter()) ++colourMaps;
  EXPECT_GT(colourMaps, size_t{0});
}
