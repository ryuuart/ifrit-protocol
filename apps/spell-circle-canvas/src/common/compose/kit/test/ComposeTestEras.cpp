// The era looks, each part on its own: the chrome body, its horizon
// sliver and the keyline the bundle stands them in; the gel body, its
// lens and the halo an orb reserves; the two chrome type ramps; and the
// rules cut to what a block of type actually occupies.

#include <include/core/SkColor.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilcompose/kit/Gel.h>
#include <sigilcompose/kit/Gloss.h>
#include <sigilcompose/kit/Typeset.h>

#include <string>
#include <vector>

#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

namespace {

/** How bright a pixel is, in the only terms a look is judged in here:
 *  more light, less light. */
int lum(SkColor c) { return SkColorGetR(c) + SkColorGetG(c) + SkColorGetB(c); }

/** A bare box of `w`×`h` at (20, 20) in a 200×140 host, wearing one
 *  decoration and nothing else — no fill, so what is measured is what the
 *  decoration painted. */
Element panel(Decoration what, float w = 160, float h = 100) {
  return box().padding(20).child(
      box().width(w).height(h).foreground(std::move(what)));
}

}  // namespace

// ---------------------------------------------------------------------------
// Chrome

TEST(KitEras, TheChromeBodyFoldsAtItsHorizonRatherThanRunningOneWay) {
  Host host(200, 140);
  host.composer.render(panel(Decoration(kit::ChromeBody{})));
  host.frame();
  // The ramp is a FOLD, not a gradient: it darkens down to the horizon and
  // lightens again below it, which is what reads as a reflected sky. So
  // the darkest row of the column is the horizon's, and both ends of the
  // shape are far brighter than it.
  const int top = 20, height = 100;
  const int horizon =
      top + (int)((float)height * sigil::material::kit::kChromeHorizonFrac);
  int darkest = 1 << 20, darkestY = top;
  for (int y = top + 2; y < top + height - 2; ++y) {
    const int v = lum(host.pixel(100, y));
    if (v < darkest) {
      darkest = v;
      darkestY = y;
    }
  }
  EXPECT_NEAR(darkestY, horizon, 4);
  EXPECT_GT(lum(host.pixel(100, top + 4)), darkest + 150);
  EXPECT_GT(lum(host.pixel(100, top + height - 4)), darkest + 150);
}

TEST(KitEras, TheSliverLightsTheHorizonAndFadesAtBothEnds) {
  Host host(200, 140);
  host.composer.render(panel(Decoration(kit::ChromeSliver{})));
  host.frame();
  const int left = 20, right = 180;
  const int horizon =
      20 + (int)(100.0f * sigil::material::kit::kChromeHorizonFrac);
  const int middle = lum(host.pixel((left + right) / 2, horizon));
  const int end = lum(host.pixel(left + 2, horizon));
  EXPECT_GT(middle, 0);
  // A hard-ended band reads as a strikethrough, so the falloff is part of
  // the value: the band is dimmer at its ends than at its middle.
  EXPECT_GT(middle, end);
  // …and the 1 px top edge is lit whatever the horizon does.
  EXPECT_GT(lum(host.pixel(100, 20)), 0);
}

TEST(KitEras, TheKeylineIsStrokedOutsideTheSilhouetteAndZeroWidthDropsIt) {
  auto barWith = [](kit::ChromeOptions opts) {
    return box().padding(20).child(
        box().width(160).height(100).style(kit::y2kChrome(opts)));
  };
  Host wide(200, 140), none(200, 140);
  wide.composer.render(barWith({.keylineWidth = 4.0f}));
  wide.frame();
  none.composer.render(barWith({.keylineWidth = 0.0f}));
  none.frame();
  // One pixel outside the box's own edge: the keyline is the only thing
  // the bundle draws there.
  EXPECT_NE(wide.pixel(19, 70), SK_ColorBLACK);
  EXPECT_EQ(none.pixel(19, 70), SK_ColorBLACK);
}

// ---------------------------------------------------------------------------
// Gel

TEST(KitEras, TheGelBodyLightsItsBottomEdgeAndTheHaloIsWhatItReserves) {
  Host host(200, 140);
  const kit::AquaBody body{hexColor(0x1E8FFF), {}};
  host.composer.render(panel(Decoration(body)));
  host.frame();
  // Light from below: the bottom of the surface beats its middle.
  EXPECT_GT(lum(host.pixel(100, 116)), lum(host.pixel(100, 70)));

  // The halo is the only thing that escapes the box, so it is the whole
  // of what the cull is asked to reserve — and turning it off asks for
  // nothing.
  kit::AquaGelOptions dark;
  dark.halo = false;
  EXPECT_GT(body.bleed(), 0.0f);
  const kit::AquaBody unhaloed{hexColor(0x1E8FFF), dark};
  EXPECT_FLOAT_EQ(unhaloed.bleed(), 0.0f);
}

