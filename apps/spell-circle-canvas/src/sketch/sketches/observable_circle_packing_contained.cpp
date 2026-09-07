/** @file
 * observable_circle_packing_contained — growing circles inside one boundary.
 */

#include <sigilsketch/draw/Draw.h>

#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

struct Circle {
  SkPoint centre;
  float radius = 5.0f;
  bool growing = true;
  SkColor4f colour;
};

struct ObservableCirclePackingContained final : sketch::DrawSketch {
  static constexpr float kBoundary = 255.0f;
  std::vector<Circle> circles;

  void setup(sketch::DrawContext& context) override {
    context.canvas(800, 800);
    context.captureAt(3.0);
    context.pen.randomSeed(0xCB81F23Bu);
  }

  bool outside(const Circle& circle) const {
    return std::hypot(circle.centre.x() - 400.0f, circle.centre.y() - 400.0f) +
               circle.radius >
           kBoundary;
  }

  bool overlaps(const Circle& candidate, float padding = 2.0f) const {
    for (const Circle& circle : circles) {
      if ((candidate.centre - circle.centre).length() <
          candidate.radius + circle.radius + padding)
        return true;
    }
    return false;
  }

  void addCircle(Pen& pen) {
    for (int attempt = 0; attempt < 10; ++attempt) {
      Circle candidate{{pen.random(400.0f - kBoundary, 400.0f + kBoundary),
                        pen.random(400.0f - kBoundary, 400.0f + kBoundary)},
                       5.0f,
                       true,
                       {pen.random(), pen.random(), pen.random(), 1.0f}};
      if (!outside(candidate) && !overlaps(candidate)) {
        circles.push_back(candidate);
        return;
      }
    }
  }

  void grow() {
    for (size_t index = 0; index < circles.size(); ++index) {
      Circle& circle = circles[index];
      if (!circle.growing) continue;
      Circle next = circle;
      next.radius += 1.0f;
      bool blocked = outside(next);
      for (size_t other = 0; other < circles.size() && !blocked; ++other)
        if (index != other && (next.centre - circles[other].centre).length() <
                                  next.radius + circles[other].radius + 1.0f)
          blocked = true;
      if (blocked)
        circle.growing = false;
      else
        circle.radius = next.radius;
    }
  }

  void draw(sketch::DrawContext& context) override {
    Pen& pen = context.pen;
    pen.background(0);
    pen.noFill();
    pen.stroke(255);
    pen.strokeWeight(1.0f);
    pen.circle(400.0f, 400.0f, kBoundary * 2.0f);
    pen.stroke(0, 110);
    for (const Circle& circle : circles) {
      pen.fill(circle.colour.fR * 255.0f, circle.colour.fG * 255.0f,
               circle.colour.fB * 255.0f);
      pen.circle(circle.centre.x(), circle.centre.y(), circle.radius * 2.0f);
    }
    addCircle(pen);
    grow();
  }
};

}  // namespace

SIGIL_SKETCH_AS(ObservableCirclePackingContained,
                "observable_circle_packing_contained",
                "Draw · Observable reproductions",
                "Growing random circles pack inside one circular boundary.")
