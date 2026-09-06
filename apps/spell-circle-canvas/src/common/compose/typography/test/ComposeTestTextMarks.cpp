// WHAT TRAVELS BESIDE THE TYPE: the tint preset over stacked tracks, the mark
// anchor that follows its unit through a reflow and stands at rest under a
// cascade, and a path run drawn outside the box it was measured in.
//
// The text binary's share of the content suites, one file per subject.

#include <memory>

#include "DressedTypeProbes.h"

// ---------------------------------------------------------------------------
// fx::tint — the colour reveal, and the inversion it hides

TEST(ComposeTextFx, TintRampsColorMulBetweenTheTwoColoursInTimeOrder) {
  // The arguments read in TIME ORDER while the mechanism runs the other
  // way: colorMul MULTIPLIES, so the element is set in the destination and
  // the effect divides down toward the origin. At t = 1 the multiplier must
  // therefore be white — anything else tints a line that has arrived.
  const SkColor4f pale{0.9f, 0.8f, 0.4f, 1};
  const SkColor4f sung{0.3f, 0.6f, 0.8f, 1};
  const TextEffect ramp = fx::tint(pale, sung);
  GlyphInfo glyph;
  sigil::core::noise::Mix64Stream rng(1);
  const GlyphMod start = ramp(glyph, 0.0f, rng);
  const GlyphMod end = ramp(glyph, 1.0f, rng);
  const GlyphMod middle = ramp(glyph, 0.5f, rng);

  // Origin: the multiplier that takes the DESTINATION to `pale`.
  EXPECT_NEAR(start.colorMul.fR * sung.fR, pale.fR, 1e-5f);
  EXPECT_NEAR(start.colorMul.fG * sung.fG, pale.fG, 1e-5f);
  EXPECT_NEAR(start.colorMul.fB * sung.fB, pale.fB, 1e-5f);
  // Destination: no tint at all.
  EXPECT_NEAR(end.colorMul.fR, 1.0f, 1e-5f);
  EXPECT_NEAR(end.colorMul.fG, 1.0f, 1e-5f);
  EXPECT_NEAR(end.colorMul.fB, 1.0f, 1e-5f);
  // And a monotone ramp between them on every channel, in whichever
  // direction that channel happens to run.
  for (auto lane : {&SkColor4f::fR, &SkColor4f::fG, &SkColor4f::fB}) {
    const float a = start.colorMul.*lane, b = middle.colorMul.*lane;
    EXPECT_GT((b - a) * (1.0f - a), 0.0f)
        << "the middle of the ramp is not between its ends";
  }
  // Alpha is left alone: a reveal that also fades is a separate track.
  EXPECT_FLOAT_EQ(start.colorMul.fA, 1.0f);
  // The value is comparable, which is what lets a re-described wipe prune.
  EXPECT_TRUE(fx::tint(pale, sung) == ramp);
  EXPECT_FALSE(fx::tint(sung, pale) == ramp);
  // A destination channel of zero cannot be departed from, and the ramp
  // says so by holding at 1 rather than dividing by nothing.
  const GlyphMod dark = fx::tint({1, 1, 1, 1}, {0, 0, 0, 1})(glyph, 0.0f, rng);
  EXPECT_FLOAT_EQ(dark.colorMul.fR, 1.0f);
}

TEST(ComposeTextFx, TintComposesWithAnotherTrackByMultiplying) {
  // Stacked tracks multiply their colour multipliers, so a tint under a
  // second tint is the product — not the last one to run. The letter is set
  // in white so the product is readable straight off the pixels.
  Host host(160, 120);
  host.composer.render(box().padding(8).child(
      text(u8"I", whiteStyle(64))
          .key("k")
          // Both tracks are AT REST (progress 0), where each contributes
          // its own origin: 0.5 on red and 0.5 on green.
          .fx({.effect = fx::tint({0.5f, 1, 1, 1}, {1, 1, 1, 1}),
               .stagger = {.eachMs = 0, .durationMs = 100},
               .progress = 0.0f})
          .fx({.effect = fx::tint({1, 0.5f, 1, 1}, {1, 1, 1, 1}),
               .stagger = {.eachMs = 0, .durationMs = 100},
               .progress = 0.0f})));
  host.frame();
  bool sawProduct = false;
  for (int y = 0; y < 120 && !sawProduct; ++y)
    for (int x = 0; x < 160; ++x) {
      const SkColor c = host.pixel(x, y);
      if (SkColorGetB(c) < 200) continue;  // not a glyph pixel
      // Half red AND half green, within the multiplier's quantization.
      if (std::abs((int)SkColorGetR(c) - 128) < 24 &&
          std::abs((int)SkColorGetG(c) - 128) < 24) {
        sawProduct = true;
        break;
      }
      EXPECT_FALSE(SkColorGetR(c) > 200 && SkColorGetG(c) > 200)
          << "one of the two tints never reached the glyph";
    }
  EXPECT_TRUE(sawProduct) << "the two tints did not multiply";
}

