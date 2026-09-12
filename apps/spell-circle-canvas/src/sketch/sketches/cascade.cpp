/** @file
 * cascade — the three things that flow down the TREE, and the one thing
 * that deliberately does not.
 *
 * The font a passage is set in, its colour (the ink) and the custom
 * properties reach everything under a node, wherever the code that built
 * that child happened to run. Everything else a node says — its fill, its
 * stroke, its padding, its transform — stays on it. That is CSS's own
 * split, and the rule of thumb transfers whole: text properties inherit,
 * box properties do not.
 *
 * The one divergence is the lexical channel. `environment::Provide<T>` is
 * read by the CODE that builds an element, so a component built under one
 * theme and mounted under another keeps the theme it was built under —
 * while its text still takes the ink of the tree it landed in. One
 * sentence holds both: the tree carries the font, the ink and the
 * properties; code reads the look.
 *
 * Twelve cells, one feature each, and every cell's label is the call that
 * made it. Two of them move: the fading panel's ink is described again on
 * a timer and the still is taken partway through the ease, and the kept
 * canvas has been laying a translucent ground under a moving dot since
 * frame one.
 *
 * EDIT THESE FIRST
 *   kSwitchAt / kFade / kCapture — when the panel's ink is re-described,
 *     how long it eases, and the moment the sheet is photographed. The
 *     capture must land inside the ease or the ninth cell shows one flat
 *     colour instead of a mixture.
 *   kSpin / kOrbit / kTrail — the dot's rate, its radius, and the alpha
 *     of the ground laid under it each frame. A larger alpha is a shorter
 *     trail.
 */

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/style/Length.h>
#include <sigilweave/style/StyleSheet.h>
#include <sigilweave/style/Type.h>

#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace weave = sigil::weave;
namespace environment = sigil::core::environment;

using namespace sigil::compose;
using namespace sigil::weave::literals;
using namespace std::chrono_literals;
using sigil::compose::toUtf8;
using sigil::draw::Pen;

