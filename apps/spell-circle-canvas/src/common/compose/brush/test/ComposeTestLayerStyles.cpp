// The layer styles on a page that states no scene and no light: the
// chrome, the gel, the bevel, the inner shadow, the outer glow and the
// phosphor each paint from their own parameters, and each is found by
// its signature in the pixels.

#include <sigilmaterial/paint/Bases.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "support/BrushTestSupport.h"

namespace {

const material::Color kGrey = {0.5f, 0.5f, 0.5f, 1};

int luminance(SkColor colour) {
  return ((int)SkColorGetR(colour) * 2 + (int)SkColorGetG(colour) * 5 +
          (int)SkColorGetB(colour)) /
         8;
}

/** @p node as an 80 px box standing 20 px in from the corner of a black
 *  120 px host, painted once. */
void paintBox(Host& host, Element node) {
  host.composer.render(
      box()
          .fill(Fill::color({0, 0, 0, 1}))
          .children({std::move(
              node.absolute().left(20).top(20).width(80).height(80))}));
  host.frame();
}

/** A grey box wearing a bevel shaded from its shoulder, lit from
 *  @p angle. */
Element shadedBevel(float angle) {
  return box().fill(material::from(kGrey).effects(
      material::Filter::bevel({.depth = 6,
                               .size = 6,
                               .angleDegrees = angle,
                               .altitudeDegrees = 30,
                               .highlight = {1, 1, 1, 0.9f},
                               .shadow = {0, 0, 0, 0.9f},
                               .gloss = {0, 1}})));
}

}  // namespace

TEST(ComposeLayerStyles, AChromeWordReflectsItsPictureAcrossTheLetters) {
  // Highlight and shadow are set clear and the picture is read unbent,
  // so the letters show the stock horizon exactly as it is laid over the
  // word's box: a bright sky above a dark horizon above a lit ground.
  Host host(420, 180);
  host.composer.render(
      box()
          .fill(Fill::color({0, 0, 0, 1}))
          .children({
              text(u8"HH", whiteStyle(110))
                  .key("word")
                  .absolute()
                  .left(20)
                  .top(20)
                  .decorationOutline(Boundary::Glyphs)
                  .ink(material::from(kGrey).effects(material::Filter::bevel(
                      {.highlight = {1, 1, 1, 0},
                       .shadow = {0, 0, 0, 0},
                       .reflection = material::horizonReflection(),
                       .bend = 0}))),
          }));
  host.frame();
  const auto bounds = host.composer.bounds("word");
  ASSERT_TRUE(bounds.has_value());
  // The mean light of the inked pixels on each row of the word's box.
  std::vector<std::pair<int, int>> rows;
  for (int y = (int)bounds->min.y; y < (int)bounds->max.y; ++y) {
    int sum = 0, count = 0;
    for (int x = (int)bounds->min.x; x < (int)bounds->max.x; ++x) {
      const SkColor colour = host.pixel(x, y);
      if (colour == SK_ColorBLACK) continue;
      sum += luminance(colour);
      ++count;
    }
    if (count > 8) rows.emplace_back(y, sum / count);
  }
  ASSERT_GT(rows.size(), 20u);
  const auto darkest = std::min_element(
      rows.begin(), rows.end(),
      [](const auto& a, const auto& b) { return a.second < b.second; });
  // The horizon crosses the letters near the middle of the box…
  const float middle = (bounds->min.y + bounds->max.y) / 2;
  EXPECT_NEAR((float)darkest->first, middle, 8.0f);
  EXPECT_LT(darkest->second, 50);
  // …with the sky above it and the ground below it.
  const auto brightest = [](auto from, auto to) {
    int most = 0;
    for (auto row = from; row != to; ++row) most = std::max(most, row->second);
    return most;
  };
  EXPECT_GT(brightest(rows.begin(), darkest), darkest->second + 100);
  EXPECT_GT(brightest(darkest + 1, rows.end()), darkest->second + 100);
  // The grey ink beneath is covered everywhere the picture is.
  EXPECT_NE(host.pixel((int)(bounds->min.x + 12), (int)middle), 0xFF808080u);
}

TEST(ComposeLayerStyles, AShadedBevelLightsTheEdgeFacingItsAngle) {
  const int grey = luminance(0xFF808080u);
  {
    // From three o'clock: the right edge lit, the left shaded.
    Host host(120, 120);
    paintBox(host, shadedBevel(0));
    EXPECT_GT(luminance(host.pixel(97, 60)), grey + 30);
    EXPECT_LT(luminance(host.pixel(22, 60)), grey - 30);
    // The flat face takes neither, within the normals' encoding.
    EXPECT_NEAR(luminance(host.pixel(60, 60)), grey, 2);
  }
  {
    // From nine o'clock: the same edges the other way round.
    Host host(120, 120);
    paintBox(host, shadedBevel(180));
    EXPECT_LT(luminance(host.pixel(97, 60)), grey - 30);
    EXPECT_GT(luminance(host.pixel(22, 60)), grey + 30);
  }
  {
    // From six o'clock: the bottom lit, the top shaded, the sides alike.
    Host host(120, 120);
    paintBox(host, shadedBevel(270));
    EXPECT_GT(luminance(host.pixel(60, 97)), grey + 30);
    EXPECT_LT(luminance(host.pixel(60, 22)), grey - 30);
    EXPECT_NEAR(luminance(host.pixel(22, 60)), luminance(host.pixel(97, 60)),
                4);
  }
}