TEST(KitEras, TheLensRampFadesToItsEndAndPaintsNothingBelowIt) {
  Host host(200, 140);
  host.composer.render(panel(Decoration(kit::AquaGloss{})));
  host.frame();
  const int top = 20, height = 100;
  const int lensBottom = top + (int)(0.52f * (float)height);
  EXPECT_GT(lum(host.pixel(100, top + 6)), 0);
  // The ramp reaches its bottom value before the lens's own arc, so the
  // highlight ends in a fade and leaves no outline.
  EXPECT_GT(lum(host.pixel(100, top + 6)),
            lum(host.pixel(100, lensBottom - 6)));
  EXPECT_EQ(host.pixel(100, lensBottom + 20), SK_ColorBLACK);
}

TEST(KitEras, AnOrbReservesItsHaloFromTheDiameterItWasGiven) {
  const LayerStyle small = kit::aquaOrb(hexColor(0x1E8FFF), 64.0f);
  const LayerStyle large = kit::aquaOrb(hexColor(0x1E8FFF), 256.0f);
  ASSERT_FALSE(small.under.empty());
  ASSERT_FALSE(large.under.empty());
  // The halo reaches beyond the box by a fraction of the height the caller
  // declared, and nothing at paint time can measure it earlier.
  EXPECT_GT(large.under.front().bleed(), small.under.front().bleed());
  EXPECT_GT(small.under.front().bleed(), 0.0f);
}

// ---------------------------------------------------------------------------
// The chrome type ramps

TEST(KitEras, TheChromeTypeRampsSitInUnitSpaceSoTheHorizonHoldsAtAnySize) {
  // The same word at two sizes: a unit-space ramp crosses the capitals at
  // the same fraction of the cap band whatever the size, so the light half
  // of the ramp is above the dark half in both.
  auto lit = [](float size, const material::skia::Paint& ramp) {
    Host host(300, 200);
    host.composer.render(box().padding(20).child(
        text(u8"HH", whiteStyle(size)).key("word").textFill(ramp)));
    host.frame();
    const SkRect at = require(host.composer.bounds("word"));
    const auto band = [&](float frac) {
      int best = 0;
      const int y = (int)(at.top() + at.height() * frac);
      for (int x = (int)at.left(); x < (int)at.right(); ++x)
        best = std::max(best, lum(host.pixel(x, y)));
      return best;
    };
    return std::pair<int, int>{band(0.35f), band(0.62f)};
  };
  for (const material::skia::Paint& ramp :
       {kit::sunsetChromeType(), kit::silverChromeType()}) {
    const auto smallWord = lit(40, ramp);
    const auto largeWord = lit(80, ramp);
    EXPECT_NE(smallWord.first, smallWord.second);
    // Same side of the horizon at both sizes: the sign of the step is the
    // size-independent claim, not the exact tone.
    EXPECT_EQ(smallWord.first > smallWord.second,
              largeWord.first > largeWord.second);
  }
}

// ---------------------------------------------------------------------------
// Block rules

TEST(KitEras, ARuleStandsWhereTheBlockIsAndTheThreeArmsDifferInWhere) {
  Host host(300, 200);
  const auto tree = [](Element overlay) {
    return box()
        .padding(20)
        .child(text(u8"EPIGRAPH", whiteStyle(20)).key("epigraph"))
        .child(std::move(overlay).absolute().inset(0));
  };
  host.composer.render(tree(positioned()));
  host.frame();
  const SkRect block = require(host.composer.bounds("epigraph"));

  const auto ruleAt = [&](kit::BlockRule::Where where) {
    Host run(300, 200);
    run.composer.render(tree(positioned()));
    run.frame();
    run.composer.render(
        tree(kit::rules(run.composer, "epigraph",
                        sigil::weave::sel::each(sigil::weave::Unit::Line),
                        {.where = where,
                         .thickness = 3,
                         .gap = 4,
                         .bleed = 4,
                         .colour = {1, 0, 0, 1}})));
    run.frame();
    const std::string key = where == kit::BlockRule::Where::Behind
                                ? "epigraph-shade"
                                : "epigraph-rule";
    return require(run.composer.bounds(key));
  };

  const SkRect above = ruleAt(kit::BlockRule::Where::Above);
  const SkRect below = ruleAt(kit::BlockRule::Where::Below);
  const SkRect behind = ruleAt(kit::BlockRule::Where::Behind);
  EXPECT_LT(above.bottom(), block.top() + 1);
  EXPECT_GT(below.top(), block.bottom() - 1);
  // Behind covers the run of lines rather than standing off it, and is
  // grown by its bleed at both ends.
  EXPECT_LT(behind.top(), block.top());
  EXPECT_GT(behind.bottom(), block.bottom());
  // Every arm is as wide as the LINES, not as wide as the box holding
  // them.
  EXPECT_LT(above.width(), 260);
  EXPECT_NEAR(above.width(), behind.width(), 1.0f);
}
