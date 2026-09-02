// volatility_cost.cpp — WHAT DECLARING A PROPERTY VOLATILE COSTS, and
// when the runtime stops charging for it.
// =============================================================================
// A bound property marks its leaf's content volatile, and volatility
// propagates UPWARD: the root answers "volatile" too, and the whole
// subtree is refused texture promotion. Picture caching cannot rescue it —
// replaying a picture still re-runs every draw call. So one leaf decides
// what a whole panel costs, and the two halves of this sheet are the two
// sides of that bill.
//
//   LEFT — THE CHEAP SIDE. Three hundred static cards under two dozen
//   binding-driven movers. The movers are volatile and are drawn live;
//   the cards are not, and are proved to be sitting still. The bill is
//   exactly the movers, because volatility is a property of a subtree
//   rather than of the frame.
//
//   RIGHT — THE RELEASED SIDE. Five hundred stroked, shaped cells whose
//   only volatile thing is one seven-point star with `fill(&output)` —
//   bound, and deliberately PARKED. What keeps this cheap: the bound
//   fill's resolved value rides in the content-scalar memo, so the
//   recording stays valid while the value holds; a settle counter clears
//   the volatility flag after 8 consecutive frames of a provably
//   identical value; and the released node re-declares itself volatile on
//   the very frame the output moves, before anything stale can be
//   replayed. A parked binding costs what a plain colour costs and only
//   starts paying when it is actually used.
//
// Parked is the whole point on the right, and moving is the whole point
// on the left. One is what the flag is for; the other is what the release
// is for.
//
// EDIT THESE FIRST
//   kMovers / kCards — the left field. The movers are what paints; the
//                      cards are what does not, whatever their count.
//   kCells      — the right panel, large enough that per-cell picture
//                 replay dominates the frame. Drop it to 32 for the
//                 opposite regime, where the subtree is too cheap for
//                 promotion to be worth firing and the distinction costs
//                 nothing either way.
//   kRepaintHz  — 0 holds the accent's colour still for the whole run, so
//                 everything above it settles, releases, and the panel
//                 becomes a blit. Set it non-zero to watch the cycle
//                 instead: each change re-declares volatility for a frame,
//                 the panel repaints, then it settles and releases 8
//                 frames later.

#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/typography/Type.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr int kCards = 300;         // static, cached, never repainted
constexpr int kMovers = 24;         // bound, volatile, painted every frame
constexpr int kCells = 512;         // enough cells for picture replay to hurt
constexpr double kRepaintHz = 0.0;  // 0 = the bound colour never moves

constexpr float kFieldWidth = 640.0f;
constexpr float kFieldHeight = 644.0f;
/** The cell panel wraps at a stated width, so both halves stand the
 *  same height and one caption line runs under each. */
constexpr float kCellsWidth = 672.0f;

const SkColor4f kInk{0.92f, 0.94f, 0.98f, 1};
const SkColor4f kDim{0.56f, 0.61f, 0.72f, 1};
const SkColor4f kAccent{0.95f, 0.35f, 0.18f, 1};

/** `kCells` stroked, shaped cells — each recording its own picture — plus
 *  ONE accent cell in the same row whose fill is BOUND rather than a plain
 *  value. Everything above that accent shares its volatility, which is why
 *  a single leaf decides the cost of the whole panel. */
Element cells(const choreograph::Output<Fill>* tint) {
  auto row = box().key("row").width(kCellsWidth).row().wrapLines().gap(2);
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
  return box().key("frame").column().child(std::move(row));
}

}  // namespace

struct VolatilityCost : sketch::Sketch {
  std::vector<std::unique_ptr<choreograph::Output<float>>> movers;
  choreograph::Output<Fill> tint{Fill::color(kAccent)};  // assigned ONCE
  int step = 0;

