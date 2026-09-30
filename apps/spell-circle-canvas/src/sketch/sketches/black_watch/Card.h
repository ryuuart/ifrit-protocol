#pragma once

/** @file
 * HOW THE CARD IS SET: its colours, read from the words file; the style
 * sheet its three voices and every class stand in; the theme the kit's
 * table reads; and the two ways the card prints what it computed, a
 * colour as the shade cards print one and threads as the register writes
 * them.
 */

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildata/decode/Json.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/kit/Hyphenation.h>
#include <sigilweave/kit/LineTables.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Board.h"
#include "Tartan.h"

namespace material = sigil::material;
namespace weave = sigil::weave;
namespace data = sigil::data;
namespace sketch = sigil::sketch;
using namespace sigil::compose;
using namespace sigil::weave::literals;

namespace {

/** The card's colours: manila board, dark ink, a rule grey and the proof
 *  red every computed line is set in. They stand in the card's words
 *  file. */
struct CardColours {
  material::Color ground, well, rule, ink, ash, proof, shadow;
};

CardColours readCard(const data::Json& card) {
  const auto colour = [&](std::string_view name) {
    return material::parseColor(card[name].string("#000000"));
  };
  return {colour("ground"), colour("well"),  colour("rule"),  colour("ink"),
          colour("ash"),    colour("proof"), colour("shadow")};
}

material::Color faded(material::Color colour, float alpha) {
  return {colour.r, colour.g, colour.b, alpha};
}

/** "#2C2C80" from a colour, as the shade cards print it. */
std::string hexOf(material::Color colour) {
  const auto byte = [](float channel) {
    return (int)std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f);
  };
  return sigil::compose::kit::formatted("#%02X%02X%02X", byte(colour.r),
                                        byte(colour.g), byte(colour.b));
}

/** Threads as the register writes them: "K2 B6 K18 G6". */
std::string spelled(const std::vector<uint8_t>& threads, size_t first,
                    size_t count) {
  std::string words;
  for (size_t at = first; at < first + count;) {
    size_t end = at;
    while (end < first + count && threads[end] == threads[at]) ++end;
    words += sigil::compose::kit::formatted("%s%c%zu", words.empty() ? "" : " ",
                                            kShadeCodes[threads[at]], end - at);
    at = end;
  }
  return words;
}

/** HOW THE CARD IS SET. The tokens are its colours; the type is three
 *  voices — a mono for everything a machine reads, a bold grotesque for
 *  the one display line, a book serif for names and quoted prose — and
 *  every class below is a whole look. The root states the one size, and
 *  every step of the scale is a multiple of it in rems. A crossing of the
 *  draft is @p draftCell pixels square. */
