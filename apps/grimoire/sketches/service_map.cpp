/** @file
 * service_map — a service map whose wires are read off the cards.
 *
 * Every card states facts about itself and nothing about lines:
 * `attribute("tier", n)`, which the arranging operator reads to put it in
 * a row, `attribute("calls", {...})`, which a connecting operator reads to
 * wire it, and `attribute("load", n)`, the traffic its calls carry. The
 * cards never change; only the operator list on the panel does.
 *
 * WIRED BY AN OPERATOR. `Tiers` runs during layout and places the cards —
 * an arranging operator written here, in the sketch, from the same
 * `arrange()` door the stock layouts use. `connect::ByLane` runs once
 * layout has settled and attaches one wire per call, routed and dressed
 * as the operator says, painted behind the cards by its `zIndex`;
 * `connect::Between` states one pairing by hand; and a scope closes at a
 * nested panel, so the trunk from outside lands on the panel, never on a
 * card inside it.
 *
 * DRAWN BY A PROGRAM. No stock operator draws a wire whose weight is a
 * fact, so a pen program does: `drawWith(...)` attaches one pen over the
 * panel and hands it the scope — every card's key, facts and bounds,
 * settled — and the program reads `calls` and `load` off each card and
 * draws with SigilDraw's own verbs. Keyed, the pen it attaches prunes
 * while the cards hold still. The last cell reads the same load as a bar
 * under each card instead.
 *
 * EDIT THESE FIRST
 *   kServices — the cards, their tiers, who they call and what they carry.
 *   kRowHeight — how far apart the tiers stand, px.
 *   kWeight — px of wire per unit of load.
 */

// TAGS: Kit/Layout

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Connect.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1070};
constexpr float kCell = 328;
constexpr float kPicture = 300;
constexpr float kRowHeight = 92;  // how far apart the tiers stand, px
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
    {"cache", 2, {}, 1.0f},
    {"store", 2, {}, 3.0f},
};

/** The heaviest load any card states, so a bar's length is a fraction of
 *  the busiest card rather than of a number written here. */
float heaviestLoad() {
  float heaviest = 0;
  for (const Service& service : kServices)
    heaviest = std::max(heaviest, service.load);
  return heaviest;
}

/** ROWS BY TIER: each card at the row its fact names, the row's cards
 *  spread evenly across the box. */
struct Tiers {
  float rowHeight = kRowHeight;
  bool operator==(const Tiers&) const = default;

  void arrange(Arrangement& arrangement) const {
    std::map<int, std::vector<Arrangement::Child*>> rows;
    for (Arrangement::Child& child : arrangement.children)
      rows[(int)child.number("tier").value_or(0)].push_back(&child);
    for (auto& [tier, cards] : rows) {
      const float step = arrangement.box.width() / (float)(cards.size() + 1);
      for (size_t index = 0; index < cards.size(); ++index)
        cards[index]->centreAt({step * (float)(index + 1),
                                rowHeight / 2 + (float)tier * rowHeight});
    }
  }
};

/** A card stating its tier, its calls and its load, and nothing about
 *  lines. */
Element card(const Service& service) {
  return box()
      .key(service.name)
      .width(kCardWidth)
      .height(kCardHeight)
      .borderRadius({6})
      .fill(var("palette-cell-ground"))
      .foreground(stroke(1.0f, var("palette-rule")))
      .attribute("tier", service.tier)
      .attribute("calls", service.calls)
      .attribute("load", service.load)
      .justifyContent(Justify::Center)
      .alignItems(Align::Center)
      .children({text(service.name)});
}

Decoration wire() { return stroke(1.6f, var("palette-figure")); }

/** The panel: the five cards under the operators given. */
Element panel(std::vector<Operator> operators) {
  return sketch::kit::well({.width = kCell, .height = kPicture})
      .children({box()
                     .inset(0)
                     .operators(std::move(operators))
                     .children({each(kServices, card)})});
}

/** A nested panel with operators of its own: one node to the scope above
 *  it. Its cards stand on a ring and wire themselves. */
Element district() {
  return box()
      .key("district")
      .left(96)
      .top(84)
      .width(212)
      .height(200)
      .borderRadius({8})
      .foreground(stroke(1.0f, var("palette-rule")))
      .operators(
          {layouts::Radial{.radiusFraction = 0.6f},
           Operator(connect::ByLane{.lane = "calls", .gap = 2, .wire = wire()})
               .zIndex(-1)})
      .children({each(
          std::vector<Service>(kServices.begin() + 1, kServices.end()), card)});
}

/** A wire per call, as wide as the load, labelled midway. The program
 *  captures its ink while the feature theme is in scope. */
ScopeProgram drawTraffic() {
  const material::Color ink =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious).palette.figure;
  return [ink](sigil::draw::Pen& pen, const Scope& scope) {
    pen.noFill();
    for (const Scope::Node* node : scope.having("calls")) {
      const float load = node->number("load").value_or(1.0f);
      const auto calls = node->attribute<std::vector<std::string>>("calls");
      if (!calls) continue;
      for (const std::string& name : *calls) {
        const Scope::Node* target = scope.find(name);
        if (!target) continue;
        const glm::vec2 from = node->bounds.centre();
        const glm::vec2 to = target->bounds.centre();
        pen.stroke(ink);
        pen.strokeWeight(std::max(1.0f, load * kWeight));
        pen.line(from.x, from.y, to.x, to.y);
        pen.noStroke();
        pen.fill(ink);
        pen.textSize(10);
        const glm::vec2 across = to - from;
        const float length = std::max(1.0f, glm::length(across));
        const glm::vec2 middle = (from + to) * 0.5f;
        pen.text(std::to_string((int)load), middle.x + across.y / length * 12,
                 middle.y - across.x / length * 12);
        pen.noFill();
      }
    }
  };
}

