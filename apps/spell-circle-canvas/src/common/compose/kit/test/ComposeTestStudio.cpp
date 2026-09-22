// The studio instruments: the meters that draw a schedule back onto the
// scene, the console's per-column feeds, and the report that lands a
// measured table in one — and the instruments' own vocabulary: one
// name per colour look, a typed options value, and the verdict a check
// prints into a scene.

#include <include/core/SkBBHFactory.h>
#include <include/core/SkFont.h>
#include <include/core/SkPictureRecorder.h>
#include <sigilcompose/core/Feed.h>

#include <numeric>

#include "support/StudioTestSupport.h"

namespace {

/** An effect under `key` returning one fixed deviation — the readable way
 *  to drive a single GlyphModifier field from a test. */
TextEffect fixed(std::string key, GlyphModifier mod) {
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
    Element root = box().padding(10).children(
        {text(u8"ABCD", whiteStyle(28))
             .key("word")
             .fx({.effect = fx::rise(4),
                  .stagger = {.eachMs = 100, .durationMs = 100},
                  .progress = 0.5f})});
    if (withMeter)
      root.children(
          {kit::trackMeter(host.composer, "word", 0, {1, 0, 0, 1}, {0, 0, 1, 1})
               .absolute()
               .inset(0)});
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
  empty.composer.render(box().children(
      {kit::trackMeter(host.composer, "typo", 0, {1, 0, 0, 1}, {0, 0, 1, 1})
           .key("meter")
           .absolute()
           .inset(0)}));
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
  GlyphModifier shove;
  shove.dx = 60.0f;
  host.composer.render(box().padding(10).children(
      {kit::restGhost(text(u8"AB", whiteStyle(40))
                          .key("word")
                          .fx({.effect = fixed("shove", shove)}),
                      ghostInk)}));
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
  host.composer.render(box().padding(10).children({kit::restGhost(
      text(u8"ALPHA BETA", whiteStyle(24))
          .key("word")
          .textAttach(sigil::weave::selectors::word(1),
                      box().key("caret").width(4).fill(green())),
      {0, 0, 1, 1})}));
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
      intrinsicSize(box().children({kit::console(
                        {.feeds = {&a, &b}, .style = voice, .plate = chrome})}),
                    fonts())
          .height();
  EXPECT_NEAR(one, row, 1.0f);
  // Two per column: two rows and the stack gap.
  const float two =
      intrinsicSize(box().children({kit::console({.feeds = {&a, &b, &c, &d},
                                                  .style = voice,
                                                  .stacked = 2,
                                                  .stackGap = 6,
                                                  .plate = chrome})}),
                    fonts())
          .height();
  EXPECT_NEAR(two, 2 * row + 6, 1.5f);
  // A null feed is skipped rather than dereferenced.
  const float gap =
      intrinsicSize(
          box().children({kit::console(
              {.feeds = {&a, nullptr}, .style = voice, .plate = chrome})}),
          fonts())
          .height();
  EXPECT_NEAR(gap, row, 1.0f);
}

TEST(ComposeReport, ATableLandsInTheFeedRowByRowInTheInkOfItsStanding) {
  namespace measure = sigil::measure;
  measure::CheckTable table;
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
  EXPECT_NE(rows[4].value.text.bytes().find(u8"residual"), std::u8string::npos);
  // A plate that tells a finding from a failure names its ink.
  test::report(ring, table.rows[3], {.finding = "measured"});
  EXPECT_EQ(ring.rows().back().value.style, "measured");
  // The two-name spelling still routes a reading to its own ink.
  test::report(ring, measure::reading("bars", 41), "ok", "bad");
  EXPECT_EQ(ring.rows().back().value.style, "number");
  test::report(ring, measure::check("bars", 41, 40), "ok", "bad");
  EXPECT_EQ(ring.rows().back().value.style, "bad");
}

// -------------------------------------------------------------------------
// The instruments' own vocabulary: one name per colour look instead of
// one body per call site, a typed options value where positional
// arguments run out, and the verdict a check prints into a scene.

TEST(ComposeStudio, TheColourOpsAreOneNamePerLookInsteadOfOneBodyPerCallSite) {
  // hexColor() is the one colour spelling this library carries, and it
  // answers in SigilMaterial's colour.
  constexpr sigil::material::Color rubric = hexColor(0x8C2F22);
  static_assert(hexColor(0xFFFFFF).r == 1.0f,
                "must stay constexpr — the "
                "palettes are constexpr");
  EXPECT_FLOAT_EQ(rubric.r, 0x8C / 255.0f);
  EXPECT_FLOAT_EQ(rubric.g, 0x2F / 255.0f);
  EXPECT_FLOAT_EQ(rubric.b, 0x22 / 255.0f);
  EXPECT_FLOAT_EQ(rubric.a, 1.0f);
  EXPECT_FLOAT_EQ(hexColor(0x000000, 0.25f).a, 0.25f);
  EXPECT_EQ(rubric, sigil::material::rgb(0x8C2F22))
      << "the same colour SigilMaterial's own spelling answers";

  // phase() wraps and never NaNs on a zero period.
  EXPECT_FLOAT_EQ(motion::phase(0.0, 4.0), 0.0f);
  EXPECT_FLOAT_EQ(motion::phase(3.0, 4.0), 0.75f);
  EXPECT_FLOAT_EQ(motion::phase(9.0, 4.0), 0.25f);
  EXPECT_FLOAT_EQ(motion::phase(1.0, 0.0), 0.0f);
}

TEST(ComposeStudio, ATypedOptionsValueCarriesWhatPositionalArgumentsCannot) {
  // GalleryCore.h:35 already ships styleAt(size, SkColor) and sixteen
  // gallery scene headers wrote their own type() anyway — because they
  // needed a face, or tracking, or condensation, or a wght variation
  // (ScenesInventory.h:99), or slnt instead (ScenesSkillTree.h:122). This
  // test asserts exactly the fields a positional two-argument helper could
  // not reach; if it ever shrinks to size+colour, the extraction has failed
  // the same way its predecessor did.
  const sigil::weave::TextStyle s = weave::textStyle(
      {.size = 18.0f,
       .color = material::skia::toSkColor(SkColor4f{0.2f, 0.4f, 0.6f, 1}),
       .track = 1.25f,
       .condense = 0.94f,
       .weight = 650.0f,
       .slant = -10.0f,
       .aliased = true});
  EXPECT_FLOAT_EQ(s.shaping.fontSize, 18.0f);
  EXPECT_FLOAT_EQ(s.shaping.letterSpacing, 1.25f);
  EXPECT_FLOAT_EQ(s.shaping.scaleX, 0.94f);
  EXPECT_TRUE(s.shaping.aliased);
  ASSERT_EQ(s.shaping.variations.size(), 2u);
  EXPECT_EQ(std::string(s.shaping.variations[0].tag, 4), "wght");
  EXPECT_FLOAT_EQ(s.shaping.variations[0].value, 650.0f);
  EXPECT_EQ(std::string(s.shaping.variations[1].tag, 4), "slnt");
  const SkColor4f c = s.paint.foreground.getColor4f();
  EXPECT_FLOAT_EQ(c.fB, 0.6f);

  // Defaults leave design space alone — an unvaried style must not carry a
  // wght entry, or every default style occupies its own varied-face memo.
  EXPECT_TRUE(weave::textStyle({.size = 12}).shaping.variations.empty());

  // It equals a hand-built style, so a study migrating to it prunes.
  sigil::weave::TextStyle byHand;
  byHand.shaping.fontSize = 18.0f;
  byHand.shaping.letterSpacing = 1.25f;
  byHand.shaping.scaleX = 0.94f;
  byHand.shaping.aliased = true;
  byHand.paint.foreground.setColor4f({0.2f, 0.4f, 0.6f, 1}, nullptr);
  byHand.paint.foreground.setAntiAlias(true);
  byHand.variation("wght", 650.0f);
  byHand.variation("slnt", -10.0f);
  EXPECT_TRUE(s == byHand) << "type() does not build the TextStyle it declares";

  // And it actually lays out — a TextStyle that measures to nothing would
  // satisfy every field assertion above.
  const SkSize measured =
      intrinsicSize(text(u8"Wm", weave::textStyle({.size = 40})), fonts());
  EXPECT_GT(measured.width(), 10.0f);
  EXPECT_GT(measured.height(), 10.0f);
}

TEST(ComposeDebug, CheckPrintsTheVerdictItComputed) {
  // A figure that prints its own verification is worthless if the printed
  // verdict is written by hand next to the numbers: the string and the claim
  // are then unconnected, and the plate cannot be falsified by its own
  // output. sigil::measure::check() derives the verdict FROM the two values it
  // prints, so a wrong number changes the word beside it.
  const sigil::measure::Check ok =
      sigil::measure::check("northern column", 422000 - 22000, 400000);
  EXPECT_TRUE(ok.pass);
  EXPECT_NE(ok.line().find("400000"), std::string::npos);
  EXPECT_NE(ok.line().find("PASS"), std::string::npos);
  EXPECT_EQ(ok.line().find("FAIL"), std::string::npos);

  const sigil::measure::Check bad =
      sigil::measure::check("Berezina", 20000 + 30000, 49000);
  EXPECT_FALSE(bad.pass);
  EXPECT_NE(bad.line().find("FAIL want 50000"), std::string::npos)
      << "a failing check must print what it wanted, or the plate says "
         "nothing an author can act on: "
      << bad.line();

  // A long label is not truncated — sigillum_aemeth.cpp:1719 documents four
  // checks silently losing their units to a feed column that clipped.
  const std::string wide =
      sigil::measure::check(std::string(80, 'L'), 1, 1).line(44);
  EXPECT_NE(wide.find(std::string(80, 'L')), std::string::npos);

  // Floats need a tolerance the STUDY chooses; there is no default.
  EXPECT_TRUE(sigil::measure::check("R", 257.972, 257.9715, 0.001).pass);
  EXPECT_FALSE(sigil::measure::check("R", 257.972, 257.9, 0.001).pass);
  EXPECT_NE(
      sigil::measure::check("R", 257.972, 257.9, 0.001).line().find("\xc2\xb1"),
      std::string::npos);

  EXPECT_TRUE(sigil::measure::check("winding", std::string_view("kCW"),
                                    std::string_view("kCW"))
                  .pass);
  EXPECT_FALSE(sigil::measure::check("closed", false).pass);
  EXPECT_TRUE(sigil::measure::check("closed", true).pass);

  const sigil::measure::Check checks[] = {ok, bad,
                                          sigil::measure::check("x", true)};
  EXPECT_EQ(sigil::measure::failures(checks), 1);
  EXPECT_EQ(sigil::measure::failures(
                std::span<const sigil::measure::Check>{checks, 1}),
            0);

  // report() lands the line in the feed under the style name the VERDICT
  // chose — that link is the whole primitive.
  feed::TextRing ring;
  test::report(ring, ok, "pass", "fail");
  test::report(ring, bad, "pass", "fail");
  ASSERT_EQ(ring.size(), 2u);
  EXPECT_EQ(ring.rows()[0].value.style, "pass");
  EXPECT_EQ(ring.rows()[1].value.style, "fail");
  EXPECT_NE(ring.rows()[1].value.text.bytes().find(u8"FAIL"),
            std::u8string::npos);

  // And it renders: a plate whose checks never reach the screen is the
  // situation this replaces.
  Host host(200, 60);
  feed::TextOptions style;
  style.styles = kit::tinted(nullptr, 9, {1, 1, 1, 1},
                             {{"pass", {0, 1, 0, 1}}, {"fail", {1, 0, 0, 1}}});
  host.composer.render(box()
                           .fill(Fill::color({0, 0, 0, 1}))
                           .children({feed::feed(ring, style).at({4, 4})}));
  host.frame();
  int inked = 0;
  for (int y = 0; y < 40; ++y)
    for (int x = 0; x < 200; ++x)
      if (host.pixel(x, y) != SK_ColorBLACK) ++inked;
  EXPECT_GT(inked, 50) << "the reported checks drew nothing";
}
