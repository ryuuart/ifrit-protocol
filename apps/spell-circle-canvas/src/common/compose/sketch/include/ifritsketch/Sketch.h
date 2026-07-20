#pragma once
// The sketch surface: a p5-style live-coding entry point over the full
// IfritCompose API. A sketch is REAL C++ — the same Composer, elements,
// decorations, bindings, and custom paint programs the products use —
// compiled on save into a small dylib and hot-swapped into the running
// ComposeSketch host (the cr.h / RuntimeCompiledC++ pattern).
//
//   #include <ifritsketch/Sketch.h>
//   struct MySketch : ifrit::compose::sketch::Sketch {
//     void setup(SketchContext &ctx) override { ctx.composer.render(...); }
//     void update(double elapsed, SketchContext &ctx) override {}
//   };
//   IFRIT_SKETCH(MySketch)
//
// Rules of the road:
//  - keep state in members; every reload constructs a fresh instance
//    (the shared clock keeps running, so elapsed time is continuous)
//  - the host never dlcloses old sketch libraries (statics stay valid;
//    one small leak per reload is the industry-standard trade)
//  - after rebuilding the framework itself, restart the host: the ABI
//    version below only guards against stale sketch binaries

#include <ifritcompose/Compose.h>
#include <ifritcompose/Decorations.h>
#include <ifritcompose/Util.h>
#include <ifritsketch/Assets.h>

namespace ifrit::compose::sketch {

/** Bumped whenever SketchContext/Sketch change shape; the host refuses
 *  sketch dylibs built against another version. */
inline constexpr unsigned kAbiVersion = 2;

/** The canvas the host realizes for the sketch: logical size (the
 *  window letterboxes it, headless captures honor it) and the clear
 *  color behind the scene. */
struct CanvasSpec {
  SkSize size = {900, 640};
  SkColor4f background = {0.043f, 0.039f, 0.078f, 1};
};

struct SketchContext {
  Composer &composer;        // render()/renderSlot()/query surface
  ifrit::tick::Ticker &ticker; // steppables + choreograph timeline
  Assets &assets;            // hot-reloading image loader
  SkSize size;               // the current logical canvas size
  CanvasSpec *spec = nullptr; // host-owned; written via the calls below

  /** p5's createCanvas: declare the logical canvas size. Usually in
   *  setup(); calling later resizes live (applied next frame). */
  void canvas(float width, float height) {
    if (spec)
      spec->size = {width, height};
    size = {width, height}; // visible immediately, p5-style
  }
  /** The clear color behind the scene (p5's background). */
  void background(SkColor4f color) {
    if (spec)
      spec->background = color;
  }
};

/** Retained-mode, not p5's redraw loop — three paths for motion:
 *  1. setup() DECLARES the scene once, wiring in its animation
 *     (bound Outputs, transitions, ticker steppables) — the runtime
 *     then animates every frame without re-describing anything.
 *  2. custom() leaves with Cache::None are the immediate-mode floor:
 *     their paint program runs per frame with elapsedSeconds.
 *  3. update() is the DATA path: when state changes, describe again
 *     via composer.render(...) and the reconciler diffs it. Do not
 *     re-render every frame out of habit — bindings are cheaper. */
struct Sketch {
  virtual ~Sketch() = default;
  /** Called once per (re)load and again when an asset file changes.
   *  Declare the scene here, animation wiring included. */
  virtual void setup(SketchContext &ctx) = 0;
  /** Called every frame with the clock's elapsed seconds — react to
   *  DATA changes here by re-rendering a fresh description; leave
   *  per-frame motion to bindings and Cache::None paint programs. */
  virtual void update(double elapsed, SketchContext &ctx) {
    (void)elapsed;
    (void)ctx;
  }
};

} // namespace ifrit::compose::sketch

/** Export the sketch entry points. Exactly one per sketch file. */
#define IFRIT_SKETCH(SketchType)                                         \
  extern "C" __attribute__((visibility("default")))                      \
  ifrit::compose::sketch::Sketch *ifritSketchCreate() {                  \
    return new SketchType();                                             \
  }                                                                      \
  extern "C" __attribute__((visibility("default"))) unsigned             \
  ifritSketchAbi() {                                                     \
    return ifrit::compose::sketch::kAbiVersion;                          \
  }
