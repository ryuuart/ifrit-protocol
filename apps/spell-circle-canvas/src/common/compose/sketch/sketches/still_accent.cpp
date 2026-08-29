// still_accent.cpp — one bound fill that never moves, in a panel big
// enough for the cost of getting it wrong to show.
// =============================================================================
// A colour bound to an animation output is not the same thing as a colour
// that is currently changing, and this panel is the case where the
// difference is expensive. A single `fill(&output)` on a single leaf marks
// that leaf's own content volatile; volatility propagates upward, so the
// root answers "volatile" too, and the whole subtree is refused texture
// promotion. Picture caching cannot rescue it — replaying a picture still
// re-runs every draw call — so all `kCells` stroked, shaped cells redraw
// on every frame for the sake of one star whose colour is standing still.
//
// What keeps this panel cheap: the bound fill's resolved value rides in the
// content-scalar memo, so the recording stays valid while the value holds;
// a settle counter then clears the volatility flag after 8 consecutive
// frames of a provably identical value; and the released node re-declares
// itself volatile on the very frame the output moves, before anything
// stale can be replayed. A parked binding therefore costs what a plain
// colour costs, and only starts paying when it is actually used.
//
// EDIT THESE FIRST
//   kCells      — large enough that per-cell picture replay dominates the
//                 frame. Drop it to 32 for the opposite regime, where the
//                 subtree is too cheap for promotion to be worth firing and
//                 the distinction costs nothing either way.
//   kRepaintHz  — 0 holds the colour still for the whole run, so everything
//                 above the accent settles, releases, and the panel becomes
//                 a blit. Set it non-zero to watch the cycle instead: each
//                 change re-declares volatility for a frame, the panel
//                 repaints, then it settles and releases 8 frames later.
//
// The accent's colour is driven by an animation output, and deliberately
// parked. Parked is the whole point.

#include <sigilcompose/Brushes.h>
#include <sigilsketch/Sketch.h>

#include <cmath>
#include <string>

using namespace sigil::compose;

namespace {

constexpr int kCells = 512;         // enough cells for picture replay to hurt
constexpr double kRepaintHz = 0.0;  // 0 = the bound colour never moves

sigil::weave::TextStyle type(float size, SkColor4f color) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = size;
  style.paint.foreground.setColor4f(color, nullptr);
  style.paint.foreground.setAntiAlias(true);
  return style;
}

const SkColor4f kInk{0.92f, 0.94f, 0.98f, 1};
const SkColor4f kDim{0.56f, 0.61f, 0.72f, 1};
const SkColor4f kAccent{0.95f, 0.35f, 0.18f, 1};

/** The same shape as the `slowThemedPanel` fixture in ComposeCoreBench.cpp:
 *  `kCells` stroked, shaped cells — each recording its own picture — plus
 *  ONE accent cell in the same row whose fill is BOUND rather than a plain
 *  value. Everything above that accent shares its volatility, which is why
 *  a single leaf decides the cost of the whole panel. */
Element panel(const choreograph::Output<Fill>* tint) {
  auto row = box().key("row").row().wrapLines().gap(2);
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
  return box().key("frame").column().padding(4).child(std::move(row));
}

}  // namespace

struct StillAccent : sigil::compose::sketch::Sketch {
  choreograph::Output<Fill> tint{Fill::color(kAccent)};  // assigned ONCE
  int step = 0;

  Element describe() {
    return box()
        .key("root")
        .column()
        .padding(6)
        .child(text(toU8("one bound fill, holding still \xc2\xb7 " +
                         std::to_string(kCells) + " cells"),
                    type(14, kInk)))
        .child(text(toU8("the seven-point star's colour is fill(&output) "
                         "\xe2\x80\x94 bound, but parked. a parked binding "
                         "settles, releases, and the panel promotes like a "
                         "plain colour; the frame the output moves, it "
                         "declares itself volatile again before anything "
                         "stale can replay."),
                    type(11, kDim)))
        .child(panel(&tint));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(900, 620);
    ctx.background({0.055f, 0.06f, 0.085f, 1});
    ctx.captureAt(1.0);  // well past the 8-frame settle: the released state
    ctx.composer.render(describe());
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

SIGIL_SKETCH(StillAccent)
