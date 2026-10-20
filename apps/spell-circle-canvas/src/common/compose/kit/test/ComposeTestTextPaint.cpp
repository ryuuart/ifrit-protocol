// kit/TextPaint.h — the text paints: one program per field over one box,
// moment and drift. The sparkle's cell is a share of its box's height, so
// over a passage it stands in points among dark glyphs, the same word at
// twice the size takes cells twice as large, and a fill over a box 70
// high keeps a cell 22 across.

#include <include/core/SkBitmap.h>
#include <sigilcompose/kit/TextPaint.h>
#include <sigilmaterial/program/Shader.h>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>
#include <vector>

#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

namespace {

/** The moment every field here is held at. */
constexpr float kMoment = 6.4f;

/** The sparkle at the moment, over the whole of the box it is laid over. */
material::Material sparkle() {
  return kit::textPaint({.field = kit::TextField::Sparkle, .time = kMoment});
}

/** A word in the instrument face at @p size, whose letters are solid bars
 *  with clear space between them, inked with @p ink over its passage and
 *  padded by a quarter of its size, so every length on it scales with the
 *  size. */
Element inkedWord(float size, material::Material ink) {
  return box()
      .padding(size * 0.25f)
      .children({text(u8"SPARKLE", styleAt(size))
                     .ink(std::move(ink), PaintBox::Element)});
}

/** How bright a pixel is: its strongest channel, the sparkle being light
 *  added over black. */
int brightness(SkColor colour) {
  return std::max(
      {SkColorGetR(colour), SkColorGetG(colour), SkColorGetB(colour)});
}

/** The 4-connected regions of the pixels of @p host that @p accepts
 *  answers yes for, each as the indices of its pixels: over a passage of
 *  solid bars inked opaque, one region per glyph. */
std::vector<std::vector<int>> regionsOf(const Host& host, int width, int height,
                                        bool (*accepts)(SkColor)) {
  std::vector<int> label(static_cast<size_t>(width * height), -1);
  std::vector<std::vector<int>> regions;
  for (int start = 0; start < width * height; ++start) {
    if (label[start] >= 0 || !accepts(host.pixel(start % width, start / width)))
      continue;
    regions.emplace_back();
    std::vector<int> frontier{start};
    label[start] = static_cast<int>(regions.size() - 1);
    while (!frontier.empty()) {
      const int index = frontier.back();
      frontier.pop_back();
      regions.back().push_back(index);
      const int column = index % width;
      const int row = index / width;
      const int neighbours[4][2] = {{column - 1, row},
                                    {column + 1, row},
                                    {column, row - 1},
                                    {column, row + 1}};
      for (const auto& [across, down] : neighbours) {
        if (across < 0 || down < 0 || across >= width || down >= height)
          continue;
        const int next = down * width + across;
        if (label[next] >= 0 || !accepts(host.pixel(across, down))) continue;
        label[next] = label[start];
        frontier.push_back(next);
      }
    }
  }
  return regions;
}

/** Whether a pixel is painted at all. */
bool painted(SkColor colour) {
  return SkColorGetA(colour) > 0 && colour != SK_ColorBLACK;
}
/** Whether a pixel holds light: bright over black. */
bool lit(SkColor colour) { return brightness(colour) > 128; }

}  // namespace

TEST(KitTextPaint, ASparkleInkedOverAPassageStandsInPointsAmongDarkGlyphs) {
  // A word set four times down a narrow measure, its passage's box nearly
  // square. The sparkle stretched over it holds light in points: some
  // glyphs carry one, the rest stand dark, and most of the ink is ground.
  constexpr int kWidth = 200;
  constexpr int kHeight = 240;
  const auto passage = [](const material::Material& ink) {
    return box().padding(12).width(174).children(
        {text(u8"SPARK SPARK SPARK SPARK", styleAt(48))
             .ink(ink, PaintBox::Element)});
  };
  Host mask(kWidth, kHeight), sparkled(kWidth, kHeight);
  mask.composer.render(passage(
      kit::textPaint({.field = kit::TextField::Water, .time = kMoment})));
  sparkled.composer.render(passage(sparkle()));
  mask.frame();
  sparkled.frame();

  const std::vector<std::vector<int>> glyphs =
      regionsOf(mask, kWidth, kHeight, painted);
  ASSERT_EQ(glyphs.size(), 20u) << "four lines of five separate bars";
  size_t litGlyphs = 0;
  size_t litPixels = 0;
  size_t glyphPixels = 0;
  for (const std::vector<int>& glyph : glyphs) {
    bool lightHere = false;
    for (int index : glyph) {
      const bool light = lit(sparkled.pixel(index % kWidth, index / kWidth));
      litPixels += light ? 1 : 0;
      lightHere |= light;
    }
    glyphPixels += glyph.size();
    litGlyphs += lightHere ? 1 : 0;
  }
  EXPECT_GE(litGlyphs, 1u) << "light lands on the passage";
  EXPECT_LT(litGlyphs, glyphs.size()) << "and some glyphs stand dark";
  EXPECT_GE(regionsOf(sparkled, kWidth, kHeight, lit).size(), 1u);
  EXPECT_LT(litPixels * 4, glyphPixels)
      << "the light stands in points, not over the whole passage";
}

