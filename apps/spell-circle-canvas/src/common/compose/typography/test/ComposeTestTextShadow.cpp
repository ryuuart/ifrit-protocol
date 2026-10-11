// A glyph's own shadow: `textShadow` casts it beneath each glyph, so it
// fades, tints and deforms with that glyph and costs what the glyphs
// cost, whatever the box they stand in.

#include <chrono>

#include "DressedTypeProbes.h"

namespace {

constexpr material::Color kGreen = {0, 1, 0, 1};

/** An inheriting leaf set white in the instrument, inside a root that
 *  states the type. */
Element shadowedPage(Text leaf) {
  return box()
      .font({.face = sigil::test::instrument::sans(), .size = 48})
      .ink(material::Color{1, 1, 1, 1})
      .children({std::move(leaf).key("t").absolute().left(30.0f).top(30.0f)});
}

/** The green the glow leaves @p reach pixels left of the first inked
 *  column of the word's middle row, where no glyph paints. */
int glowBeside(Host& host, int reach) {
  const auto bounds = host.composer.bounds("t");
  if (!bounds) return -1;
  const int row = (int)(bounds->top() + bounds->height() * 0.6f);
  for (int x = (int)bounds->left(); x < (int)bounds->right(); ++x)
    if (SkColorGetR(host.pixel(x, row)) > 200)
      return (int)SkColorGetG(host.pixel(x - reach, row));
  return -1;
}

}  // namespace

TEST(ComposeTextShadow, AGlowFadesWithItsGlyph) {
  // The glow is drawn with its glyph, so a track that fades the glyph to a
  // quarter fades its glow to a quarter: over black, a quarter of the
  // light at every point beside it.
  const auto render = [](Host& host, float alpha) {
    host.composer.render(
        shadowedPage(text(u8"I")
                         .textShadow({.blur = 6, .color = kGreen})
                         .textFx({.effect = fixed("fade", {.alpha = alpha})})));
    host.frame();
  };
  Host lit(200, 140), faded(200, 140);
  render(lit, 1.0f);
  render(faded, 0.25f);
  const int full = glowBeside(lit, 4);
  ASSERT_GT(full, 40) << "no glow stands beside the glyph";
  // The faded glyph is found by its own ink too, a quarter as bright.
  const auto bounds = faded.composer.bounds("t");
  ASSERT_TRUE(bounds.has_value());
  const int row = (int)(bounds->top() + bounds->height() * 0.6f);
  int quarter = -1;
  for (int x = (int)bounds->left(); x < (int)bounds->right(); ++x)
    if (SkColorGetR(faded.pixel(x, row)) > 40) {
      quarter = (int)SkColorGetG(faded.pixel(x - 4, row));
      break;
    }
  EXPECT_NEAR(quarter, full * 0.25f, 2.0f);
}

TEST(ComposeTextShadow, AGlowFollowsItsGlyphsDeformation) {
  // A track that doubles a glyph's width doubles its glow's: the glow is
  // the glyph's own pass, drawn under the glyph's own matrix.
  const auto glowWidth = [](float scaleX) {
    Host host(320, 140);
    host.composer.render(shadowedPage(
        text(u8"I")
            .textShadow({.blur = 3, .color = kGreen})
            .textFx({.effect = fixed("widen", {.scaleX = scaleX})})));
    host.frame();
    int left = 320, right = -1;
    for (int y = 0; y < 140; ++y)
      for (int x = 0; x < 320; ++x)
        if (SkColorGetG(host.pixel(x, y)) > 60 &&
            SkColorGetR(host.pixel(x, y)) < 60) {
          left = std::min(left, x);
          right = std::max(right, x);
        }
    return right - left;
  };
  const int rest = glowWidth(1.0f);
  const int doubled = glowWidth(2.0f);
  ASSERT_GT(rest, 0);
  EXPECT_GT(doubled, rest + 8) << "the glow stayed at the glyph's rest width";
}

