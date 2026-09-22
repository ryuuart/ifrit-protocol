/** @file
 * draw_with_scope — the imperative door of the operator family: a pen
 * program handed the same table the connecting operators read.
 *
 * The cards are the ones `connect_by_lane` wires: each states its tier
 * and its calls, and here a third fact, `load`, the traffic a call
 * carries. No stock operator draws a wire whose weight is a fact, so a
 * program does — `drawWith(drawTraffic)` attaches one pen over the panel
 * and hands it the scope: every card's key, facts and bounds, settled.
 * The program reads `calls` and `load` off each card and draws each wire
 * with SigilDraw's own verbs, weighted by the load and labelled at its
 * midpoint. The second panel is the same program keyed, so the pen it
 * attaches prunes while the cards hold still; the third draws the load
 * as a bar under each card instead — the same facts, another reading.
 *
 * EDIT THESE FIRST
 *   kServices — the cards, their tiers, their calls and their loads.
 *   kWeight — px of wire per unit of load.
 */

// TAGS: Kit/Layout

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 640};
constexpr float kCell = 328;
constexpr float kPicture = 300;
constexpr float kRowHeight = 92;
constexpr float kCardWidth = 84;
constexpr float kCardHeight = 34;
constexpr float kWeight = 0.9f;  // px of wire per unit of load

struct Service {
  const char* name;
  int tier;
  std::vector<std::string> calls;
  float load;
};

const std::vector<Service> kServices = {
    {"gateway", 0, {"auth", "ledger"}, 6.0f},
    {"auth", 1, {"cache"}, 2.0f},
    {"ledger", 1, {"cache", "store"}, 4.0f},
    {"cache", 2, {}, 0.0f},
    {"store", 2, {}, 0.0f},
};

/** Rows by tier, the cards of a row spread evenly across the box. */
struct Tiers {
  float rowHeight = kRowHeight;
  bool operator==(const Tiers&) const = default;

  void arrange(Arrangement& arrangement) const {
    std::map<int, std::vector<Arrangement::Child*>> rows;
    for (Arrangement::Child& child : arrangement.children)
      rows[(int)child.number("tier").value_or(0)].push_back(&child);
    for (auto& [tier, cards] : rows) {
      const float step = arrangement.box.width() / (float)(cards.size() + 1);
      for (size_t i = 0; i < cards.size(); ++i)
        cards[i]->centreAt({step * (float)(i + 1),
                            rowHeight / 2 + (float)tier * rowHeight});
    }
  }
};

Element card(const Service& service) {
  return box()
      .key(service.name)
      .width(kCardWidth)
      .height(kCardHeight)
      .borderRadius({6})
      .fill(Fill::color(sketch::kit::theme().palette.cellGround))
      .foreground(stroke(1.0f, Fill::color(sketch::kit::theme().palette.rule)))
      .attribute("tier", service.tier)
      .attribute("calls", service.calls)
      .attribute("load", service.load)
      .justifyContent(Justify::Center)
      .alignItems(Align::Center)
      .children({text(service.name)});
}

/** THE PROGRAM: a wire per call, as wide as the load, labelled midway. */
void drawTraffic(sigil::draw::Pen& pen, const Scope& scope) {
  const SkColor4f ink = sketch::kit::theme().palette.figure;
  pen.noFill();
  for (const Scope::Node* node : scope.having("calls")) {
    const float load = node->number("load").value_or(1.0f);
    const auto calls = node->attribute<std::vector<std::string>>("calls");
    if (!calls) continue;
    for (const std::string& name : *calls) {
      const Scope::Node* target = scope.find(name);
      if (!target) continue;
      const SkPoint from = node->bounds.center();
      const SkPoint to = target->bounds.center();
      pen.stroke(ink);
      pen.strokeWeight(std::max(1.0f, load * kWeight));
      pen.line(from, to);
      pen.noStroke();
      pen.fill(ink);
      pen.textSize(10);
      pen.text(std::to_string((int)load), (from.x() + to.x()) / 2 + 6,
               (from.y() + to.y()) / 2 - 4);
      pen.noFill();
    }
  }
}

/** THE SAME FACTS, ANOTHER READING: the load as a bar under each card. */
void drawLoadBars(sigil::draw::Pen& pen, const Scope& scope) {
  pen.noStroke();
  pen.fill(sketch::kit::theme().palette.figure);
  for (const Scope::Node& node : scope.nodes()) {
    const float load = node.number("load").value_or(0.0f);
    if (load <= 0) continue;
    pen.rect(node.bounds.left(), node.bounds.bottom() + 4,
             node.bounds.width() * (load / 6.0f), 3);
  }
}

Element panel(std::vector<Operator> operators) {
  return sketch::kit::well({.width = kCell, .height = kPicture})
      .children({box()
                     .inset(0)
                     .operators(std::move(operators))
                     .children({each(kServices, card)})});
}

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const char* note, Element figure) {
  return {.title = title,
          .control = call,
          .figure = std::move(figure),
          .note = note};
}

}  // namespace

struct DrawWithScope {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    ctx.composer.render(sketch::kit::page(
        {.title = "Drawn from the facts",
         .subtitle = "A pen program is handed the same table the connecting "
                     "operators read, and draws what no stock operator does.",
         .footer = "drawWith attaches one pen over the scope; keyed, the pen "
                   "prunes while the cards hold still. Its output is pixels: "
                   "nothing downstream reads it."},
        box().column().gap(22).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  THE SAME CARDS, THREE PROGRAMS",
                  .note = "Weighted wires · the same, keyed · the load as a "
                          "bar"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("WEIGHTED WIRES", "drawWith(drawTraffic).zIndex(-1)",
                            "Each wire as wide as the load its card states, "
                            "labelled at its midpoint, behind the cards.",
                            panel({Tiers{},
                                   drawWith(drawTraffic).zIndex(-1)})),
                       cell("THE SAME, KEYED",
                            "drawWith(\"traffic\", drawTraffic)",
                            "The key vouches for the program, so the pen it "
                            "attaches prunes while the scope holds still.",
                            panel({Tiers{},
                                   drawWith("traffic", drawTraffic).zIndex(-1)})),
                       cell("ANOTHER READING", "drawWith(drawLoadBars)",
                            "The same load fact read as a bar under each card "
                            "rather than as a wire.",
                            panel({Tiers{}, drawWith(drawLoadBars)}))},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(DrawWithScope, "Kit · API",
             "five service cards stating tier, calls and load, drawn over "
             "by pen programs handed the scope: wires weighted by load and "
             "labelled, the same keyed, and the load read as a bar")
