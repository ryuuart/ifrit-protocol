/** @file
 * flocking — Reynolds' three steerings as one force, at three balances.
 *
 * A flock is three steerings over the birds within reach: away from the
 * ones too close (separation), along with the ones nearby (alignment) and
 * towards where they are (cohesion). SigilMotion states them as ONE force,
 * `physics::boids()`, because the three share one neighbourhood grid over
 * the set; the weights are ratios, and the shape of a flock is those
 * ratios. Each panel below is the same birds from the same seed under a
 * different balance.
 *
 * A bird also cruises: a push along its own heading against a drag, which
 * is what holds it near one speed without a clamp — the push is the
 * library's escape hatch, a `ForceKind::Body` force, and the drag is
 * `physics::drag()`. The trails are a kept canvas washed with a
 * translucent ground every frame, which fades all the way to the ground.
 *
 * EDIT THESE FIRST
 *   kBirds      how many birds each panel flies
 *   kReach      how far a bird looks for neighbours, px
 *   kCruise     the push along the heading; the speed is kCruise / kDrag
 *   kSteering   how hard the three steerings pull together, per second
 *               squared for each pixel of steering
 *   the three Flocking weights on each case below
 */

// TAGS: Drawing/Generative, Motion/Physics

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcore/compute/Chance.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Pen.h>
#include <sigilmotion/physics/Forces.h>
#include <sigilmotion/physics/Points.h>
#include <sigilmotion/physics/Verlet.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace chance = sigil::core::chance;
namespace compose = sigil::compose;
namespace physics = sigil::motion::physics;
namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

constexpr SkSize kCanvas = {1240, 620};
constexpr float kPanelWidth = 372;
constexpr float kPanelHeight = 340;
constexpr int kBirds = 60;
constexpr float kReach = 46.0f;
constexpr float kCruise = 90.0f;
constexpr float kSteering = 4.0f;
constexpr float kDrag = 1.2f;
constexpr double kStepRate = 60.0;
/** How far past the edge a bird flies before it re-enters opposite. */
constexpr float kMargin = 12.0f;

/** THE CRUISE: a push along each bird's own heading, at the force's
 *  strength. Against the drag beside it, a bird settles near
 *  strength / drag whatever the steerings ask. */
void cruise(physics::Points& birds, float, const physics::Force& force) {
  for (size_t index = 0; index < birds.size(); ++index) {
    const glm::vec2 velocity = birds.velocity[index];
    const float speed = glm::length(velocity);
    if (speed > 0.0f) birds.force[index] += velocity * (force.strength / speed);
  }
}

/** ONE PANEL'S FLOCK: its birds, its forces, and the stepper. */
struct Flock {
  physics::Points birds;
  std::vector<physics::Force> forces;
  physics::Verlet stepper{.timeStep =
                              std::chrono::duration<double>(1.0 / kStepRate)};

  explicit Flock(physics::Flocking weights) {
    forces = {
        physics::boids(weights, kReach, kSteering),
        {.kind = physics::ForceKind::Body, .strength = kCruise, .body = cruise},
        physics::drag(kDrag)};
    // The same seed in every panel, so the panels differ only in weights.
    chance::Stream rolls = chance::Stream::xorshift(0xC2A16u);
    for (int bird = 0; bird < kBirds; ++bird) {
      const glm::vec2 at{rolls.unit() * kPanelWidth,
                         rolls.unit() * kPanelHeight};
      const float heading = rolls.unit() * TAU;
      birds.add(at, glm::vec2{std::cos(heading), std::sin(heading)} * 60.0f);
    }
  }

  /** One fixed step, then every bird that left the panel brought round to
   *  the opposite edge, its step's start moved with it. */
  void step() {
    stepper.step(birds, forces);
    for (size_t index = 0; index < birds.size(); ++index) {
      glm::vec2& at = birds.position[index];
      const glm::vec2 before = at;
      if (at.x < -kMargin) at.x += kPanelWidth + 2 * kMargin;
      if (at.x > kPanelWidth + kMargin) at.x -= kPanelWidth + 2 * kMargin;
      if (at.y < -kMargin) at.y += kPanelHeight + 2 * kMargin;
      if (at.y > kPanelHeight + kMargin) at.y -= kPanelHeight + 2 * kMargin;
      birds.previous[index] += at - before;
    }
  }
};

