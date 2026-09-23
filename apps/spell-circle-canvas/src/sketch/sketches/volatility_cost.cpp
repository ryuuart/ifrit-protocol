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
 *   RIGHT — THE RELEASED SIDE. A dense field of stroked, shaped cells whose
 *     only volatile thing is one seven-point star with `fill(&output)` —
 *     bound, and deliberately PARKED. The bound fill's resolved value
 *     rides in the content-scalar memo, so the recording stays valid
 *     while the value holds; a settle counter clears the volatility flag
 *     after eight consecutive frames of a provably identical value; and
 *     the released node re-declares itself volatile on the very frame the
 *     output moves. A parked binding costs what a plain colour costs and
 *     only starts paying when it is actually used.
 *
 * THE PROOF, in three readings, all taken from a COMPOSER THE SHEET OWNS.
 * A caching verdict is taken against a promotion policy, so a sheet that
 * read its own tree would be reporting the host it was opened by rather
 * than a property of what it describes. This one hands the same tree to a
 * composer of its own, opened EAGER, and reads that: what every node's
 * DESCRIPTION admits or refuses, identical on every machine and under
 * every host.
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
 * Counts and cache verdicts are read from the probe. Stopwatch readings
 * are hidden in deterministic captures, where machine-dependent timings
 * cannot be compared meaningfully.
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

// TAGS: Runtime/Caching

#include <include/utils/SkNoDrawCanvas.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace draw = sigil::draw;
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

constexpr float kFieldWidth = 620.0f;
constexpr float kFieldHeight = 560.0f;
constexpr float kCellsWidth = 620.0f;

constexpr material::Color kInk{0.92f, 0.94f, 0.98f, 1};
constexpr material::Color kDim{0.56f, 0.61f, 0.72f, 1};
constexpr material::Color kAccent{0.95f, 0.35f, 0.18f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.055f, 0.06f, 0.085f, 1};
  look.palette.ink = kInk;
  look.palette.ash = kDim;
  look.palette.rule = {0.19f, 0.20f, 0.26f, 1};
  // The readout's own register: every reading is one size, the name in
  // the quiet ink and the figure in the bright one.
  look.palette.figure = look.palette.ink;
  look.type.captionLabel = {.size = 11};
  look.spacing.labelGap = 8;
  look.spacing.rowGap = 5;
  look.spacing.swatchSide = 9;
  return look;
}

/** ONE ROW PER TIER: the colour the map and the key are both drawn in,
 *  the name `Composer::profile()` reports and what the node actually did.
 *  The map and its key cannot disagree when they read one table. */
struct Tier {
  Composer::CacheState state;
  material::Color color;
  const char* name;
  const char* what;
};
constexpr Tier kTiers[] = {{Composer::CacheState::Live,
                            {0.98f, 0.35f, 0.30f, 1},
                            "Live",
                            "painted from scratch"},
                           {Composer::CacheState::Picture,
                            {0.98f, 0.76f, 0.28f, 1},
                            "Picture",
                            "replayed a recording"},
                           {Composer::CacheState::Texture,
                            {0.36f, 0.72f, 1.00f, 1},
                            "Texture",
                            "blitted the author's bake"},
                           {Composer::CacheState::Promoted,
                            {0.40f, 0.90f, 0.55f, 1},
                            "Promoted",
                            "blitted the library's bake"},
                           {Composer::CacheState::SplitOwn,
                            {0.75f, 0.55f, 1.00f, 1},
                            "SplitOwn",
                            "own paint blitted, children live"},
                           {Composer::CacheState::Group,
                            {0.30f, 0.88f, 0.82f, 1},
                            "Group",
                            "whole subtree blitted, held still"}};

const Tier& tierOf(Composer::CacheState state) {
  for (const Tier& tier : kTiers)
    if (tier.state == state) return tier;
  return kTiers[0];
}

/** The sheet's one class past the registers: the heading over each block
 *  of the readout, in the page's own ink. */
sigil::compose::StyleSheet sheetClasses(const sketch::kit::Theme& look) {
  sigil::compose::StyleSheet classes =
      look.styleSheet() +
      sigil::compose::StyleSheet{sigil::compose::rule(".heading")
                                     .font({.size = 12.5f, .track = 0.8f})};
  return classes;
}

std::string ms(double v) { return kit::formatted("%.2f", v); }

/** `kCells` stroked, shaped cells — each recording its own picture — plus
 *  ONE accent cell in the same row whose fill is BOUND rather than a plain
 *  value. Everything above that accent shares its volatility, which is why
 *  a single leaf decides the cost of the whole panel. */