namespace {

constexpr SkSize kCanvas = {1168, 812};
constexpr float kCell = 268;  // one cell's measure
constexpr float kBody = 164;  // the well every specimen stands in
constexpr float kPad = 12;

constexpr double kSwitchAt = 0.30;  // when the panel's ink is re-described
constexpr auto kFade = 900ms;       // how long it eases
constexpr double kCapture = 0.70;   // the moment inside the ease

constexpr float kSpin = 3.0f;   // the dot's rate, radians per second
constexpr float kOrbit = 40;    // its radius, px
constexpr float kTrail = 14;    // alpha of the ground laid each frame, 0..255

constexpr SkColor4f kPale{0.92f, 0.94f, 0.97f, 1};
constexpr SkColor4f kTeal{0.38f, 0.85f, 0.80f, 1};
constexpr SkColor4f kWarm{1.00f, 0.67f, 0.28f, 1};
constexpr SkColor4f kCool{0.44f, 0.64f, 1.00f, 1};
constexpr SkColor4f kPanel{0.05f, 0.07f, 0.10f, 1};

/** The house sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.type.captionLabel = {.size = 10.5f, .mono = true};
  look.type.captionNote = {.size = 10, .track = 0.2f};
  look.spacing.captionGap = 7;
  return look;
}

// ------------------------------------------------------ what flows down

/** (1) A leaf that names no style at all, three boxes under the node that
 *  set the font and the ink. */
Element inheritance() {
  return box()
      .column()
      .gap(10)
      .font({.size = 15})
      .ink(kTeal)
      .child(text(u8"text(utf8)"))
      .child(box().child(box().child(box().child(text(u8"three boxes down")))));
}

/** (2) One field named on one sibling; the rest of the font still comes
 *  down to it. */
Element oneField() {
  return box()
      .column()
      .gap(6)
      .font({.size = 13})
      .ink(kPale)
      .child(text(u8"a sibling"))
      .child(text(u8"the size alone").font({.size = 22}))
      .child(text(u8"a sibling"));
}

/** (3) Marks that name no colour, under one ink. */
Element inkMarks() {
  return box()
      .row()
      .gap(14)
      .alignItems(Align::Center)
      .font({.size = 12})
      .ink(kWarm)
      .child(box().width(58).height(58).stroke(stroke(1.5f)))
      .child(box().width(58).height(58).fill(Fill::currentInk()))
      .child(text(u8"and the words"));
}

/** (4) THE WELL, and the divergence beside it. The left child is built
 *  with no ancestor in reach and adopted afterwards: the panel's ink
 *  reaches it because the TREE carries the ink. The right child read a
 *  colour out of the theme in scope while it was being built, and that
 *  read landed in its description — so the panel cannot reach it. */
Element adoption() {
  Element inheriting = box()
                           .column()
                           .gap(5)
                           .child(text(u8"built first"))
                           .child(text(u8"adopted after"));
  Element lexical = [] {
    sketch::kit::Theme hot = sheetTheme();
    hot.palette.figure = kWarm;
    const sketch::kit::Provide look(hot);
    return box()
        .column()
        .gap(5)
        .alignItems(Align::Center)
        .child(box().width(30).height(30).corners({4}).fill(
            Fill::color(sketch::kit::theme().palette.figure)))
        .child(text(u8"read theme()"));
  }();
  return box()
      .row()
      .gap(18)
      .padding(14)
      .alignItems(Align::Center)
      .fill(Fill::color(kPanel))
      .font({.size = 12})
      .ink(kPale)
      .child(std::move(inheriting))
      .child(std::move(lexical));
}

/** (5) A class is a named partial, looked up in the sheet bound around the
 *  code that WRITES the leaf. The third name is on no sheet here, which is
 *  the once-only warning exercised on purpose. */
Element classes() {
  weave::StyleSheet sheet;
  sheet.set("label", weave::Type{.size = 9.5f, .track = 1.8f});
  sheet.set("figure", weave::Type{.size = 30, .color = kTeal});
  const environment::Provide<weave::StyleSheet> bound(sheet);
  return box()
      .column()
      .gap(7)
      .font({.size = 12})
      .ink(kPale)
      .child(text(u8"THROUGHPUT").styleClass("label"))
      .child(text(u8"18.4").styleClass("figure"))
      .child(text(u8"no sheet carries this").styleClass("headline"));
}

/** (6) A passage started with no base inherits; a run added with a partial
 *  keeps every field it did not name. */
Element richRuns() {
  return box()
      .font({.size = 14})
      .ink(kPale)
      .child(text(weave::rich()
                      .add(u8"inherits, then ")
                      .add(u8"a partial run", weave::Type{.color = kWarm})
                      .add(u8", then ")
                      .add(u8"a total one",
                           weave::textStyle({.size = 10,
                                             .color = kCool,
                                             .track = 1.4f}))));
}

/** (7) A property set on a subtree and read by anything under it, with a
 *  nearer ancestor overriding a farther one. */
Element properties() {
  const auto reader = [] {
    return box()
        .row()
        .gap(10)
        .alignItems(Align::Center)
        .child(box().width(26).height(26).corners({4}).fill(Fill::var("accent")))
        .child(text(u8"ink(var(\"accent\"))").ink(var("accent")));
  };
  return box()
      .column()
      .gap(10)
      .font({.size = 11})
      .ink(kPale)
      .var("accent", kTeal)
      .var("gutter", Dimension(12.0f))
      .child(box()
                 .padding(var("gutter"))
                 .fill(Fill::color(kPanel))
                 .child(reader()))
      .child(box()
                 .var("accent", kWarm)
                 .padding(var("gutter"))
                 .fill(Fill::color(kPanel))
                 .child(reader()));
}

/** (8) A relative length measures against the font in force: a box
 *  property against the node's own size, a type size against the
 *  parent's. */
Element lengths() {
  const auto block = [](float size) {
    return box().font({.size = size}).ink(kPale).child(
        box().padding(1_em).fill(Fill::color(kPanel)).child(text(u8"1_em")));
  };
  return box()
      .column()
      .gap(10)
      .alignItems(Align::Start)
      .child(block(9))
      .child(block(17))
      .child(box().font({.size = 11}).ink(kTeal).child(
          text(u8"1.5_em of 11 px").font({.size = 1.5_em})));
}

/** (9) The node that declares the ink eases it, and everything under it
 *  follows — the words, the stroke that named no colour and the fill
 *  written as the ink, all mid-mixture in the still. */
Element crossFade(bool cooled) {
  return box()
      .key("cascade.fade")
      .column()
      .gap(10)
      .padding(14)
      .fill(Fill::color(kPanel))
      .font({.size = 12})
      .ink(cooled ? kCool : kWarm)
      .transition({.duration = kFade})
      .child(text(u8"everything under it"))
      .child(box().height(26).stroke(stroke(1.5f)))
      .child(box().width(60).height(26).fill(Fill::currentInk()));
}

// ------------------------------------------------------------- the pen

/** (10) A pen program's shapes and its type SIZE come from the node it
 *  stands in, unasked for; `push`/`pop` restyles for a scope and hands the
 *  ink back at the end of it. The GLYPHS are the one mark the ink does not
 *  reach: a pen fills text black until a fill is set, so the line below
 *  stands on a plate that makes the black legible. */
Element penCell() {
  return box()
      .padding(kPad)
      .font({.size = 13})
      .ink(kTeal)
      .child(compose::pen("cascade.pen",
                          [](Pen& pen) {
                            pen.noStroke();
                            pen.circle(30, 32, 44);
                            pen.push();
                            pen.fill(kPale);
                            pen.rect(60, 10, 176, 42);
                            pen.pop();
                            pen.text("no fill: p5's black", 70, 38);
                            pen.push();
                            pen.fill(kWarm);
                            pen.circle(30, 104, 44);
                            pen.text("pen.fill in a push", 70, 100);
                            pen.pop();
                            pen.circle(216, 104, 26);
                          })
                 .width(kCell - 2 * kPad)
                 .height(kBody - 2 * kPad));
}

/** (11) The kept canvas: the same door, onto pixels that stand between
 *  frames. A translucent ground each frame is p5's trail, which the
 *  repainting pen above cannot do at all. */
Element trailCell() {
  return box()
      .padding(kPad)
      .ink(kTeal)
      .child(compose::graphics("cascade.trail",
                               [](Pen& pen) {
                                 pen.background(kPanel.fR * 255,
                                                kPanel.fG * 255,
                                                kPanel.fB * 255, kTrail);
                                 const float t =
                                     (float)pen.millis() / 1000.0f * kSpin;
                                 pen.noStroke();
                                 pen.circle(
                                     pen.width * 0.5f + std::cos(t) * kOrbit,
                                     pen.height * 0.5f + std::sin(t) * kOrbit,
                                     16);
                               })
                 .width(kCell - 2 * kPad)
                 .height(kBody - 2 * kPad));
}

// ------------------------------------------------- the lexical channel

/** What a component reads where it is COMPOSED. A comparable value, which
 *  is what makes the reconciler's structural prune an exact dependency
 *  tracker over it. */
struct Accent {
  SkColor4f colour{0.42f, 0.45f, 0.52f, 1};
  bool operator==(const Accent&) const = default;
};

/** Handed nothing, and four plain containers below whatever bound one. */
Element chip(int depth = 3) {
  if (depth > 0) return box().padding(2).child(chip(depth - 1));
  return box().width(44).height(20).corners({4}).fill(
      Fill::color(environment::inheritedOr(Accent{}).colour));
}

/** (12) The one channel the tree does NOT carry: a binding in scope for
 *  the code that describes, shadowed LIFO by an inner one. */
Element lexicalChannel() {
  Element none = chip();
  Element outer = [] {
    const environment::Provide<Accent> bound(Accent{kTeal});
    return box().row().gap(8).child(chip()).child(chip());
  }();
  Element shadowed = [] {
    const environment::Provide<Accent> bound(Accent{kTeal});
    Element before = chip();
    const environment::Provide<Accent> nested(Accent{kWarm});
    return box().row().gap(8).child(std::move(before)).child(chip());
  }();
  return box()
      .column()
      .gap(5)
      .font({.size = 10})
      .ink(kPale)
      .child(text(u8"nothing bound: chip()'s own default"))
      .child(std::move(none))
      .child(text(u8"one Provide, four levels up"))
      .child(std::move(outer))
      .child(text(u8"an inner Provide shadows it"))
      .child(std::move(shadowed));
}

// --------------------------------------------------------- the furniture

Element cell(const char* call, const char* note, Element body) {
  return sketch::kit::caption(
      kCell, toUtf8(call), toUtf8(note),
      sketch::kit::well({.width = kCell, .height = kBody, .padding = kPad})
          .child(std::move(body)));
}

/** A cell whose body IS the well's surface — a pen node sized to the well
 *  pads itself, so a second padding would shrink it off its own measure. */
Element bare(const char* call, const char* note, Element body) {
  return sketch::kit::caption(
      kCell, toUtf8(call), toUtf8(note),
      sketch::kit::well({.width = kCell, .height = kBody, .padding = 0.0f})
          .child(std::move(body)));
}

Element row(std::vector<Element> four) {
  return sketch::kit::cells({.cells = std::move(four), .gap = 16});
}

}  // namespace

