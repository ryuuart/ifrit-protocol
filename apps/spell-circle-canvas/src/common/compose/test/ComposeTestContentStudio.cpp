// The studio binary's share of ComposeTestContent.cpp: the suites whose
// subjects are studio-tier values, cut from that file so each test binary links
// only the target it exercises.

#include <include/core/SkBBHFactory.h>
#include <include/core/SkFont.h>
#include <include/core/SkPictureRecorder.h>
#include <sigilcompose/Feed.h>

#include <numeric>

#include "support/StudioTestSupport.h"

namespace {

/** An effect under `key` returning one fixed deviation — the readable way
 *  to drive a single GlyphMod field from a test. */
TextEffect fixed(std::string key, GlyphMod mod) {
  return fx::effect(
      std::move(key), [mod](const GlyphInfo&, float, Rng&) { return mod; },
      /*reach=*/120.0f);
}

}  // namespace

// ---------------------------------------------------------------------------
// The Debug.h instruments

TEST(ComposeDebug, TrackMeterDrawsACellPerBeatAtItsRect) {
  // The meter is beatsOf drawn: a cell on each unit's own rect, filled by
  // that unit's local time. Half way through a four-beat cascade, the first
  // cells are full, the last are empty, and every cell stands where its
  // letters do.
  Host host(300, 140);
  const auto describe = [&](bool withMeter) {
    Element root = box().padding(10).child(
        text(u8"ABCD", whiteStyle(28))
            .key("word")
            .fx({.effect = fx::rise(4),
                 .stagger = {.eachMs = 100, .durationMs = 100},
                 .progress = 0.5f}));
    if (withMeter)
      root.child(
          kit::trackMeter(host.composer, "word", 0, {1, 0, 0, 1}, {0, 0, 1, 1})
              .absolute()
              .inset(0));
    return root;
  };
  host.composer.render(describe(false));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("word", 0);
  ASSERT_EQ(beats.size(), 4u);
  host.composer.render(describe(true));
  host.frame();

  // Every beat's rect carries a cell: bed where the beat has not run, fill
  // where it has, and the boundary between them at its localT.
  for (const Beat& beat : beats) {
    const int y = (int)beat.rect.centerY();
    const int left = (int)beat.rect.left() + 1;
    const int right = (int)beat.rect.right() - 1;
    ASSERT_LT(left, right) << "a beat rect with no width to draw in";
    const SkColor at = host.pixel(left, y);
    if (beat.localT > 0.05f)
      EXPECT_GT(SkColorGetR(at), 200u)
          << "a beat that has run shows no fill at its left edge";
    if (beat.localT < 0.95f)
      EXPECT_GT(SkColorGetB(host.pixel(right, y)), 200u)
          << "a beat that has not finished shows no bed at its right edge";
  }
  // …and outside the last beat's rect there is no meter at all: the cells
  // are the units' boxes and not one strip across the node.
  EXPECT_EQ(SkColorGetB(host.pixel((int)beats.back().rect.right() + 6,
                                   (int)beats.back().rect.centerY())),
            0u);
  // An unknown key is the query family's silent nothing, drawn: an overlay
  // with no cells in it, which measures as nothing rather than warning.
  Host empty(120, 80);
  empty.composer.render(box().child(
      kit::trackMeter(host.composer, "typo", 0, {1, 0, 0, 1}, {0, 0, 1, 1})
          .key("meter")
          .absolute()
          .inset(0)));
  empty.frame();
  for (int y = 0; y < 80; ++y)
    for (int x = 0; x < 120; ++x)
      ASSERT_EQ(empty.pixel(x, y), SK_ColorBLACK)
          << "an unknown key drew a meter anyway";
}

TEST(ComposeDebug, RestGhostDrawsTheSameWordUndeformedUnderTheMovingOne) {
  // The ghost is the rest position a deviation is measured against, so it
  // must be where the letters WOULD be — and must not be carrying the track
  // that moved them.
  Host host(300, 140);
  const SkColor4f ghostInk{0, 0, 1, 1};
  GlyphMod shove;
  shove.dx = 60.0f;
  host.composer.render(box().padding(10).child(
      kit::restGhost(text(u8"AB", whiteStyle(40))
                         .key("word")
                         .fx({.effect = fixed("shove", shove)}),
                     ghostInk)));
  host.frame();
  const auto countBlue = [&](SkIRect region) {
    int hits = 0;
    for (int y = region.top(); y < region.bottom(); ++y)
      for (int x = region.left(); x < region.right(); ++x) {
        const SkColor c = host.pixel(x, y);
        if (SkColorGetB(c) > 180 && SkColorGetR(c) < 80) ++hits;
      }
    return hits;
  };
  const auto countWhite = [&](SkIRect region) {
    int hits = 0;
    for (int y = region.top(); y < region.bottom(); ++y)
      for (int x = region.left(); x < region.right(); ++x)
        if (host.pixel(x, y) == SK_ColorWHITE) ++hits;
    return hits;
  };
  // The ghost sits at rest, near the left; the moving copy is 60 px right
  // of it. Neither region may hold the other's ink.
  const SkIRect atRest = SkIRect::MakeXYWH(0, 0, 60, 140);
  const SkIRect shoved = SkIRect::MakeXYWH(66, 0, 120, 140);
  EXPECT_GT(countBlue(atRest), 20) << "no ghost at the rest position";
  EXPECT_EQ(countWhite(atRest), 0)
      << "the moving copy never left its rest position";
  EXPECT_GT(countWhite(shoved), 20) << "the moving copy did not move";
  EXPECT_EQ(countBlue(shoved), 0) << "the ghost is carrying the track too";
  // The ghost is addressable, and it is exactly as wide as the word.
  const SkRect ghost =
      host.composer.bounds("word-rest").value_or(SkRect::MakeEmpty());
  const SkRect moving =
      host.composer.bounds("word").value_or(SkRect::MakeEmpty());
  ASSERT_FALSE(ghost.isEmpty());
  EXPECT_NEAR(ghost.width(), moving.width(), 0.51f);
  EXPECT_NEAR(ghost.left(), moving.left(), 0.51f);
}

TEST(ComposeDebug, RestGhostCopiesTheTypeAndNotTheMarksOnIt) {
  // A text node's children are already on screen once. Ghosting them would
  // draw each of them twice under one key, which the composer's key index
  // cannot answer for — so the ghost is the type and nothing else.
  Host host(300, 140);
  host.composer.render(box().padding(10).child(kit::restGhost(
      text(u8"ALPHA BETA", whiteStyle(24))
          .key("word")
          .mark(sel::word(1), box().key("caret").width(4).fill(green())),
      {0, 0, 1, 1})));
  host.frame();
  const SkRect caret =
      host.composer.bounds("caret").value_or(SkRect::MakeEmpty());
  ASSERT_FALSE(caret.isEmpty()) << "the mark on the moving copy is gone";
  int greens = 0;
  for (int y = 0; y < 140; ++y)
    for (int x = 0; x < 300; ++x)
      if (host.pixel(x, y) == SK_ColorGREEN) ++greens;
  EXPECT_GT(greens, 0);
  // One mark, drawn once: every green pixel is inside the one rect the
  // query answers for.
  for (int y = 0; y < 140; ++y)
    for (int x = 0; x < 300; ++x)
      if (host.pixel(x, y) == SK_ColorGREEN)
        ASSERT_TRUE(caret.contains((float)x + 0.5f, (float)y + 0.5f))
            << "the ghost carries a second copy of the mark";
}
