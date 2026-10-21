/** @file
 * flow_fields — one angle field read four ways.
 *
 * A flow field is a number at every place, read as a heading. Every panel
 * on this sheet reads THE SAME FIELD over the same rectangle, so the four
 * pictures are four readings of one flow rather than four flows:
 *
 *   GRID SAMPLES        a short line at every grid point, pointing where
 *                       the field points;
 *   RANDOM SAMPLES      the same at seeded random places, thousands of
 *                       them in one batched draw;
 *   STREAMLINES         a walk that keeps stepping along the heading,
 *                       inked with a live material whose noise child and
 *                       clock move the colour while the paths hold;
 *   PARTICLES           a point set pushed by the field as a wind,
 *                       stepped at a fixed rate, and deposited on a
 *                       canvas that is never cleared.
 *
 * The field is a `core::noise::Field`, the value SigilMotion's `wind()`
 * force reads, so the panel that steps particles hands the field itself
 * to the force and nothing here converts it.
 *
 * EDIT THESE FIRST
 *   kCurrent     the field: its frequency is the size of the eddies
 *   kSamples     how many random samples the second panel draws
 *   kRibbons     how many brush gestures enter the particle panel
 *   kBristles    how many particles travel together in each gesture
 */

// TAGS: Drawing/Generative, Patterns/Noise, Motion/Physics

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcore/compute/Chance.h>
#include <sigilcore/compute/Field.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/physics/Forces.h>
#include <sigilmotion/physics/Points.h>
#include <sigilmotion/physics/Verlet.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace chance = sigil::core::chance;
namespace compose = sigil::compose;
namespace field = sigil::material::field;
namespace material = sigil::material;
namespace noise = sigil::core::noise;
namespace physics = sigil::motion::physics;
namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

constexpr SkSize kCanvas = {1240, 1060};
/** Every panel's picture, and the rectangle the field is read over. */
constexpr float kPanelWidth = 560;
constexpr float kPanelHeight = 300;

/** THE FIELD every panel reads: simplex noise whose one number, taken
 *  over a whole turn, is the heading at that place. */
const noise::Field kCurrent{.kind = noise::FieldKind::Simplex,
                            .seed = 809,
                            .dimension = 2,
                            .frequency = 0.0024f,
                            .octaves = 2};

constexpr int kSamples = 5200;
constexpr int kRibbons = 11;
constexpr int kBristles = 9;
constexpr double kStepRate = 60.0;

/** The heading at @p x, @p y: the field's number over a whole turn, which
 *  is exactly how `wind()` reads it. */
float headingAt(float x, float y) { return kCurrent.at(x, y) * TAU; }

/** The sheet's own voice: the house feature sheet. */
sketch::kit::Theme sheetTheme() {
  return sketch::kit::featureTheme(sketch::kit::Density::Spacious);
}

const sketch::kit::Cell kPanel{
    .plate = {.width = kPanelWidth, .height = kPanelHeight}};

/** THE GRID: one segment per grid point, the run of them one draw. */
compose::Element gridSamples() {
  return compose::pen(
      "flow_fields.grid",
      [](Pen& pen) {
        constexpr float kStep = 14.0f;
        pen.background(0);
        pen.stroke(255);
        pen.strokeWeight(1.0f);
        pen.strokeCap(SQUARE);
        pen.beginShape(LINES);
        for (float x = kStep * 0.5f; x < pen.width; x += kStep)
          for (float y = kStep * 0.5f; y < pen.height; y += kStep) {
            const float angle = headingAt(x, y);
            pen.vertex(x, y);
            pen.vertex(x + std::cos(angle) * kStep, y + std::sin(angle) * kStep);
          }
        pen.endShape();
      },
      compose::Cache::Texture);
}

/** THE SAME FIELD AT RANDOM PLACES: one seeded stream places every
 *  sample, and every segment goes down in one draw. */
compose::Element randomSamples() {
  return compose::pen(
      "flow_fields.random",
      [](Pen& pen) {
        constexpr float kLength = 18.0f;
        chance::Stream places = chance::Stream::xorshift(0x6BCECDu);
        pen.background(0);
        pen.stroke(255, 190);
        pen.strokeWeight(0.55f);
        pen.strokeCap(SQUARE);
        pen.beginShape(LINES);
        for (int index = 0; index < kSamples; ++index) {
          const float x = places.unit() * pen.width;
          const float y = places.unit() * pen.height;
          const float angle = headingAt(x, y);
          pen.vertex(x, y);
          pen.vertex(x + std::cos(angle) * kLength,
                     y + std::sin(angle) * kLength);
        }
        pen.endShape();
      },
      compose::Cache::Texture);
}

/** The light a streamline ends in. */
material::Material sparkLight() {
  return material::radialGradient(
      {0.34f, 0.30f}, 0.92f,
      {{0.0f, {1.0f, 1.0f, 0.88f, 1.0f}},
       {0.32f, {0.30f, 0.94f, 1.0f, 0.96f}},
       {1.0f, {0.18f, 0.08f, 0.42f, 0.0f}}},
      {.extent = material::RadialExtent::ClosestSide});
}