namespace {

/** The mark's rect, as the composer reports it. */
SkRect markRect(Host& host, std::string_view key) {
  const std::optional<SkRect> rect = host.composer.bounds(key);
  return rect.value_or(SkRect::MakeEmpty());
}

}  // namespace

TEST(ComposeTextFx, MarkPlacesAChildOnTheRectItsSelectorResolves) {
  // A mark's box IS the unit's rect when it says nothing about its own
  // placement — and it is the SAME rect the schedule read-back reports, so
  // a caret and a beat can never disagree about where a word is.
  Host host(400, 140);
  host.composer.render(box().padding(10).child(
      text(u8"ALPHA BETA GAMMA", whiteStyle(24))
          .key("line")
          .fx({.effect = fx::rise(4), .over = sigil::weave::Unit::Word})
          .mark(sigil::weave::sel::word(1), box().key("caret").fill(green()))));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("line", 0);
  ASSERT_EQ(beats.size(), 3u);
  const SkRect caret = markRect(host, "caret");
  EXPECT_NEAR(caret.left(), beats[1].rect.left(), 0.01f);
  EXPECT_NEAR(caret.top(), beats[1].rect.top(), 0.01f);
  EXPECT_NEAR(caret.width(), beats[1].rect.width(), 0.01f);
  EXPECT_NEAR(caret.height(), beats[1].rect.height(), 0.01f);
  EXPECT_GT(caret.width(), 1.0f) << "the mark collapsed to nothing";

  // Its own placement longhand is read INSIDE that rect, which is the whole
  // difference from a slot: a 2 px caret pinned to the unit's leading edge
  // and hanging below it.
  Host pinned(400, 140);
  pinned.composer.render(box().padding(10).child(
      text(u8"ALPHA BETA GAMMA", whiteStyle(24))
          .key("line")
          .mark(
              sigil::weave::sel::word(1),
              box().key("caret").left(0).top(pct(100)).width(2).height(9).fill(
                  green()))));
  pinned.frame();
  const SkRect tick = markRect(pinned, "caret");
  EXPECT_NEAR(tick.left(), caret.left(), 0.01f);
  EXPECT_NEAR(tick.top(), caret.bottom(), 0.01f)
      << "pct(100) of the unit's rect is its bottom edge";
  EXPECT_FLOAT_EQ(tick.width(), 2.0f);
}

TEST(ComposeTextFx, MarkFollowsItsUnitWhenTheTextReflows) {
  // The rect is read off the placement, so a narrower box that pushes the
  // word onto the next line takes the mark with it — the reason to anchor a
  // caret rather than compute one.
  const auto placeAt = [](float width) {
    Host host(400, 200);
    host.composer.render(
        box().padding(10).child(text(u8"ALPHA BETA GAMMA DELTA", whiteStyle(24))
                                    .key("line")
                                    .width(width)
                                    .mark(sigil::weave::sel::word(3),
                                          box().key("caret").fill(green()))));
    host.frame();
    return markRect(host, "caret");
  };
  const SkRect wide = placeAt(360);
  const SkRect narrow = placeAt(150);
  ASSERT_FALSE(wide.isEmpty());
  ASSERT_FALSE(narrow.isEmpty());
  EXPECT_GT(narrow.top(), wide.top())
      << "the word wrapped onto another line and the mark stayed behind";
}

TEST(ComposeTextFx, MarkStandsAtRestWhileACascadeDeviatesTheGlyphs) {
  // The rect is where the LAYOUT put the glyphs, not where a track has
  // thrown them: a deviation is per glyph and per track and several
  // compose, so there is no one place a moving unit "is". A mark that must
  // ride the motion reads beatsOf and drives its own transform.
  const auto placeAtProgress = [](float progress) {
    Host host(400, 200);
    host.composer.render(box().padding(10).child(
        text(u8"ALPHA BETA", whiteStyle(24))
            .key("line")
            .fx({.effect = fx::rise(40),
                 .stagger = {.eachMs = 0, .durationMs = 100},
                 .progress = progress})
            .mark(sigil::weave::sel::word(1),
                  box().key("caret").fill(green()))));
    host.frame();
    return markRect(host, "caret");
  };
  const SkRect early = placeAtProgress(0.0f);
  const SkRect settled = placeAtProgress(1.0f);
  ASSERT_FALSE(settled.isEmpty());
  EXPECT_NEAR(early.top(), settled.top(), 0.01f);
  EXPECT_NEAR(early.left(), settled.left(), 0.01f);
}

