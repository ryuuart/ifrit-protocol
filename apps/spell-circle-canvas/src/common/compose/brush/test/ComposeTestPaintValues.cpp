// SigilMaterial values as a node's paint: fields, tiles, SDF surfaces and
// the layer styles this tier spells over them, plus the view transform a
// composer carries.

#include <include/core/SkBitmap.h>
#include <sigilmaterial/texture/Flipbook.h>
#include <sigilmedia/advanced/Skia.h>

#include "support/BrushTestSupport.h"
#include "support/Rows.h"

namespace {

/** A 2 x 2 sheet of four colours, a cell of four texels each, read row by
 *  row: red, green, blue, white. */
constexpr uint8_t kCells[4][3] = {
    {220, 30, 30}, {30, 200, 40}, {40, 60, 230}, {240, 240, 240}};

material::Texture fourCells() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(8, 8);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x) {
      const uint8_t* cell = kCells[(y / 4) * 2 + (x / 4)];
      *bitmap.getAddr32(x, y) =
          SkPreMultiplyARGB(255, cell[0], cell[1], cell[2]);
    }
  bitmap.setImmutable();
  return material::Texture(sigil::media::PixelSource(bitmap.asImage()));
}

}  // namespace

TEST(ComposeMaterial, AMaterialFillPaintsWhatItsRecipeStatesOnTheNodesBox) {
  sigil::compose::test::checkRows({
      {"AFlipbookShowsTheCellItsClockStandsAt",
       [&] {
         // The same sheet and the same clock a 3D body plays it at: a node's
         // box is one cell, and the cell is the one the clock stands in.
         for (int frame = 0; frame < 4; ++frame) {
           Host host(100, 100);
           host.composer.render(box().children(
               {box().width(100).height(100).absolute().inset(0).fill(
                   material::flipbook(
                       fourCells(),
                       {.columns = 2, .rows = 2, .framesPerSecond = 1}))}));
           host.frame(frame + 0.5);
           const SkColor shown = host.pixel(50, 50);
           EXPECT_NEAR((int)SkColorGetR(shown), kCells[frame][0], 1) << frame;
           EXPECT_NEAR((int)SkColorGetG(shown), kCells[frame][1], 1) << frame;
           EXPECT_NEAR((int)SkColorGetB(shown), kCells[frame][2], 1) << frame;
         }
       }},
      {"ABlendWithAnSdfLayerResolvesAgainstTheBox",
       [&] {
         // A blend containing a geometry-dependent (SDF) layer must defer its
         // flatten to resolve time, when the node's size is known. Flattening
         // at build time bakes a zero resolution and renders a degenerate
         // speck.
         material::Paint m = material::Paint::blend({
             {material::Paint::solid({0, 0, 0, 1}),
              material::BlendMode::Normal},
             {material::Paint::recipe(material::sdf::material(
                  material::sdf::circle(), {.fill = {1, 0, 0, 1}})),
              material::BlendMode::PlusLighter},
         });
         EXPECT_TRUE(m.geometryDependent());  // inherited from the SDF layer
         EXPECT_FALSE(m.isRunning());         // still cacheable
         Host host;
         host.composer.render(
             box().children({box()
                                 .width(100)
                                 .height(100)
                                 .inset(0, 100, 100, 0)
                                 .absolute()
                                 .fill(material::skia::base(m))}));
         host.frame();
         EXPECT_GT(SkColorGetR(host.pixel(50, 50)),
                   150u);  // circle body visible
         EXPECT_LT(SkColorGetR(host.pixel(3, 3)),
                   40u);  // corner outside circle
       }},
  });
}