/** THE SAME FACTS, ANOTHER READING: the load as a bar under each card,
 *  as long as the card where the load is the heaviest. */
ScopeProgram drawLoadBars() {
  const material::Color ink =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious).palette.figure;
  const float heaviest = heaviestLoad();
  return [ink, heaviest](sigil::draw::Pen& pen, const Scope& scope) {
    pen.noStroke();
    pen.fill(ink);
    for (const Scope::Node& node : scope.nodes()) {
      const float load = node.number("load").value_or(0.0f);
      if (load <= 0) continue;
      pen.rect(node.bounds.left(), node.bounds.bottom() + 4,
               node.bounds.width() * (load / heaviest), 3);
    }
  };
}

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const char* note, Element figure) {
  return {.title = title,
          .control = call,
          .figure = std::move(figure),
          .note = note};
}

}  // namespace

struct ServiceMap {
  void setup(sketch::Scene2DContext& ctx) {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(
        ctx,
        {.theme = sketch::kit::featureTheme(sketch::kit::Density::Spacious),
         .size = kCanvas,
         .captureAt = 0.05});
    ctx.composer.render(sketch::kit::page(
        {.title = "Wired by a fact",
         .subtitle = "Five cards state their tier, their calls and their "
                     "load. The operators on the panel read them.",
         .footer = "An arranging operator runs during layout; an adding one "
                   "runs after it. A pen program is handed the same scope "
                   "and draws what no stock operator does."},
        box().column().gap(26).children(
            {sketch::kit::section(
                 {.label = "01  WIRED BY AN OPERATOR",
                  .note = "Read off the cards · stated by hand · closed at "
                          "a nested panel"},
                 sketch::kit::comparison(
                     {.cases =
                          {cell("BY THE LANE",
                                "Tiers{}, ByLane{.lane = \"calls\"}",
                                "One wire per call each card states, routed "
                                "orthogonally, behind the cards.",
                                panel({Tiers{},
                                       Operator(
                                           connect::ByLane{
                                               .lane = "calls",
                                               .router = routers::orthogonal(
                                                   routers::Bend::MidX, 8),
                                               .gap = 4,
                                               .wire = wire()})
                                           .zIndex(-1)})),
                           cell("STATED BY HAND",
                                "Tiers{}, Between{\"gateway\", \"store\"}",
                                "The calls are ignored; one pairing is "
                                "stated in the operator and bowed as an arc.",
                                panel({Tiers{},
                                       Operator(connect::Between{
                                                    .from = "gateway",
                                                    .to = "store",
                                                    .router = routers::arc(
                                                        -0.35f),
                                                    .gap = 4,
                                                    .wire = wire()})
                                           .zIndex(-1)})),
                           cell("A CLOSED SCOPE",
                                "Between{\"gateway\", \"district\"}",
                                "The district wires itself; the trunk from "
                                "outside lands on the district, never on a "
                                "card in it.",
                                sketch::kit::well(
                                    {.width = kCell, .height = kPicture})
                                    .children(
                                        {box()
                                             .inset(0)
                                             .operators(
                                                 {Operator(
                                                      connect::Between{
                                                          .from =
                                                              "gateway",
                                                          .to = "district",
                                                          .router = routers::
                                                              orthogonal(
                                                                  routers::Bend::
                                                                      HFirst,
                                                                  8),
                                                          .gap = 6,
                                                          .wire = wire()})
                                                      .zIndex(-1)})
                                             .children({card(kServices[0])
                                                            .left(20)
                                                            .top(20),
                                                        district()})}))},
                      .measure = 1020,
                      .gap = 18})),
             sketch::kit::section(
                 {.label = "02  DRAWN BY A PROGRAM",
                  .note = "Weighted wires · the same, keyed · the load as a "
                          "bar"},
                 sketch::kit::comparison(
                     {.cases =
                          {cell("WEIGHTED WIRES",
                                "drawWith(drawTraffic()).zIndex(-1)",
                                "Each wire as wide as the load its card "
                                "states, labelled at its midpoint, behind "
                                "the cards.",
                                panel({Tiers{},
                                       drawWith(drawTraffic()).zIndex(-1)})),
                           cell("THE SAME, KEYED",
                                "drawWith(\"traffic\", drawTraffic())",
                                "The key vouches for the program, so the pen "
                                "it attaches prunes while the scope holds "
                                "still. The picture is the same.",
                                panel({Tiers{},
                                       drawWith("traffic", drawTraffic())
                                           .zIndex(-1)})),
                           cell("ANOTHER READING", "drawWith(drawLoadBars())",
                                "The same load read as a bar under each "
                                "card, as long as the card at the heaviest.",
                                panel({Tiers{}, drawWith(drawLoadBars())}))},
                      .measure = 1020,
                      .gap = 18}))})));
  }
};

SIGIL_SKETCH(ServiceMap, "Kit · API",
             "five service cards stating tier, calls and load: placed in "
             "rows by an operator written in the sketch, wired by the lane, "
             "by a pairing stated by hand and across a closed scope, then "
             "drawn over by pen programs reading the same facts")
