/** @file
 * A composed card carries type, ink, classes and custom properties down
 * its tree. Local declarations override only their named fields. A second
 * comparison follows one custom property through nested boxes, shadowed
 * nearer in and defaulted where nothing sets it; two drawing surfaces then
 * show inherited ink with fresh and retained pixels.
 */
// TAGS: Typography/Styles, Runtime/Composition

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Rows.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/style/Length.h>

#include <chrono>
#include <cmath>
#include <utility>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace weave = sigil::weave;
using namespace sigil::compose;
using namespace sigil::weave::literals;
using namespace std::chrono_literals;
using sigil::draw::Pen;

namespace {
constexpr material::Color kPale{0.90f, 0.93f, 0.97f, 1};
constexpr material::Color kTeal{0.38f, 0.85f, 0.80f, 1};
constexpr material::Color kWarm{1.00f, 0.67f, 0.28f, 1};
constexpr material::Color kCool{0.44f, 0.64f, 1.00f, 1};
constexpr material::Color kPanel{0.05f, 0.07f, 0.10f, 1};
constexpr double kSwitchAt = 0.30;
constexpr auto kFade = 900ms;
constexpr double kCapture = 0.70;

Element property(const char* label) {
  return box()
      .row()
      .gap(12)
      .alignItems(Align::Center)
      .children({box().width(24).height(24).fill(var("accent")),
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
  compose::StyleSheet styles{
      compose::rule("label, .label")
          .font(weave::Type{.size = 11, .letterSpacing = 1.3f}),
      compose::rule("figure, .figure")
          .font(weave::Type{.size = 48, .letterSpacing = -0.6f})};
  return sketch::kit::well({.width = 660, .height = 328, .padding = 24})
      .key("cascade.card")
      .font({.size = 14, .letterSpacing = 0})
      .ink(cooled ? kCool : kWarm)
      .transition(kFade)
      .applyStyleSheet(std::move(styles))
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
                          box().flexGrow(),
                          box().width(42).height(42).stroke(stroke(1.5f)),
                          box().width(42).height(42).fill(Fill::currentInk())}),
           document::paragraph(
               weave::rich()
                   .add(u8"The base inherits. ")
                   .add(u8"This run changes only color. ",
                        weave::Type{.ink = kTeal})
                   .add(u8"A whole style stands alone.",
                        weave::textStyle(
                            {.size = 12, .ink = kPale, .letterSpacing = 0})))
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
  Element figure = box().width(38).height(38).fill(var("palette-figure"));
  return sketch::kit::well({.width = 501, .height = 152, .padding = 24})
      .var("palette-figure", kWarm)
      .font({.size = 15, .letterSpacing = 0})
      .ink(kTeal)
      .column()
      .gap(22)
      .children({std::move(builtFirst),
                 box()
                     .row()
                     .gap(14)
                     .alignItems(Align::Center)
                     .children({std::move(figure),
                                text("The figure color is read where it lands.")
                                    .font({.size = 13})})});
}

/** A swatch four boxes deep in the accent in force, and its own grey where
 *  no ancestor sets one. */
Element chip(int depth = 3) {
  if (depth > 0) return box().padding(1).children({chip(depth - 1)});
  VarTable fallback;
  fallback.set(var("accent"), material::Color{0.42f, 0.45f, 0.52f, 1});
  return box()
      .width(36)
      .height(22)
      .varDefaults(std::move(fallback))
      .fill(var("accent"));
}

Element nesting() {
  const auto row = [](Element swatch, const char* label) {
    return box()
        .row()
        .gap(16)
        .alignItems(Align::Center)
        .children({std::move(swatch),
                   text(label).font({.size = 14, .letterSpacing = 0})});
  };
  return sketch::kit::well({.width = 501, .height = 152, .padding = 20})
      .column()
      .gap(10)
      .ink(kPale)
      .children({row(chip(), "No binding: the component's default"),
                 row(chip(), "An outer property reaches through four boxes")
                     .var("accent", kTeal),
                 row(chip().var("accent", kWarm),
                     "An inner property shadows the outer value")
                     .var("accent", kTeal)});
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
        pen.background(kPanel.r * 255, kPanel.g * 255, kPanel.b * 255, 14);
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

  void setup(sketch::Scene2DContext& ctx) {
    sketch::kit::stage(
        ctx,
        {.theme = sketch::kit::featureTheme(sketch::kit::Density::Spacious),
         .size = {1100, 1150},
         .captureAt = kCapture});
    trail = trailCell();
    ctx.composer.render(sheet());
  }

  void update(double elapsed, sketch::Scene2DContext& ctx) {
    if (cooled || elapsed < kSwitchAt) return;
    cooled = true;
    ctx.composer.render(sheet());
  }

  Element sheet() const {
    Element reading =
        sketch::kit::well({.width = 332, .height = 328, .padding = 22})
            .column()
            .gap(24)
            .children({document::eyebrow("WHAT THE CARD INHERITS"),
                       sigil::compose::kit::readout(
                           std::vector<sigil::compose::kit::Reading>{
                               {.name = "Base type", .value = "14 px"},
                               {.name = "Figure class", .value = "48 px"},
                               {.name = "Parent ink", .value = "warm → cool"},
                               {.name = "Transition", .value = "900 ms"},
                               {.name = "Gutter property", .value = "12 px"},
                               {.name = "Relative type", .value = "21 px"}},
                           {.measure = 288,
                            .divider = sigil::compose::var("palette-rule")}),
                       document::caption(
                           "Watch the number, outline and filled square ease "
                           "together. Their color comes from one ancestor.")
                           .width(288)});
    return sketch::kit::page(
        {.title = "What flows through a composition",
         .subtitle = "One card, one parent voice · local declarations change "
                     "only the fields they name",
         .footer = "Type, ink, classes and properties resolve through the "
                   "mounted tree, wherever the code that built a node ran."},
        box().column().gap(28).children(
            {sketch::kit::cells(
                 {.cells = {editorialCard(cooled), std::move(reading)},
                  .gap = 28}),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "THE TREE CARRIES THE INK",
                        .control = "Build first, adopt later",
                        .figure = adoption(),
                        .note = "The words take their new parent's ink, and "
                                "the swatch the figure color its new parent "
                                "sets."},
                       {.title = "THE NEAREST PROPERTY WINS",
                        .control = "Element::var",
                        .figure = nesting(),
                        .note = "A property set above reaches every box under "
                                "it; one set nearer shadows it."}},
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
    "animated ink, with nested properties and fresh versus retained drawing")
