/** @file
 * A composed card carries type, ink, classes and custom properties down
 * its tree. Local declarations override only their named fields. A second
 * comparison separates that structural channel from a lexical environment;
 * two drawing surfaces then show inherited ink with fresh and retained pixels.
 */
// TAGS: Typography/Styles, Runtime/Composition

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/style/Length.h>

#include <chrono>
#include <cmath>
#include <utility>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace weave = sigil::weave;
namespace environment = sigil::core::environment;
using namespace sigil::compose;
using namespace sigil::weave::literals;
using namespace std::chrono_literals;
using sigil::draw::Pen;

namespace {
constexpr SkColor4f kPale{0.90f, 0.93f, 0.97f, 1};
constexpr SkColor4f kTeal{0.38f, 0.85f, 0.80f, 1};
constexpr SkColor4f kWarm{1.00f, 0.67f, 0.28f, 1};
constexpr SkColor4f kCool{0.44f, 0.64f, 1.00f, 1};
constexpr SkColor4f kPanel{0.05f, 0.07f, 0.10f, 1};
constexpr double kSwitchAt = 0.30;
constexpr auto kFade = 900ms;
constexpr double kCapture = 0.70;

Element property(const char* label) {
  return box()
      .row()
      .gap(12)
      .alignItems(Align::Center)
      .children({box().width(24).height(24).fill(Fill::var("accent")),
                 text(label).ink(var("accent"))});
}

Element relative(float size) {
  return box()
      .font({.size = size})
      .children({box()
                     .padding(1_em)
                     .fill(Fill::color(kPanel))
                     .children({text("1 em")})});
}

Element editorialCard(bool cooled) {
  weave::StyleSheet styles;
  styles.set("label", weave::Type{.size = 11, .track = 1.3f});
  styles.set("figure", weave::Type{.size = 48, .track = -0.6f});
  return sketch::kit::well({.width = 660, .height = 328, .padding = 24})
      .key("cascade.card")
      .font({.size = 14, .track = 0})
      .ink(cooled ? kCool : kWarm)
      .transition({.duration = kFade})
      .styleSheet(std::move(styles))
      .var("accent", kTeal)
      .var("gutter", Dimension(12))
      .column()
      .gap(18)
      .children(
          {text("FLOW THROUGH A TREE").styleClass("label"),
           box()
               .row()
               .gap(18)
               .alignItems(Align::Center)
               .children({text("18.4").styleClass("figure"),
                          box().column().gap(5).children(
                              {text("units / second").font({.size = 16}),
                               text("same inherited ink")}),
                          box().grow(),
                          box().width(42).height(42).stroke(stroke(1.5f)),
                          box().width(42).height(42).fill(Fill::currentInk())}),
           text(weave::rich()
                    .add(u8"The base inherits. ")
                    .add(u8"This run changes only color. ",
                         weave::Type{.color = kTeal})
                    .add(u8"A whole style stands alone.",
                         weave::textStyle(
                             {.size = 12, .color = kPale, .track = 0})))
               .width(612),
           box().row().gap(22).children(
               {box()
                    .padding(var("gutter"))
                    .fill(Fill::color(kPanel))
                    .children({property("Parent accent")}),
                box()
                    .var("accent", kWarm)
                    .padding(var("gutter"))
                    .fill(Fill::color(kPanel))
                    .children({property("Nearer override")})}),
           box()
               .row()
               .gap(20)
               .alignItems(Align::Center)
               .children({relative(9), relative(17),
                          text("1.5 em of 14 px").font({.size = 1.5_em})})});
}

Element adoption() {
  Element builtFirst = box().column().gap(12).children(
      {text("Built before its parent"), text("Adopted into this tree")});
  Element fixed = [] {
    sketch::kit::Theme local = sketch::kit::studyTheme();
    local.palette.figure = kWarm;
    const sketch::kit::Provide look(local);
    return box().width(38).height(38).fill(
        Fill::color(sketch::kit::theme().palette.figure));
  }();
  return sketch::kit::well({.width = 501, .height = 152, .padding = 24})
      .font({.size = 15, .track = 0})
      .ink(kTeal)
      .column()
      .gap(22)
      .children({std::move(builtFirst),
                 box()
                     .row()
                     .gap(14)
                     .alignItems(Align::Center)
                     .children({std::move(fixed),
                                text("The theme color was already read.")
                                    .font({.size = 13})})});
}

struct Accent {
  SkColor4f color{0.42f, 0.45f, 0.52f, 1};
  bool operator==(const Accent&) const = default;
};

Element chip(int depth = 3) {
  if (depth > 0) return box().padding(1).children({chip(depth - 1)});
  return box().width(36).height(22).fill(
      Fill::color(environment::inheritedOr(Accent{}).color));
}

Element lexical() {
  Element none = chip();
  const environment::Provide<Accent> outer(Accent{kTeal});
  Element bound = chip();
  Element shadowed = [&] {
    const environment::Provide<Accent> inner(Accent{kWarm});
    return chip();
  }();
  const auto row = [](Element swatch, const char* label) {
    return box()
        .row()
        .gap(16)
        .alignItems(Align::Center)
        .children(
            {std::move(swatch), text(label).font({.size = 14, .track = 0})});
  };
  return sketch::kit::well({.width = 501, .height = 152, .padding = 20})
      .column()
      .gap(10)
      .ink(kPale)
      .children(
          {row(std::move(none), "No binding: the component's default"),
           row(std::move(bound), "An outer Provide reaches through four boxes"),
           row(std::move(shadowed),
               "An inner Provide shadows the outer value")});
}

Element penCell() {
  return box()
      .width(501)
      .height(178)
      .font({.size = 14})
      .ink(kTeal)
      .fill(Fill::color(kPanel))
      .children({compose::pen("cascade.pen", [](Pen& pen) {
        pen.noStroke();
        pen.circle(48, 48, 38);
        pen.text("The node supplies ink and type", 84, 53);
        pen.push();
        pen.fill(kWarm);
        pen.circle(48, 120, 38);
        pen.text("A push / pop gives one local scope", 84, 125);
        pen.pop();
        pen.circle(455, 120, 24);
      })});
}

Element trailCell() {
  return box().width(501).height(178).ink(kTeal).children(
      {compose::graphics("cascade.trail", [](Pen& pen) {
        pen.background(kPanel.fR * 255, kPanel.fG * 255, kPanel.fB * 255, 14);
        const float angle = static_cast<float>(pen.millis()) / 1000 * 3;
        pen.noStroke();
        pen.circle(pen.width * 0.5f + std::cos(angle) * 58,
                   pen.height * 0.5f + std::sin(angle) * 58, 16);
      })});
}
}  // namespace

