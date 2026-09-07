/** @file
 * observable_circle_packing — circles added and grown until they collide.
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

struct ObservableCirclePacking final : sketch::DrawSketch {
  std::vector<Circle> circles;

  void setup(sketch::DrawContext& context) override {
    context.canvas(900, 720);
    context.captureAt(4.0);
    context.pen.randomSeed(0x89FB1EB3u);
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
    for (int attempt = 0; attempt < 8; ++attempt) {
      Circle candidate{{pen.random(pen.width), pen.random(pen.height)},
                       5.0f,
                       true,
                       {pen.random(), pen.random(), pen.random(), 1.0f}};
      if (!overlaps(candidate)) {
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
      bool blocked = next.centre.x() - next.radius < 0 ||
                     next.centre.x() + next.radius > 900 ||
                     next.centre.y() - next.radius < 0 ||
                     next.centre.y() + next.radius > 720;
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

SIGIL_SKETCH_AS(ObservableCirclePacking, "observable_circle_packing",
                "Draw · Observable reproductions",
                "Random circles grow one pixel per frame until they collide.")
