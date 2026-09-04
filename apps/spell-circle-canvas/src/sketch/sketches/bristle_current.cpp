// bristle_current.cpp — dry pigment carried through a curl-noise current.
//
// A flow field is the small generative-painting loop: seed many particles,
// turn each one by noise at its position, and leave the segment between its
// old and new positions on a canvas that is never cleared. Here nearby
// particles begin as a brush bundle, so the field pulls coherent ribbons
// apart into individual hairs. A translucent wide pass is the wet body of
// the mark; a narrow pass is the dry bristle that survives over it.
//
// EDIT THESE FIRST
//   kRibbonCount   how many separate brush gestures enter the field
//   kBristles      how many hairs travel together in each gesture
//   kFieldScale    the size of the eddies; smaller values make broader turns
//   kTurn          how far the noise may turn a bristle from the prevailing
//   flow

#include <sigilsketch/draw/Draw.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

constexpr float kCanvas = 820.0f;
constexpr int kRibbonCount = 24;
constexpr int kBristles = 11;
constexpr float kFieldScale = 0.0038f;
constexpr float kTurn = 1.55f;
constexpr double kSimHz = 60.0;

constexpr std::array<SkColor4f, 5> kPigments = {{
    {0.08f, 0.18f, 0.19f, 1.0f},
    {0.12f, 0.35f, 0.39f, 1.0f},
    {0.76f, 0.20f, 0.14f, 1.0f},
    {0.84f, 0.48f, 0.10f, 1.0f},
    {0.36f, 0.16f, 0.29f, 1.0f},
}};

SkColor4f withAlpha(SkColor4f color, float alpha) {
  color.fA = alpha;
  return color;
}

struct Bristle {
  float x = 0.0f;
  float y = 0.0f;
  float heading = 0.0f;
  float speed = 1.0f;
  float weight = 1.0f;
  float opacity = 1.0f;
  uint8_t pigment = 0;
};

struct Mark {
  float x0, y0, x1, y1;
  float weight;
  float opacity;
  uint8_t pigment;
};

struct BristleCurrent final : sketch::DrawSketch {
  NoiseField field{0xC011A6Eu};
  std::vector<Bristle> bristles;
  std::vector<Mark> pending;
  uint32_t rng = 0x7F4A7C15u;
  float fieldTime = 0.0f;

  float unit() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(rng & 0x00FFFFFFu) / 16777216.0f;
  }

  void placeRibbon(int ribbon, bool atLeft) {
    const float centerX =
        atLeft ? -20.0f - 90.0f * unit() : -70.0f + (kCanvas + 30.0f) * unit();
    const float centerY = 36.0f + (kCanvas - 72.0f) * unit();
    const float tilt = -0.16f + 0.32f * unit();
    const float width = 1.15f + 1.15f * unit();
    const uint8_t pigment = (uint8_t)(ribbon % (int)kPigments.size());

    for (int hair = 0; hair < kBristles; ++hair) {
      const int index = ribbon * kBristles + hair;
      const float across = (float)hair - 0.5f * (float)(kBristles - 1);
      Bristle& b = bristles[(size_t)index];
      b.x = centerX - across * std::sin(tilt) * width + 5.0f * (unit() - 0.5f);
      b.y = centerY + across * std::cos(tilt) * width + 1.2f * (unit() - 0.5f);
      b.heading = tilt;
      b.speed = 1.05f + 0.75f * unit();
      b.weight = 0.38f + 1.05f * unit();
      b.opacity = 0.34f + 0.38f * unit();
      b.pigment = pigment;
    }
  }

  void resetBristle(Bristle& b) {
    b.x = -24.0f - 80.0f * unit();
    b.y = 32.0f + (kCanvas - 64.0f) * unit();
    b.heading = -0.12f + 0.24f * unit();
    b.speed = 1.05f + 0.75f * unit();
  }

  void step() {
    fieldTime += 1.0f / (float)kSimHz;
    pending.reserve(pending.size() + bristles.size());

    for (Bristle& b : bristles) {
      if (b.x < -120.0f || b.x > kCanvas + 120.0f || b.y < -100.0f ||
          b.y > kCanvas + 100.0f) {
        resetBristle(b);
        continue;
      }

      const float coarse =
          field.at(b.x * kFieldScale, b.y * kFieldScale, fieldTime * 0.055f);
      const float fine =
          field.at(b.x * kFieldScale * 2.7f + 31.0f,
                   b.y * kFieldScale * 2.7f - 19.0f, fieldTime * 0.035f);
      const float desired =
          (coarse / 0.9375f - 0.5f) * kTurn + (fine / 0.9375f - 0.5f) * 0.30f;
      b.heading += (desired - b.heading) * 0.085f;

      const float x0 = b.x;
      const float y0 = b.y;
      b.x += std::cos(b.heading) * b.speed;
      b.y += std::sin(b.heading) * b.speed;

      // Missing hairs and changing pressure make the bundle read as a
      // brush rather than as an evenly sampled stream plot.
      if (unit() > 0.055f) {
        const float pressure = 0.72f + 0.28f * field.at(b.x * 0.012f + 71.0f,
                                                        b.y * 0.012f - 43.0f);
        pending.push_back(
            {x0, y0, b.x, b.y, b.weight, b.opacity * pressure, b.pigment});
      }
    }
  }

  void setup(sketch::DrawContext& ctx) override {
    ctx.canvas(kCanvas, kCanvas);
    ctx.background(244, 238, 221);
    ctx.captureAt(5.2);

    rng = 0x7F4A7C15u;
    fieldTime = 0.0f;
    field.seed(0xC011A6Eu);
    field.detail(4, 0.5f);
    pending.clear();
    bristles.assign((size_t)kRibbonCount * kBristles, {});
    for (int ribbon = 0; ribbon < kRibbonCount; ++ribbon)
      placeRibbon(ribbon, false);

    Pen& pen = ctx.pen;
    pen.randomSeed(0xD12B57u);
    pen.background(244, 238, 221);
    pen.strokeCap(ROUND);
    pen.stroke(78, 64, 44, 13);
    for (int i = 0; i < 1700; ++i) {
      pen.strokeWeight(pen.random(0.22f, 0.95f));
      pen.point(pen.random(kCanvas), pen.random(kCanvas));
    }

    ctx.ticker.addFixed(kSimHz, [this] {
      step();
      return true;
    });
  }

  void draw(Pen& pen) override {
    pen.blendMode(MULTIPLY);
    pen.strokeCap(ROUND);
    for (const Mark& mark : pending) {
      const SkColor4f pigment = kPigments[(size_t)mark.pigment];
      pen.stroke(withAlpha(pigment, mark.opacity * 0.11f));
      pen.strokeWeight(mark.weight * 4.6f);
      pen.line(mark.x0, mark.y0, mark.x1, mark.y1);

      pen.stroke(withAlpha(pigment, mark.opacity));
      pen.strokeWeight(mark.weight);
      pen.line(mark.x0, mark.y0, mark.x1, mark.y1);
    }
    pending.clear();
  }
};

}  // namespace

SIGIL_SKETCH(BristleCurrent, "Draw",
             "Curl-noise currents comb dry pigment into layered ribbons.")