TEST(KitTextPaint, ASparkleOverAWideWordStandsInPointsAcrossItsGlyphs) {
  // The wordmark is far wider than its cap band is high, and the ink hands
  // the program that band as its resolution, so a cell is a share of the
  // type's height on every glyph rather than a third of the word: the
  // light stands in separate points spread across the word.
  constexpr int kWidth = 420;
  constexpr int kHeight = 150;
  const auto wordmark = [](material::Material ink) {
    return box().padding(24).children(
        {text(u8"SIGIL", styleAt(104)).ink(std::move(ink))});
  };
  Host mask(kWidth, kHeight), sparkled(kWidth, kHeight);
  mask.composer.render(wordmark(
      kit::textPaint({.field = kit::TextField::Water, .time = kMoment})));
  sparkled.composer.render(wordmark(sparkle()));
  mask.frame();
  sparkled.frame();

  const std::vector<std::vector<int>> glyphs =
      regionsOf(mask, kWidth, kHeight, painted);
  ASSERT_EQ(glyphs.size(), 5u) << "five separate bars";
  size_t litGlyphs = 0;
  for (const std::vector<int>& glyph : glyphs)
    litGlyphs += std::any_of(glyph.begin(), glyph.end(),
                             [&](int index) {
                               return lit(sparkled.pixel(index % kWidth,
                                                         index / kWidth));
                             })
                     ? 1
                     : 0;
  EXPECT_GE(regionsOf(sparkled, kWidth, kHeight, lit).size(), 3u)
      << "the light stands in separate points";
  EXPECT_GE(litGlyphs, 3u) << "spread across the word";
}

TEST(KitTextPaint, AWordAtTwiceTheSizeTakesCellsTwiceAsLarge) {
  // Everything about the word scales with its size — the bars, the
  // passage they are inked over, the padding — so where the cells scale
  // with it, the large word reduced by half is the small word again.
  constexpr int kWidth = 280;
  constexpr int kHeight = 90;
  Host small(kWidth, kHeight), large(kWidth * 2, kHeight * 2);
  small.composer.render(inkedWord(60, sparkle()));
  large.composer.render(inkedWord(120, sparkle()));
  small.frame();
  large.frame();

  std::vector<double> smallValues, reducedValues;
  for (int row = 0; row < kHeight; ++row)
    for (int column = 0; column < kWidth; ++column) {
      smallValues.push_back(brightness(small.pixel(column, row)));
      double block = 0;
      for (int down = 0; down < 2; ++down)
        for (int across = 0; across < 2; ++across)
          block += brightness(large.pixel(column * 2 + across, row * 2 + down));
      reducedValues.push_back(block / 4);
    }
  const auto mean = [](const std::vector<double>& values) {
    double total = 0;
    for (double value : values) total += value;
    return total / static_cast<double>(values.size());
  };
  const double smallMean = mean(smallValues);
  const double reducedMean = mean(reducedValues);
  double covariance = 0, smallSpread = 0, reducedSpread = 0;
  for (size_t index = 0; index < smallValues.size(); ++index) {
    const double smallOffset = smallValues[index] - smallMean;
    const double reducedOffset = reducedValues[index] - reducedMean;
    covariance += smallOffset * reducedOffset;
    smallSpread += smallOffset * smallOffset;
    reducedSpread += reducedOffset * reducedOffset;
  }
  ASSERT_GT(smallSpread, 0) << "the small word holds light";
  ASSERT_GT(reducedSpread, 0) << "the large word holds light";
  const double correlation =
      covariance / std::sqrt(smallSpread * reducedSpread);
  EXPECT_GT(correlation, 0.9)
      << "the large word's light, reduced by half, stands where the small "
         "word's does";
}

