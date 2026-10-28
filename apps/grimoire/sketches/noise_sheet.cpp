/** @file
 * noise_sheet — coherent noise against chance.
 *
 * Noise and randomness are both seeded numbers, and the difference
 * between them is what NEIGHBOURS do. A random draw keyed on a position
 * owes nothing to the draw beside it; coherent noise is built so nearby
 * inputs answer nearby numbers, which is what makes it read as terrain,
 * cloud or a hand's tremor rather than static.
 *
 *   A CURVE           the pen's noise along a line beside random heights
 *                     drawn from a stream, both moving with the clock;
 *   A MAP             coherent noise over a plane as ONE field image, a
 *                     material the device evaluates per pixel;
 *   A WALK            chance accumulated: each step one of four ways, the
 *                     path wandering from the centre.
 *
 * EDIT THESE FIRST
 *   kWalkStep    how far one step of the walk goes, px
 *   the field in mapPanel()
 */

// TAGS: Drawing/Generative, Patterns/Noise

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcore/compute/Chance.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/field/Field.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace chance = sigil::core::chance;
namespace compose = sigil::compose;
namespace field = sigil::material::field;
namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

constexpr SkSize kCanvas = {1240, 600};
constexpr float kPanelWidth = 372;
constexpr float kPanelHeight = 340;
constexpr float kWalkStep = 10.0f;

/** THE CURVE: the noise read along x, one vertex a pixel, scrolling with
 *  the clock; beside it a polyline whose heights are fresh draws three
 *  times a second. */
compose::Element curvePanel() {
  return compose::pen("noise_sheet.curve", [](Pen& pen) {
    if (pen.frameCount == 1) pen.noiseSeed(0x8B554407u);
    const float clock = static_cast<float>(pen.millis() * 0.001);
    pen.background(10, 11, 15);
    pen.noFill();
    pen.strokeJoin(ROUND);

    pen.stroke(90, 140, 230);
    pen.strokeWeight(2.0f);
    chance::Stream heights =
        chance::Stream::mix64((uint64_t)std::floor(clock * 3.0f) + 0x448u);
    pen.beginShape();
    for (float x = 0; x <= pen.width + 24; x += 24)
      pen.vertex(x, pen.height * (0.1f + 0.8f * heights.unit()));
    pen.endShape();

    pen.stroke(230, 212, 173);
    pen.strokeWeight(3.0f);
    pen.beginShape();
    for (float x = 0; x <= pen.width; x += 1)
      pen.vertex(x, pen.height * (0.1f + 0.8f * pen.noise(x / pen.width * 4.0f +
                                                          clock * 0.15f)));
    pen.endShape();
  });
}

/** THE MAP: coherent noise over the whole panel as one field image. */
compose::Element mapPanel() {
  return compose::pen(
      "noise_sheet.map",
      [](Pen& pen) { pen.background(field::grain(0.012f, 4, 7.0f, 1.0f)); },
      compose::Cache::Texture);
}

/** THE WALK: a step one of four ways per draw from a stream, as many
 *  steps as the clock has reached, each a disc shaded by how far it has
 *  wandered from the centre. */
compose::Element walkPanel() {
  return compose::pen("noise_sheet.walk", [](Pen& pen) {
    const float clock = static_cast<float>(pen.millis() * 0.001);
    const int shown = 60 + static_cast<int>(std::fmod(clock * 60.0f, 600.0f));
    chance::Stream ways = chance::Stream::mix64(0x5EED7A1u);
    const glm::vec2 centre{pen.width * 0.5f, pen.height * 0.5f};
    glm::vec2 at = centre;
    pen.background(10, 11, 15);
    pen.stroke(10, 11, 15);
    pen.strokeWeight(1.0f);
    for (int step = 0; step < shown; ++step) {
      switch (ways.bits() & 3u) {
        case 0:
          at.x += kWalkStep;
          break;
        case 1:
          at.x -= kWalkStep;
          break;
        case 2:
          at.y += kWalkStep;
          break;
        default:
          at.y -= kWalkStep;
          break;
      }
      at = glm::clamp(at, glm::vec2{kWalkStep, kWalkStep},
                      glm::vec2{pen.width, pen.height} - kWalkStep);
      const float near = std::clamp(
          1.0f - glm::length(at - centre) / (pen.height * 0.5f), 0.0f, 1.0f);
      pen.fill(90 + 140 * near, 80 + 132 * near, 70 + 103 * near);
      pen.circle(at.x, at.y, kWalkStep);
    }
  });
}

}  // namespace

struct NoiseSheet {
  void setup(sketch::Scene2DContext& context) {
    sketch::kit::stage(context, {.styleSheet = sketch::kit::featureStyleSheet(
                                     sketch::kit::Density::Spacious),
                                 .size = kCanvas,
                                 .captureAt = 6.0});

    const sketch::kit::Cell panel{
        .plate = {.width = kPanelWidth, .height = kPanelHeight}};
    context.composer.render(sketch::kit::page(
        {.title = "Noise against chance",
         .subtitle = "Both are seeded numbers; what differs is what "
                     "neighbours answer",
         .footer = "pen.noise() and field::grain() are coherent; "
                   "chance::Stream draws are not."},
        sketch::kit::comparison(
            {.cases = {{.title = "A CURVE",
                        .control = "pen.noise(x) · Stream::unit()",
                        .figure =
                            sketch::kit::cell(panel, "", "", curvePanel()),
                        .note = "The noise curve drifts as one line; the "
                                "random one jumps at every vertex and "
                                "rerolls three times a second."},
                       {.title = "A MAP",
                        .control = "field::grain(0.012, 4) · one image",
                        .figure = sketch::kit::cell(panel, "", "", mapPanel()),
                        .note = "The same idea over a plane, evaluated per "
                                "pixel by one material rather than a rect "
                                "per cell."},
                       {.title = "A WALK",
                        .control = "Stream::bits() & 3 · one of four ways",
                        .figure = sketch::kit::cell(panel, "", "", walkPanel()),
                        .note = "Chance accumulated: no step knows the last, "
                                "yet the path wanders as a whole."}},
             .measure = 1180,
             .gap = 32})));
  }
};

SIGIL_SKETCH(NoiseSheet, "Draw · Generative",
             "Coherent noise against chance: a noise curve beside random "
             "heights, the noise map as one field image, and a random walk.")
