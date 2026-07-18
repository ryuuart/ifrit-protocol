/** @file
 * TextFlowKit: rebuild guards, layout memoization, and glyph bucketing.
 * These tests pin the invalidation semantics the kit exists to make
 * explicit — which key changes fire a rebuild, and which must not.
 */

#include "TestSupport.h"

#include <textflowkit/TextFlowKit.h>

#include <gtest/gtest.h>

#include <string>
#include <tuple>

using namespace textflow;
using namespace textflow::test;

namespace {

TEST(RebuildGuard, FiresOnFirstUseThenOnlyOnKeyChange) {
  textflowkit::RebuildGuard<std::string, float> guard;
  int builds = 0;
  auto build = [&] { ++builds; };

  EXPECT_TRUE(guard.ensure({"a", 1.0f}, build));
  EXPECT_FALSE(guard.ensure({"a", 1.0f}, build));
  EXPECT_TRUE(guard.ensure({"a", 2.0f}, build));
  EXPECT_TRUE(guard.ensure({"b", 2.0f}, build));
  EXPECT_FALSE(guard.ensure({"b", 2.0f}, build));
  EXPECT_EQ(builds, 3);

  guard.invalidate();
  EXPECT_TRUE(guard.ensure({"b", 2.0f}, build));
  EXPECT_EQ(builds, 4);
}

TEST(RebuildGuard, ThrowingBuildStaysInvalidAndRetries) {
  textflowkit::RebuildGuard<int> guard;
  EXPECT_THROW(guard.ensure({1}, [] { throw std::runtime_error("boom"); }),
               std::runtime_error);
  EXPECT_FALSE(guard.built());
  int builds = 0;
  EXPECT_TRUE(guard.ensure({1}, [&] { ++builds; }));
  EXPECT_EQ(builds, 1);
}

TEST(CachedValue, ReturnsCachedValueUntilKeyChanges) {
  textflowkit::CachedValue<int, int> cached;
  EXPECT_EQ(cached.ensure({10}, [] { return 100; }), 100);
  // Same key: the stale-looking callable must not run.
  EXPECT_EQ(cached.ensure({10}, [] { return 999; }), 100);
  EXPECT_EQ(cached.ensure({20}, [] { return 200; }), 200);
  EXPECT_EQ(cached.value(), 200);
}

TEST(CachedValue, KeylessEnsureBuildsOnce) {
  textflowkit::CachedValue<int> lazy;
  int builds = 0;
  auto build = [&] {
    ++builds;
    return 7;
  };
  EXPECT_EQ(lazy.ensure(build), 7);
  EXPECT_EQ(lazy.ensure(build), 7);
  EXPECT_EQ(builds, 1);
}

TEST(LayoutGuard, RelayoutsOnEditAndDeclaredKeysOnly) {
  Paragraph paragraph = makeParagraph(u8"guarded layout text");
  textflowkit::LayoutGuard<SkISize, TextAlignment> guard;
  ParagraphLayout layout;
  int relayouts = 0;
  auto relayout = [&] {
    ++relayouts;
    BlockFlow flow(SkRect::MakeXYWH(0, 0, 200, 200));
    layout = layoutParagraph(sharedContext(), paragraph, flow);
  };

  const SkISize size{400, 300};
  EXPECT_TRUE(guard.ensure(paragraph, {size, TextAlignment::kStart}, relayout));
  // Steady frames: same content, same keys — the layout must be reused.
  EXPECT_FALSE(guard.ensure(paragraph, {size, TextAlignment::kStart}, relayout));
  EXPECT_FALSE(guard.ensure(paragraph, {size, TextAlignment::kStart}, relayout));
  EXPECT_EQ(relayouts, 1);

  // A declared key change fires exactly one relayout.
  EXPECT_TRUE(
      guard.ensure(paragraph, {size, TextAlignment::kJustify}, relayout));
  EXPECT_EQ(relayouts, 2);

  // A live edit bumps the revision/dirties shaping; the guard must notice
  // without the caller declaring anything.
  paragraph.appendText(u8" and more", basicStyle());
  EXPECT_TRUE(
      guard.ensure(paragraph, {size, TextAlignment::kJustify}, relayout));
  EXPECT_FALSE(
      guard.ensure(paragraph, {size, TextAlignment::kJustify}, relayout));
  EXPECT_EQ(relayouts, 3);
  EXPECT_FALSE(layout.runs.empty());
}

TEST(LayoutGuard, PaintOnlyRestyleDoesNotRelayout) {
  Paragraph paragraph = makeParagraph(u8"repaint me freely");
  textflowkit::LayoutGuard<SkISize> guard;
  ParagraphLayout layout;
  int relayouts = 0;
  auto relayout = [&] {
    ++relayouts;
    BlockFlow flow(SkRect::MakeXYWH(0, 0, 300, 100));
    layout = layoutParagraph(sharedContext(), paragraph, flow);
  };

  const SkISize size{300, 100};
  // The first paint introduces new span boundaries, so the first ensure
  // (which also covers first use) lays out once.
  paragraph.setPaint(0, 7, PaintStyle(SK_ColorRED));
  EXPECT_TRUE(guard.ensure(paragraph, {size}, relayout));
  // The hue-cycling-marker idiom: repainting the *same* range every frame
  // leaves span boundaries untouched and must re-hit the cached layout.
  paragraph.setPaint(0, 7, PaintStyle(SK_ColorBLUE));
  EXPECT_FALSE(guard.ensure(paragraph, {size}, relayout));
  EXPECT_EQ(relayouts, 1);
}

TEST(Quantize, SnapsToStepMultiples) {
  EXPECT_FLOAT_EQ(textflowkit::quantize(10.3f), 10.0f);
  EXPECT_FLOAT_EQ(textflowkit::quantize(10.6f), 11.0f);
  EXPECT_FLOAT_EQ(textflowkit::quantize(103.0f, 8.0f), 104.0f);
  EXPECT_FLOAT_EQ(textflowkit::quantize(-2.6f), -3.0f);
}

TEST(GlyphBuckets, GroupsByKeyAndSkipsEmptyOnDraw) {
  struct Shade {
    int level = 0;
    int fade = 0;
    bool operator==(const Shade &) const = default;
  };
  textflowkit::GlyphBuckets<Shade> buckets;
  buckets.add({1, 0}, 10, {0, 0});
  buckets.add({1, 0}, 11, {1, 0});
  buckets.add({2, 3}, 12, {2, 0});
  ASSERT_EQ(buckets.buckets.size(), 2u);
  EXPECT_EQ(buckets.buckets[0].glyphs.size(), 2u);
  EXPECT_EQ(buckets.buckets[0].placements.size(), 2u);

  int visited = 0;
  EXPECT_EQ(buckets.drawEach([&](const auto &) { ++visited; }), 3);
  EXPECT_EQ(visited, 2);

  // clear() keeps the buckets (and their allocations) but empties them, so
  // the next frame's drawEach visits nothing.
  buckets.clear();
  ASSERT_EQ(buckets.buckets.size(), 2u);
  EXPECT_EQ(buckets.drawEach([&](const auto &) { ADD_FAILURE(); }), 0);
}

TEST(SampleText, FillerIsDeterministicAndMultiSpan) {
  const Paragraph first = textflowkit::mixedScriptFiller(240, 16.0f);
  const Paragraph second = textflowkit::mixedScriptFiller(240, 16.0f);
  EXPECT_EQ(first.text(), second.text());
  EXPECT_GT(first.spans().size(), 1u);
}

} // namespace
