// THE COLOUR TERMS a deviation carries: the multiplier, the additive and the
// screen term across stacked tracks and inside a keys table, the snap ladder
// that bounds them, and the two paths — flat colour and filter — agreeing.
//
// The text binary's share of the content suites, one file per subject.

#include "DressedTypeProbes.h"

TEST(ComposeTextFx, ColorMulTintsEveryPassOfADressedGlyph) {
  // The multiplier is a statement about the GLYPH, not about its fill: it
  // has to reach the underlays and overlays a span was styled with too.
  // Reaching only the foreground would leave a tinted letter wearing its
  // old drop shadow, which is the bug this test exists to name.
  sigil::weave::TextStyle shadowed = whiteStyle(52);
  shadowed.paint.addUnderlay(sigil::weave::PaintLayer(SK_ColorRED, {16, 0}));

  const auto render = [&](Host& host, std::string key, SkColor4f tint) {
    GlyphModifier mod;
    mod.colorMultiplier = tint;
    host.composer.render(box().padding(10).child(
        text(u8"I", shadowed)
            .key("k")
            .fx({.effect = fixed(std::move(key), mod)})));
    host.frame();
  };
  const auto count = [](Host& host, auto&& predicate) {
    int hits = 0;
    for (int y = 0; y < 140; ++y)
      for (int x = 0; x < 140; ++x)
        if (predicate(host.pixel(x, y))) ++hits;
    return hits;
  };
  const auto reddish = [](SkColor c) {
    return SkColorGetR(c) > 150 && SkColorGetG(c) < 80 && SkColorGetB(c) < 80;
  };
  const auto greenish = [](SkColor c) {
    return SkColorGetG(c) > 150 && SkColorGetR(c) < 80 && SkColorGetB(c) < 80;
  };
  const auto whitish = [](SkColor c) {
    return SkColorGetR(c) > 200 && SkColorGetG(c) > 200 && SkColorGetB(c) > 200;
  };

  Host plain(140, 140);
  render(plain, "plain", {1, 1, 1, 1});
  ASSERT_GT(count(plain, reddish), 10) << "the underlay pass never painted";
  ASSERT_GT(count(plain, whitish), 10) << "the foreground pass never painted";

  Host tinted(140, 140);
  render(tinted, "greenOnly", {0, 1, 0, 1});
  EXPECT_EQ(count(tinted, reddish), 0)
      << "the underlay kept its red under a tint that multiplies red by zero";
  EXPECT_EQ(count(tinted, whitish), 0) << "the foreground kept its white";
  EXPECT_GT(count(tinted, greenish), 10)
      << "the tint painted nothing at all — the glyph vanished instead";
}

namespace {

/** A style whose ink is one flat grey — headroom in every channel, so an
 *  added or screened term has somewhere to go. */
sigil::weave::TextStyle greyStyle(float size, float level) {
  sigil::weave::TextStyle s = styleAt(size);
  s.paint.foreground.setColor4f({level, level, level, 1.0f}, nullptr);
  return s;
}

/** One glyph under one or two colour-term tracks, drawn and read back. */
std::vector<uint8_t> renderColorTracks(
    std::vector<std::pair<std::string, GlyphModifier>> tracks,
    float greyLevel = 0.25f, bool continuous = false) {
  Host host(140, 140);
  Element leaf = text(u8"I", greyStyle(52, greyLevel)).key("k");
  for (auto& [key, mod] : tracks)
    leaf.fx({.effect = fixed(key, mod), .continuous = continuous});
  host.composer.render(box().padding(10).child(std::move(leaf)));
  host.frame();
  return surfaceBytes(host, 140, 140);
}

}  // namespace

