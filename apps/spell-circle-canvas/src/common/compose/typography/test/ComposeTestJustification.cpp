// The door from a leaf to the engine's justification: what a leaf is told
// reaches the layout, and what the layout decided reaches the glyphs
// handed to the canvas.

#include <vector>

#include "GlyphCanvas.h"
#include "support/TextTestSupport.h"

namespace {

/** The right edge each line of a justified passage reached, in the measure
 *  `spec` set it in. */
std::vector<float> justifiedEdges(sigil::weave::JustificationOptions spec,
                                  const char* body, float measure) {
  Host host(400, 400);
  host.composer.render(box().children(
      {text(body, whiteStyle(12))
           .key("t")
           .width(measure)
           .paragraph({.alignment = sigil::weave::TextAlignment::kJustify})
           .paragraph({.justification = spec})}));
  host.frame();
  std::vector<float> edges;
  for (const TextUnit& line : host.composer.units(
           "t", sigil::weave::selectors::each(sigil::weave::Unit::Line),
           sigil::weave::Unit::Line))
    edges.push_back(line.rect.right());
  return edges;
}

/** A passage that wraps to several justified lines at the measure below:
 *  at 12 px in the instrument face its greedy lines are 140.4, 144, 177.6,
 *  97.2 and 171.6 px wide, so the last holds gaps and is short of it. */
const char* kJustified =
    "Justification spends interword gaps before letterspacing, and "
    "reaches for horizontal glyph-scaling last of all.";
constexpr float kJustifiedMeasure = 180.0f;

}  // namespace

TEST(ComposeJustification, WhatALeafIsToldAboutJustificationReachesTheLayout) {
  // How a justified line is fitted is SigilWeave's, and it is asked there.
  // What this tier promises is that the options a leaf is handed arrive:
  // the same passage set with the last line justified reaches the measure
  // where the stock setting leaves it ragged.
  const std::vector<float> ragged =
      justifiedEdges({}, kJustified, kJustifiedMeasure);
  sigil::weave::JustificationOptions all;
  all.justifyLastLine = true;
  const std::vector<float> full =
      justifiedEdges(all, kJustified, kJustifiedMeasure);
  ASSERT_EQ(ragged.size(), full.size());
  EXPECT_LT(ragged.back(), kJustifiedMeasure - 2.0f);
  EXPECT_NEAR(full.back(), kJustifiedMeasure, 1.0f);
}

TEST(ComposeJustification, ASingleWordFitReachesTheGlyphsHandedToTheCanvas) {
  using SingleWord = sigil::weave::JustificationOptions::SingleWord;
  const auto draw = [](SingleWord rule) {
    Host host(200, 80);
    sigil::weave::JustificationOptions options;
    options.justifyLastLine = true;
    options.singleWord = rule;
    host.composer.render(box().children(
        {text("Alone.", whiteStyle(12))
             .width(130.0f)
             .paragraph({.alignment = sigil::weave::TextAlignment::kJustify,
                         .justification = options})}));
    sigil::test::GlyphCanvas canvas(200, 80);
    host.composer.draw(canvas);
    return canvas.glyphs;
  };
  const auto aligned = draw(SingleWord::kAlign);
  const auto justified = draw(SingleWord::kJustify);
  ASSERT_GE(aligned.size(), 2u);
  ASSERT_EQ(justified.size(), aligned.size());
  EXPECT_FLOAT_EQ(justified.front().position.x(), aligned.front().position.x());
  EXPECT_LT(aligned.back().position.x(), 65.0f);
  EXPECT_GT(justified.back().position.x(), 65.0f);
  for (size_t index = 1; index < aligned.size(); ++index) {
    EXPECT_EQ(justified[index].glyph, aligned[index].glyph);
    EXPECT_EQ(justified[index].font, aligned[index].font);
    EXPECT_GT(justified[index].position.x(), aligned[index].position.x());
  }
}
