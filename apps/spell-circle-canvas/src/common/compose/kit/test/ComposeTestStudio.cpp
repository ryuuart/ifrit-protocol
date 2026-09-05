// The studio instruments: the meters that draw a schedule back onto the
// scene, the console's per-column feeds, and the report that lands a
// measured table in one.

#include <include/core/SkBBHFactory.h>
#include <include/core/SkFont.h>
#include <include/core/SkPictureRecorder.h>
#include <sigilcompose/core/Feed.h>

#include <numeric>

#include "support/StudioTestSupport.h"

namespace {

/** An effect under `key` returning one fixed deviation — the readable way
 *  to drive a single GlyphMod field from a test. */
TextEffect fixed(std::string key, GlyphMod mod) {
  return fx::effect(
      std::move(key),
      [mod](const GlyphInfo&, float, core::noise::Mix64Stream&) { return mod; },
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
  int running = 0, unfinished = 0;
  for (const Beat& beat : beats) {
    const int y = (int)beat.rect.centerY();
    const int left = (int)beat.rect.left() + 1;
    const int right = (int)beat.rect.right() - 1;
    ASSERT_LT(left, right) << "a beat rect with no width to draw in";
    const SkColor at = host.pixel(left, y);
    if (beat.localT > 0.05f) {
      ++running;
      EXPECT_GT(SkColorGetR(at), 200u)
          << "a beat that has run shows no fill at its left edge";
    }
    if (beat.localT < 0.95f) {
      ++unfinished;
      EXPECT_GT(SkColorGetB(host.pixel(right, y)), 200u)
          << "a beat that has not finished shows no bed at its right edge";
    }
  }
  // Both arms have to have been reached, or the loop above asserted
  // nothing about one of the two states it exists to tell apart.
  EXPECT_GT(running, 0) << "no beat had started at this moment";
  EXPECT_GT(unfinished, 0) << "every beat had finished at this moment";
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
  host.composer.render(box().padding(10).child(
      kit::restGhost(text(u8"ALPHA BETA", whiteStyle(24))
                         .key("word")
                         .mark(sigil::weave::sel::word(1),
                               box().key("caret").width(4).fill(green())),
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

// ---------------------------------------------------------------------------
// kit::console and test::report — the two halves of a verification plate.

#include <sigilcompose/kit/Plate.h>
#include <sigilcompose/testing/Checks.h>

TEST(ComposeConsole, StacksFeedsPerColumnInOneVoice) {
  feed::TextRing a{8}, b{8}, c{8}, d{8};
  for (feed::TextRing* ring : {&a, &b, &c, &d}) ring->append({u8"a row", ""});
  feed::TextOptions voice;
  voice.styles =
      kit::tinted(nullptr, 12.0f, {1, 1, 1, 1}, {{"pass", {0, 1, 0, 1}}});
  voice.window.visible = 4;
  const float row = feed::height(voice, 1, fonts());
  kit::Plate chrome;
  chrome.paddingX = 0;
  chrome.paddingY = 0;
  // One feed per column: the plate is one row tall.
  const float one =
      intrinsicSize(box().child(kit::console(
                  {.feeds = {&a, &b}, .style = voice, .plate = chrome})),
              fonts())
          .height();
  EXPECT_NEAR(one, row, 1.0f);
  // Two per column: two rows and the stack gap.
  const float two = intrinsicSize(box().child(kit::console({.feeds = {&a, &b, &c, &d},
                                                      .style = voice,
                                                      .stacked = 2,
                                                      .stackGap = 6,
                                                      .plate = chrome})),
                            fonts())
                        .height();
  EXPECT_NEAR(two, 2 * row + 6, 1.5f);
  // A null feed is skipped rather than dereferenced.
  const float gap = intrinsicSize(box().child(kit::console({.feeds = {&a, nullptr},
                                                      .style = voice,
                                                      .plate = chrome})),
                            fonts())
                        .height();
  EXPECT_NEAR(gap, row, 1.0f);
}

TEST(ComposeReport, ATableLandsInTheFeedRowByRowInTheInkOfItsStanding) {
  namespace measure = sigil::measure;
  measure::Table table;
  table.add(measure::heading("THE RETE"))
      .add(measure::check("spurs", 0, 0))
      .add(measure::check("components", 1, 2))
      .add(measure::finding(measure::check("legend", 1.0, 1.1, 0.01)))
      .add(measure::reading("residual", 5.6e-16));
  feed::TextRing ring{16};
  test::report(ring, table, {.labelWidth = 12, .valueWidth = 4});
  ASSERT_EQ(ring.size(), 5u);
  const auto& rows = ring.rows();
  EXPECT_EQ(rows[0].value.style, "heading");
  EXPECT_TRUE(rows[0].value.text == u8"THE RETE");
  EXPECT_EQ(rows[1].value.style, "pass");
  EXPECT_EQ(rows[2].value.style, "fail");
  EXPECT_EQ(rows[3].value.style, "fail");
  EXPECT_EQ(rows[4].value.style, "number");
  // The row carries the formatter's line -- how SigilMeasure lays a
  // reading out is SigilMeasure's own claim, so what is asserted here is
  // that the line arrived whole and named its reading.
  EXPECT_NE(rows[4].value.text.find(u8"residual"), std::u8string::npos);
  // A plate that tells a finding from a failure names its ink.
  test::report(ring, table.rows[3], {.finding = "measured"});
  EXPECT_EQ(ring.rows().back().value.style, "measured");
  // The two-name spelling still routes a reading to its own ink.
  test::report(ring, measure::reading("bars", 41), "ok", "bad");
  EXPECT_EQ(ring.rows().back().value.style, "number");
  test::report(ring, measure::check("bars", 41, 40), "ok", "bad");
  EXPECT_EQ(ring.rows().back().value.style, "bad");
}
