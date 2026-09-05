/** @file
 * volatility_cost — THE CACHING PROOF: what every node on the sheet did
 * to produce its pixels, drawn on the sheet.
 *
 * A bound property marks its leaf's content volatile, and volatility
 * propagates UPWARD: the root answers "volatile" too, and the whole
 * subtree is refused texture promotion. Picture caching cannot rescue it —
 * replaying a picture still re-runs every draw call. So one leaf decides
 * what a whole panel costs. That claim is invisible in a still, which is
 * why this sheet reads its own answer back and colours itself with it.
 *
 * THE SUBJECT, in two fields:
 *   LEFT — THE CHEAP SIDE. Three hundred static cards under two dozen
 *     binding-driven movers. The movers are volatile and are painted
 *     live; the cards are not, and are proved to be sitting still. The
 *     bill is exactly the movers, because volatility is a property of a
 *     subtree rather than of the frame.
 *   RIGHT — THE RELEASED SIDE. Five hundred stroked, shaped cells whose
 *     only volatile thing is one seven-point star with `fill(&output)` —
 *     bound, and deliberately PARKED. The bound fill's resolved value
 *     rides in the content-scalar memo, so the recording stays valid
 *     while the value holds; a settle counter clears the volatility flag
 *     after eight consecutive frames of a provably identical value; and
 *     the released node re-declares itself volatile on the very frame the
 *     output moves. A parked binding costs what a plain colour costs and
 *     only starts paying when it is actually used.
 *
 * THE PROOF, in three readings, all taken from the composer itself:
 *   THE TIER PER NODE. `Composer::profile()` reports, for every node, how
 *     it produced its pixels — Live, Picture, Texture, Promoted, SplitOwn
 *     or Group — and, when it was NOT baked, which condition refused. Each
 *     keyed node's rect is read back with `Composer::bounds()` and
 *     outlined in its tier's colour, so the sheet is a map of its own
 *     cache.
 *   THE SPLIT. Every node whose refusal mask carries `Volatile` is counted
 *     against every node that reached a bake. That count is the claim
 *     above, as a number rather than as a sentence.
 *   THE FRAME. `Composer::stats()` — nodes described, memo hits, pictures
 *     and textures live, recordings and bakes made, nodes painted, and the
 *     four phase times — printed as a block.
 *
 * Every number the sheet measured about its own execution goes through
 * `ctx.measured(value, pinned)`, so a capture taken for a pixel diff shows
 * the pinned values and two runs of the same binary agree.
 *
 * THE READING IS TAKEN ONCE, at `kSnapAt`, and frozen. A readout that
 * re-described itself every frame would be measuring a sheet that
 * contains it, and the observer would be the largest term in its own
 * report.
 *
 * EDIT THESE FIRST
 *   kMovers / kCards — the left field. The movers are what paints; the
 *                      cards are what does not, whatever their count.
 *   kCells      — the right panel, large enough that per-cell picture
 *                 replay dominates the frame. Drop it to 32 for the
 *                 opposite regime, where the subtree is too cheap for
 *                 promotion to be worth firing.
 *   kRepaintHz  — 0 holds the accent's colour still for the whole run, so
 *                 everything above it settles, releases, and the panel
 *                 becomes a blit. Set it non-zero and the split below
 *                 moves: each change re-declares volatility for a frame.
 *   kSnapAt     — when the reading is taken. Before the eight-frame
 *                 settle, the right panel is still volatile and says so.
 */

#include <include/core/SkPaint.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;