/** THE PANEL: a kept canvas washed by a translucent ground each frame,
 *  every bird a small dart along its heading. */
compose::Element flight(const std::string& key, const Flock& flock, float hue) {
  return compose::graphics(key, [&flock, hue](Pen& pen) {
    if (pen.frameCount == 1) pen.background(8, 9, 14);
    pen.background(8, 9, 14, 14);
    pen.colorMode(HSB, 360, 100, 100, 255);
    pen.noStroke();
    pen.fill(hue, 62, 100);
    for (size_t index = 0; index < flock.birds.size(); ++index) {
      const glm::vec2 at = flock.birds.position[index];
      const glm::vec2 velocity = flock.birds.velocity[index];
      pen.push();
      pen.translate(at.x, at.y);
      pen.rotate(std::atan2(velocity.y, velocity.x));
      pen.triangle(7, 0, -5, -3.5f, -5, 3.5f);
      pen.pop();
    }
    pen.colorMode(RGB, 255);
  });
}

}  // namespace

struct Flocking {
  std::array<std::unique_ptr<Flock>, 3> flocks;

  void setup(sketch::Scene2DContext& context) {
    sketch::kit::stage(context, {.styleSheet = sketch::kit::featureStyleSheet(
                                     sketch::kit::Density::Spacious),
                                 .size = kCanvas,
                                 .captureAt = 6.0});

    flocks = {std::make_unique<Flock>(physics::Flocking{
                  .separation = 3.0f, .alignment = 0.4f, .cohesion = 0.2f}),
              std::make_unique<Flock>(physics::Flocking{
                  .separation = 2.0f, .alignment = 1.6f, .cohesion = 0.6f}),
              std::make_unique<Flock>(physics::Flocking{
                  .separation = 0.6f, .alignment = 1.0f, .cohesion = 2.4f})};
    context.engine.timer(
        [this] {
          for (const auto& flock : flocks) flock->step();
        },
        {.stepRate = kStepRate});

    const sketch::kit::Cell panel{
        .plate = {.width = kPanelWidth, .height = kPanelHeight}};
    const auto figure = [&](int which, float hue) {
      return sketch::kit::cell(
          panel, "", "",
          flight("flocking." + std::to_string(which), *flocks[which], hue));
    };

    context.composer.render(sketch::kit::page(
        {.title = "Flocking",
         .subtitle = "Separation, alignment and cohesion as one force over "
                     "one neighbourhood",
         .footer = "physics::boids(weights, reach, strength) · a cruise "
                   "against physics::drag() · stepped by physics::Verlet at "
                   "a fixed rate"},
        sketch::kit::comparison(
            {.cases = {{.title = "SEPARATION LEADS",
                        .control = "{3, 0.4, 0.2}",
                        .figure = figure(0, 196),
                        .note = "Birds keep their distance and drift as a "
                                "loose, even scatter."},
                       {.title = "ALIGNMENT LEADS",
                        .control = "{2, 1.6, 0.6}",
                        .figure = figure(1, 38),
                        .note = "Neighbours agree on a heading, and the "
                                "panel fills with parallel streams."},
                       {.title = "COHESION LEADS",
                        .control = "{0.6, 1.0, 2.4}",
                        .figure = figure(2, 340),
                        .note = "Birds gather into knots that wheel and "
                                "merge."}},
             .measure = 1180,
             .gap = 32})));
  }
};

SIGIL_SKETCH(Flocking, "Draw · Generative",
             "Reynolds' separation, alignment and cohesion as SigilMotion's "
             "one boids force, flown at three balances with trails on a "
             "kept canvas.")