TEST(ComposeLayerStyles, ADrawnBevelLightsTheEdgeFacingItsAngle) {
  // The bevel drawn as two inner shadows follows its angle too: from the
  // lower right, the lower right edge is lit and the upper left shaded.
  Host host(120, 120);
  paintBox(host, box().fill(material::from(kGrey).effects(
                     material::Filter::bevel({.angleDegrees = 300}))));
  EXPECT_GT(luminance(host.pixel(97, 60)), luminance(host.pixel(60, 60)));
  EXPECT_LT(luminance(host.pixel(22, 60)), luminance(host.pixel(60, 60)));
}

TEST(ComposeLayerStyles, AGelCarriesAHighlightBandUnderItsTopEdge) {
  // A gel pill: a deep-to-light body and a gloss ring on the shoulder lit
  // from straight above, so a bright band lies inside the top edge and
  // nothing like it inside the bottom one, against the same pill bare.
  const auto pill = [](bool styled) {
    material::Material body = material::from(
        material::linearGradient({0, 0}, {0, 1},
                                 {material::Color{0.03f, 0.18f, 0.40f, 1},
                                  material::Color{0.12f, 0.55f, 0.95f, 1}}));
    if (styled)
      body.effects(material::Filter::bevel({.depth = 10,
                                            .size = 10,
                                            .angleDegrees = 90,
                                            .altitudeDegrees = 40,
                                            .highlight = {1, 1, 1, 0.9f},
                                            .shadow = {0, 0, 0, 0},
                                            .gloss = {0, 1, 0.8f, 0.3f}}));
    return box().borderRadius({20}).fill(std::move(body));
  };
  Host bare(120, 120), gel(120, 120);
  paintBox(bare, pill(false));
  paintBox(gel, pill(true));
  EXPECT_GT(luminance(gel.pixel(60, 24)), luminance(bare.pixel(60, 24)) + 40);
  EXPECT_LE(luminance(gel.pixel(60, 96)), luminance(bare.pixel(60, 96)) + 4);
  EXPECT_NEAR(luminance(gel.pixel(60, 60)), luminance(bare.pixel(60, 60)), 3);
}

TEST(ComposeLayerStyles, AnInnerShadowDarkensARimInsideTheOutline) {
  Host host(120, 120);
  paintBox(host,
           box().fill(material::from(kGrey).effects(material::Filter::shadow(
               {0, 0, 0, 0.9f}, {.blur = 6, .inside = true}))));
  const int centre = luminance(host.pixel(60, 60));
  EXPECT_EQ(host.pixel(60, 60), 0xFF808080u);
  for (const auto& [x, y] : {std::pair{21, 60}, std::pair{98, 60},
                             std::pair{60, 21}, std::pair{60, 98}})
    EXPECT_LT(luminance(host.pixel(x, y)), centre - 30) << x << ", " << y;
  EXPECT_EQ(host.pixel(10, 60), SK_ColorBLACK);  // nothing outside
}

TEST(ComposeLayerStyles, AnOuterGlowHaloesOutsideTheOutline) {
  Host host(120, 120);
  paintBox(host,
           box().fill(material::from(kGrey).effects(material::Filter::shadow(
               {0, 1, 0, 1}, {.blur = 6, .spread = 3}))));
  for (const auto& [x, y] : {std::pair{60, 16}, std::pair{104, 60},
                             std::pair{16, 60}, std::pair{60, 104}}) {
    const SkColor halo = host.pixel(x, y);
    EXPECT_GT(SkColorGetG(halo), 60u) << x << ", " << y;
    EXPECT_LT(SkColorGetR(halo), 20u) << x << ", " << y;
  }
  EXPECT_EQ(host.pixel(60, 60), 0xFF808080u);  // the fill keeps its colour
  EXPECT_EQ(host.pixel(1, 1), SK_ColorBLACK);  // and the halo ends
}

TEST(ComposeLayerStyles, APhosphorLiftsTheMidtonesAroundALitMark) {
  // A lit bar on a dim panel: the phosphor's halo raises the panel's
  // midtones beside the bar and leaves the panel far from it as it was.
  const auto panel = [](bool phosphor) {
    Element held =
        box()
            .width(120)
            .height(120)
            .fill(Fill::color({0.25f, 0.25f, 0.25f, 1}))
            .children(
                {box().absolute().left(54).top(10).width(12).height(100).fill(
                    Fill::color({0.9f, 0.95f, 1, 1}))});
    if (phosphor)
      held.filter(material::Filter::phosphorBloom(9, 0.5f, 0.8f, 0.8f));
    return box().children({std::move(held)});
  };
  Host plain(120, 120), lifted(120, 120);
  plain.composer.render(panel(false));
  plain.frame();
  lifted.composer.render(panel(true));
  lifted.frame();
  EXPECT_GT(luminance(lifted.pixel(70, 60)),
            luminance(plain.pixel(70, 60)) + 8);
  EXPECT_NEAR(luminance(lifted.pixel(4, 60)), luminance(plain.pixel(4, 60)), 3);
}
