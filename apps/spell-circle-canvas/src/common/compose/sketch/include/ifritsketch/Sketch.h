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
inline constexpr unsigned kAbiVersion = 1;

struct SketchContext {
  Composer &composer;        // render()/renderSlot()/query surface
  ifrit::tick::Ticker &ticker; // steppables + choreograph timeline
  Assets &assets;            // hot-reloading image loader
  SkSize size;               // the logical canvas size
};

struct Sketch {
  virtual ~Sketch() = default;
  /** Called once per (re)load and again when an asset file changes. */
  virtual void setup(SketchContext &ctx) = 0;
  /** Called every frame with the clock's elapsed seconds. */
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