struct Cascade {
  Element trail;
  bool cooled = false;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1100, 1150}, .captureAt = kCapture});
    trail = trailCell();
    ctx.composer.render(sheet());
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    if (cooled || elapsed < kSwitchAt) return;
    cooled = true;
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    ctx.composer.render(sheet());
  }

  Element sheet() const {
    Element reading =
        sketch::kit::well({.width = 332, .height = 328, .padding = 22})
            .column()
            .gap(24)
            .children({text("WHAT THE CARD INHERITS").styleClass("eyebrow"),
                       sketch::kit::readout(
                           {{.name = "Base type", .value = "14 px"},
                            {.name = "Figure class", .value = "48 px"},
                            {.name = "Parent ink", .value = "warm → cool"},
                            {.name = "Transition", .value = "900 ms"},
                            {.name = "Gutter property", .value = "12 px"},
                            {.name = "Relative type", .value = "21 px"}},
                           {.measure = 288, .ruled = true}),
                       text("Watch the number, outline and filled square ease "
                            "together. Their color comes from one ancestor.")
                           .width(288)
                           .styleClass("captionNote")});
    return sketch::kit::page(
        {.title = "What flows through a composition",
         .subtitle = "One card, one parent voice · local declarations change "
                     "only the fields they name",
         .footer = "Type, ink, classes and properties resolve through the "
                   "mounted tree. Provide and theme reads belong to the code's "
                   "lexical scope."},
        box().column().gap(28).children(
            {sketch::kit::cells(
                 {.cells = {editorialCard(cooled), std::move(reading)},
                  .gap = 28}),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "THE TREE CARRIES THE INK",
                        .control = "Build first, adopt later",
                        .figure = adoption(),
                        .note = "The words take their new parent's ink. The "
                                "explicit swatch keeps the color chosen during "
                                "construction."},
                       {.title = "CODE READS THE ENVIRONMENT",
                        .control = "environment::Provide<Accent>",
                        .figure = lexical(),
                        .note =
                            "Nested bindings affect the component where it is "
                            "composed, independently of later adoption."}},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "DRAW AGAIN",
                        .control = "compose::pen",
                        .figure = penCell(),
                        .note = "Fresh shapes inherit the node's type and ink; "
                                "the final dot shows the restored scope."},
                       {.title = "KEEP THE PIXELS",
                        .control = "compose::graphics",
                        .figure = trail,
                        .note = "A translucent ground leaves a trail. This "
                                "same retained surface survives the card's "
                                "color update."}},
                  .measure = 1020,
                  .gap = 18})}));
  }
};

SIGIL_SKETCH(
    Cascade, "Kit · API",
    "an editorial card demonstrating inherited type, classes, properties and "
    "animated ink, with lexical scope and fresh versus retained drawing")