  /** The left field: cards that never repaint, under movers that always
   *  do. The cards are laid from a fixed seed, so the field is the same
   *  picture on every run. */
  Element field(sigil::motion::Ticker& ticker) {
    auto root = box()
                    .key("field")
                    .width(kFieldWidth)
                    .height(kFieldHeight)
                    .fill(Fill::color({0.04f, 0.04f, 0.08f, 1}));
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    std::mt19937 rng{3};
    for (int i = 0; i < kCards; ++i) {
      const float x = (float)(rng() % 600), y = (float)(rng() % 616);
      root.child(box()
                     .width(34)
                     .height(22)
                     .corners({4})
                     .inset(x, y, 0, 0)
                     .fill(Fill::color({0.09f, 0.10f, 0.16f, 1})));
    }
    for (int i = 0; i < kMovers; ++i) {
      auto out = std::make_unique<choreograph::Output<float>>(0.0f);
      const float y = 14.0f + 26.0f * (float)i;
      const float phase = (float)i * 0.7f;
      root.child(box()
                     .width(46)
                     .height(18)
                     .corners({4})
                     .inset(0, y, 0, 0)
                     .translateX(out.get())
                     .fill(Fill::color({0.49f, 0.91f, 1.0f, 0.8f})));
      movers.push_back(std::move(out));
      ticker.add([o = movers.back().get(), phase, t = 0.0](double dt) mutable {
        t += dt;
        *o = 300.0f + 290.0f * (float)std::sin(t * 0.9 + phase);
        return true;
      });
    }
    return root;
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1400, 764);
    ctx.background({0.055f, 0.06f, 0.085f, 1});
    ctx.captureAt(2.5);  // well past the 8-frame settle: the released state
    movers.clear();

    ctx.composer.render(
        stack()
            .child(text(toU8("volatility propagates upward \xc2\xb7 one bound "
                             "leaf decides what its whole subtree costs"),
                        type({.size = 15.0f, .color = kInk}))
                       .left(24)
                       .top(16))
            .child(
                box()
                    .row()
                    .left(24)
                    .top(52)
                    .gap(24)
                    .child(
                        box()
                            .column()
                            .gap(6)
                            .child(field(ctx.ticker))
                            .child(text(
                                toU8(std::to_string(kMovers) +
                                     " bound movers over " +
                                     std::to_string(kCards) + " cached cards"),
                                type({.size = 13.0f, .color = kInk})))
                            .child(text(toU8("only the movers paint; the cards "
                                             "are proved to be sitting still"),
                                        type({.size = 11.0f, .color = kDim}))))
                    .child(
                        box()
                            .column()
                            .gap(6)
                            .child(cells(&tint))
                            .child(text(toU8("one bound fill, holding "
                                             "still \xc2\xb7 " +
                                             std::to_string(kCells) + " cells"),
                                        type({.size = 13.0f, .color = kInk})))
                            .child(
                                text(toU8("the seven-point star's colour is "
                                          "fill(&output) \xe2\x80\x94 bound, "
                                          "but parked: it settles, releases, "
                                          "and the panel promotes like a "
                                          "plain colour"),
                                     type({.size = 11.0f, .color = kDim}))))));
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    (void)ctx;
    if (kRepaintHz <= 0.0) return;  // the colour never moves after setup
    // The demonstration mode: a change every 1/kRepaintHz seconds — each
    // one re-declares volatility for a frame, after which the value holds
    // long enough to settle and release again. Derived from `elapsed`, so a
    // still captured at a given time is reproducible.
    const int now = (int)std::floor(elapsed * kRepaintHz);
    if (now != step) {
      step = now;
      const float t = (float)(step % 5) / 5.0f;
      tint = Fill::color(
          {0.20f + 0.70f * t, 0.55f - 0.30f * t, 0.85f - 0.40f * t, 1.0f});
    }
  }
};

SIGIL_SKETCH(VolatilityCost, "Kit \xc2\xb7 API",
             "what a volatile property costs and when the runtime stops "
             "charging \xe2\x80\x94 bound movers over cached cards, beside "
             "one parked fill over hundreds of cells")