TEST(KitTextPaint, AFillOverABoxSeventyHighKeepsACellTwentyTwoAcross) {
  // The sparkle with its cell written as 22 px, the cell a box 70 high
  // gives the kit's sparkle; over that box the two are one field.
  constexpr std::string_view kFixedCell = R"SHADER(
float sparkleHash(float2 cell) {
  return fract(sin(dot(cell, float2(41.3, 289.1))) * 43758.5453123);
}

half4 main(float2 point) {
  float2 drift = float2(sin(time * 0.83), cos(time * 0.61));
  float2 position = point - origin + float2(time * 3.0, time * 1.3);
  float2 cell = floor(position / 22.0);
  float2 local = fract(position / 22.0) - 0.5;
  float present = step(0.6, sparkleHash(cell + 3.0));
  float sizeSeed = sparkleHash(cell + 11.0);
  float brightSeed = sparkleHash(cell + 19.0);
  float phaseSeed = sparkleHash(cell + 27.0);
  float rateSeed = sparkleHash(cell + 5.0);
  float2 jitter = float2(sparkleHash(cell), sparkleHash(cell + 41.0)) - 0.5;
  float reach = length(local - jitter * 0.7);
  float radius = mix(0.035, 0.14, sizeSeed);
  float twinkleRate = mix(1.1, 3.6, rateSeed) + drift.x * 0.15;
  float twinkle =
      0.5 + 0.5 * sin(time * twinkleRate + phaseSeed * 6.2831853);
  twinkle = twinkle * twinkle * (3.0 - 2.0 * twinkle);
  float core = smoothstep(radius, 0.0, reach);
  float halo = smoothstep(radius * 5.5, 0.0, reach) * 0.30;
  float brightness =
      (core + halo) * mix(0.6, 2.2, brightSeed) * twinkle * present;
  half3 tint =
      mix(half3(0.80, 0.88, 1.0), half3(1.0, 0.92, 0.75), half(brightSeed));
  half alpha = half(clamp(brightness, 0.0, 1.0));
  return half4(tint * alpha, alpha);
}
)SHADER";
  struct FixedCellParameters {
    glm::vec2 origin;
    glm::vec2 extent;
    float time;
  };
  const auto filled = [](material::Material paint) {
    return box().children({box().width(220).height(70).fill(std::move(paint))});
  };
  Host kit(240, 90), fixed(240, 90);
  kit.composer.render(filled(sparkle()));
  fixed.composer.render(filled(material::shader(
      kFixedCell, FixedCellParameters{{0, 0}, {220, 70}, kMoment})));
  kit.frame();
  fixed.frame();

  int litPixels = 0;
  int differing = 0;
  for (int row = 0; row < 70; ++row)
    for (int column = 0; column < 220; ++column) {
      const SkColor ours = kit.pixel(column, row);
      const SkColor theirs = fixed.pixel(column, row);
      litPixels += brightness(ours) > 128 ? 1 : 0;
      const int apart =
          std::max({std::abs(static_cast<int>(SkColorGetR(ours)) -
                             static_cast<int>(SkColorGetR(theirs))),
                    std::abs(static_cast<int>(SkColorGetG(ours)) -
                             static_cast<int>(SkColorGetG(theirs))),
                    std::abs(static_cast<int>(SkColorGetB(ours)) -
                             static_cast<int>(SkColorGetB(theirs)))});
      differing += apart > 1 ? 1 : 0;
    }
  EXPECT_GT(litPixels, 0) << "the plate holds light";
  EXPECT_EQ(differing, 0) << "over a box 70 high the cell is 22 across";
}

TEST(KitTextPaint, AFillIsAuthoredOverItsNodesOwnBox) {
  // A fill hands the program its node's box, so the mesh gradient over the
  // whole of it crosses its colours from the warm corner to the cool one.
  Host host(140, 80);
  host.composer.render(box().children({box().width(120).height(60).fill(
      kit::textPaint({.field = kit::TextField::MeshGradient}))}));
  host.frame();
  const SkColor warm = host.pixel(2, 2);
  const SkColor cool = host.pixel(117, 57);
  EXPECT_GT(SkColorGetR(warm), SkColorGetG(warm)) << "the top left is warm";
  EXPECT_GT(SkColorGetG(cool), SkColorGetR(cool)) << "the bottom right is cool";
}

TEST(KitTextPaint, TheStarsArePointsOfLightOverAColouredHaze) {
  // Filled over a node's box, the star field is a coloured haze
  // everywhere, never black, with white points standing apart on it.
  constexpr int kWidth = 240;
  constexpr int kHeight = 80;
  Host host(kWidth, kHeight);
  host.composer.render(box().children({box().width(kWidth).height(kHeight).fill(
      kit::textPaint({.field = kit::TextField::Stars, .time = kMoment}))}));
  host.frame();
  const auto isWhite = [](SkColor colour) {
    return std::min({SkColorGetR(colour), SkColorGetG(colour),
                     SkColorGetB(colour)}) > 170;
  };
  int coloured = 0;
  int white = 0;
  for (int row = 0; row < kHeight; ++row)
    for (int column = 0; column < kWidth; ++column) {
      const SkColor colour = host.pixel(column, row);
      coloured += SkColorGetB(colour) > 40 ? 1 : 0;
      white += isWhite(colour) ? 1 : 0;
    }
  EXPECT_EQ(coloured, kWidth * kHeight) << "the haze holds colour everywhere";
  EXPECT_LT(white * 20, kWidth * kHeight) << "the light stands in points";
  EXPECT_GE(regionsOf(host, kWidth, kHeight, isWhite).size(), 12u)
      << "and the points stand apart";
}

