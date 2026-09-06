// The kernel's small values, asked directly rather than through a draw:
// what a cache mode says to the settled-subtree proof, what a paint
// collapses to when nothing about it moves, what frame a paint is
// resolved against, and the store a node keeps its stamp bakes in.

#include <memory>
#include <vector>

#include "support/CoreTestSupport.h"

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
  const material::skia::Paint solid =
      material::skia::Paint::solid({0.25f, 0.5f, 0.75f, 1.0f});
  const Fill collapsed = toFill(solid);
  EXPECT_EQ(collapsed.kind, Fill::Kind::Color);
  EXPECT_FLOAT_EQ(collapsed.colorValue.fR, 0.25f);
  EXPECT_FLOAT_EQ(collapsed.colorValue.fB, 0.75f);

  PaintContext ctx;
  ctx.size = {40, 20};
  const Fill resolved = resolveFill(solid, ctx);
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
  ctx.toRoot = SkMatrix::Translate(30, 40);
  ctx.elapsedSeconds = 2.5;
  ctx.contentScale = 2.0f;
  const material::skia::PaintFrame frame = frameOf(ctx);
  EXPECT_EQ(frame.size, SkSize::Make(40, 20));
  EXPECT_EQ(frame.rootSize, SkSize::Make(800, 600));
  EXPECT_EQ(frame.toRoot, SkMatrix::Translate(30, 40));
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