TEST(ComposeTextFx, MarkOnAPathRunStandsOnTheCurve) {
  // A path-laid run's marks stand on the CURVE the letters stand on: the
  // rect is the union of the advance boxes where the baseline placed them,
  // the same placement beatsOf reports — so a caret and a beat cannot
  // disagree on a ring any more than they can on a line. Resolved after
  // layout, because the curve resolves against the node's final box.
  Host host;
  host.composer.render(box().padding(10).child(
      text(u8"AROUND THE RING IT GOES", whiteStyle(18))
          .key("ring")
          .width(180)
          .height(180)
          .onPath({.path = geometry::shapes::circle()})
          .fx({.effect = fx::rise(4), .over = sigil::weave::Unit::Word})
          .mark(sigil::weave::sel::word(2), box().key("caret").fill(green()))));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("ring", 0);
  ASSERT_GT(beats.size(), 2u);
  const SkRect caret = markRect(host, "caret");
  ASSERT_FALSE(caret.isEmpty()) << "the mark placed nothing on the curve";
  EXPECT_NEAR(caret.left(), beats[2].rect.left(), 0.01f);
  EXPECT_NEAR(caret.top(), beats[2].rect.top(), 0.01f);
  EXPECT_NEAR(caret.width(), beats[2].rect.width(), 0.01f);
  EXPECT_NEAR(caret.height(), beats[2].rect.height(), 0.01f);
  // And it IS the curved placement, not the straight flow line the run
  // does not use: the same content laid straight puts the word somewhere
  // else entirely.
  Host straight;
  straight.composer.render(box().padding(10).child(
      text(u8"AROUND THE RING IT GOES", whiteStyle(18))
          .key("ring")
          .width(180)
          .height(180)
          .fx({.effect = fx::rise(4), .over = sigil::weave::Unit::Word})
          .mark(sigil::weave::sel::word(2), box().key("caret").fill(green()))));
  straight.frame();
  const SkRect flow = markRect(straight, "caret");
  EXPECT_TRUE(std::abs(caret.left() - flow.left()) > 1.0f ||
              std::abs(caret.top() - flow.top()) > 1.0f)
      << "the curved rect matched the straight one — the probe proves "
         "nothing at this size";
}

TEST(ComposeTextFx, MarkResolvingNothingPlacesNothing) {
  // The silent-no-op family's terms, with the warning that goes with them:
  // a style name no run carries selects nothing, and a mark on nothing must
  // draw nothing rather than land at the text node's origin.
  Host host(300, 140);
  host.composer.render(box().padding(10).child(
      text(u8"ALPHA BETA", whiteStyle(24))
          .key("line")
          .mark(sel::style("nobody"),
                box().key("caret").width(30).height(30).fill(green()))));
  host.frame();
  EXPECT_TRUE(markRect(host, "caret").isEmpty())
      << "a mark on nothing took a box anyway";
  int greens = 0;
  for (int y = 0; y < 140; ++y)
    for (int x = 0; x < 300; ++x)
      if (host.pixel(x, y) == SK_ColorGREEN) ++greens;
  EXPECT_EQ(greens, 0) << "a mark on nothing drew something";
}

TEST(ComposeTextFx, MarkPrunesAndReResolvesWhenItMoves) {
  // A mark is a comparable selector plus the key of the child it anchors,
  // so a re-described identical mark must prune — and one pointed at a
  // different word must not, or the caret keeps the rect it had.
  Host host(400, 140);
  const auto describe = [](uint32_t word) {
    return box().padding(10).child(text(u8"ALPHA BETA GAMMA", whiteStyle(24))
                                       .key("line")
                                       .mark(sigil::weave::sel::word(word),
                                             box().key("caret").fill(green())));
  };
  host.composer.render(describe(0));
  host.frame();
  const SkRect first = markRect(host, "caret");
  host.composer.render(describe(0));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an unchanged mark list did not prune";
  EXPECT_NEAR(markRect(host, "caret").left(), first.left(), 0.01f);

  host.composer.render(describe(2));
  host.frame();
  EXPECT_GT(markRect(host, "caret").left(), first.left() + 1.0f)
      << "the mark kept the rect the previous selector resolved";
}