TEST(KitTextPaint, TheCloudsLieOverASkyDeepeningTowardTheTop) {
  // Filled over a node's box, the clouds leave sky between
  // them, blue and deeper at the top than at the bottom, and are pale
  // where they stand.
  constexpr int kWidth = 240;
  constexpr int kHeight = 120;
  Host host(kWidth, kHeight);
  host.composer.render(box().children({box().width(kWidth).height(kHeight).fill(
      kit::textPaint({.field = kit::TextField::Clouds, .time = kMoment}))}));
  host.frame();
  const auto isSky = [](SkColor colour) {
    return SkColorGetB(colour) > SkColorGetR(colour) + 60;
  };
  const auto isCloud = [](SkColor colour) {
    return std::min({SkColorGetR(colour), SkColorGetG(colour),
                     SkColorGetB(colour)}) > 170;
  };
  int sky = 0;
  int cloud = 0;
  int topSkyRed = 0;
  int topSky = 0;
  int bottomSkyRed = 0;
  int bottomSky = 0;
  for (int row = 0; row < kHeight; ++row)
    for (int column = 0; column < kWidth; ++column) {
      const SkColor colour = host.pixel(column, row);
      cloud += isCloud(colour) ? 1 : 0;
      if (!isSky(colour)) continue;
      ++sky;
      if (row < kHeight / 4) {
        topSkyRed += SkColorGetR(colour);
        ++topSky;
      } else if (row >= kHeight * 3 / 4) {
        bottomSkyRed += SkColorGetR(colour);
        ++bottomSky;
      }
    }
  EXPECT_GT(sky * 5, kWidth * kHeight) << "sky shows between the clouds";
  EXPECT_GT(cloud * 10, kWidth * kHeight) << "the clouds are pale";
  ASSERT_GT(topSky, 0);
  ASSERT_GT(bottomSky, 0);
  EXPECT_LT(topSkyRed / topSky, bottomSkyRed / bottomSky)
      << "the sky deepens toward the top";
}

TEST(KitTextPaint, EqualPropertiesAnswerEqualMaterialsAndEachFieldItsOwn) {
  const std::vector<kit::TextField> fields{
      kit::TextField::Water,   kit::TextField::MeshGradient,
      kit::TextField::Sparkle, kit::TextField::Stars,
      kit::TextField::Clouds,  kit::TextField::Tunnel};
  for (kit::TextField field : fields) {
    EXPECT_EQ(kit::textPaint({.field = field, .time = kMoment}),
              kit::textPaint({.field = field, .time = kMoment}));
    EXPECT_NE(kit::textPaint({.field = field, .time = kMoment}),
              kit::textPaint({.field = field, .time = kMoment + 1}));
  }
  for (size_t first = 0; first < fields.size(); ++first)
    for (size_t second = first + 1; second < fields.size(); ++second)
      EXPECT_NE(kit::textPaint({.field = fields[first]}),
                kit::textPaint({.field = fields[second]}));
}

TEST(KitTextPaint, EveryFieldPaintsAPassage) {
  // Each field's program compiles with the drift ahead of it and paints
  // the word it inks; only the sparkle leaves ground between its points.
  for (kit::TextField field :
       {kit::TextField::Water, kit::TextField::MeshGradient,
        kit::TextField::Stars, kit::TextField::Clouds,
        kit::TextField::Tunnel}) {
    Host host(280, 90);
    host.composer.render(
        inkedWord(60, kit::textPaint({.field = field, .time = kMoment})));
    host.frame();
    int painted = 0;
    for (int row = 0; row < 90; ++row)
      for (int column = 0; column < 280; ++column)
        painted += SkColorGetA(host.pixel(column, row)) == 255 &&
                           host.pixel(column, row) != SK_ColorBLACK
                       ? 1
                       : 0;
    EXPECT_GT(painted, 1000) << "field " << static_cast<int>(field);
  }
}

TEST(KitTextPaint, ATimeThatMovesIsBoundAndOneThatHoldsIsNot) {
  EXPECT_FALSE(kit::textPaint({.time = 2.0f}).isBound("time"));
  const motion::Animatable<float> clock = motion::animatable(0.0f);
  EXPECT_TRUE(kit::textPaint({.time = clock}).isBound("time"));
}
