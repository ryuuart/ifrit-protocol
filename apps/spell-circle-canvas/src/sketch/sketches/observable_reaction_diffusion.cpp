/** @file
 * observable_reaction_diffusion — a Gray-Scott field advanced on the CPU.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilsketch/draw/Draw.h>

#include <algorithm>
#include <array>
#include <vector>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

constexpr int kGrid = 144;

struct Cell {
  float a = 1.0f;
  float b = 0.0f;
};

struct ObservableReactionDiffusion final : sketch::DrawSketch {
  std::vector<Cell> field =
      std::vector<Cell>(static_cast<size_t>(kGrid * kGrid));
  std::vector<Cell> next = field;
  SkBitmap bitmap;

  void setup(sketch::DrawContext& context) override {
    context.canvas(720, 720);
    context.captureAt(5.0);
    bitmap.allocN32Pixels(kGrid, kGrid);
    for (int y = 56; y < 88; ++y)
      for (int x = 56; x < 88; ++x) field[y * kGrid + x].b = 1.0f;
    for (int y = 30; y < 42; ++y)
      for (int x = 94; x < 106; ++x) field[y * kGrid + x].b = 1.0f;
  }

  Cell at(int x, int y) const {
    x = std::clamp(x, 0, kGrid - 1);
    y = std::clamp(y, 0, kGrid - 1);
    return field[y * kGrid + x];
  }

  float laplace(int x, int y, bool chemicalB) const {
    const auto value = [&](int sx, int sy) {
      const Cell cell = at(sx, sy);
      return chemicalB ? cell.b : cell.a;
    };
    return -value(x, y) +
           0.2f * (value(x - 1, y) + value(x + 1, y) + value(x, y - 1) +
                   value(x, y + 1)) +
           0.05f * (value(x - 1, y - 1) + value(x + 1, y - 1) +
                    value(x - 1, y + 1) + value(x + 1, y + 1));
  }

  void advance() {
    constexpr float kFeed = 0.055f;
    constexpr float kKill = 0.062f;
    for (int y = 1; y < kGrid - 1; ++y) {
      for (int x = 1; x < kGrid - 1; ++x) {
        const Cell cell = at(x, y);
        const float reaction = cell.a * cell.b * cell.b;
        Cell& output = next[y * kGrid + x];
        output.a = std::clamp(
            cell.a + laplace(x, y, false) - reaction + kFeed * (1.0f - cell.a),
            0.0f, 1.0f);
        output.b = std::clamp(cell.b + 0.5f * laplace(x, y, true) + reaction -
                                  (kKill + kFeed) * cell.b,
                              0.0f, 1.0f);
      }
    }
    field.swap(next);
  }

  void rasterize() {
    for (int y = 0; y < kGrid; ++y) {
      for (int x = 0; x < kGrid; ++x) {
        const Cell cell = field[y * kGrid + x];
        const int shade =
            std::clamp(static_cast<int>((cell.a - cell.b) * 255.0f), 0, 255);
        *bitmap.getAddr32(x, y) = SkColorSetARGB(255, shade, shade, shade);
      }
    }
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    for (int iteration = 0; iteration < 5; ++iteration) advance();
    rasterize();
    pen.background(0);
    pen.image(bitmap.asImage(), 0, 0, pen.width, pen.height);
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableReactionDiffusion, "observable_reaction_diffusion",
                "Draw · Observable reproductions",
                "A Gray-Scott reaction field grows from two seeded regions.")