namespace {

constexpr int kCards = 300;         // static, cached, never repainted
constexpr int kMovers = 24;         // bound, volatile, painted every frame
constexpr int kCells = 417;         // enough cells for picture replay to hurt
constexpr double kRepaintHz = 0.0;  // 0 = the bound colour never moves
constexpr double kSnapAt = 2.0;     // when the reading is taken, seconds
constexpr int kRows = 12;           // costliest nodes listed

constexpr float kFieldWidth = 620.0f;
constexpr float kFieldHeight = 560.0f;
constexpr float kCellsWidth = 620.0f;

constexpr SkColor4f kInk{0.92f, 0.94f, 0.98f, 1};
constexpr SkColor4f kDim{0.56f, 0.61f, 0.72f, 1};
constexpr SkColor4f kAccent{0.95f, 0.35f, 0.18f, 1};

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.ground = {0.055f, 0.06f, 0.085f, 1};
  look.palette.ink = {0.92f, 0.94f, 0.98f, 1};
  look.palette.ash = {0.56f, 0.61f, 0.72f, 1};
  look.palette.rule = {0.19f, 0.20f, 0.26f, 1};
  look.type.title = {.size = 15, .track = 2};
  look.type.subtitle = {.size = 11, .track = 0.5f};
  look.type.footer = {.size = 10, .track = 0.2f};
  look.spacing.marginX = 26;
  look.spacing.marginBottom = 14;
  return look;
}

/** ONE COLOUR PER TIER, used by the outlines and by the legend, so the
 *  map and its key cannot disagree. */
SkColor4f tierColor(Composer::CacheState state) {
  switch (state) {
    case Composer::CacheState::Live:
      return {0.98f, 0.35f, 0.30f, 1};
    case Composer::CacheState::Picture:
      return {0.98f, 0.76f, 0.28f, 1};
    case Composer::CacheState::Texture:
      return {0.36f, 0.72f, 1.00f, 1};
    case Composer::CacheState::Promoted:
      return {0.40f, 0.90f, 0.55f, 1};
    case Composer::CacheState::SplitOwn:
      return {0.75f, 0.55f, 1.00f, 1};
    case Composer::CacheState::Group:
      return {0.30f, 0.88f, 0.82f, 1};
  }
  return kDim;
}

const char* tierName(Composer::CacheState state) {
  switch (state) {
    case Composer::CacheState::Live:
      return "Live";
    case Composer::CacheState::Picture:
      return "Picture";
    case Composer::CacheState::Texture:
      return "Texture";
    case Composer::CacheState::Promoted:
      return "Promoted";
    case Composer::CacheState::SplitOwn:
      return "SplitOwn";
    case Composer::CacheState::Group:
      return "Group";
  }
  return "?";
}

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

std::string ms(double v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.2f", v);
  return buf;
}

/** `kCells` stroked, shaped cells — each recording its own picture — plus
 *  ONE accent cell in the same row whose fill is BOUND rather than a plain
 *  value. Everything above that accent shares its volatility, which is why
 *  a single leaf decides the cost of the whole panel. */
Element cells(const choreograph::Output<Fill>* tint) {
  auto row = box().key("cells").width(kCellsWidth).row().wrapLines().gap(2);
  for (int id = 0; id < kCells; ++id) {
    const float t = 0.20f + 0.04f * (float)(id % 6);
    row.child(box()
                  .key("c" + std::to_string(id))
                  .width(26)
                  .height(26)
                  .shape(shapes::star(5 + id % 3, 0.45f, 0.08f))
                  .fill(Fill::color({t, 0.45f, 0.68f, 1.0f}))
                  .stroke(brush::solid(
                      1.5f, Fill::color({0.95f, 0.86f, 0.55f, 1.0f}))));
  }
  row.child(box()
                .key("accent")
                .width(26)
                .height(26)
                .shape(shapes::star(7, 0.45f, 0.08f))
                .fill(Animatable<Fill>(tint))
                .stroke(brush::solid(
                    1.5f, Fill::color({0.10f, 0.10f, 0.12f, 1.0f}))));
  // The panel is held to the field's box so the two stand the same
  // height and the readout below them starts on one line.
  return box()
      .key("cellPanel")
      .column()
      .width(kCellsWidth)
      .height(kFieldHeight)
      .child(std::move(row));
}

}  // namespace

struct VolatilityCost final : sketch::Sketch {
  std::vector<std::unique_ptr<choreograph::Output<float>>> movers;
  choreograph::Output<Fill> tint{Fill::color(kAccent)};  // assigned ONCE
  int step = 0;
  bool snapped = false;