TEST(ComposeTextFx, MarkIsNotASlotAndReservesNoSpaceInTheFlow) {
  // The distinction the header states: a slot's box is woven into the line
  // and the type after it starts further along; a mark is placed on a line
  // laid out as though it were not there.
  const auto widthOf = [](Element leaf) {
    Host host(400, 140);
    host.composer.render(box().padding(10).child(std::move(leaf).key("line")));
    host.frame();
    return host.composer.bounds("line").value_or(SkRect::MakeEmpty()).width();
  };
  const float bare = widthOf(text(u8"ALPHA BETA", whiteStyle(24)));
  const float marked =
      widthOf(text(u8"ALPHA BETA", whiteStyle(24))
                  .mark(sigil::weave::sel::word(0),
                        box().key("m").width(40).fill(green())));
  EXPECT_NEAR(marked, bare, 0.01f) << "the mark reserved space in the flow";
}

namespace {

/** A baseline that deliberately leaves the node's box: a ring centred well
 *  to the right of it. A comparable scheme rather than a raw callable, so
 *  the node can still prune and the cache under test is really reached. */
struct RingBesideTheBox {
  SkPath path(SkSize) const {
    SkPathBuilder builder;
    builder.addCircle(200, 100, 60);
    return builder.detach();
  }
  bool operator==(const RingBesideTheBox&) const = default;
};

/** How many pixels of `host` right of `fromX` carry ink. */
int litPixelsRightOf(Host& host, int fromX, int width, int height) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  host.surface->readPixels(bitmap.pixmap(), 0, 0);
  int lit = 0;
  for (int y = 0; y < height; ++y)
    for (int x = fromX; x < width; ++x)
      if (SkColorGetR(*bitmap.getAddr32(x, y)) > 40) ++lit;
  return lit;
}

}  // namespace

TEST(ComposeCache, TextOnAPathOutsideItsBoxSurvivesTheCull) {
  // A TextPath baseline resolves against the node's box but is not bounded
  // by it: this ring sits entirely beside the box, and `offset` would ride
  // the type further off it again. If the paint bounds stop at the box, the
  // bake surface is sized to the box and every glyph is truncated with no
  // diagnostic — the failure `bleed()` and `reach()` exist to prevent.
  // Cache::None is the ground truth: it re-paints straight to the canvas
  // with no surface to truncate against.
  const auto plate = [](Cache cache) {
    auto host = std::make_unique<Host>(300, 200);
    host->composer.render(box().child(text(u8"CIRCVMFERENTIA", whiteStyle(18))
                                          .width(100)
                                          .height(100)
                                          .onPath({.path = RingBesideTheBox{}})
                                          .cache(cache)
                                          .key("ring")));
    for (int i = 0; i < 4; ++i) host->frame(1.0 / 60.0);
    return host;
  };
  std::unique_ptr<Host> truth = plate(Cache::None);
  const int expected = litPixelsRightOf(*truth, 110, 300, 200);
  ASSERT_GT(expected, 40) << "the run never left the box: nothing is proven";

  std::unique_ptr<Host> baked = plate(Cache::Texture);
  EXPECT_GT(litPixelsRightOf(*baked, 110, 300, 200), expected * 9 / 10)
      << "the baked plate lost the glyphs the baseline put outside the box";
}

TEST(ComposeShapeValues, TextOnAComparableBaselinePrunes) {
  // A TextPath's baseline is a Shape, so TextPath compares and a curved run
  // prunes. Without this every radial label in a figure re-records on every
  // render(), and a ring of labels is exactly where a figure has the most of
  // them.
  Host host(240, 240);
  auto ring = [](float at) {
    return text(u8"HHHHHHHHHH", whiteStyle(22))
        .width(240)
        .height(240)
        .absolute()
        .left(0)
        .top(0)
        .onPath({.path = geometry::shapes::arc(180.0f, 359.9f),
                 .at = at,
                 .align = TextPath::Align::Center});
  };
  host.composer.render(box().child(ring(0.25f)));
  host.frame();
  host.composer.render(box().child(ring(0.25f)));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical curved run re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  // …and the equality is honest: moving `at` IS a change. Omit `at` from
  // textEqual and a run that slides along its baseline compares equal to
  // where it was, prunes, and keeps the OLD placement forever with no
  // diagnostic — so this half of the case is the load-bearing one.
  host.composer.render(box().child(ring(0.75f)));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
}
