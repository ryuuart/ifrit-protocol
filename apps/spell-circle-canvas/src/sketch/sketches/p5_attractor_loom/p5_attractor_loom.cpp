/** @file
 * p5_attractor_loom — iterated strange-attractor traces woven into a field.
 *
 * Every thread follows a Clifford attractor. Its segments are batched through
 * the canvas the Pen lends so chaotic turns never create pathological joins.
 * Nearby seeds diverge into repeated folds, and slowly moving coefficients
 * make those folds breathe without translating the frame. A live material
 * shades the traces through a nested grain field; the material follows the
 * geometry instead of supplying the pattern.
 */

// TAGS: Drawing/Generative

#include <sigilmaterial/program/Shader.h>
#include <include/core/SkPathBuilder.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>

#include <cmath>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace field = sigil::material::field;
using namespace sigil::draw;

namespace {

constexpr int kThreads = 9;
constexpr int kSettlingSteps = 90;
constexpr int kTraceSteps = 720;

material::Material threadInk(material::Material program) {
  return std::move(program)
      .slot("uGrain", field::grain(0.07f, 3, 41.0f))
      .quantizeTime(30.0f);
}

material::Material ground() {
  return material::from(sigil::material::Color{0.018f, 0.025f, 0.052f, 1.0f}).layer(field::grain(0.018f, 4, 29.0f, 0.8f, 1.7f), {.blend = material::BlendMode::SoftLight, .opacity = 0.18f});
}

struct P5AttractorLoom {
  material::Material threads = material::Color{0, 0, 0, 0};
  const material::Material background = ground();

  void setup(sketch::SketchContext& context) {
    threads = threadInk(material::shader(context.assets.hub(), context.local("thread.sksl"),
                       {.textures = {{"uGrain", {}}}}));
    // The loom is a direct function of the clock, so the plate is the
    // first moment.
    sketch::kit::stage(context, {.size = {900, 720},
                                 .captureAt = 0.05,
                                 .background = material::Color{
                                     4 / 255.0f, 6 / 255.0f, 14 / 255.0f, 1}});

    context.composer.render(compose::graphics("p5_attractor_loom.loop",
                                              [this](Pen& pen) { draw(pen); }));
  }

  void trace(Pen& pen, int thread, float clock) {
    const float a = -1.66f + 0.055f * std::sin(clock * 0.17f);
    const float b = 1.20f + 0.045f * std::sin(clock * 0.13f + 1.4f);
    const float c = -1.47f + 0.050f * std::cos(clock * 0.19f + 0.7f);
    const float d = -0.86f + 0.040f * std::sin(clock * 0.11f + 2.2f);
    float x = 0.014f * thread + 0.03f;
    float y = -0.011f * thread - 0.02f;

    for (int step = 0; step < kSettlingSteps; ++step) {
      const float nextX = std::sin(a * y) + c * std::cos(a * x);
      const float nextY = std::sin(b * x) + d * std::cos(b * y);
      x = nextX;
      y = nextY;
    }

    SkPathBuilder segments;
    SkPoint previous = {0.0f, 0.0f};
    for (int step = 0; step < kTraceSteps; ++step) {
      const float nextX = std::sin(a * y) + c * std::cos(a * x);
      const float nextY = std::sin(b * x) + d * std::cos(b * y);
      x = nextX;
      y = nextY;
      const float px = pen.width * (0.50f + x * 0.205f);
      const float py = pen.height * (0.50f + y * 0.225f);
      const SkPoint point = {px, py};
      if (step > 0) {
        segments.moveTo(previous);
        segments.lineTo(point);
      }
      previous = point;
    }
    if (const SkPaint* stroke = pen.strokePaint())
      pen.canvas()->drawPath(segments.detach(), *stroke);
  }

  void draw(Pen& pen) {
    if (pen.frameCount == 1) {
      pen.noFill();
      pen.strokeCap(ROUND);
      pen.strokeJoin(ROUND);
    }
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(background);

    pen.noFill();
    pen.blendMode(BLEND);
    pen.stroke(material::skia::paint(threads), CANVAS);
    pen.strokeWeight(1.9f);
    for (int thread = 0; thread < kThreads; ++thread)
      trace(pen, thread, clock + thread * 0.012f);

    pen.blendMode(BLEND);
  }
};

}  // namespace

SIGIL_SKETCH_AS(P5AttractorLoom, "p5_attractor_loom", "Draw · Generative",
                "Long strange-attractor traces woven in live grained ink and "
                "slowly breathing Clifford coefficients.")