  /** The frozen reading. One rect and one tier per keyed node the profile
   *  named and `bounds()` could resolve. */
  struct Marked {
    SkRect rect;
    Composer::CacheState state = Composer::CacheState::Live;
  };
  std::vector<Marked> marks;
  std::vector<Composer::NodeCost> worst;
  Composer::Stats frame;
  int volatileNodes = 0;
  int bakedNodes = 0;
  size_t profiled = 0;

  /** The movers' outputs and the one lambda per output that drives them,
   *  made ONCE. The tree below is described twice — once at setup and
   *  once when the reading is taken — and a describe that also registered
   *  steppables would double them. */
  void makeMovers(sigil::motion::Ticker& ticker) {
    for (int i = 0; i < kMovers; ++i) {
      auto out = std::make_unique<choreograph::Output<float>>(0.0f);
      const float phase = (float)i * 0.7f;
      movers.push_back(std::move(out));
      ticker.add([o = movers.back().get(), phase, t = 0.0](double dt) mutable {
        t += dt;
        *o = 280.0f + 270.0f * (float)std::sin(t * 0.9 + phase);
        return true;
      });
    }
  }

  /** The left field: cards that never repaint, under movers that always
   *  do. The cards are laid from a fixed seed, so the field is the same
   *  picture on every run. */
  Element field() {
    auto root = box()
                    .key("field")
                    .width(kFieldWidth)
                    .height(kFieldHeight)
                    .fill(Fill::color({0.04f, 0.04f, 0.08f, 1}));
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    std::mt19937 rng{3};
    for (int i = 0; i < kCards; ++i) {
      const float x = (float)(rng() % 570), y = (float)(rng() % 532);
      root.child(box()
                     .key("k" + std::to_string(i))
                     .width(34)
                     .height(22)
                     .corners({4})
                     .inset(x, y, 0, 0)
                     .fill(Fill::color({0.09f, 0.10f, 0.16f, 1})));
    }
    for (int i = 0; i < kMovers; ++i) {
      const float y = 12.0f + 22.0f * (float)i;
      root.child(box()
                     .key("m" + std::to_string(i))
                     .width(46)
                     .height(18)
                     .corners({4})
                     .inset(0, y, 0, 0)
                     .translateX(movers[(size_t)i].get())
                     .fill(Fill::color({0.49f, 0.91f, 1.0f, 0.8f})));
    }
    return root;
  }

  /** THE MAP: every keyed node the profile named, outlined in the colour
   *  of the tier it took. Drawn in one paint program over the whole
   *  canvas, because it is a reading of the tree and not a part of it —
   *  a node per outline would change what it is measuring. */
  Element tierMap() const {
    return custom([marks = marks](SkCanvas& canvas, const PaintContext&) {
             SkPaint edge;
             edge.setAntiAlias(true);
             edge.setStyle(SkPaint::kStroke_Style);
             edge.setStrokeWidth(1.4f);
             for (const Marked& m : marks) {
               edge.setColor4f(tierColor(m.state));
               canvas.drawRect(m.rect.makeOutset(1.5f, 1.5f), edge);
             }
           })
        .absolute()
        .inset(0)
        .zIndex(8)
        .hitTestable(false)
        .cache(Cache::None);
  }

  Element legend() const {
    const std::pair<Composer::CacheState, const char*> tiers[] = {
        {Composer::CacheState::Live, "painted from scratch"},
        {Composer::CacheState::Picture, "replayed a recording"},
        {Composer::CacheState::Texture, "blitted the author's bake"},
        {Composer::CacheState::Promoted, "blitted the library's bake"},
        {Composer::CacheState::SplitOwn, "own paint blitted, children live"},
        {Composer::CacheState::Group, "whole subtree blitted, held still"}};
    Element row = box().row().gap(16).wrapLines();
    for (const auto& [state, what] : tiers)
      row.child(box()
                    .row()
                    .gap(6)
                    .alignItems(Align::Center)
                    .child(box().width(11).height(11).stroke(
                        stroke(1.4f, Fill::color(tierColor(state)))))
                    .child(text(toU8(std::string(tierName(state)) +
                                     "  \xc2\xb7  " + what),
                                label(10.5f, kDim))));
    return row;
  }