/** WHAT A PIGMENT IS on the particle panel's paper. */
constexpr std::array<material::Color, 5> kPigments = {{
    {0.08f, 0.18f, 0.19f, 1.0f},
    {0.12f, 0.35f, 0.39f, 1.0f},
    {0.76f, 0.20f, 0.14f, 1.0f},
    {0.84f, 0.48f, 0.10f, 1.0f},
    {0.36f, 0.16f, 0.29f, 1.0f},
}};
constexpr material::Color kPaper{244 / 255.0f, 238 / 255.0f, 221 / 255.0f, 1};

/** One step's movement of one particle: where it went from and to, and
 *  which pigment and which hair width it lays. */
struct Mark {
  glm::vec2 from;
  glm::vec2 to;
  uint8_t pigment;
  uint8_t width;
};

}  // namespace

struct FlowFields {
  material::Material ink = material::Color{0, 0, 0, 0};
  material::Material sparks = sparkLight();

  /** THE PARTICLES: a point set the field pushes as a wind, a steady
   *  current carries left to right, and drag holds to a speed. */
  physics::Points particles;
  std::vector<uint8_t> pigment;
  std::vector<uint8_t> width;
  std::vector<Mark> pending;
  chance::Stream rolls = chance::Stream::xorshift(0x7F4A7C15u);
  const std::array<physics::Force, 3> forces{
      physics::wind(kCurrent, 80.0f), physics::gravity({70.0f, 0.0f}),
      physics::drag(1.0f)};
  const physics::Verlet stepper{
      .timeStep = std::chrono::duration<double>(1.0 / kStepRate)};

  void setup(sketch::Scene2DContext& context) {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(context, {.size = kCanvas, .captureAt = 5.2});

    ink = material::shader(context.assets.hub(), context.local("flow.sksl"),
                           {.textures = {{"uField", {}}}})
              .slot("uField", field::noise(0.025f, 4, 23.0f))
              .quantizeTime(30.0f);

    seedParticles();
    context.engine.timer([this] { step(); }, {.stepRate = kStepRate});

    const sketch::kit::Comparison sampled{
        .cases = {{.title = "GRID SAMPLES",
                   .control = "beginShape(LINES) · one segment a grid point",
                   .figure = sketch::kit::cell(kPanel, "", "", gridSamples()),
                   .note = "The field read where a lattice says: even, and "
                           "blind between its points."},
                  {.title = "RANDOM SAMPLES",
                   .control = "chance::Stream · five thousand in one draw",
                   .figure =
                       sketch::kit::cell(kPanel, "", "", randomSamples()),
                   .note = "Seeded places, so the feathering is the field's "
                           "and the picture rerolls only with the seed."}},
        .measure = 1160,
        .gap = 40};
    const sketch::kit::Comparison followed{
        .cases = {{.title = "STREAMLINES",
                   .control = "vertex() walk · material ink, CANVAS",
                   .figure = sketch::kit::cell(kPanel, "", "", streamlines()),
                   .note = "Each line keeps stepping along the heading; the "
                           "ink's noise child and clock move the colour, "
                           "not the paths."},
                  {.title = "PARTICLES ON A KEPT CANVAS",
                   .control = "physics::wind(field) · graphics()",
                   .figure = sketch::kit::cell(kPanel, "", "", deposit()),
                   .note = "Bundles of particles pushed by the same field, "
                           "stepped at a fixed rate, each step laid on a "
                           "canvas that is never cleared."}},
        .measure = 1160,
        .gap = 40};

    context.composer.render(sketch::kit::page(
        {.title = "One angle field, four readings",
         .subtitle = "Every panel reads the same noise field over the same "
                     "rectangle",
         .footer = "The field is a core::noise::Field: the value wind() "
                   "reads, handed to the force unchanged."},
        compose::box().column().gap(34).children(
            {sketch::kit::section(
                 {.label = "SAMPLED", .note = "The heading drawn where it is read"},
                 sketch::kit::comparison(sampled)),
             sketch::kit::section(
                 {.label = "FOLLOWED",
                  .note = "The heading taken as the way to go next"},
                 sketch::kit::comparison(followed))})));
  }