TEST(ComposeTextFx, ColorAddAddsAcrossTracksAndClampsAtTheDraw) {
  // One flash brightens: an added half-red over a quarter-grey glyph reads
  // red-forward where the untouched glyph reads flat grey.
  GlyphModifier addHalfRed;
  addHalfRed.colorAdd = {0.5f, 0, 0, 0};
  const std::vector<uint8_t> flashed =
      renderColorTracks({{"addHalfRed", addHalfRed}});
  const std::vector<uint8_t> plain = renderColorTracks({});
  EXPECT_NE(flashed, plain) << "an added term changed nothing";

  // TWO tracks ADD — 0.5 + 0.75 — and the sum clamps ONCE at the draw, so
  // the pair is byte-identical to a single track adding full red. Clamping
  // per track instead would land at 1.25-before-snap only by accident; the
  // law is sum-then-clamp, and byte identity against the saturated single
  // track is that law observed.
  GlyphModifier addMoreRed;
  addMoreRed.colorAdd = {0.75f, 0, 0, 0};
  GlyphModifier addFullRed;
  addFullRed.colorAdd = {1.0f, 0, 0, 0};
  EXPECT_EQ(renderColorTracks(
                {{"addHalfRed", addHalfRed}, {"addMoreRed", addMoreRed}}),
            renderColorTracks({{"addFullRed", addFullRed}}))
      << "0.5 + 0.75 across two tracks must draw as a clamped 1.0";
}

TEST(ComposeTextFx, ColorScreenScreensCommutativelyAcrossTracks) {
  GlyphModifier screenRed;
  screenRed.colorScreen = {0.5f, 0, 0, 0};
  GlyphModifier screenGreen;
  screenGreen.colorScreen = {0, 0.5f, 0, 0};
  // Order-free: 1 − (1−a)(1−b) reads the same both ways, so two glow
  // tracks land identically whichever is declared first.
  EXPECT_EQ(renderColorTracks({{"sR", screenRed}, {"sG", screenGreen}}),
            renderColorTracks({{"sG", screenGreen}, {"sR", screenRed}}))
      << "screen composition depended on track order";
  // …and the arithmetic is the screen blend itself: half screened twice is
  // 1 − 0.5·0.5 = 0.75 screened once. Both sides land on the snap ladder
  // (16/32 and 24/32), so the compare is exact.
  GlyphModifier screenThreeQuarters;
  screenThreeQuarters.colorScreen = {0.75f, 0, 0, 0};
  EXPECT_EQ(renderColorTracks({{"sR", screenRed}, {"sR2", screenRed}}),
            renderColorTracks({{"s34", screenThreeQuarters}}))
      << "two half screens must compose to one three-quarter screen";
}

TEST(ComposeTextFx, TheColourTermsLerpComponentwiseInAKeysTable) {
  // A keys segment interpolates the two terms channel by channel, so the
  // table's midpoint draws exactly as a constant half-strength term does.
  // Local t is pinned at 0.5 by a zero-spread cascade and progress 0.5;
  // 0.5 sits on the snap ladder, so the compare is byte-exact.
  const auto renderKeysAt = [](TextEffect effect) {
    Host host(140, 140);
    host.composer.render(
        box().padding(10).child(text(u8"I", greyStyle(52, 0.25f))
                                    .key("k")
                                    .fx({.effect = std::move(effect),
                                         .stagger = {.eachMs = 0},
                                         .progress = 0.5f})));
    host.frame();
    return surfaceBytes(host, 140, 140);
  };
  GlyphModifier fullAdd;
  fullAdd.colorAdd = {1.0f, 0, 0, 0};
  GlyphModifier halfAdd;
  halfAdd.colorAdd = {0.5f, 0, 0, 0};
  EXPECT_EQ(renderKeysAt(fx::keys({{0.0f, {}}, {1.0f, fullAdd}})),
            renderKeysAt(fixed("halfAddK", halfAdd)))
      << "colorAdd did not lerp componentwise across a keys segment";
  GlyphModifier fullScreen;
  fullScreen.colorScreen = {0, 1.0f, 0, 0};
  GlyphModifier halfScreen;
  halfScreen.colorScreen = {0, 0.5f, 0, 0};
  EXPECT_EQ(renderKeysAt(fx::keys({{0.0f, {}}, {1.0f, fullScreen}})),
            renderKeysAt(fixed("halfScreenK", halfScreen)))
      << "colorScreen did not lerp componentwise across a keys segment";
  // The terms are part of a table's identity: two tables differing only in
  // a colour term are two different effects, and must not prune onto each
  // other.
  EXPECT_FALSE(fx::keys({{0.0f, {}}, {1.0f, fullAdd}}) ==
               fx::keys({{0.0f, {}}, {1.0f, halfAdd}}));
  EXPECT_FALSE(fx::keys({{0.0f, {}}, {1.0f, fullScreen}}) ==
               fx::keys({{0.0f, {}}, {1.0f, halfScreen}}));
}