  /** THE FRAME, as the composer reports it. Every number here is one the
   *  sketch measured about its own execution, so every one is pinned when
   *  the host is capturing for a diff. */
  Element statsBlock(const sketch::SketchContext& ctx) const {
    const auto line = [&](const std::string& name, const std::string& value) {
      return box()
          .row()
          .gap(8)
          .child(text(toU8(name), label(11, kDim)).width(Dim(168)))
          .child(text(toU8(value), label(11, kInk)));
    };
    // A COUNT IS A FUNCTION OF THE DESCRIPTION and is printed as it is;
    // a TIME is a function of the machine and is pinned when the host is
    // capturing for a diff, so a plate carries the shape and not the
    // stopwatch.
    const auto count = [](size_t v) { return std::to_string(v); };
    (void)ctx;
    Element column = box().column().gap(3);
    column.child(text(toU8("Composer::stats()"), label(12.5f, kInk, 0.8f))
                     .margin(0, 0, 0, 4));
    column.child(line("instances", count(frame.instances)));
    column.child(line("describedNodes", count(frame.describedNodes)));
    column.child(line("memoHits", count(frame.memoHits)));
    column.child(line("patchedNodes", count(frame.patchedNodes)));
    column.child(line("picturesLive", count(frame.picturesLive)));
    column.child(line("texturesLive", count(frame.texturesLive)));
    column.child(line("picturesRecorded", count(frame.picturesRecorded)));
    column.child(line("texturesBaked", count(frame.texturesBaked)));
    column.child(line("nodesPainted", count(frame.nodesPainted)));
    column.child(line("reconcile ms", ms(ctx.measured(frame.reconcileMs))));
    column.child(line("layout ms", ms(ctx.measured(frame.layoutMs))));
    column.child(line("volatile ms", ms(ctx.measured(frame.volatileMs))));
    column.child(line("paint ms", ms(ctx.measured(frame.paintMs))));
    column.child(box().height(8));
    column.child(
        text(toU8("the split"), label(12.5f, kInk, 0.8f)).margin(0, 0, 0, 4));
    column.child(line("refused: Volatile", count((size_t)volatileNodes)));
    column.child(line("reached a bake", count((size_t)bakedNodes)));
    column.child(line("nodes profiled", count(profiled)));
    return column;
  }

  /** THE COSTLIEST NODES, worst first, each with the tier it took and the
   *  condition that refused it a bake. `selfMs` excludes children, so the
   *  number lands on the node that actually costs. */
  Element costTable(const sketch::SketchContext& ctx) const {
    Element column = box().column().gap(3);
    column.child(text(toU8("Composer::profile() \xc2\xb7 self ms, worst first"),
                      label(12.5f, kInk, 0.8f))
                     .margin(0, 0, 0, 4));
    for (const Composer::NodeCost& row : worst) {
      column.child(
          box()
              .row()
              .gap(8)
              .alignItems(Align::Center)
              .child(box().width(9).height(9).fill(
                  Fill::color(tierColor(row.cacheState))))
              .child(text(toU8(row.label), label(11, kInk)).width(Dim(126)))
              .child(text(toU8(ms(ctx.measured(row.selfMs))), label(11, kInk))
                         .width(Dim(46)))
              .child(text(toU8(tierName(row.cacheState)), label(11, kDim))
                         .width(Dim(66)))
              .child(text(toU8(Composer::promotionReason(row.promotion)),
                          label(11, kDim))));
    }
    return column;
  }