TEST(ComposeSdf, AnSdfMaterialCostsWhatItsVolatilityTierSays) {
  sigil::compose::test::checkRows({
      {"AGeometryDependentOneCachesAndPrunes",
       [&] {
         // An SDF material reads uResolution (geometry-dependent) but binds no
         // live values: it must CACHE like static content (0 live paints, 0
         // re-records) AND prune across an identical re-describe (recipe
         // equality — same per-kind effect pointer, equal constants).
         Host host;
         auto tree = [] {
           return box().children(
               {box().width(80).height(60).fill(material::sdf::material(
                   material::sdf::roundBox(12),
                   {.fill = {0, 1, 0, 1}, .borderWidth = 3}))});
         };
         host.composer.render(tree());
         host.frame();  // records
         host.frame();  // replays
         EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
         EXPECT_EQ(host.composer.stats().nodesPainted, 0u);
         host.composer.render(tree());  // fresh describe, identical recipe
         EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
         EXPECT_FALSE(host.composer.dirty());
       }},
      {"ABoundGlowRepaintsWithoutADescribeWithinItsReserve",
       [&] {
         // Alive chrome: bind uGlowR to a live value — the material goes live
         // and the glow breathes with the value, no render() calls. The style's
         // glowRadius reserves the pad; the binding animates within it.
         sigil::motion::Animatable<float> glow =
             sigil::motion::animatable(0.01f);
         const material::sdf::Style style{
             .fill = {1, 0, 0, 1}, .glowRadius = 12, .glowColor = {1, 1, 1, 1}};
         Host host;
         host.composer.render(
             box().children({box()
                                 .width(100)
                                 .height(100)
                                 .inset(0, 100, 100, 0)
                                 .absolute()
                                 .fill(material::skia::base(
                                     material::Paint::recipe(
                                         material::sdf::material(
                                             material::sdf::circle(), style))
                                         .bind("uGlowR", glow)))}));
         host.frame();
         // Size the probe from the PUBLIC pad helper (no hand-copied formula):
         // circle radius = 50 − pad; sample 6px outside the edge.
         const int probeX =
             (int)(50.0f + (50.0f - material::sdf::pad(style)) + 6.0f);
         const uint32_t dim = SkColorGetR(host.pixel(probeX, 50));
         glow = 12.0f;  // brighten the falloff — no re-render
         host.frame();
         const uint32_t lit = SkColorGetR(host.pixel(probeX, 50));
         EXPECT_LT(dim, 25u);  // exp(-6/0.01) ≈ 0
         EXPECT_GT(lit, 80u);  // exp(-6/12) · edge cutoff ≈ 0.51 → ~130
       }},
  });
}

TEST(ComposeSdf, AnSdfMaterialResolvesAgainstTheNodesBox) {
  sigil::compose::test::checkRows({
      {"AResizeResolvesItAgain",
       [&] {
         // uResolution bakes into the recording; a size change must re-resolve
         // — the materialSize invalidation, without any prop change.
         Host host;  // 200x200 surface
         host.composer.render(
             box().children({box().flexGrow(1).fill(material::sdf::material(
                 material::sdf::circle(), {.fill = {1, 0, 0, 1}}))}));
         host.frame();  // circle c=(100,100) r≈99
         host.composer.setSize({120, 120});
         host.frame();  // circle c=(60,60) r≈59
         // (15,110): inside the OLD circle (dist≈85.6<99) but outside the new
         // one (dist≈67.3>59) — red here means a stale bake replayed.
         EXPECT_LT(SkColorGetR(host.pixel(15, 110)), 30u);
         EXPECT_GT(SkColorGetR(host.pixel(60, 60)), 200u);  // new body
       }},
      {"APadThatSwallowsTheBoxWarnsOnceNamingTheMinimumBox",
       [&] {
         // material::sdf::pad() is reserved INSIDE the node's box, so a small
         // box with a large glow radius leaves almost no room for the shape and
         // renders a speck. material::sdf::minimumBoxFor() gives the size that
         // would fit, but an author who does not already know it exists has no
         // way to find it — so the warning fires where the two numbers first
         // meet, at resolve, and names it.
         const material::sdf::Style style{
             .fill = {1, 0, 0, 1}, .glowRadius = 20, .glowColor = {1, 1, 1, 1}};
         ASSERT_GE(material::sdf::pad(style),
                   30.0f);  // the premise: pad >= half of 60
         ::testing::internal::CaptureStderr();
         {
           Host host;
           host.composer.render(box().children({box().width(60).height(60).fill(
               material::sdf::material(material::sdf::circle(), style))}));
           host.frame();
         }
         const std::string first = ::testing::internal::GetCapturedStderr();
         EXPECT_NE(first.find("material::sdf::minimumBoxFor"),
                   std::string::npos)
             << first;
         // Warned ONCE, process-wide: a second offender stays silent — the
         // house diagnostic contract (renderSlot's unknown-name warning), not a
         // per-frame log.
         ::testing::internal::CaptureStderr();
         {
           Host host;
           host.composer.render(box().children({box().width(50).height(50).fill(
               material::sdf::material(material::sdf::circle(), style))}));
           host.frame();
         }
         EXPECT_EQ(::testing::internal::GetCapturedStderr().find(
                       "material::sdf::minimumBoxFor"),
                   std::string::npos);
       }},
  });
}