TEST(ComposeTextFx, NeutralColourTermsKeepTheFastPathByteIdentical) {
  // A deviation that spells the neutral terms outright must draw the very
  // bytes one that never mentions them draws: neutral means the untouched
  // source paint, not an identity-shaped filter over it.
  GlyphModifier spelled;
  spelled.dy = -4.0f;
  spelled.colorAdd = {0, 0, 0, 0};
  spelled.colorScreen = {0, 0, 0, 0};
  GlyphModifier silent;
  silent.dy = -4.0f;
  EXPECT_EQ(renderColorTracks({{"spelledNeutral", spelled}}),
            renderColorTracks({{"silentNeutral", silent}}));
}

TEST(ComposeTextFx, TheSnapLadderBoundsTheColourTermsAndContinuousLiftsIt) {
  // Below half a ladder step the snapped term rounds to neutral — that
  // rounding is what bounds the memoized filter population — and
  // Track::continuous lifts it, letting the raw value through at the cost
  // the opt-out names.
  GlyphModifier faint;
  faint.colorAdd = {0.01f, 0.01f, 0.01f, 0};
  EXPECT_EQ(renderColorTracks({{"faintAdd", faint}}, 0.5f),
            renderColorTracks({}, 0.5f))
      << "a term below half a snap step must round to neutral";
  EXPECT_NE(renderColorTracks({{"faintAdd", faint}}, 0.5f,
                              /*continuous=*/true),
            renderColorTracks({}, 0.5f, /*continuous=*/true))
      << "continuous must lift the snap and let the faint term through";
}

TEST(ComposeTextFx, TheFilterPathAgreesWithTheFlatColourPath) {
  // A pass whose colour is decided by a shader takes the colour terms as a
  // memoized matrix filter; a flat pass takes them in its colour. Same
  // arithmetic by contract — multiply, add, clamp, then screen — so the
  // two paths must land within rounding of each other.
  GlyphModifier mod;
  mod.colorMultiplier = {0.5f, 1.0f, 1.0f, 1.0f};
  mod.colorAdd = {0.25f, 0.25f, 0, 0};
  mod.colorScreen = {0, 0.5f, 0.5f, 0};
  const auto render = [&](Host& host, bool shaderFill) {
    sigil::weave::TextStyle style = greyStyle(52, 0.5f);
    if (shaderFill)
      style.paint.foreground.setShader(
          SkShaders::Color({0.5f, 0.5f, 0.5f, 1.0f}, nullptr));
    host.composer.render(box().padding(10).child(
        text(u8"I", style).key("k").fx({.effect = fixed("both", mod)})));
    host.frame();
  };
  Host flat(140, 140);
  render(flat, false);
  Host filtered(140, 140);
  render(filtered, true);
  int worst = 0;
  for (int y = 0; y < 140; ++y)
    for (int x = 0; x < 140; ++x) {
      const SkColor a = flat.pixel(x, y);
      const SkColor b = filtered.pixel(x, y);
      worst =
          std::max({worst, std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)),
                    std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)),
                    std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b))});
    }
  EXPECT_LE(worst, 2)
      << "the matrix-filter path and the flat-colour path disagree about "
         "the colour-term arithmetic";
}