  /** THE STREAMLINES: a grid of seeds drifting with the clock, each walked
   *  along the field in fixed steps, every eighth ending in a spark. */
  compose::Element streamlines() {
    return compose::pen("flow_fields.streamlines", [this](Pen& pen) {
      constexpr int kAcross = 9;
      constexpr int kDown = 5;
      constexpr int kSteps = 36;
      constexpr float kStepLength = 5.0f;
      const float clock = static_cast<float>(pen.millis() * 0.001);
      pen.background(4, 7, 17);
      pen.blendMode(ADD);
      pen.strokeCap(ROUND);
      pen.strokeJoin(ROUND);
      pen.noFill();
      pen.stroke(material::skia::paint(ink), CANVAS);
      pen.strokeWeight(3.0f);

      const glm::vec2 module =
          arrange::moduleSize({pen.width, pen.height}, kAcross, kDown, {0, 0});
      for (int row = 0; row < kDown; ++row)
        for (int column = 0; column < kAcross; ++column) {
          const int seed = row * kAcross + column;
          chance::Stream jitter = chance::Stream::mix64(seed);
          float x = (column + 0.5f + jitter.signedUnit() * 0.31f) * module.x +
                    11.0f * std::sin(clock * 0.31f + seed * 0.73f);
          float y = (row + 0.5f + jitter.signedUnit() * 0.31f) * module.y +
                    8.0f * std::cos(clock * 0.27f + seed * 0.51f);
          pen.beginShape();
          for (int step = 0; step < kSteps; ++step) {
            if (x < -20.0f || x > pen.width + 20.0f || y < -20.0f ||
                y > pen.height + 20.0f)
              break;
            pen.vertex(x, y);
            const float angle = headingAt(x, y);
            x += std::cos(angle) * kStepLength;
            y += std::sin(angle) * kStepLength;
          }
          pen.endShape();

          if (seed % 8 == 0) {
            pen.push();
            pen.noStroke();
            pen.fill(material::skia::paint(sparks), SHAPE);
            pen.circle(x, y, 12.0f + 3.0f * std::sin(clock * 1.6f + seed));
            pen.pop();
          }
        }
      pen.blendMode(BLEND);
    });
  }

  /** THE DEPOSIT: the paper is laid on the canvas's first frame, and every
   *  frame after lays the steps taken since — a wide wet pass and a narrow
   *  dry one per pigment, each one batched draw. */
  compose::Element deposit() {
    return compose::graphics("flow_fields.deposit", [this](Pen& pen) {
      if (pen.frameCount == 1)
        pen.background(material::grained(kPaper, 0.10f, 0.05f));
      pen.blendMode(MULTIPLY);
      pen.strokeCap(ROUND);
      for (uint8_t which = 0; which < kPigments.size(); ++which)
        for (const bool wet : {true, false}) {
          for (uint8_t hair = 0; hair < 2; ++hair) {
            const float weight = hair == 0 ? 0.6f : 1.2f;
            pen.stroke(material::withAlpha(kPigments[which],
                                           wet ? 0.05f : 0.42f));
            pen.strokeWeight(wet ? weight * 4.6f : weight);
            pen.beginShape(LINES);
            for (const Mark& mark : pending)
              if (mark.pigment == which && mark.width == hair) {
                pen.vertex(mark.from.x, mark.from.y);
                pen.vertex(mark.to.x, mark.to.y);
              }
            pen.endShape();
          }
        }
      pen.blendMode(BLEND);
      pending.clear();
    });
  }

  /** A gesture of @p kBristles particles side by side, entering at the left
   *  edge or, for the first gestures, anywhere on the panel. */
  void placeRibbon(int ribbon, bool anywhere) {
    const float x = anywhere ? rolls.unit() * kPanelWidth : -10.0f;
    const float y = 20.0f + rolls.unit() * (kPanelHeight - 40.0f);
    const float spread = 0.9f + rolls.unit();
    for (int hair = 0; hair < kBristles; ++hair) {
      const size_t index = (size_t)(ribbon * kBristles + hair);
      const glm::vec2 at{x + rolls.signedUnit() * 3.0f,
                         y + (hair - 0.5f * (kBristles - 1)) * spread};
      particles.position[index] = at;
      particles.previous[index] = at;
      particles.velocity[index] = {70.0f + rolls.unit() * 30.0f, 0.0f};
    }
  }

  void seedParticles() {
    particles.clear();
    pigment.clear();
    width.clear();
    for (int ribbon = 0; ribbon < kRibbons; ++ribbon)
      for (int hair = 0; hair < kBristles; ++hair) {
        particles.add({});
        pigment.push_back((uint8_t)(ribbon % kPigments.size()));
        width.push_back((uint8_t)(hair % 2));
      }
    for (int ribbon = 0; ribbon < kRibbons; ++ribbon)
      placeRibbon(ribbon, true);
  }

  /** ONE FIXED STEP: the forces move every particle, the movement is
   *  queued for the canvas, and a gesture that has left the panel whole
   *  re-enters at the left. A missing hair now and then is what makes a
   *  bundle read as a brush rather than a stream plot. */
  void step() {
    stepper.step(particles, forces);
    for (size_t index = 0; index < particles.size(); ++index)
      if (rolls.unit() > 0.055f)
        pending.push_back({particles.previous[index],
                           particles.position[index], pigment[index],
                           width[index]});
    for (int ribbon = 0; ribbon < kRibbons; ++ribbon) {
      bool gone = true;
      for (int hair = 0; hair < kBristles && gone; ++hair) {
        const glm::vec2 at =
            particles.position[(size_t)(ribbon * kBristles + hair)];
        gone = at.x > kPanelWidth + 20.0f || at.x < -60.0f ||
               at.y < -40.0f || at.y > kPanelHeight + 40.0f;
      }
      if (gone) placeRibbon(ribbon, false);
    }
  }
};

SIGIL_SKETCH(FlowFields, "Draw · Generative",
             "One noise angle field read four ways: grid samples, random "
             "samples in one batched draw, streamlines in a live material "
             "ink, and particles pushed by it as a wind onto a kept canvas.")