  Element readout(const sketch::SketchContext& ctx) const {
    if (!snapped)
      return box().child(
          text(toU8("reading at " + ms(kSnapAt) + " s\xe2\x80\xa6"),
               label(12, kDim)));
    return box().column().gap(12).child(legend()).child(
        box().row().gap(34).child(statsBlock(ctx)).child(costTable(ctx)));
  }

  Element describe(sketch::SketchContext& ctx) {
    // The theme is bound where the tree is DESCRIBED, not where setup
    // runs: this sketch describes again on every reading, and a scope
    // that ended with setup would not be there.
    const sketch::kit::Provide look(sheetTheme());
    // The map is a SIBLING of the sheet, not a child of it: it draws in
    // canvas coordinates, which is what `bounds()` answers in, and a
    // child of the padded page would be offset by the page's margins.
    return stack().inset(0).child(tierMap()).child(sketch::kit::page(
        {.title = toU8("THE CACHING PROOF \xc2\xb7 what every node "
                       "did to produce its pixels"),
         .subtitle = toU8("volatility propagates upward, so one "
                          "bound leaf decides what its whole subtree "
                          "costs \xe2\x80\x94 every keyed node is "
                          "outlined in the tier it took, read back "
                          "from the composer at " +
                          ms(kSnapAt) + " s"),
         .footer = toU8("a picture records the DRAW CALLS, so "
                        "replaying one re-runs every shader over "
                        "every pixel; only a bake replaces that with "
                        "a blit \xc2\xb7 numbers the sheet measured "
                        "about itself are pinned for a diff")},
        box()
            .column()
            .gap(16)
            .child(kit::cells({.cells = {field(), cells(&tint)}, .gap = 22}))
            .child(readout(ctx))));
  }

  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    // the reading is taken and frozen by then
    sketch::kit::stage(ctx, {.size = {1320, 980}, .captureAt = kSnapAt + 0.5});
    movers.clear();
    marks.clear();
    worst.clear();
    snapped = false;
    // The per-node reading is what this sheet is; it costs a timing call
    // per node and is off everywhere else.
    ctx.composer.setProfiling(true);
    makeMovers(ctx.ticker);
    ctx.composer.render(describe(ctx));
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    if (kRepaintHz > 0.0) {
      // The demonstration mode: a change every 1/kRepaintHz seconds — each
      // one re-declares volatility for a frame, after which the value
      // holds long enough to settle and release again.
      const int now = (int)std::floor(elapsed * kRepaintHz);
      if (now != step) {
        step = now;
        const float t = (float)(step % 5) / 5.0f;
        tint = Fill::color(
            {0.20f + 0.70f * t, 0.55f - 0.30f * t, 0.85f - 0.40f * t, 1.0f});
      }
    }
    if (snapped || elapsed < kSnapAt) return;

    // THE READING, once. Everything below is read off the tree that was
    // just drawn; nothing here computes a second copy of it.
    Composer& composer = ctx.composer;
    frame = composer.stats();
    const std::vector<Composer::NodeCost>& rows = composer.profile();
    volatileNodes = 0;
    bakedNodes = 0;
    for (const Composer::NodeCost& row : rows) {
      if (row.refused(Composer::Promotion::Volatile)) ++volatileNodes;
      if (row.cacheState == Composer::CacheState::Texture ||
          row.cacheState == Composer::CacheState::Promoted ||
          row.cacheState == Composer::CacheState::Group)
        ++bakedNodes;
      // A profile row's label is the node's key followed by its kind and
      // size, so the key is what stands before the first space.
      const std::string key = row.label.substr(0, row.label.find(' '));
      if (const std::optional<SkRect> rect = composer.bounds(key))
        marks.push_back({*rect, row.cacheState});
    }
    profiled = rows.size();
    worst.assign(rows.begin(),
                 rows.begin() + (long)std::min<size_t>(kRows, rows.size()));
    snapped = true;
    composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(VolatilityCost, "Kit \xc2\xb7 API",
             "the caching proof \xe2\x80\x94 every keyed node outlined in "
             "the tier it took, the costliest listed with the condition "
             "that refused each a bake, and Composer::stats() beside them")
