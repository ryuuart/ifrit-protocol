/** @file
 * l_systems — one rewriting value, one turtle, two grammars.
 *
 * A Lindenmayer system is a word rewritten in parallel: every letter with
 * a rule is replaced by that rule's word, every other letter is copied,
 * and the result is rewritten again. The word that comes out is read by a
 * TURTLE — `F` steps forward drawing, `+` and `-` turn, `[` remembers where
 * the turtle is and `]` returns there — so a few characters of grammar
 * become a curve or a tree.
 *
 * Both panels are the same two pieces with different values: a `Grammar`
 * (an axiom, its rules and how many generations) and a `Turtle` (the turn
 * and what a letter without a stroke leaves behind).
 *
 *   A SPACE-FILLING CURVE   two rules over `A` and `B`, turned a quarter at a
 *                           time, a leaf at every `A` and `B`, revealed
 *                           along the word as the clock runs;
 *   A BRACKETED TREE        one rule over `F` with branches, its angle
 *                           breathing with the clock.
 *
 * EDIT THESE FIRST
 *   the two Grammar values in setup()
 */

// TAGS: Drawing/Generative, Patterns/Ornament

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcore/compute/Chance.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace chance = sigil::core::chance;
namespace compose = sigil::compose;
namespace sketch = sigil::sketch;
using namespace sigil::draw;

namespace {

constexpr SkSize kCanvas = {1240, 720};
constexpr float kPanelWidth = 560;
constexpr float kPanelHeight = 460;

/** THE REWRITING VALUE: where the word starts, what each letter becomes,
 *  and how many times the whole word is rewritten. */
struct Grammar {
  std::string axiom;
  std::vector<std::pair<char, std::string>> rules;
  int generations = 1;

  /** The word after every generation. */
  [[nodiscard]] std::string word() const {
    std::string current = axiom;
    for (int generation = 0; generation < generations; ++generation) {
      std::string next;
      next.reserve(current.size() * 6);
      for (const char letter : current) {
        const auto rule =
            std::find_if(rules.begin(), rules.end(),
                         [letter](const auto& entry) { return entry.first == letter; });
        if (rule != rules.end())
          next += rule->second;
        else
          next += letter;
      }
      current = std::move(next);
    }
    return current;
  }
};

/** WHAT A TURTLE LEAVES: the segments it drew, each with the index of the
 *  letter that drew it, the leaves at letters without a stroke, and the
 *  box around all of it. */
struct Drawing {
  struct Segment {
    glm::vec2 from, to;
    size_t letter;
  };
  std::vector<Segment> segments;
  std::vector<std::pair<glm::vec2, size_t>> leaves;
  glm::vec2 low{0, 0}, high{0, 0};
};

/** THE TURTLE: unit steps, @p turn radians a turn, and a leaf at every
 *  letter in @p leafLetters. It reads at most @p length letters. */
Drawing walk(const std::string& word, float turn, std::string_view leafLetters,
             size_t length) {
  struct State {
    glm::vec2 at;
    float heading;
  };
  Drawing drawing;
  State turtle{{0, 0}, -HALF_PI};
  std::vector<State> stack;
  for (size_t index = 0; index < std::min(length, word.size()); ++index) {
    const char letter = word[index];
    if (letter == 'F') {
      const glm::vec2 next =
          turtle.at + glm::vec2{std::cos(turtle.heading), std::sin(turtle.heading)};
      drawing.segments.push_back({turtle.at, next, index});
      drawing.low = glm::min(drawing.low, next);
      drawing.high = glm::max(drawing.high, next);
      turtle.at = next;
    } else if (letter == '+') {
      turtle.heading += turn;
    } else if (letter == '-') {
      turtle.heading -= turn;
    } else if (letter == '[') {
      stack.push_back(turtle);
    } else if (letter == ']' && !stack.empty()) {
      turtle = stack.back();
      stack.pop_back();
    } else if (leafLetters.find(letter) != std::string_view::npos) {
      drawing.leaves.emplace_back(turtle.at, index);
    }
  }
  return drawing;
}

/** The box from @p low to @p high fitted into the pen's box with
 *  @p margin around it: the pen moved so the two centres meet, and the
 *  scale it was drawn up by. */
float fit(Pen& pen, glm::vec2 low, glm::vec2 high, float margin) {
  const glm::vec2 extent = glm::max(high - low, glm::vec2{1, 1});
  const float scale = std::min((pen.width - 2 * margin) / extent.x,
                               (pen.height - 2 * margin) / extent.y);
  pen.translate(pen.width * 0.5f, pen.height * 0.5f);
  pen.scale(scale);
  pen.translate(-(low.x + high.x) * 0.5f, -(low.y + high.y) * 0.5f);
  return scale;
}

}  // namespace