TEST(ComposeTextShadow, AShadowNamingNoColourTakesEachGlyphsTint) {
  // Unset, the shadow is the colour its glyph is painted in, after the
  // glyph's own tint: a track that holds one glyph white and darkens the
  // other leaves a lit glow beside the first and none beside the second.
  Host host(320, 140);
  host.composer.render(shadowedPage(
      text(u8"I I")
          .textShadow({.blur = 5})
          .textFx(
              {.where = sigil::weave::selectors::word(1),
               .effect = fixed("dark", {.colorMultiplier = {0, 0, 0, 1}})})));
  host.frame();
  const auto bounds = host.composer.bounds("t");
  ASSERT_TRUE(bounds.has_value());
  const int row = (int)(bounds->top() + bounds->height() * 0.6f);
  std::vector<int> stems;
  bool inStem = false;
  for (int x = (int)bounds->left(); x < (int)bounds->right(); ++x) {
    const bool lit = SkColorGetR(host.pixel(x, row)) > 200;
    if (lit && !inStem) stems.push_back(x);
    inStem = lit;
  }
  ASSERT_EQ(stems.size(), 1u) << "the darkened glyph should paint no light";
  EXPECT_GT(SkColorGetR(host.pixel(stems[0] - 3, row)), 40u);
}

TEST(ComposeTextShadow, AChangingGlowCostsWhatItsGlyphsCostNotItsBox) {
  // A track fading every glyph on every frame repaints the glyphs and their
  // glows. Nothing about that reads the leaf's box, so a leaf filling a
  // screen costs what a leaf the size of its word costs.
  const auto perFrame = [](float width, float height) {
    Host host(1300, 800);
    const auto describe = [&](float progress) {
      return box()
          .font({.face = sigil::test::instrument::sans(), .size = 24})
          .ink(material::Color{1, 1, 1, 1})
          .children({text(u8"RAIN")
                         .width(width)
                         .height(height)
                         .textShadow({.blur = 9, .color = kGreen})
                         .textFx({.effect = textFx::tween(
                                      {.from = GlyphModifier{.alpha = 0.0f}}),
                                  .progress = progress})});
    };
    double fastest = 1e9;
    for (int frame = 0; frame < 24; ++frame) {
      host.composer.render(describe(0.2f + 0.03f * (float)frame));
      const auto start = std::chrono::steady_clock::now();
      host.frame();
      const std::chrono::duration<double, std::milli> spent =
          std::chrono::steady_clock::now() - start;
      fastest = std::min(fastest, spent.count());
    }
    return fastest;
  };
  const double small = perFrame(64, 64);
  const double large = perFrame(1280, 780);
  EXPECT_LT(large, small * 3.0 + 0.5)
      << "a changing glow costs " << large << " ms in a screen-sized leaf and "
      << small << " ms in a word-sized one";
}

TEST(ComposeTextShadow, AnEchoNamingNoColourFollowsTheSheetsInk) {
  // The ink in force is read from a custom property; an echo of the text
  // that names no colour is painted in it, twelve pixels along.
  const auto redSpan = [](bool echoed) {
    Text letter = text(u8"I").key("t").absolute().left(30.0f).top(30.0f);
    if (echoed) letter.textShadow({.offset = {12, 0}});
    Host host(240, 140);
    host.composer.render(
        box()
            .var("accent", material::Color{1, 0, 0, 1})
            .ink(sigil::compose::var("accent"))
            .font({.face = sigil::test::instrument::sans(), .size = 48})
            .children({std::move(letter)}));
    host.frame();
    const auto bounds = host.composer.bounds("t");
    if (!bounds) return -1;
    const int row = (int)(bounds->top() + bounds->height() * 0.6f);
    int first = -1, last = -1;
    for (int x = 0; x < 240; ++x)
      if (host.pixel(x, row) == SK_ColorRED) {
        if (first < 0) first = x;
        last = x;
      }
    return first < 0 ? -1 : last - first;
  };
  const int plain = redSpan(false);
  ASSERT_GT(plain, 0) << "the letter was not set in the property's red";
  EXPECT_EQ(redSpan(true), plain + 12)
      << "the echo is not the letter's own red, twelve pixels along";
}

TEST(ComposeTextShadow, ARestatedShadowRepaintsAndAnEqualOneHolds) {
  // The shadow is the font's own field, so the tree described again with
  // another shadow repaints in it, and described again with the same one
  // paints exactly as it did.
  const auto page = [](float blur) {
    return shadowedPage(text(u8"I").textShadow(
        {.offset = {6, 0}, .blur = blur, .color = kGreen}));
  };
  Host host(200, 140), fresh(200, 140);
  fresh.composer.render(page(4));
  fresh.frame();
  host.composer.render(page(0));
  host.frame();
  ASSERT_FALSE(identicalPixels(host, fresh, 200, 140));
  host.composer.render(page(4));
  host.frame();
  EXPECT_TRUE(identicalPixels(host, fresh, 200, 140))
      << "the restated shadow did not repaint as a fresh one paints";
  host.composer.render(page(4));
  host.frame();
  EXPECT_TRUE(identicalPixels(host, fresh, 200, 140));
}
