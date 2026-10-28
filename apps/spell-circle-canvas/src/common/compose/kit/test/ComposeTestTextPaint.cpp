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

TEST(KitTextPaint, ATimeThatMovesIsBoundAndOneThatHoldsIsNot) {
  EXPECT_FALSE(kit::textPaint({.time = 2.0f}).isBound("time"));
  const motion::Animatable<float> clock = motion::animatable(0.0f);
  EXPECT_TRUE(kit::textPaint({.time = clock}).isBound("time"));
}