struct Cascade final : sketch::Sketch {
  /** The kept canvas is built ONCE and described by value afterwards: a
   *  rebuilt program would be a new surface, and the trail would start
   *  over on the frame the fading panel is re-described. */
  Element trail;
  bool cooled = false;

  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = kCapture});
    trail = trailCell();
    ctx.composer.render(sheet());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // ONE re-describe, on a timer. The ink on the keyed panel is the other
    // colour this time, and the transition on that node eases it; nothing
    // under it is described differently at all.
    if (cooled || elapsed < kSwitchAt) return;
    cooled = true;
    const sketch::kit::Provide look(sheetTheme());
    ctx.composer.render(sheet());
  }

  [[nodiscard]] Element sheet() const {
    return sketch::kit::page(
        {.title = toUtf8("THE CASCADE \xc2\xb7 the font, the ink and the "
                         "custom properties"),
         .subtitle = toUtf8(
             "they flow down the TREE, wherever the code that built a "
             "child ran \xc2\xb7 everything else a node says stays on it"),
         .footer = toUtf8(
             "a class and a theme are LEXICAL, read where an element is "
             "written \xc2\xb7 the cascade is STRUCTURAL, carried by the "
             "tree the element ends up in \xc2\xb7 a bake is a root")},
        sketch::kit::cells(
            {.cells =
                 {row({cell(".font({.size = 15}).ink(teal)",
                            "the boxes between say nothing about type, and "
                            "the leaf names no style \xe2\x80\x94 it is set "
                            "in what is in force where it LANDS",
                            inheritance()),
                       cell("font({.size = 22})",
                            "a partial: the size alone is named, and the "
                            "face, the tracking and the ink still come down "
                            "to it",
                            oneField()),
                       cell("stroke(1.5f) \xc2\xb7 Fill::currentInk()",
                            "one ink \xe2\x80\x94 a stroke that named no "
                            "colour, a fill written as the ink, and the "
                            "words, all the same value",
                            inkMarks()),
                       cell(".ink(pale) over a child built first",
                            "the tree carried the ink into a child already "
                            "built \xc2\xb7 the swatch read theme() where "
                            "the code RAN, so the panel cannot reach it",
                            adoption())}),
                  row({cell("styleClass(\"label\" | \"figure\" | \"headline\")",
                            "the sheet is read where the leaf is WRITTEN "
                            "\xc2\xb7 the third name is on no sheet here, "
                            "so it warns once and sets nothing",
                            classes()),
                       cell("rich().add(utf8, Type{.color = warm})",
                            "a partial run keeps the inherited face and "
                            "size and changes only what it names \xc2\xb7 a "
                            "whole style keeps nothing",
                            richRuns()),
                       cell(".var(\"accent\") \xc2\xb7 Fill::var \xc2\xb7 "
                            "ink(var)",
                            "one property read as a fill and as the ink two "
                            "levels down, at the same gutter \xc2\xb7 the "
                            "nearer ancestor wins on the second row",
                            properties()),
                       cell("padding(1_em) \xc2\xb7 font({.size = 1.5_em})",
                            "a box length is the node's OWN resolved size, "
                            "so one call is 9 px and 17 px \xc2\xb7 a type "
                            "size measures against the PARENT's",
                            lengths())}),
                  row({cell(".ink(warm to cool).transition({900ms})",
                            "the ink was described again on a timer \xc2\xb7 "
                            "the node that declares it eases, and the still "
                            "is taken partway through the mixture",
                            crossFade(cooled)),
                       bare("compose::pen(program)",
                            "the circle and the type size are the node's, "
                            "unasked for, and a fill inside push/pop "
                            "restyles the scope \xc2\xb7 the GLYPHS are p5's "
                            "black until a fill is set",
                            penCell()),
                       bare("compose::graphics(program)",
                            "the same door onto pixels that are KEPT \xc2\xb7 "
                            "a translucent ground each frame is the trail "
                            "the repainting pen cannot leave",
                            trail),
                       cell("environment::Provide<Accent>",
                            "the divergence, on its own \xc2\xb7 a component "
                            "handed nothing reads the binding in scope where "
                            "it is COMPOSED, and an inner one shadows it",
                            lexicalChannel())})},
             .column = true,
             .gap = 18}));
  }
};

SIGIL_SKETCH(Cascade, "Kit \xc2\xb7 API",
             "the font, the ink and the custom properties flowing down the "
             "tree \xe2\x80\x94 with classes, relative lengths, an easing "
             "ink, both pens, and the one channel that is lexical instead")
