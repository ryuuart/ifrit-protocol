// The kernel's small values, asked directly rather than through a draw:
// what a cache mode says to the settled-subtree proof, what a paint
// collapses to when nothing about it moves, what frame a paint is
// resolved against, the store a node keeps its stamp bakes in, and the
// value semantics of an Element that has already been rendered.

#include <sigilmaterial/skia/Paint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cache/StampCache.h"
#include "support/CoreTestSupport.h"
#include "support/Rows.h"

using sigil::compose::test::checkRows;
using sigil::compose::test::forEachRow;

TEST(ComposeValues, ACacheModeSaysOneOfThreeThingsToTheProof) {
  // Five names, three answers: three of them ask for a bake and differ
  // only in which artefact the painter makes, which is not a question the
  // proof is asked.
  EXPECT_EQ(cachePolicy(Cache::Auto), sigil::core::Cache::Auto);
  EXPECT_EQ(cachePolicy(Cache::None), sigil::core::Cache::Never);
  EXPECT_EQ(cachePolicy(Cache::Picture), sigil::core::Cache::Always);
  EXPECT_EQ(cachePolicy(Cache::Texture), sigil::core::Cache::Always);
  EXPECT_EQ(cachePolicy(Cache::Group), sigil::core::Cache::Always);
}

TEST(ComposeValues, AStaticPaintCollapsesToTheFillItResolvesTo) {
  // The collapse is what lets a paint ride the fill caching and the prune
  // path unchanged: a paint with nothing moving in it IS a fill, and the
  // per-frame resolve answers the same thing.
  const material::Paint solid =
      material::Paint::solid({0.25f, 0.5f, 0.75f, 1.0f});
  const Fill collapsed = toFill(material::skia::base(solid));
  EXPECT_EQ(collapsed.kind, Fill::Kind::Color);
  EXPECT_FLOAT_EQ(collapsed.colorValue.r, 0.25f);
  EXPECT_FLOAT_EQ(collapsed.colorValue.b, 0.75f);

  PaintContext ctx;
  ctx.size = {40, 20};
  const Fill resolved = resolveFill(material::skia::base(solid), ctx);
  EXPECT_EQ(resolved.kind, collapsed.kind);
  EXPECT_EQ(resolved.colorValue, collapsed.colorValue);
}

TEST(ComposeValues, TheFrameAPaintResolvesAgainstIsTheContextsOwn) {
  // Every number a paint reads about where it is being drawn comes from
  // one place, so a world-space paint anchored by the painter and one
  // resolved standalone cannot disagree about what the frame was.
  PaintContext ctx;
  ctx.size = {40, 20};
  ctx.rootSize = {800, 600};
  ctx.toRoot = geometry::path::Transform::translate({30, 40});
  ctx.elapsedSeconds = 2.5;
  ctx.contentScale = 2.0f;
  const material::FrameData frame = frameOf(ctx);
  EXPECT_EQ(frame.resolution, glm::vec2(40, 20));
  EXPECT_EQ(frame.rootResolution, glm::vec2(800, 600));
  EXPECT_EQ(material::skia::toSkMatrix(frame.world),
            SkMatrix::Translate(30, 40));
  EXPECT_DOUBLE_EQ(frame.seconds, 2.5);
  EXPECT_FLOAT_EQ(frame.contentScale, 2.0f);
}

TEST(ComposeStamps, ReBakingOneArtKeepsEveryOtherBakeInTheStore) {
  // The store is a node's, and its capacity is there to stop a scene
  // whose keys churn from pinning their nodes' memory. A key the store
  // ALREADY holds is not churn: re-baking one art of a full store must
  // replace that one entry and leave the others where they are, or a
  // stamped scene at capacity re-rasterises everything it draws every
  // time any one of its arts is re-baked.
  StampCache cache;
  std::vector<std::shared_ptr<const void>> arts;
  for (int i = 0; i < 16; ++i) {
    arts.push_back(std::make_shared<int>(i));
    cache.put(arts.back(), StampCache::Entry{});
  }
  for (const auto& art : arts) ASSERT_NE(cache.get(art), nullptr);

  cache.put(arts[3], StampCache::Entry{.artSize = {8, 8}});
  for (const auto& art : arts)
    EXPECT_NE(cache.get(art), nullptr) << "a re-bake emptied the store";
  const StampCache::Entry* replaced = cache.get(arts[3]);
  ASSERT_NE(replaced, nullptr);
  EXPECT_EQ(replaced->artSize, SkSize::Make(8, 8)) << "…and replaced in place";

  // An art the store never held is answered with nothing, however full
  // the store is.
  EXPECT_EQ(cache.get(std::make_shared<int>(99)), nullptr);
}

TEST(ComposeElement, AnElementIsACopyOnWriteValue) {
  checkRows({
      {"MutatingRenderedValueDetachesDescription",
       [] {
         Host host;
         Element panel = box().width(100).height(100).fill(red());

         host.composer.render(box().children({panel}));
         host.frame();
         EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);

         // Composer retains the first description. Mutating the caller's value
         // must create a new description so pointer-identity pruning cannot
         // preserve the old cached picture.
         panel.fill(blue());
         host.composer.render(box().children({panel}));
         host.frame();
         EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);
       }},
      {"CopiedValuesMutateIndependently",
       [] {
         Host host;
         Element left = box().width(100).height(100).fill(red());
         Element right = left;
         right.fill(blue());

         host.composer.render(box().row().children({left, right}));
         host.frame();
         EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);
         EXPECT_EQ(host.pixel(150, 50), SK_ColorBLUE);
       }},
  });
}

TEST(ComposeElement, AWholeNumberOnALaneIsTheNumberItSpells) {
  // Every lane takes an animatable, which a pointer to an output also
  // converts to: an integer literal, zero included, has to arrive as the
  // plain value it reads as and never as a null binding.
  Host host;
  host.composer.render(box().children({box()
                                           .key("mover")
                                           .width(20)
                                           .height(20)
                                           .fill(red())
                                           .opacity(1)
                                           .translateX(40)
                                           .translateY(0)
                                           .rotate(0)
                                           .scale(1)
                                           .scaleX(2)
                                           .scaleY(1)
                                           .skewX(0)
                                           .skewY(0)
                                           .rotateX(0)
                                           .rotateY(0)
                                           .translateZ(0)
                                           .scaleZ(1)
                                           .perspective(0)}));
  host.frame();
  // Twice as wide about its centre, forty pixels to the right: x 30 to 70.
  EXPECT_EQ(host.pixel(35, 10), SK_ColorRED);
  EXPECT_EQ(host.pixel(65, 10), SK_ColorRED);
  EXPECT_NE(host.pixel(25, 10), SK_ColorRED);
  EXPECT_NE(host.pixel(75, 10), SK_ColorRED);
}