StyleSheet cardSheet(const CardColours& colours, float draftCell) {
  const std::string mono = "Menlo, Courier New, monospace";
  const std::string grotesque = "Helvetica Neue, Arial, sans-serif";
  const std::string book = "Baskerville, Times New Roman, serif";
  return StyleSheet{
      rule(":root")
          .var("ground", colours.ground)
          .var("well", colours.well)
          .var("rule", colours.rule)
          .var("ink", colours.ink)
          .var("ash", colours.ash)
          .var("proof", colours.proof)
          // The board is one recipe, paint and tooth together: a mount
          // board reads as one even card, a fine tooth the eye takes as
          // paper and almost no wear, since any slow blotch on a light card
          // reads as marble rather than as board.
          .var("board", black_watch::board({.paint = colours.ground,
                                            .tooth = 0.05f,
                                            .toothScale = 0.06f,
                                            .wear = 0.004f,
                                            .wearScale = 0.004f,
                                            .seed = 7.0f}))
          // The yarn's tooth keeps frequency · stretch · 2^(octaves−1)
          // under 0.4, past which its y axis aliases into hash noise.
          .var("yarn", material::field::grain(0.09f, 3, 3.0f, 0.75f))
          .fontFamily(mono)
          .fontSize(10)
          .ink(var("ash")),
      // The keylines a class carries: a bar's or a sample's rule, a
      // draft's inked edge, and the rule a panel is framed inside.
      rule(".keyline").stroke(
          stroke(1, Fill::var("rule"), PathFormat::Align::Outer)),
      rule(".draft").stroke(
          stroke(1, Fill::var("ink"), PathFormat::Align::Outer)),
      rule(".frame").stroke(
          stroke(1, Fill::var("rule"), PathFormat::Align::Inner)),
      rule("h1")
          .fontFamily(grotesque)
          .fontWeight(700)
          .fontSize(3.4_rem)
          .letterSpacing(0.135_em)
          .ink(var("ink")),
      rule("lead").fontSize(1.05_rem).letterSpacing(0.1_em),
      rule("h2").fontSize(0.9_rem).letterSpacing(0.055_em).ink(var("ink")),
      rule("caption").fontSize(0.8_rem).letterSpacing(0.05_em),
      rule(".ticket caption").fontSize(0.75_rem),
      rule("footer").fontSize(0.85_rem).letterSpacing(0.08_em),
      rule(".tag").fontSize(0.7_rem).letterSpacing(0.085_em),
      rule(".count").fontSize(1.15_rem).letterSpacing(0.026_em).ink(var("ink")),
      rule(".proof, .emphasis").ink(var("proof")),
      rule(".proof").fontSize(0.85_rem),
      // The run numerals alternate between two rows so a two-thread band
      // still gets its number.
      rule(".runs > *").ink(var("ink")),
      rule(".runs > :nth-child(even)").paddingTop(10).ink(var("ash")),
      rule(".code").fontSize(0.9_rem).letterSpacing(0.09_em),
      rule(".card-name")
          .fontSize(0.75_rem)
          .letterSpacing(0.05_em)
          .ink(var("ink")),
      rule(".shades eyebrow").fontSize(0.7_rem).letterSpacing(0.03_em),
      rule(".bar-name caption")
          .fontSize(0.85_rem)
          .letterSpacing(0.047_em)
          .ink(var("ink")),
      rule(".lifted").fill(Fill::var("ink")),
      rule(".cell").width(draftCell).height(draftCell),
      // Quoted prose hangs its quotation marks and hyphens past the measure,
      // so the column's edge is squared on the letters rather than on the
      // punctuation's advances.
      rule(".name, .quote, .reading, .douglas")
          .fontFamily(book)
          .letterSpacing(0)
          .paragraph({.hanging = weave::kit::hanging::latin()}),
      rule(".name")
          .fontStyle(FontStyle::Italic)
          .fontSize(1.3_rem)
          .ink(var("ink")),
      rule(".name.honest").ink(var("proof")),
      rule(".quote").fontSize(1.05_rem),
      rule(".note").fontSize(0.8_rem).letterSpacing(0.025_em),
      rule(".reading").fontStyle(FontStyle::Italic).fontSize(1.1_rem),
      rule(".attribution").fontStyle(FontStyle::Italic),
      rule(".douglas")
          .fontSize(1.3_rem)
          .ink(var("ink"))
          .font({.language = "en-GB"})
          .lineHeight(weave::Leading::absolute(16))
          .textAlign(weave::TextAlignment::kJustify)
          .textWrap(TextWrap::Pretty)
          .hyphens({.patterns = weave::kit::englishHyphenator()}),
      // The attribution's words are joined by no-break spaces in the words
      // file, so it stands whole on one line, never split around a year.
      rule(".douglas .attribution").fontSize(1_rem).ink(var("ash")),
  };
}

/** The verification's own theme, which the kit's table reads its registers
 *  and colours from: the card's ink on the card's well, every register in
 *  the mono a machine-read line is set in. */
sketch::kit::Theme cardTheme(const CardColours& colours) {
  sketch::kit::Theme look = sketch::kit::featureTheme();
  look.palette = {.ground = colours.ground,
                  .cellGround = colours.well,
                  .ink = colours.ink,
                  .ash = colours.ash,
                  .rule = colours.rule,
                  .figure = colours.ink};
  look.type.captionLabel = {9.5f, 0.1f, true};
  look.type.captionNote = {9.5f, 0.1f, true};
  look.spacing.rowGap = 2.4f;
  look.spacing.labelGap = 8;
  look.spacing.swatchSide = 5;
  return look;
}

}  // namespace