Element cells(const choreograph::Output<Fill>* tint) {
  const auto cell = [](int id) {
    const float t = 0.20f + 0.04f * (float)(id % 6);
    return box()
        .key("c" + std::to_string(id))
        .width(26)
        .height(26)
        .shape(shapes::star(5 + id % 3, 0.45f, 0.08f))
        .fill(Fill::color({t, 0.45f, 0.68f, 1.0f}))
        .stroke(brush::solid(1.5f, Fill::color({0.95f, 0.86f, 0.55f, 1.0f})));
  };
  // The panel is held to the field's box so the two stand the same
  // height and the readout below them starts on one line.
  return box()
      .key("cellPanel")
      .column()
      .width(kCellsWidth)
      .height(kFieldHeight)
      .children({box().key("cells").row().flexWrap().gap(2).children(
          {each(kCells, cell),
           box()
               .key("accent")
               .width(26)
               .height(26)
               .shape(shapes::star(7, 0.45f, 0.08f))
               .fill(Animatable<Fill>(tint))
               .stroke(brush::solid(
                   1.5f, Fill::color({0.10f, 0.10f, 0.12f, 1.0f})))})});
}

}  // namespace

struct VolatilityCost {
  /** THE PROBE, and why the sheet owns one. Every number here is a
   *  CACHING VERDICT, and a verdict is taken against a promotion policy —
   *  so a sheet reading its own tree would report the host's policy rather
   *  than a property of the description, and the same binary would draw a
   *  different sheet depending on how it was opened. This composer is the
   *  sheet's own: it is handed the SAME tree, stepped on the same clock,
   *  and opened EAGER, so what it reports is which nodes the promoter's
   *  rules admit and which condition refused the rest — the description's
   *  own answer, identical on every machine and under every host.
   *
   *  It draws into nothing. Painting is what produces a verdict, so the
   *  probe is painted; a no-draw canvas runs the whole phase and fills no
   *  pixels. And it is LET GO the moment the reading is frozen, so the
   *  sheet's steady-state frame costs exactly what it did before. */
  std::unique_ptr<Composer> probe;
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
      ticker.add([o = movers.back().get(), phase, &ticker] {
        const double t = ticker.elapsed();
        *o = 280.0f + 270.0f * (float)std::sin(t * 0.9 + phase);
      });
    }
  }

  /** The left field: cards that never repaint, under movers that always
   *  do. The cards are laid from a fixed seed, so the field is the same
   *  picture on every run. */
  Element field() {
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    std::mt19937 rng{3};
    const auto card = [&rng](int i) {
      return kit::at(box()
                         .key("k" + std::to_string(i))
                         .borderRadius({4})
                         .fill(Fill::color({0.09f, 0.10f, 0.16f, 1})),
                     (float)(rng() % 570), (float)(rng() % 532), 34, 22);
    };
    const auto mover = [this](int i) {
      return kit::at(box()
                         .key("m" + std::to_string(i))
                         .borderRadius({4})
                         .translateX(movers[(size_t)i].get())
                         .fill(Fill::color({0.49f, 0.91f, 1.0f, 0.8f})),
                     0, 12.0f + 22.0f * (float)i, 46, 18);
    };
    return box()
        .key("field")
        .width(kFieldWidth)
        .height(kFieldHeight)
        .fill(Fill::color({0.04f, 0.04f, 0.08f, 1}))
        .children({each(kCards, card), each(kMovers, mover)});
  }

  /** THE MAP: subject nodes the profile named, outlined in the colour
   *  of the tier it took. Drawn in one paint program over the whole
   *  canvas, because it is a reading of the tree and not a part of it —
   *  a node per outline would change what it is measuring. */
  Element tierMap() const {
    // KEYLESS, and it has to be: the marks ARE the reading, and they change
    // with the tree this measures — a key naming them would spell the
    // picture twice.
    return pen([marks = marks](draw::Pen& pen) {
             pen.noFill();
             pen.strokeWeight(1.4f);
             for (const Marked& m : marks) {
               pen.stroke(tierOf(m.state).color);
               pen.rect(m.rect.left() - 1.5f, m.rect.top() - 1.5f,
                        m.rect.width() + 3.0f, m.rect.height() + 3.0f);
             }
           })
        .zIndex(8)
        .hitTestable(false);
  }

  Element legend() const {
    std::vector<sketch::kit::LegendEntry> first, second;
    for (size_t i = 0; i < std::size(kTiers); ++i) {
      const Tier& tier = kTiers[i];
      (i < 3 ? first : second)
          .push_back({Fill::color(tier.color), tier.name, tier.what});
    }
    return box().row().gap(24).children(
        {sketch::kit::legend({.entries = std::move(first),
                              .column = true,
                              .gap = 10,
                              .strokeWidth = 1.4f})
             .width(310),
         sketch::kit::legend({.entries = std::move(second),
                              .column = true,
                              .gap = 10,
                              .strokeWidth = 1.4f})
             .width(400)});
  }

  /** THE FRAME, as the composer reports it: counts remain visible in a
   *  deterministic capture; stopwatch readings are hidden. */
  Element statsBlock(const sketch::SketchContext& ctx) const {
    // A COUNT IS A FUNCTION OF THE DESCRIPTION and is printed as it is;
    // a TIME is a function of the machine and is hidden when the host is
    // capturing for a diff.
    const auto count = [](size_t v) { return std::to_string(v); };
    const sketch::kit::Readout how{.measure = 280};
    const auto time = [&](double value) {
      return ctx.deterministic ? std::string("—") : ms(ctx.measured(value));
    };
    return box().column().gap(3).children(
        {text("FRAME / COUNTS + TIME").styleClass("heading"),
         sketch::kit::readout(
             {{u8"instances", count(frame.instances)},
              {u8"described", count(frame.describedNodes)},
              {u8"memo hits", count(frame.memoHits)},
              {u8"patched", count(frame.patchedNodes)},
              {u8"pictures held", count(frame.picturesLive)},
              {u8"textures held", count(frame.texturesLive)},
              {u8"recordings made", count(frame.picturesRecorded)},
              {u8"bakes made", count(frame.texturesBaked)},
              {u8"nodes painted", count(frame.nodesPainted)},
              {u8"reconcile ms", time(frame.reconcileMs)},
              {u8"layout ms", time(frame.layoutMs)},
              {u8"volatile ms", time(frame.volatileMs)},
              {u8"paint ms", time(frame.paintMs)}},
             how),
         box().height(8),
         text("CACHE VERDICT / ALL NODES").styleClass("heading"),
         sketch::kit::readout(
             {{u8"refused: Volatile", count((size_t)volatileNodes)},
              {u8"reached a bake", count((size_t)bakedNodes)},
              {u8"nodes profiled", count(profiled)}},
             how)});
  }

  /** REPRESENTATIVE SUBJECT NODES, costliest first, each with the tier it took
   * and the condition that refused it a bake. `selfMs` excludes children, so
   * the number lands on the node that actually costs. */
  Element costTable(const sketch::SketchContext& ctx) const {
    const auto subject = [](const std::string& key) {
      if (key == "field") return "moving field";
      if (key == "cellPanel") return "parked panel";
      if (key == "cells") return "star group";
      if (key == "accent") return "bound star";
      if (key == "k0") return "static card";
      if (key == "m0") return "moving card";
      return "static star";
    };
    std::vector<sketch::kit::Row> rows;
    for (const Composer::NodeCost& row : worst) {
      const std::string key = row.label.substr(0, row.label.find(' '));
      rows.push_back({{subject(key),
                       ctx.deterministic ? "—" : ms(ctx.measured(row.selfMs)),
                       tierOf(row.cacheState).name,
                       Composer::promotionReason(row.promotion)},
                      Fill::color(tierOf(row.cacheState).color)});
    }
    return box().width(752).column().gap(14).children(
        {text("SUBJECT / THE NODE'S OWN COST").styleClass("heading"),
         sketch::kit::table(
             std::move(rows),
             {.columns = {{.head = "SUBJECT", .width = 128, .figure = true},
                          {.head = "SELF MS", .width = 72, .figure = true},
                          {.head = "TIER", .width = 98},
                          {.head = "PROMOTION VERDICT", .width = 340}},
              .gap = 14,
              .swatchSide = 9,
              .ruled = true,
              .headRuled = true}),
         document::caption(
             ctx.deterministic
                 ? "Capture mode hides machine-dependent times and orders "
                   "rows by key. Counts and cache decisions remain the "
                   "probe's actual results."
                 : "Rows are ordered by self time. Self time excludes "
                   "children; the probe freezes its result at two seconds.")
             .width(440),
         legend()});
  }

  Element readout(const sketch::SketchContext& ctx) const {
    if (!snapped)
      return box().height(350).children({document::caption(
          "Observing the two workloads… snapshot at " + ms(kSnapAt) + " s")});
    return box().row().gap(48).children({statsBlock(ctx), costTable(ctx)});
  }

  Element describe(sketch::SketchContext& ctx) {
    // The theme is bound where the tree is DESCRIBED, not where setup
    // runs: this sketch describes again on every reading, and a scope
    // that ended with setup would not be there.
    const sketch::kit::Theme look = sheetTheme();
    const sketch::kit::Provide bound(look);
    // The map is a SIBLING of the sheet, not a child of it: it draws in
    // canvas coordinates, which is what `bounds()` answers in, and a
    // child of the padded page would be offset by the page's margins.
    return stack()
        .applyStyleSheet(sheetClasses(look))
        .inset(0)
        .children(
            {tierMap(),
             sketch::kit::page(
                 {.title = "What still needs to be painted?",
                  .subtitle = "Two workloads, one eager promotion policy · "
                              "actual cache decisions sampled at " +
                              ms(kSnapAt) + " s",
                  .footer = "Outlines mark subject nodes only. A picture "
                            "replays drawing commands; a texture replaces that "
                            "work with a blit. One parked binding can settle "
                            "without losing its connection."},
                 box().column().gap(30).children(
                     {sketch::kit::comparison(
                          {.cases =
                               {{.title = "MOVING / STATIC CARDS + LIVE MARKS",
                                 .control = "300 cached cards · 24 "
                                            "continuously bound movers",
                                 .figure = field(),
                                 .note =
                                     "Motion stays local to the moving leaves. "
                                     "The static cards can retain their work."},
                                {.title = "PARKED / A BOUND COLOUR THAT HOLDS",
                                 .control =
                                     "417 shaped stars · one parked bound fill",
                                 .figure = cells(&tint),
                                 .note = "After identical frames, the bound "
                                         "star can release volatility and let "
                                         "its containing panel settle."}},
                           .measure = 1280,
                           .gap = 40}),
                      readout(ctx)}))});
  }

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // the reading is taken and frozen by then
    sketch::kit::stage(ctx, {.size = {1360, 1400}, .captureAt = kSnapAt + 0.5});
    movers.clear();
    marks.clear();
    worst.clear();
    snapped = false;
    makeMovers(ctx.ticker);
    // The probe carries the same tree at the same size, so every rect it
    // answers lands where the sheet drew the node it is about.
    probe = std::make_unique<Composer>(ctx.ticker, *ctx.fonts);
    probe->setSize(ctx.size);
    // The per-node reading is what this sheet is; it costs a timing call
    // per node and is off everywhere else.
    probe->setProfiling(true);
    probe->setAutoTexturePromotion(Composer::PromotionPolicy::Eager);
    probe->render(describe(ctx));
    ctx.composer.render(describe(ctx));
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
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
    if (snapped) return;
    // The probe is painted every frame up to the reading and never after
    // it: a verdict is what a paint produces, and there is no second
    // reading to take.
    if (probe) {
      SkNoDrawCanvas nowhere((int)ctx.size.width(), (int)ctx.size.height());
      probe->draw(nowhere);
    }
    if (elapsed < kSnapAt) return;

    // THE READING, once, off the probe's tree — the same tree the sheet
    // drew, judged under a policy the sheet owns.
    Composer& composer = *probe;
    frame = composer.stats();
    // A PROFILE IS A RANKING BY THE STOPWATCH, and a capture that will be
    // diffed cannot carry one: on a machine that ran differently the same
    // tree ranks differently, so the sheet would draw a different table
    // from itself. WHICH nodes there are, what tier each took and what
    // refused each a bake are facts of the description; only their order
    // is a fact of the machine. So under a capture the rows are read in
    // the description's own order, and the times beside them are hidden.
    std::vector<Composer::NodeCost> rows = composer.profile();
    if (ctx.deterministic)
      std::sort(rows.begin(), rows.end(),
                [](const Composer::NodeCost& a, const Composer::NodeCost& b) {
                  return a.label < b.label;
                });
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
      const bool numbered =
          key.size() > 1 && (key[0] == 'c' || key[0] == 'k' || key[0] == 'm') &&
          std::all_of(key.begin() + 1, key.end(),
                      [](char c) { return c >= '0' && c <= '9'; });
      const bool subject = numbered || key == "field" || key == "cellPanel" ||
                           key == "cells" || key == "accent";
      if (subject)
        if (const std::optional<SkRect> rect = composer.bounds(key))
          marks.push_back({*rect, row.cacheState});
      if (key == "field" || key == "cellPanel" || key == "cells" ||
          key == "accent" || key == "k0" || key == "m0" || key == "c0")
        worst.push_back(row);
    }
    profiled = rows.size();
    snapped = true;
    probe.reset();  // the reading is frozen; nothing else asks it anything
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(VolatilityCost, "Kit · API",
             "the caching proof — subject nodes outlined in "
             "the tier it took, the costliest listed with the condition "
             "that refused each a bake, and Composer::stats() beside them")
