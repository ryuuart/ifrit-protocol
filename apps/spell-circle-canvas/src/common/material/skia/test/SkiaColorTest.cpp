/** @file
 * The colour bridge: what crosses between SkColor4f and this library's
 * colour leaf, and what a palette of them becomes.
 */

#include <gtest/gtest.h>
#include <include/utils/SkParse.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace sigil::material;

TEST(SkiaColor, ChannelsAndAlphaSurviveTheRoundTrip) {
  constexpr SkColor4f in{0.25f, 0.5f, 0.75f, 0.125f};
  constexpr Color mid = skia::toColor(in);
  EXPECT_FLOAT_EQ(mid.r, 0.25f);
  EXPECT_FLOAT_EQ(mid.g, 0.5f);
  EXPECT_FLOAT_EQ(mid.b, 0.75f);
  EXPECT_FLOAT_EQ(mid.a, 0.125f);
  constexpr SkColor4f back = skia::toSkColor(mid);
  EXPECT_EQ(back, in);
}

TEST(SkiaColor, AChannelAboveOneIsCarriedRatherThanClamped) {
  // Straight copy, no transfer function: a wide-gamut channel survives.
  const Color c = skia::toColor(SkColor4f{1.5f, -0.25f, 0, 1});
  EXPECT_FLOAT_EQ(c.r, 1.5f);
  EXPECT_FLOAT_EQ(c.g, -0.25f);
}

// Every CSS name reads to the colour Skia's own table of the same names
// gives, which is the SVG list. `rebeccapurple`, which CSS added after
// it, is read against its own value, and a grey Skia's table spells only
// one way is read against that spelling.
TEST(SkiaColor, EveryCssNameReadsAsSkiaNamesIt) {
  const char* const names[] = {"aliceblue",
                               "antiquewhite",
                               "aqua",
                               "aquamarine",
                               "azure",
                               "beige",
                               "bisque",
                               "black",
                               "blanchedalmond",
                               "blue",
                               "blueviolet",
                               "brown",
                               "burlywood",
                               "cadetblue",
                               "chartreuse",
                               "chocolate",
                               "coral",
                               "cornflowerblue",
                               "cornsilk",
                               "crimson",
                               "cyan",
                               "darkblue",
                               "darkcyan",
                               "darkgoldenrod",
                               "darkgray",
                               "darkgreen",
                               "darkgrey",
                               "darkkhaki",
                               "darkmagenta",
                               "darkolivegreen",
                               "darkorange",
                               "darkorchid",
                               "darkred",
                               "darksalmon",
                               "darkseagreen",
                               "darkslateblue",
                               "darkslategray",
                               "darkslategrey",
                               "darkturquoise",
                               "darkviolet",
                               "deeppink",
                               "deepskyblue",
                               "dimgray",
                               "dimgrey",
                               "dodgerblue",
                               "firebrick",
                               "floralwhite",
                               "forestgreen",
                               "fuchsia",
                               "gainsboro",
                               "ghostwhite",
                               "gold",
                               "goldenrod",
                               "gray",
                               "green",
                               "greenyellow",
                               "grey",
                               "honeydew",
                               "hotpink",
                               "indianred",
                               "indigo",
                               "ivory",
                               "khaki",
                               "lavender",
                               "lavenderblush",
                               "lawngreen",
                               "lemonchiffon",
                               "lightblue",
                               "lightcoral",
                               "lightcyan",
                               "lightgoldenrodyellow",
                               "lightgray",
                               "lightgreen",
                               "lightgrey",
                               "lightpink",
                               "lightsalmon",
                               "lightseagreen",
                               "lightskyblue",
                               "lightslategray",
                               "lightslategrey",
                               "lightsteelblue",
                               "lightyellow",
                               "lime",
                               "limegreen",
                               "linen",
                               "magenta",
                               "maroon",
                               "mediumaquamarine",
                               "mediumblue",
                               "mediumorchid",
                               "mediumpurple",
                               "mediumseagreen",
                               "mediumslateblue",
                               "mediumspringgreen",
                               "mediumturquoise",
                               "mediumvioletred",
                               "midnightblue",
                               "mintcream",
                               "mistyrose",
                               "moccasin",
                               "navajowhite",
                               "navy",
                               "oldlace",
                               "olive",
                               "olivedrab",
                               "orange",
                               "orangered",
                               "orchid",
                               "palegoldenrod",
                               "palegreen",
                               "paleturquoise",
                               "palevioletred",
                               "papayawhip",
                               "peachpuff",
                               "peru",
                               "pink",
                               "plum",
                               "powderblue",
                               "purple",
                               "rebeccapurple",
                               "red",
                               "rosybrown",
                               "royalblue",
                               "saddlebrown",
                               "salmon",
                               "sandybrown",
                               "seagreen",
                               "seashell",
                               "sienna",
                               "silver",
                               "skyblue",
                               "slateblue",
                               "slategray",
                               "slategrey",
                               "snow",
                               "springgreen",
                               "steelblue",
                               "tan",
                               "teal",
                               "thistle",
                               "tomato",
                               "turquoise",
                               "violet",
                               "wheat",
                               "white",
                               "whitesmoke",
                               "yellow",
                               "yellowgreen"};
  EXPECT_EQ(std::size(names), 148u);
  for (const char* name : names) {
    SkColor named = 0;
    const char* found =
        SkParse::FindNamedColor(name, std::strlen(name), &named);
    const std::optional<Color> read = namedColor(name);
    ASSERT_TRUE(read) << name;
    if (std::strcmp(name, "rebeccapurple") == 0) {
      EXPECT_EQ(*read, hexColor(0x663399));
      continue;
    }
    std::string spelling = name;
    for (const auto& [written, other] :
         {std::pair{"grey", "gray"}, std::pair{"gray", "grey"}}) {
      const size_t at = spelling.find(written);
      if (found || at == std::string::npos) continue;
      spelling.replace(at, 4, other);
      found =
          SkParse::FindNamedColor(spelling.c_str(), spelling.size(), &named);
    }
    ASSERT_NE(found, nullptr) << name;
    EXPECT_EQ(*read, hexColor(named & 0xffffffu)) << name;
  }
}