TEST(ComposePattern, APatternIsAValueWhoseRecipeDecidesItsPrune) {
  sigil::compose::test::checkRows({
      {"AHeldPatternPrunesAndAReseedRegenerates",
       [&] {
         // The identity contract: a HELD pattern re-described is pointer-equal
         // (prunes, no rebake); .seed(n) drops the bake and shows up as exactly
         // one changed recipe.
         Pattern grain =
             Pattern(material::pattern::speckle(64, 40, 1, 3, {{1, 1, 1, 1}}));
         Host host;
         auto tree = [&] {
           return box().children(
               {box().width(80).height(80).fill(grain.material())});
         };
         host.composer.render(tree());
         host.frame();
         host.composer.render(tree());  // same bake → same recipe → prune
         EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
         EXPECT_FALSE(host.composer.dirty());
         grain.seed(7);  // regenerate
         host.composer.render(tree());
         EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
         EXPECT_TRUE(host.composer.dirty());
       }},
      {"ReseedingACopyLeavesTheOriginalAlone",
       [&] {
         // A Pattern is a VALUE — scale, rotate, offset and sampling are all
         // per-object. seed() and retile() touch the shared recipe, so they
         // must copy on write: editing it in place would drop the original's
         // bake and regenerate every element still drawing the old tile. Same
         // aliasing hazard, and same answer, as binding a uniform on a copied
         // Material.
         Pattern base =
             Pattern(material::pattern::speckle(64, 40, 1, 3, {{1, 1, 1, 1}}));
         base.seed(11);
         auto plate = [&] {
           Host host(64, 64);
           host.composer.render(box().children(
               {box().width(64).height(64).fill(base.material())}));
           host.frame();
           std::vector<SkColor> px;
           for (int y = 0; y < 64; y += 3)
             for (int x = 0; x < 64; x += 3) px.push_back(host.pixel(x, y));
           return px;
         };
         const std::vector<SkColor> before = plate();

         Pattern copy = base;
         copy.seed(99);
         EXPECT_EQ(base.currentSeed(), 11u)
             << "the copy re-rolled the original";
         EXPECT_EQ(copy.currentSeed(), 99u);
         EXPECT_EQ(plate(), before) << "and dropped its bake with it";
       }},
  });
}

TEST(ComposePattern, AnElementTreeIsATile) {
  // Patterns are compositions: an element tree (two boxes) as the tile.
  Pattern duo = Pattern::tile(
      {20, 10},
      box().row().children({box().width(10).height(10).fill(red()),
                            box().width(10).height(10).fill(blue())}));
  Host host;
  host.composer.render(box().children({box()
                                           .width(40)
                                           .height(10)
                                           .inset(0, 160, 190, 0)
                                           .absolute()
                                           .fill(duo.material(fonts()))}));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(15, 5), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(25, 5), SK_ColorRED);  // the repeat
}

// ---- a material's effects: the Photoshop route
// --------------------------------------

TEST(ComposeStyles, ALayerStyleKeepsItsReachThroughACachedReplay) {
  // A blurred shadow reaches well past its node, so the recording cull has
  // to grow by the decoration's declared bleed(). Otherwise the shadow draws
  // on the first frame and is truncated by every cached replay after it.
  Host host;
  host.composer.render(box().padding(40).children(
      {box()
           .width(60)
           .height(40)
           .background(sigil::compose::shadow(material::Color{1, 0, 0, 0.9f},
                                              {0, 10}, 20))
           .fill(Fill::color({0.2f, 0.2f, 0.2f, 1}))}));
  host.frame();
  host.frame();  // cached replay
  // Node spans y∈[40,80); sample 14px below it — the soft red reach.
  EXPECT_GT(SkColorGetR(host.pixel(70, 94)), 25u);
}

TEST(ComposeColor, AViewTransformFiltersTheOutputAndClearingItPassesThrough) {
  // An exponent transform darkens mid-grey (0.5^2.2 ~ 0.218); clearing the
  // view restores pass-through. Exercises bake, response row, the
  // lowering the host's eight-bit surface asks for, and saveLayer.
#ifndef SIGILMATERIAL_ENABLE_OCIO
  GTEST_SKIP() << "built without OpenColorIO, so a view transform is a "
                  "no-op and this proves nothing";
#else
  ASSERT_TRUE(sigil::material::ocio::available());
  Host host;
  host.composer.setView(sigil::material::ocio::exponent(2.2f));
  host.composer.render(box().children(
      {box().width(60).height(60).fill(Fill::color({0.5f, 0.5f, 0.5f, 1}))}));
  host.frame();
  const uint32_t dark = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(dark, 30u);
  EXPECT_LT(dark, 80u);
  host.composer.setView({});  // pass-through again
  host.frame();
  const uint32_t plain = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(plain, 118u);
  EXPECT_LT(plain, 138u);
#endif
}
