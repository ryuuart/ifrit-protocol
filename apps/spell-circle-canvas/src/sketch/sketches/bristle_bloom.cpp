// bristle_bloom.cpp — a radial painting made from loaded procedural brushes.
//
// Each gesture is a bundle of translucent Bezier bristles. A broad wet body,
// darker edge pools, missing hairs and pale drag marks make the repeated curve
// read as paint deposited by one imperfect tool rather than as vector ribbons.
// The same brush is rotated and scaled into a bloom, so the transforms, colour,
// seeded random stream, blending and curve verbs all remain SigilDraw's.
//
// EDIT THESE FIRST
//   kPetals       how many large gestures make the bloom
//   kBristles     how many hairs make each loaded brush
//   kPigmentAlpha how heavily each bristle deposits pigment

#include <sigilsketch/draw/Draw.h>

#include <array>
#include <cmath>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

constexpr int kPetals = 30;
constexpr int kBristles = 22;
constexpr float kPigmentAlpha = 34.0f;

struct Pigment {
  float red;
  float green;
  float blue;
};

constexpr std::array<Pigment, 5> kPigments{{
    {236, 79, 103},
    {244, 134, 73},
    {238, 190, 92},
    {77, 174, 179},
    {105, 112, 191},
}};

struct BristleBloom final : sketch::DrawSketch {
  void setup(sketch::DrawContext& ctx) override {
    ctx.canvas(900, 900);
    ctx.background(18, 15, 24);
    ctx.captureAt(0.25);
    ctx.pen.randomSeed(0xB10550u);
    ctx.pen.noiseSeed(0xB10550u);
    ctx.pen.strokeCap(ROUND);
    ctx.pen.strokeJoin(ROUND);
    ctx.pen.noFill();
  }

  void brush(Pen& pen, float length, float width, float bend,
             const Pigment& pigment, float load) {
    // The faint body is the water carrying the pigment. It prevents the gaps
    // between discrete hairs from making the brush read as a comb.
    pen.stroke(pigment.red, pigment.green, pigment.blue, 5.0f * load);
    pen.strokeWeight(width * 0.72f);
    pen.bezier(0, 0, length * 0.28f, -width * 0.18f, length * 0.70f,
               bend - width * 0.12f, length, bend);

    for (int hair = 0; hair < kBristles; ++hair) {
      const float lane = map((float)hair, 0, (float)(kBristles - 1),
                             -width * 0.5f, width * 0.5f);
      const float tooth = pen.noise((float)hair * 0.43f, length * 0.013f);
      if (tooth < 0.14f) continue;

      const float wander = pen.randomGaussian(0, width * 0.035f);
      const float taper = 1.0f - std::abs(lane) / (width * 0.5f);
      const float alpha = kPigmentAlpha * load * (0.35f + taper * 0.65f) *
                          pen.random(0.72f, 1.18f);
      pen.stroke(pigment.red, pigment.green, pigment.blue, alpha);
      pen.strokeWeight(pen.random(0.65f, 1.65f));
      pen.bezier(0, lane + wander, length * 0.27f, lane * 0.72f - width * 0.12f,
                 length * 0.72f, bend + lane * 0.42f,
                 length * pen.random(0.94f, 1.02f),
                 bend + lane * 0.18f + wander);
    }

    // Pigment pools at the two edges of a loaded brush and breaks where the
    // paper catches it. These two hairs carry more colour than the body.
    for (float edge : {-0.48f, 0.48f}) {
      pen.stroke(pigment.red * 0.72f, pigment.green * 0.72f,
                 pigment.blue * 0.78f, 62.0f * load);
      pen.strokeWeight(1.3f);
      const float y = width * edge;
      pen.bezier(0, y, length * 0.30f, y * 0.70f - width * 0.10f,
                 length * 0.73f, bend + y * 0.35f, length, bend + y * 0.16f);
    }
  }

  void draw(Pen& pen) override {
    pen.background(18, 15, 24);

    // Fine warm and cool flecks keep the ground from being a perfectly flat
    // digital field. They are points, so stroke weight is their grain size.
    for (int i = 0; i < 2100; ++i) {
      const float light = pen.random(35, 76);
      pen.stroke(light + 12, light, light + pen.random(4, 20),
                 pen.random(8, 24));
      pen.strokeWeight(pen.random(0.35f, 1.15f));
      pen.point(pen.random(pen.width), pen.random(pen.height));
    }

    pen.push();
    pen.translate(pen.width * 0.5f, pen.height * 0.5f);

    // The rear crown is a looser, dimmer set of gestures. SCREEN lets thin
    // overlaps gather luminosity without turning every crossing opaque.
    pen.blendMode(SCREEN);
    for (int petal = 0; petal < kPetals; ++petal) {
      const float angle = TWO_PI * (float)petal / (float)kPetals;
      const Pigment& pigment = kPigments[(petal + 3) % kPigments.size()];
      pen.push();
      pen.rotate(angle + pen.random(-0.035f, 0.035f));
      pen.translate(56 + pen.random(-8, 12), pen.random(-7, 7));
      brush(pen, pen.random(245, 345), pen.random(25, 48), pen.random(-74, 74),
            pigment, 0.58f);
      pen.pop();
    }

    // A denser inner pass crosses the first at a small angle. Reusing the same
    // brush construction at a different scale exposes the hairs at the centre
    // and lets their ends dissolve toward the rim.
    for (int petal = 0; petal < kPetals; ++petal) {
      const float angle = TWO_PI * ((float)petal + 0.38f) / (float)kPetals;
      const Pigment& pigment = kPigments[petal % kPigments.size()];
      pen.push();
      pen.rotate(angle);
      pen.translate(28 + pen.random(-5, 9), pen.random(-4, 4));
      brush(pen, pen.random(170, 255), pen.random(18, 36), pen.random(-48, 48),
            pigment, 0.92f);
      pen.pop();
    }

    // Short pale hairs turn the centre into the visibly loaded heel of the
    // brush rather than a mechanical pivot shared by all curves.
    pen.blendMode(ADD);
    for (int i = 0; i < 180; ++i) {
      const float angle = pen.random(TWO_PI);
      const float radius = std::sqrt(pen.random()) * 70.0f;
      const float x = std::cos(angle) * radius;
      const float y = std::sin(angle) * radius;
      pen.stroke(255, 207 + pen.random(35), 170 + pen.random(60),
                 pen.random(16, 50));
      pen.strokeWeight(pen.random(0.5f, 2.2f));
      pen.line(x, y, x + std::cos(angle) * pen.random(5, 22),
               y + std::sin(angle) * pen.random(5, 22));
    }
    pen.pop();
    pen.noLoop();
  }
};

}  // namespace

SIGIL_SKETCH(BristleBloom, "Draw · Procedural",
             "A radial bloom painted by translucent bundles of imperfect "
             "Bezier bristles.")
