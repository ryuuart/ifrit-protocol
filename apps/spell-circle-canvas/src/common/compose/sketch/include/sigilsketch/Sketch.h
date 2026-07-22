#pragma once
// The sketch surface: a p5-style live-coding entry point over the full
// SigilCompose API. A sketch is REAL C++ — the same Composer, elements,
// decorations, bindings, and custom paint programs the products use —
// compiled on save into a small dylib and hot-swapped into the running
// ComposeSketch host (the cr.h / RuntimeCompiledC++ pattern).
//
//   #include <sigilsketch/Sketch.h>
//   struct MySketch : sigil::compose::sketch::Sketch {
//     void setup(SketchContext &ctx) override { ctx.composer.render(...); }
//     void update(double elapsed, SketchContext &ctx) override {}
//   };
//   SIGIL_SKETCH(MySketch)
//
// Rules of the road:
//  - keep state in members; every reload constructs a fresh instance
//    (the shared clock keeps running, so elapsed time is continuous)
//  - the host never dlcloses old sketch libraries (statics stay valid;
//    one small leak per reload is the industry-standard trade)
//  - after rebuilding the framework itself, restart the host: the ABI
//    version below only guards against stale sketch binaries

#include <sigilcompose/Compose.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Util.h>
#include <sigilsketch/Assets.h>

#include <string_view>
#include <vector>

namespace sigil::compose::sketch {

/** Bumped whenever SketchContext/Sketch change shape; the host refuses
 *  sketch dylibs built against another version. */
inline constexpr unsigned kAbiVersion = 3;

/** The canvas the host realizes for the sketch: logical size (the
 *  window letterboxes it, headless captures honor it) and the clear
 *  color behind the scene. */
struct CanvasSpec {
  SkSize size = {900, 640};
  SkColor4f background = {0.043f, 0.039f, 0.078f, 1};
};

struct SketchContext {
  Composer &composer;        // render()/renderSlot()/query surface
  sigil::motion::Ticker &ticker; // steppables + choreograph timeline
  Assets &assets;            // hot-reloading image loader
  SkSize size;               // the current logical canvas size
  CanvasSpec *spec = nullptr; // host-owned; written via the calls below
  sigil::weave::FontContext *fonts = nullptr; // the composer's fonts —
                                              // measure()/snapshot() fuel
  /** `--deterministic`: the host is taking a capture that will be DIFFED,
   *  so anything the sketch measured about its own execution must be
   *  pinned. See `measured()` below — read the flag directly only when
   *  you need to suppress a whole panel rather than one number. */
  bool deterministic = false;

  /** A number the sketch measured about ITS OWN EXECUTION — a build time,
   *  a bake cost, a live node count, a frame counter. Returns @p value
   *  normally and @p pinned under `--deterministic`.
   *
   *  WHY THIS EXISTS. A study that draws its own measurements into its own
   *  plate is not a reproducible capture: it differs from ITSELF between
   *  two runs of the same binary. Measured on this corpus —
   *  `genesis_fire` 34 differing pixels, `slitscan_2001` 16, against a
   *  negative control at 0. So every pixel sweep reports those studies as
   *  changed by any patch, including a patch that changes nothing, and the
   *  false positive has exactly the shape (small, clustered, plausible) of
   *  the real regression a sweep is for. It cost this program two rounds
   *  of cropping and looking to clear a change that had altered nothing.
   *
   *  The rule is broader than clocks: it is any value the sketch computed
   *  from its own execution rather than from its data. A node count is
   *  usually stable and a bake time never is, but both belong here,
   *  because "usually stable" is what makes the eventual diff mystifying.
   *
   *      std::snprintf(buf, n, "BUILD %.2f ms", ctx.measured(buildMs)); */
  double measured(double value, double pinned = 0.0) const {
    return deterministic ? pinned : value;
  }

  /** One-shot intrinsic measurement (compose::measure with the host's
   *  fonts): size marquee strips, tooltips, badges from their content. */
  SkSize measure(Element element, SkSize maxSize = SkSize::MakeEmpty()) {
    return fonts ? sigil::compose::measure(std::move(element), *fonts, maxSize)
                 : SkSize::MakeEmpty();
  }

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

// ---------------------------------------------------------------------------
// Statically linked sketches
//
// A sketch normally compiles to a dylib the host dlopens. Compile one with
// SIGIL_SKETCH_STATIC defined to its key instead and SIGIL_SKETCH registers a
// factory under that key rather than exporting the C entry points — the same
// file is then both a hot-reload sketch and a scene an ordinary host can
// construct. ComposeGallery builds every study this way, so a study needs no
// per-file bookkeeping to appear there and cannot drift out of sync with the
// file people actually edit.

/// Builds one instance of a sketch. Registration keeps the factory, not the
/// sketch: a host constructs a fresh one every time it activates the scene,
/// exactly as a reload does.
using SketchFactory = Sketch *(*)();

struct StaticSketch {
  const char *key = nullptr; ///< SIGIL_SKETCH_STATIC, i.e. the file stem
  SketchFactory factory = nullptr;
};

/** Every sketch linked into this binary, in static-initialization order
 *  (which is link order, and therefore not worth depending on — hosts
 *  should look up by key). */
[[nodiscard]] std::vector<StaticSketch> &staticSketches();

/** Registers `factory` under `key`. Returns true so it can initialize a
 *  namespace-scope bool; SIGIL_SKETCH is the only intended caller. */
bool registerStaticSketch(const char *key, SketchFactory factory);

/** The factory registered under `key`, or nullptr when this binary was not
 *  built with that sketch. */
[[nodiscard]] SketchFactory findStaticSketch(std::string_view key);

} // namespace sigil::compose::sketch

#ifdef SIGIL_SKETCH_STATIC

/** Register the sketch with the host it is compiled into. */
#define SIGIL_SKETCH(SketchType)                                         \
  namespace {                                                            \
  [[maybe_unused]] const bool sigilSketchRegistered =                    \
      ::sigil::compose::sketch::registerStaticSketch(                    \
          SIGIL_SKETCH_STATIC,                                           \
          []() -> ::sigil::compose::sketch::Sketch * {                   \
            return new SketchType();                                     \
          });                                                            \
  }

#else

/** Export the sketch entry points. Exactly one per sketch file. */
#define SIGIL_SKETCH(SketchType)                                         \
  extern "C" __attribute__((visibility("default")))                      \
  sigil::compose::sketch::Sketch *sigilSketchCreate() {                  \
    return new SketchType();                                             \
  }                                                                      \
  extern "C" __attribute__((visibility("default"))) unsigned             \
  sigilSketchAbi() {                                                     \
    return sigil::compose::sketch::kAbiVersion;                          \
  }

#endif
