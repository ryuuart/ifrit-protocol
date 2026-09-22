/** @file
 * connect_by_lane — a service map whose wires are read off the cards.
 *
 * Every card states two facts about itself and nothing about lines:
 * `attribute("tier", n)`, which the arranging operator reads to put it in
 * a row, and `attribute("calls", {...})`, which the connecting operator
 * reads to wire it. The operators are listed on the panel: `Tiers` runs
 * during layout and places the cards; `connect::ByLane` runs once layout
 * has settled and attaches one wire per call, routed and dressed as the
 * operator says, painted behind the cards by the operator's `zIndex`.
 * The same cards under a different list are a different picture — the
 * second panel wires by hand with `connect::Between`, and the third
 * shows a scope closing at a nested panel: the trunk lands on the panel,
 * not on a card inside it.
 *
 * EDIT THESE FIRST
 *   kServices — the cards, their tiers and who they call.
 *   kRowHeight — how far apart the tiers stand, px.
 */

// TAGS: Kit/Layout

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Connect.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Routers.h>
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
constexpr float kRowHeight = 92;  // how far apart the tiers stand, px
constexpr float kCardWidth = 84;
constexpr float kCardHeight = 34;

struct Service {
  const char* name;
  int tier;
  std::vector<std::string> calls;
};

const std::vector<Service> kServices = {
    {"gateway", 0, {"auth", "ledger"}},
    {"auth", 1, {"cache"}},
    {"ledger", 1, {"cache", "store"}},
    {"cache", 2, {}},
    {"store", 2, {}},
};

/** ROWS BY TIER: each card at the row its fact names, the row's cards
 *  spread evenly across the box. An arranging operator an author writes
 *  in a sketch, reading what the cards say of themselves. */
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

/** A card stating its tier and its calls, and nothing about lines. */
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
      .justifyContent(Justify::Center)
      .alignItems(Align::Center)
      .children({text(service.name)});
}

Decoration wire() {
  return stroke(1.6f, Fill::color(sketch::kit::theme().palette.figure));
}

/** The panel: the cards under the operators given. */
Element panel(std::vector<Operator> operators) {
  return sketch::kit::well({.width = kCell, .height = kPicture})
      .children({box()
                     .inset(0)
                     .operators(std::move(operators))
                     .children({each(kServices, card)})});
}

/** A nested panel with operators of its own: one node to the scope
 *  above it, its port stated as a fact on its root. */
Element district() {
  return box()
      .key("district")
      .left(110)
      .top(96)
      .width(210)
      .height(196)
      .borderRadius({8})
      .foreground(stroke(1.0f, Fill::color(sketch::kit::theme().palette.rule)))
      .operators({layouts::Radial{.radiusFraction = 0.62f},
                  connect::ByLane{.lane = "calls", .wire = wire()}})
      .children({each(kServices, card)});
}

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const char* note, Element figure) {
  return {.title = title,
          .control = call,
          .figure = std::move(figure),
          .note = note};
}

}  // namespace

struct ConnectByLane {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    ctx.composer.render(sketch::kit::page(
        {.title = "Wired by a fact",
         .subtitle = "The cards state their tier and their calls. The "
                     "operators on the panel read both.",
         .footer = "An arranging operator runs during layout; an adding one "
                   "runs after it and attaches what it builds beside the "
                   "cards, behind them when its zIndex says so."},
        box().column().gap(22).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  THE SAME CARDS, THREE OPERATOR LISTS",
                  .note = "Read off the cards · stated by hand · closed at "
                          "a nested panel"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("BY THE LANE",
                            "Tiers{}, ByLane{.lane = \"calls\"}.zIndex(-1)",
                            "One wire per call each card states, routed "
                            "orthogonally, behind the cards.",
                            panel({Tiers{},
                                   Operator(connect::ByLane{
                                                .lane = "calls",
                                                .router = routers::orthogonal(
                                                    routers::Bend::MidX, 8),
                                                .gap = 4,
                                                .wire = wire()})
                                       .zIndex(-1)})),
                       cell("STATED BY HAND", "Tiers{}, Between{\"gateway\", \"store\"}",
                            "The calls are ignored; one pairing is stated in "
                            "the operator and bowed as an arc.",
                            panel({Tiers{},
                                   connect::Between{.from = "gateway",
                                                    .to = "store",
                                                    .router = routers::arc(0.3f),
                                                    .gap = 4,
                                                    .wire = wire()}})),
                       cell("A CLOSED SCOPE",
                            "Between{\"gateway\", \"district\"}",
                            "The district wires itself; the trunk from outside "
                            "lands on the district, never on a card in it.",
                            sketch::kit::well({.width = kCell,
                                               .height = kPicture})
                                .children(
                                    {box()
                                         .inset(0)
                                         .operators({connect::Between{
                                             .from = "gateway",
                                             .to = "district",
                                             .router = routers::orthogonal(
                                                 routers::Bend::VFirst, 8),
                                             .gap = 6,
                                             .wire = wire()}})
                                         .children(
                                             {card(kServices[0]).left(20).top(
                                                  20),
                                              district()})}))},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(ConnectByLane, "Kit · API",
             "five service cards stating their tier and their calls, "
             "placed in rows and wired by the lane, by a pairing stated "
             "by hand, and across a scope that closes at a nested panel")
