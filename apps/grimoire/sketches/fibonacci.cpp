/** @file
 * fibonacci — the golden angle and the golden rectangle, side by side.
 *
 * Two pictures of one number. On the left, phyllotaxis: seeds dealt round
 * a centre, each a golden angle (137.508°) past the one before, at a
 * radius growing with the square root of its index so every seed has the
 * same area — the arrangement of a sunflower head. It is the geometry
 * library's radial pattern with that step and that growth, and nothing
 * here computes an angle.
 *
 * On the right, the square spiral: squares whose sides are successive
 * Fibonacci numbers, each laid against the rectangle the ones before it
 * make, turning a quarter each time, with a quarter circle drawn across
 * each square. The rectangles they leave tend to the golden ratio, and
 * the arcs join into the golden spiral.
 *
 * EDIT THESE FIRST
 *   kSeeds     how many seeds the head holds
 *   kSquares   how many squares the spiral lays
 */

// TAGS: Drawing/Generative, Patterns/Ornament

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Points.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace compose = sigil::compose;
namespace path = sigil::geometry::path;
namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

constexpr SkSize kCanvas = {1240, 680};
constexpr float kPanelWidth = 560;
constexpr float kPanelHeight = 420;
constexpr int kSeeds = 900;
constexpr int kSquares = 11;

/** THE HEAD: the radial pattern at the golden angle with square-root
 *  growth, a square at every seed, larger towards the rim. */
compose::Element sunflower() {
  return compose::pen(
      "fibonacci.phyllotaxis",
      [](Pen& pen) {
        const std::vector<glm::vec2> seeds = path::points(
            path::Rect::of({0, 0}, {pen.width, pen.height}),
            path::radial(kSeeds, {.stepDegrees = 137.508f,
                                  .growth = path::Growth::SquareRoot}));
        pen.background(10, 11, 15);
        pen.noStroke();
        pen.rectMode(CENTER);
        for (size_t index = 0; index < seeds.size(); ++index) {
          const float fraction = (float)index / (float)seeds.size();
          const float side = 1.5f + fraction * 9.0f;
          pen.fill(230, 212, 173, 90 + 165 * fraction);
          pen.rect(seeds[index].x, seeds[index].y, side, side);
        }
      },
      compose::Cache::Texture);
}

/** One square of the spiral: its corner, its side, and which way it was
 *  laid (0 right, 1 above, 2 left, 3 below the rectangle before it). */
struct Square {
  glm::vec2 corner;
  float side;
  int turn;
};

/** THE SQUARES, in units of the first, with y up. The first is taken as
 *  laid below, so the turn order right, above, left, below starts with
 *  the second. */
std::vector<Square> spiral() {
  std::vector<Square> squares{{{0, 0}, 1, 3}};
  glm::vec2 low{0, 0}, high{1, 1};
  float previous = 0, current = 1;
  for (int index = 1; index < kSquares; ++index) {
    const float side = previous + current;
    previous = current;
    current = side;
    const int turn = (index - 1) % 4;
    glm::vec2 corner;
    switch (turn) {
      case 0:
        corner = {high.x, low.y};
        break;
      case 1:
        corner = {low.x, high.y};
        break;
      case 2:
        corner = {low.x - side, low.y};
        break;
      default:
        corner = {low.x, low.y - side};
        break;
    }
    squares.push_back({corner, side, turn});
    low = glm::min(low, corner);
    high = glm::max(high, corner + glm::vec2{side, side});
  }
  return squares;
}

/** THE SPIRAL: every square outlined and tinted, and across each a
 *  quarter circle about the corner its turn names, so the arcs meet. */
compose::Element squareSpiral() {
  return compose::pen(
      "fibonacci.squares",
      [](Pen& pen) {
        const std::vector<Square> squares = spiral();
        glm::vec2 low{0, 0}, high{0, 0};
        for (const Square& square : squares) {
          low = glm::min(low, square.corner);
          high = glm::max(high, square.corner + square.side);
        }
        const float scale = std::min((pen.width - 40) / (high.x - low.x),
                                     (pen.height - 40) / (high.y - low.y));
        pen.background(10, 11, 15);
        pen.push();
        // y up in the squares' units, so the page's y is flipped.
        pen.translate(pen.width * 0.5f, pen.height * 0.5f);
        pen.scale(scale, -scale);
        pen.translate(-(low.x + high.x) * 0.5f, -(low.y + high.y) * 0.5f);
        pen.strokeWeight(1.0f / scale);
        for (size_t index = 0; index < squares.size(); ++index) {
          const Square& square = squares[index];
          const float shade = 0.12f + 0.5f * (float)index / squares.size();
          pen.fill(230 * shade, 212 * shade, 173 * shade);
          pen.stroke(255, 60);
          pen.rect(square.corner.x, square.corner.y, square.side, square.side);
        }
        pen.noFill();
        pen.stroke(240, 200, 120);
        pen.strokeWeight(2.0f / scale);
        pen.beginShape();
        for (const Square& square : squares) {
          const glm::vec2 far = square.corner + square.side;
          // The arc's centre is the corner opposite the way it was laid.
          const glm::vec2 centre =
              square.turn == 0   ? glm::vec2{square.corner.x, far.y}
              : square.turn == 1 ? square.corner
              : square.turn == 2 ? glm::vec2{far.x, square.corner.y}
                                 : far;
          const float start = (square.turn - 1) * HALF_PI;
          for (int step = 0; step <= 24; ++step) {
            const float angle = start + HALF_PI * step / 24.0f;
            pen.vertex(centre.x + std::cos(angle) * square.side,
                       centre.y + std::sin(angle) * square.side);
          }
        }
        pen.endShape();
        pen.pop();
      },
      compose::Cache::Texture);
}

}  // namespace

struct Fibonacci {
  void setup(sketch::Scene2DContext& context) {
    sketch::kit::stage(
        context,
        {.theme = sketch::kit::featureTheme(sketch::kit::Density::Spacious),
         .size = kCanvas,
         .captureAt = 0.05});

    const sketch::kit::Cell panel{
        .plate = {.width = kPanelWidth, .height = kPanelHeight}};
    context.composer.render(sketch::kit::page(
        {.title = "Fibonacci",
         .subtitle = "The golden angle dealt round a centre, and the golden "
                     "rectangle grown square by square",
         .footer = "path::points(rect, path::radial(count, {.stepDegrees = "
                   "137.508, .growth = SquareRoot}))"},
        sketch::kit::comparison(
            {.cases = {{.title = "PHYLLOTAXIS",
                        .control = "radial · golden angle · square-root growth",
                        .figure = sketch::kit::cell(panel, "", "", sunflower()),
                        .note = "Equal area per seed: the spirals a reader "
                                "counts are consecutive Fibonacci numbers."},
                       {.title = "THE SQUARE SPIRAL",
                        .control = "1, 1, 2, 3, 5, 8 … a quarter turn each",
                        .figure =
                            sketch::kit::cell(panel, "", "", squareSpiral()),
                        .note = "Each square is the sum of the two before; "
                                "the quarter arcs join into the golden "
                                "spiral."}},
             .measure = 1160,
             .gap = 40})));
  }
};

SIGIL_SKETCH(Fibonacci, "Draw · Generative",
             "Golden-angle phyllotaxis through the geometry library's radial "
             "pattern, beside the Fibonacci squares and their spiral.")
