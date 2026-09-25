/** @file
 * p5_refractive_metaballs — merging liquid glass and drawn tendrils.
 *
 * Eight moving radial fields become one implicit surface. The surface can
 * split into islands, grow a neck, merge and separate without a tessellated
 * outline. Its material derives a normal from the same field, refracts a live
 * algorithmic backdrop through a child shader, disperses its channels and
 * lights the changing edge. Pen-drawn Bezier filaments connect the field's
 * moving centres, so the geometry and the material share one animation.
 */

// TAGS: Drawing/Generative, Materials/Shaders

#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>

#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace material = sigil::material;
using namespace sigil::draw;

namespace {

constexpr int kLobeCount = 8;
constexpr float kThreshold = 1.16f;

struct Lobe {
  SkPoint centre;
  float radius;
};

material::skia::Paint lineField(sk_sp<SkRuntimeEffect> program) {
  return material::skia::Paint::sksl(std::move(program)).quantizeTime(30.0f);
}

material::skia::Paint tendrilInk(sk_sp<SkRuntimeEffect> program) {
  return material::skia::Paint::sksl(std::move(program)).quantizeTime(30.0f);
}

std::array<float, 4> uniform(const Lobe& lobe) {
  return {lobe.centre.x(), lobe.centre.y(), lobe.radius, 0.0f};
}

/** THE REFRACTION, over @p source. The effect is handed in rather than
 *  read here, because this is asked for on every frame. */
material::skia::Paint glass(const sk_sp<SkRuntimeEffect>& effect,
                   const material::skia::Paint& source,
                   const std::array<Lobe, kLobeCount>& lobes) {
  material::skia::Paint paint = material::skia::Paint::sksl(effect, {{"uThreshold", kThreshold},
                                                   {"uStrength", 42.0f}})
                           .slot("uSource", source)
                           .quantizeTime(30.0f);
  for (int index = 0; index < kLobeCount; ++index)
    paint =
        paint.uniform("uBall" + std::to_string(index), uniform(lobes[index]));
  return paint;
}

SkPoint mix(SkPoint a, SkPoint b, float amount) { return a + (b - a) * amount; }

void drawTendril(Pen& pen, SkPoint from, SkPoint to, int index, float clock,
                 float offset) {
  const SkPoint delta = to - from;
  const float length = std::max(delta.length(), 1.0f);
  const SkPoint normal = {-delta.y() / length, delta.x() / length};
  const float curl =
      std::sin(clock * (0.74f + index * 0.017f) + index * 1.71f) *
      (66.0f + std::fmod(index * 17.0f, 52.0f));
  const float ripple = std::cos(clock * 1.13f + index * 0.83f) * 38.0f;
  const SkPoint controlA = mix(from, to, 0.28f) + normal * (curl + offset);
  const SkPoint controlB =
      mix(from, to, 0.72f) + normal * (-curl * 0.58f + ripple + offset);
  pen.bezier(from.x(), from.y(), controlA.x(), controlA.y(), controlB.x(),
             controlB.y(), to.x(), to.y());
}

struct P5RefractiveMetaballs {
  material::skia::Paint source;
  material::skia::Paint filament;
  /** Held on the sketch: the frame asks for it. */
  sk_sp<SkRuntimeEffect> refraction;

  void setup(sketch::SketchContext& context) {
    refraction = context.assets.shader(context.local("glass.sksl"));
    source = lineField(context.assets.shader(context.local("line_field.sksl")));
    filament = tendrilInk(context.assets.shader(context.local("tendril.sksl")));
    context.canvas(720, 720);
    context.background({2 / 255.0f, 5 / 255.0f, 14 / 255.0f, 1});
    context.captureAt(0.05);  // the field is a direct function of the clock

    context.composer.render(compose::graphics("p5_refractive_metaballs.loop",
                                              [this](Pen& pen) { draw(pen); }));
  }

  std::array<Lobe, kLobeCount> lobes(float clock) const {
    const float left = std::sin(clock * 0.82f);
    const float right = std::sin(clock * 0.71f + 2.0f);
    const float vertical = std::cos(clock * 0.63f + 0.8f);
    return {{{{302.0f + std::sin(clock * 0.47f) * 22.0f,
               352.0f + std::cos(clock * 0.54f) * 17.0f},
              112.0f},
             {{418.0f + std::cos(clock * 0.43f) * 24.0f,
               360.0f + std::sin(clock * 0.51f) * 20.0f},
              108.0f},
             {{132.0f + left * 88.0f, 224.0f + std::cos(clock * 0.91f) * 42.0f},
              88.0f},
             {{142.0f + std::cos(clock * 0.68f + 0.4f) * 82.0f,
               514.0f + std::sin(clock * 0.77f) * 48.0f},
              92.0f},
             {{584.0f - right * 86.0f,
               220.0f + std::sin(clock * 0.88f + 1.2f) * 48.0f},
              90.0f},
             {{578.0f - std::cos(clock * 0.73f + 1.1f) * 84.0f,
               520.0f + std::cos(clock * 0.84f) * 42.0f},
              96.0f},
             {{356.0f + std::sin(clock * 0.66f + 1.7f) * 74.0f,
               102.0f + vertical * 88.0f},
              82.0f},
             {{366.0f + std::cos(clock * 0.61f + 2.5f) * 70.0f,
               610.0f - vertical * 78.0f},
              88.0f}}};
  }

  void drawTendrils(Pen& pen, const std::array<Lobe, kLobeCount>& balls,
                    float clock) const {
    constexpr std::array<std::pair<int, int>, 13> links = {
        std::pair{0, 1}, std::pair{0, 2}, std::pair{0, 3}, std::pair{1, 4},
        std::pair{1, 5}, std::pair{0, 6}, std::pair{1, 6}, std::pair{0, 7},
        std::pair{1, 7}, std::pair{2, 6}, std::pair{4, 6}, std::pair{3, 7},
        std::pair{5, 7}};

    // THE WHOLE WEAVE, at one standoff from the line between two centres:
    // the halo is one pass down the middle and the filament is three.
    const auto weave = [&](float offset) {
      for (int index = 0; index < (int)links.size(); ++index) {
        const auto [from, to] = links[index];
        drawTendril(pen, balls[from].centre, balls[to].centre, index, clock,
                    offset);
      }
    };

    pen.noFill();
    pen.blendMode(ADD);
    pen.stroke(30, 205, 255, 30);
    pen.strokeWeight(5.0f);
    weave(0.0f);

    pen.stroke(filament, CANVAS);
    pen.strokeWeight(1.35f);
    for (float offset : {-7.0f, 0.0f, 7.0f}) weave(offset);
  }

  void draw(Pen& pen) {
    if (pen.frameCount == 1) {
      pen.strokeCap(ROUND);
      pen.strokeJoin(ROUND);
    }
    const float clock = static_cast<float>(pen.millis() * 0.001);
    const std::array<Lobe, kLobeCount> balls = lobes(clock);
    pen.background(source);

    pen.blendMode(BLEND);
    pen.noStroke();
    pen.fill(glass(refraction, source, balls), CANVAS);
    pen.rect(0.0f, 0.0f, pen.width, pen.height);

    drawTendrils(pen, balls, clock);
    pen.blendMode(BLEND);
  }
};

}  // namespace

SIGIL_SKETCH_AS(P5RefractiveMetaballs, "p5_refractive_metaballs",
                "Draw · Generative",
                "Merging liquid-glass fields refract a live line shader while "
                "animated Pen tendrils weave through their moving centres.")
