// still_accent.cpp — §38's measured fixture, kept ALIVE: one bound fill
// that never moves, in a panel big enough for the cost to matter.
// =============================================================================
// ROADMAP §38 measured what §Argument 3 and §10g(4) filed twice without
// measuring: `isAnimated()` was per-node and BINARY, so one `fill(&output)`
// on one leaf — a colour that never moves — set `ownContent`,
// `computeVolatile` carried it up, promotion answered `Promotion::Volatile`
// at the root, and 512 stroked cells replayed 512 pictures at ~11 µs each
// forever. 5.02 ms/frame, 19.6× over the identical panel with the colour
// spelled as a plain value. Picture caching cannot save it (a picture
// re-runs its draw calls); texture promotion is the only thing that does,
// and the binary declaration is what denied it.
//
// THE EXTENSION THIS PANEL NOW DEMONSTRATES (§38, built 2026-08-04):
// `ContentScalars` carries the bound Fill's resolved value, so the §17 memo
// keeps the recording, §20's settle counter releases the volatility flag
// after 8 provably-identical frames, and the per-draw released scan
// re-declares THE FRAME the Output moves — before anything stale replays.
// A held bound fill now costs what a plain one costs; the binding is free
// until it is used.
//
// No corpus scene carried a slow bound fill (the §38 ledger sweep checked),
// which is why this study exists: it is the living reproduction of the
// measurement AND the demonstration that the extension retires it. NOTE it
// is new — the plate ledger reports it "not in baseline" until the owner
// rebases.
//
// EDIT THESE FIRST
//   kCells      — 512 reproduces the measured arm; drop to 32 to see the
//                 regime where promotion would never fire and the defect
//                 cost exactly nothing.
//   kRepaintHz  — 0 is the fixture (the property NEVER moves; with the
//                 release, everything above the accent caches and the
//                 panel is a blit). Set it non-zero to watch the §20
//                 cycle: each change re-declares for one frame, the panel
//                 repaints, then settles and re-releases 8 frames later.
//
// The three ways things move (hello.cpp): the accent is door 2 (driven) —
// and deliberately parked. Parked is the whole point.

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>

#include <cmath>
#include <string>

using namespace sigil::compose;
using namespace sigil::compose::util;

namespace {

constexpr int kCells = 512;        // the measured arm's population
constexpr double kRepaintHz = 0.0; // 0 = §38's fixture: never moves

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

/** The bench fixture verbatim (ComposeCoreBench.cpp, slowThemedPanel):
 *  `kCells` stroked, shaped cells — each records its own picture — plus
 *  ONE accent cell in the same row whose fill is BOUND. The accent's
 *  ancestors are what the binding used to poison. */
Element panel(const choreograph::Output<Fill> *tint) {
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

} // namespace

struct StillAccent : sigil::compose::sketch::Sketch {
  choreograph::Output<Fill> tint{Fill::color(kAccent)}; // assigned ONCE
  int step = 0;

  Element describe() {
    return box()
        .key("root")
        .column()
        .padding(6)
        .child(text(toU8("\xc2\xa7""38 \xc2\xb7 one bound fill, never "
                         "moving \xc2\xb7 512 cells"),
                    type(14, kInk)))
        .child(text(toU8("the seven-point star's colour is fill(&output). "
                         "before \xc2\xa7""38 that one binding denied "
                         "texture promotion to every ancestor forever: "
                         "5.02 ms/frame, 19.6\xc3\x97. now it settles, "
                         "releases, and the panel promotes like a plain "
                         "one."),
                    type(11, kDim)))
        .child(panel(&tint));
  }

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(900, 620);
    ctx.background({0.055f, 0.06f, 0.085f, 1});
    ctx.captureAt(1.0); // well past the 8-frame settle: the released state
    ctx.composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    (void)ctx;
    if (kRepaintHz <= 0.0)
      return; // §38's fixture: the property never moves after setup
    // The demonstration mode: a change every 1/kRepaintHz seconds — each
    // one re-declares volatility for a frame, then the §20 cycle settles
    // and re-releases. Derived from `elapsed`, so a still is reproducible.
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
