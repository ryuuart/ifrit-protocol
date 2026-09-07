/** @file
 * observable_reynolds_steering — alignment, cohesion and separation flocking.
 */

#include <sigilsketch/draw/Draw.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;
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

struct ObservableReynoldsSteering final : sketch::DrawSketch {
  std::vector<Boid> boids;

  void setup(sketch::DrawContext& context) override {
    context.canvas(900, 720);
    context.captureAt(5.0);
    context.pen.randomSeed(0xC2A16u);
    context.pen.colorMode(HSB, 360, 100, 100, 255);
    for (int index = 0; index < 50; ++index)
      boids.push_back({{450.0f, 360.0f},
                       {context.pen.random(-1, 1), context.pen.random(-1, 1)}});
  }

  Vec seek(const Boid& boid, Vec target) const {
    return ((target - boid.position).normalized() * 8.0f - boid.velocity)
        .limited(0.05f);
  }

  void update() {
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
        if (distance < 25.0f && distance > 0.0f) {
          separation += delta.normalized() / distance;
          ++close;
        }
        if (distance < 50.0f) {
          alignment += boids[other].velocity;
          centre += boids[other].position;
          ++nearby;
        }
      }
      Vec force;
      if (close > 0)
        force += ((separation / close).normalized() * 8.0f - boid.velocity)
                     .limited(0.05f);
      if (nearby > 0) {
        force += ((alignment / nearby).normalized() * 8.0f - boid.velocity)
                     .limited(0.05f);
        force += seek(boid, centre / nearby);
      }
      forces[index] = force;
    }
    for (size_t index = 0; index < boids.size(); ++index) {
      Boid& boid = boids[index];
      boid.velocity = (boid.velocity + forces[index]).limited(8.0f);
      boid.position += boid.velocity;
      if (boid.position.x < -20) boid.position.x = 920;
      if (boid.position.x > 920) boid.position.x = -20;
      if (boid.position.y < -20) boid.position.y = 740;
      if (boid.position.y > 740) boid.position.y = -20;
    }
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(0, 1);
    update();
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
