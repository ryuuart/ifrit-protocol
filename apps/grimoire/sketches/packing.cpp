/** @file
 * packing — circles grown until they touch, inside a wall that is a shape.
 *
 * The loop is the oldest in generative drawing: drop a small circle where
 * nothing is, grow every circle a step at a time, and stop each one the
 * moment it would touch a neighbour or the wall. Here the wall is an
 * OUTLINE, a value the geometry library answers containment and nearest
 * points for, so a rectangle and a star are the same loop with a
 * different value handed in.
 *
 * What a circle can reach is known at its birth: its centre never moves,
 * so the distance from the centre to the wall is the largest it may ever
 * be. That clearance is asked of the outline once, and only neighbours are
 * checked as it grows.
 *
 * EDIT THESE FIRST
 *   kBirthsPerStep   how many places a step tries for a new circle
 *   kStepRate        how many growth steps a second
 *   the two walls in setup()
 */

// TAGS: Drawing/Generative

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcore/compute/Chance.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Contour.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace chance = sigil::core::chance;
namespace compose = sigil::compose;
namespace material = sigil::material;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

constexpr SkSize kCanvas = {1240, 640};
constexpr float kPanelWidth = 560;
constexpr float kPanelHeight = 400;
constexpr int kBirthsPerStep = 3;
constexpr double kStepRate = 30.0;
/** The air left between two circles, and between a circle and the wall. */
constexpr float kGap = 1.5f;

constexpr std::array<material::Color, 5> kPigments = {{
    {0.90f, 0.83f, 0.68f, 1.0f},
    {0.80f, 0.42f, 0.30f, 1.0f},
    {0.36f, 0.58f, 0.62f, 1.0f},
    {0.62f, 0.66f, 0.42f, 1.0f},
    {0.56f, 0.44f, 0.62f, 1.0f},
}};

struct Circle {
  glm::vec2 centre;
  float radius = 1.0f;
  /** How far the centre stands from the wall: the most it may grow to. */
  float clearance = 0.0f;
  bool growing = true;
  uint8_t pigment = 0;
};

/** ONE WALL AND WHAT GREW IN IT. */
struct Packing {
  path::Outline wall;
  std::vector<path::Contour> contours;
  path::Rect bounds;
  std::vector<Circle> circles;
  chance::Stream rolls;

  Packing(path::Outline outline, uint64_t seed)
      : wall(std::move(outline)),
        contours(path::Contour::of(wall, true)),
        bounds(wall.bounds()),
        rolls(chance::Stream::xorshift(seed)) {}

  /** How far @p at stands inside the wall; nothing when it is outside. */
  float clearance(glm::vec2 at) const {
    if (!wall.contains(at)) return 0.0f;
    float nearest = std::numeric_limits<float>::max();
    for (const path::Contour& contour : contours)
      nearest = std::min(nearest, contour.nearest(at).gap);
    return nearest;
  }

  /** Whether a circle at @p at of @p radius would touch any but @p skip. */
  bool touches(glm::vec2 at, float radius, size_t skip) const {
    for (size_t other = 0; other < circles.size(); ++other)
      if (other != skip && glm::length(at - circles[other].centre) <
                               radius + circles[other].radius + kGap)
        return true;
    return false;
  }

  void step() {
    for (int attempt = 0; attempt < kBirthsPerStep; ++attempt) {
      const glm::vec2 at{
          bounds.min.x + rolls.unit() * (bounds.max.x - bounds.min.x),
          bounds.min.y + rolls.unit() * (bounds.max.y - bounds.min.y)};
      const float room = clearance(at) - kGap;
      const auto pigment = (uint8_t)(rolls.unit() * kPigments.size());
      if (room > 2.0f && !touches(at, 1.0f, circles.size()))
        circles.push_back({at, 1.0f, room, true, pigment});
    }
    for (size_t index = 0; index < circles.size(); ++index) {
      Circle& circle = circles[index];
      if (!circle.growing) continue;
      const float next = circle.radius + 1.0f;
      if (next > circle.clearance || touches(circle.centre, next, index))
        circle.growing = false;
      else
        circle.radius = next;
    }
  }
};

/** THE PANEL: the wall as a hairline and every circle in its pigment,
 *  @p offset placing the wall's own coordinates in the panel. */
compose::Element picture(const std::string& key, const Packing& packing,
                         glm::vec2 offset) {
  return compose::pen(key, [&packing, offset](Pen& pen) {
    pen.background(10, 11, 15);
    pen.translate(offset.x, offset.y);
    pen.noFill();
    pen.stroke(255, 120);
    pen.strokeWeight(1.0f);
    for (const path::Polyline& line : packing.wall.resampled({.spacing = 3})) {
      pen.beginShape();
      for (const glm::vec2 point : line.points) pen.vertex(point.x, point.y);
      pen.endShape(CLOSE);
    }
    pen.stroke(10, 11, 15);
    pen.strokeWeight(1.0f);
    for (const Circle& circle : packing.circles) {
      pen.fill(kPigments[circle.pigment]);
      pen.circle(circle.centre.x, circle.centre.y, circle.radius * 2.0f);
    }
  });
}

}  // namespace

struct PackingSheet {
  std::unique_ptr<Packing> boxed;
  std::unique_ptr<Packing> starred;

  void setup(sketch::Scene2DContext& context) {
    sketch::kit::stage(context, {.styleSheet = sketch::kit::featureStyleSheet(
                                     sketch::kit::Density::Spacious),
                                 .size = kCanvas,
                                 .captureAt = 5.0});

    constexpr float kInset = 12;
    boxed = std::make_unique<Packing>(
        path::Outline::rectangle(path::Rect::of(
            {kInset, kInset},
            {kPanelWidth - 2 * kInset, kPanelHeight - 2 * kInset})),
        0x89FB1EB3u);
    // The star is drawn square in the panel's height and centred across.
    const float side = kPanelHeight - 2 * kInset;
    starred = std::make_unique<Packing>(
        shapes::star(5, 0.48f).outline({side, side}), 0xCB81F23Bu);
    context.engine.timer(
        [this] {
          boxed->step();
          starred->step();
        },
        {.stepRate = kStepRate});

    const sketch::kit::Cell panel{
        .plate = {.width = kPanelWidth, .height = kPanelHeight}};
    context.composer.render(sketch::kit::page(
        {.title = "Packing",
         .subtitle = "Circles grow a step at a time until they touch a "
                     "neighbour or the wall",
         .footer = "The wall is a path::Outline: contains() admits a birth "
                   "and Contour::nearest() says how far it may grow."},
        sketch::kit::comparison(
            {.cases = {{.title = "A RECTANGLE WALL",
                        .control = "Outline::rectangle(rect)",
                        .figure = sketch::kit::cell(
                            panel, "", "",
                            picture("packing.rectangle", *boxed, {0, 0})),
                        .note = "Corners fill last, with the smallest "
                                "circles, because two walls crowd them."},
                       {.title = "A SILHOUETTE WALL",
                        .control = "shapes::star(5, 0.48).outline(size)",
                        .figure = sketch::kit::cell(
                            panel, "", "",
                            picture("packing.star", *starred,
                                    {(kPanelWidth - side) * 0.5f, kInset})),
                        .note = "The same loop with another outline handed "
                                "in; the points of the star pack down to "
                                "dust."}},
             .measure = 1160,
             .gap = 40})));
  }
};

SIGIL_SKETCH_AS(PackingSheet, "packing", "Draw · Generative",
                "Circles grown to contact inside a wall that is an outline "
                "value: a rectangle beside a star.")
