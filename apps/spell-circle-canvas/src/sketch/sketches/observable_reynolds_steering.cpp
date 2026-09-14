/** @file
 * observable_reynolds_steering — alignment, cohesion and separation flocking.
 */

// TAGS: Drawing/Generative, Motion/Physics

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Draw.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
using namespace sigil::draw;

namespace {

struct Vec {
  float x = 0;
  float y = 0;

  Vec operator+(Vec other) const { return {x + other.x, y + other.y}; }
  Vec operator-(Vec other) const { return {x - other.x, y - other.y}; }
  Vec operator*(float scalar) const { return {x * scalar, y * scalar}; }
  Vec operator/(float scalar) const { return {x / scalar, y / scalar}; }
  Vec& operator+=(Vec other) {
    x += other.x;
    y += other.y;
    return *this;
  }
  float length() const { return std::hypot(x, y); }
  Vec limited(float maximum) const {
    const float magnitude = length();
    return magnitude > maximum ? *this * (maximum / magnitude) : *this;
  }
  Vec normalized() const {
    const float magnitude = length();
    return magnitude > 0.0001f ? *this / magnitude : Vec{};
  }
};

struct Boid {
  Vec position;
  Vec velocity;
};

/** EDIT THESE FIRST: how fast a boid may fly, how much of that one frame
 *  may change, and how far it looks for a neighbour to avoid and for one
 *  to keep step with. */
constexpr float kSpeed = 8.0f;
constexpr float kForce = 0.05f;
constexpr float kTooClose = 25.0f;
constexpr float kNeighbour = 50.0f;
constexpr int kBoids = 50;
constexpr float kMargin = 20.0f;  // how far past the edge a boid wraps

struct ObservableReynoldsSteering final : sketch::Sketch {
  std::vector<Boid> boids;

  void setup(sketch::SketchContext& context) override {
    context.canvas(900, 720);
    context.captureAt(5.0);
    boids.clear();

    context.composer.render(
        compose::graphics("observable_reynolds_steering.loop",
                          [this](Pen& pen) { draw(pen); })
            .absolute()
            .inset(0));
  }

  /** REYNOLDS' STEERING TERM: where the boid WANTS to go, at full speed,
   *  less what it is already doing, truncated to what one frame may
   *  change. Separation, alignment and cohesion are all this term — they
   *  differ only in what they desire. */
  static Vec steer(Vec desired, Vec velocity) {
    return (desired.normalized() * kSpeed - velocity).limited(kForce);
  }

  void update(float width, float height) {
    std::vector<Vec> forces(boids.size());
    for (size_t index = 0; index < boids.size(); ++index) {
      const Boid& boid = boids[index];
      Vec separation;
      Vec alignment;
      Vec centre;
      int close = 0;
      int nearby = 0;
      for (size_t other = 0; other < boids.size(); ++other) {
        if (index == other) continue;
        const Vec delta = boid.position - boids[other].position;
        const float distance = delta.length();
        if (distance < kTooClose && distance > 0.0f) {
          separation += delta.normalized() / distance;
          ++close;
        }
        if (distance < kNeighbour) {
          alignment += boids[other].velocity;
          centre += boids[other].position;
          ++nearby;
        }
      }
      Vec force;
      if (close > 0) force += steer(separation / close, boid.velocity);
      if (nearby > 0) {
        force += steer(alignment / nearby, boid.velocity);
        force += steer(centre / nearby - boid.position, boid.velocity);
      }
      forces[index] = force;
    }
    for (size_t index = 0; index < boids.size(); ++index) {
      Boid& boid = boids[index];
      boid.velocity = (boid.velocity + forces[index]).limited(kSpeed);
      boid.position += boid.velocity;
      // The field wraps a margin outside the canvas, so a boid leaves and
      // returns rather than appearing on the edge it left from.
      if (boid.position.x < -kMargin) boid.position.x = width + kMargin;
      if (boid.position.x > width + kMargin) boid.position.x = -kMargin;
      if (boid.position.y < -kMargin) boid.position.y = height + kMargin;
      if (boid.position.y > height + kMargin) boid.position.y = -kMargin;
    }
  }

  void draw(Pen& pen) {
    if (pen.frameCount == 1) {
      pen.randomSeed(0xC2A16u);
      pen.colorMode(HSB, 360, 100, 100, 255);
      for (int index = 0; index < kBoids; ++index)
        boids.push_back({{pen.width * 0.5f, pen.height * 0.5f},
                         {pen.random(-1, 1), pen.random(-1, 1)}});
      // THE GROUND, laid once. The canvas this loop keeps opens with
      // nothing on it, and the wash below is one alpha step: over a
      // transparent surface it never builds up, so the trails would
      // stand on whatever is behind the canvas rather than on black.
      pen.background(0);
    }
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(0, 1);
    update(pen.width, pen.height);
    pen.noFill();
    pen.stroke(
        std::fmod(180.0f + std::cos(clock * 0.06f) * 180.0f + 360.0f, 360.0f),
        100, 100);
    pen.strokeWeight(1.0f);
    for (const Boid& boid : boids) {
      pen.push();
      pen.translate(boid.position.x, boid.position.y);
      pen.rotate(std::atan2(boid.velocity.y, boid.velocity.x) + HALF_PI);
      pen.beginShape();
      pen.vertex(0, -40);
      pen.vertex(-20, 40);
      pen.vertex(20, 40);
      pen.endShape(CLOSE);
      pen.pop();
    }
  }
};

}  // namespace

SIGIL_SKETCH_AS(
    ObservableReynoldsSteering, "observable_reynolds_steering",
    "Draw · Observable reproductions",
    "Fifty triangular boids combine separation, alignment and cohesion.")