struct LSystems {
  std::string curve;
  std::string tree;
  /** The curve's whole extent, so a partly revealed curve stays put. */
  Drawing curveWhole;

  void setup(sketch::Scene2DContext& context) {
    const sketch::kit::Provide look(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    sketch::kit::stage(context, {.size = kCanvas, .captureAt = 7.48});

    curve = Grammar{.axiom = "A",
                    .rules = {{'A', "-BF+AFA+FB-"}, {'B', "+AF-BFB-FA+"}},
                    .generations = 5}
                .word();
    tree = Grammar{.axiom = "F",
                   .rules = {{'F', "FF+[+F-F-F]-[-F+F+F]"}},
                   .generations = 4}
               .word();
    curveWhole = walk(curve, HALF_PI, "AB", curve.size());

    const sketch::kit::Cell panel{
        .plate = {.width = kPanelWidth, .height = kPanelHeight}};
    context.composer.render(sketch::kit::page(
        {.title = "L-systems",
         .subtitle = "A word rewritten in parallel, then read by a turtle",
         .footer = "F steps forward drawing · + and − turn · [ remembers "
                   "the turtle · ] returns to it"},
        sketch::kit::comparison(
            {.cases = {{.title = "A SPACE-FILLING CURVE",
                        .control = "A → −BF+AFA+FB− · B → +AF−BFB−FA+ · 5 "
                                   "generations · 90°",
                        .figure = sketch::kit::cell(panel, "", "", curvePanel()),
                        .note = "Read along the word as the clock runs; a "
                                "leaf stands wherever the turtle meets A "
                                "or B."},
                       {.title = "A BRACKETED TREE",
                        .control = "F → FF+[+F−F−F]−[−F+F+F] · 4 "
                                   "generations · 25° ± 3°",
                        .figure = sketch::kit::cell(panel, "", "", treePanel()),
                        .note = "Brackets branch: the turtle returns to where "
                                "it stood, so one rule grows a crown."}},
             .measure = 1160,
             .gap = 40})));
  }

  compose::Element curvePanel() {
    return compose::pen("l_systems.curve", [this](Pen& pen) {
      const float clock = static_cast<float>(pen.millis() * 0.001);
      const float cycle = 0.5f - 0.5f * std::cos(clock * 0.42f);
      const auto shown = std::max<size_t>(
          1, static_cast<size_t>(curve.size() * (0.18f + cycle * 0.82f)));
      const Drawing drawing = walk(curve, HALF_PI, "AB", shown);
      pen.background(10, 11, 15);
      pen.push();
      const float scale = fit(pen, curveWhole.low, curveWhole.high, 30);
      pen.stroke(230, 212, 173);
      pen.strokeWeight(1.2f / scale);
      pen.beginShape(LINES);
      for (const Drawing::Segment& segment : drawing.segments) {
        pen.vertex(segment.from.x, segment.from.y);
        pen.vertex(segment.to.x, segment.to.y);
      }
      pen.endShape();
      pen.noStroke();
      pen.fill(240, 200, 120, 200);
      for (const auto& [at, letter] : drawing.leaves)
        pen.circle(at.x, at.y,
                   chance::Stream::mix64(letter).range(3.0f, 9.0f) / scale);
      pen.pop();
    });
  }

  compose::Element treePanel() {
    return compose::pen("l_systems.tree", [this](Pen& pen) {
      const float clock = static_cast<float>(pen.millis() * 0.001);
      const float turn = radians(25.0f + 3.0f * std::sin(clock * 0.55f));
      const Drawing drawing = walk(tree, turn, "", tree.size());
      pen.background(10, 11, 15);
      pen.push();
      const float scale = fit(pen, drawing.low, drawing.high, 24);
      pen.strokeCap(ROUND);
      pen.strokeWeight(1.0f / scale);
      // Shaded by where in the word a branch was drawn: the trunk dark,
      // the last twigs light. One batched draw per band of shade.
      constexpr int kBands = 6;
      for (int band = 0; band < kBands; ++band) {
        const float shade = 90.0f + 165.0f * (band + 0.5f) / kBands;
        pen.stroke(shade * 0.82f, shade * 0.92f, shade * 0.70f);
        pen.beginShape(LINES);
        for (const Drawing::Segment& segment : drawing.segments)
          if ((int)(segment.letter * kBands / tree.size()) == band) {
            pen.vertex(segment.from.x, segment.from.y);
            pen.vertex(segment.to.x, segment.to.y);
          }
        pen.endShape();
      }
      pen.pop();
    });
  }
};

SIGIL_SKETCH(LSystems, "Draw · Generative",
             "One rewriting value and one turtle read two grammars: a "
             "space-filling curve with leaves and a bracketed tree.")
