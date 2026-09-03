#pragma once

/** @file
 * The 2D sketch surface: a p5-style entry point over the full compose
 * API. Include this and SIGIL_SKETCH registers a sketch that draws a
 * compose Element tree.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/CanvasSpec.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>

#include <algorithm>
#include <concepts>
#include <memory>
#include <vector>

namespace sigil::compose {
class TextureScene;
}
namespace sigil::world {
class Frame;
}
namespace sigil::geometry::mesh::camera {
struct Camera;
}

namespace sigil::sketch {

/** WHAT A SKETCH IS HANDED every time it is asked to describe itself.
 *
 *  It is a PER-FRAME value the host rebuilds, and non-copyable
 *  deliberately: capturing it in a steppable by reference dangles next
 *  frame, and capturing a copy would hold a stale spec and a stale size
 *  just as silently. Neither compiles. Capture `ctx.composer` (stable
 *  for the sketch's life) or plain data instead. */
struct SketchContext {
  compose::Composer& composer;    // render()/renderSlot()/query surface
  sigil::motion::Ticker& ticker;  // steppables + choreograph timeline
  Assets& assets;                 // hot-reloading resource access
  SkSize size;                    // the current logical canvas size
  CanvasSpec* spec = nullptr;     // host-owned; written via the calls below
  sigil::weave::FontContext* fonts = nullptr;  // measure()/snapshot() fuel
  /** Host-owned: the texture scenes `textureScene()` handed out, kept
   *  for the session's life. */
  std::vector<std::shared_ptr<compose::TextureScene>>* scenes = nullptr;

  SketchContext(compose::Composer& composerIn, sigil::motion::Ticker& tickerIn,
                Assets& assetsIn, SkSize sizeIn, CanvasSpec* specIn = nullptr,
                sigil::weave::FontContext* fontsIn = nullptr,
                bool deterministicIn = false,
                std::vector<std::shared_ptr<compose::TextureScene>>* scenesIn =
                    nullptr)
      : composer(composerIn),
        ticker(tickerIn),
        assets(assetsIn),
        size(sizeIn),
        spec(specIn),
        fonts(fontsIn),
        scenes(scenesIn),
        deterministic(deterministicIn) {}
  SketchContext(const SketchContext&) = delete;
  SketchContext& operator=(const SketchContext&) = delete;

  /** A COMPOSE SCENE PAINTED INTO A TEXTURE, @p size pixels across and
   *  cleared to @p background: hand it a tree with `render()` and read
   *  `image()` or `texture()` back. Its own words are SigilCompose's,
   *  from `<sigilcompose/texture/Texture.h>`.
   *
   *  THE SESSION KEEPS IT and lets go of everything it kept when the
   *  body declares again, so a sketch may take the image and drop the
   *  scene — which it could not do on its own, because a scene standing
   *  on a device destroys the texture its image names when it goes, and
   *  only the raster path leaves a picture behind that outlives it.
   *
   *  ASK WHILE DECLARING. A body that asks every frame holds every
   *  frame's scene until the next declaration, the way cooking a mesh
   *  every frame holds every frame's mesh; the session's counters say
   *  how many it is holding, so that costs what it costs in the open.
   *  Nothing has to be remade when time moves — a session's clock only
   *  goes forward, and a run that starts over is a new session with new
   *  scenes.
   *
   *  Null only where the host lent no fonts. */
  [[nodiscard]] std::shared_ptr<compose::TextureScene> textureScene(
      SkISize size, SkColor4f background = {0, 0, 0, 0});

  /** A LIT SET RENDERED ONCE INTO AN IMAGE @p size pixels across, over
   *  @p background — the picture INSIDE a page. A canvas plate is
   *  re-rendered at the capture scale, so a document that wanted a set
   *  in one of its panels cannot wear it as a texture the way a body
   *  does: it bakes the set at the pixels the panel will have and paints
   *  the image. The frame's own words are SigilWorld's, from
   *  `<sigilworld/frame/Frame.h>`.
   *
   *  SEEN FROM @p camera — unless the tree carries a viewpoint of its
   *  own, which wins here exactly as it wins in the set runtime, so a
   *  set that put its camera on a rail is photographed from that rail
   *  wherever it is photographed. The frame is formed and presented
   *  through the one viewpoint, so the two cannot disagree.
   *
   *  It draws on the CPU mesh executor whatever device the process
   *  holds, and declares no passes: the picture is a function of the
   *  frame, the viewpoint and the size, and of nothing the machine it
   *  ran on decides. The scene it stands the frame in lives for this
   *  call alone, so what comes back is an image and not a view onto
   *  something still standing. */
  [[nodiscard]] sk_sp<SkImage> bakeSet(
      const world::Frame& frame, const geometry::mesh::camera::Camera& camera,
      SkISize size, SkColor4f background = {0, 0, 0, 0});

  /** The host is taking a capture that will be DIFFED, so anything the
   *  sketch measured about its own execution must be pinned. See
   *  `measured()`; read the flag directly only when you need to suppress
   *  a whole panel rather than one number. */
  bool deterministic = false;

  /** A number the sketch measured about ITS OWN EXECUTION — a build
   *  time, a bake cost, a live node count, a frame counter. Returns
   *  @p value normally and @p pinned when the host is capturing for a
   *  diff.
   *
   *  WHY THIS EXISTS. A sketch that draws its own timings into its own
   *  plate is not a reproducible capture: it differs from ITSELF between
   *  two runs of the same binary. A pixel-comparison sweep then reports
   *  it as changed by a patch that changed nothing, and that false
   *  positive has exactly the shape — small, clustered, plausible — of
   *  the real regression the sweep exists to catch.
   *
   *  The rule is broader than clocks: it covers any value the sketch
   *  computed from its own execution rather than from its data. A node
   *  count is usually stable and a bake time never is, but both belong
   *  here, because "usually stable" is what makes the eventual diff
   *  mystifying.
   *
   *      std::snprintf(buf, n, "BUILD %.2f ms", ctx.measured(buildMs)); */
  [[nodiscard]] double measured(double value, double pinned = 0.0) const {
    return deterministic ? pinned : value;
  }

  /** One-shot intrinsic measurement with the host's fonts: size marquee
   *  strips, tooltips and badges from their content. */
  SkSize measure(const compose::Element& element,
                 SkSize maxSize = SkSize::MakeEmpty()) {
    return fonts ? compose::measure(element, *fonts, maxSize)
                 : SkSize::MakeEmpty();
  }

  /** Declare the logical canvas size. Usually in setup(); calling later
   *  resizes live, applied on the next frame. */
  void canvas(float width, float height) {
    if (spec) spec->size = {width, height};
    size = {width, height};  // visible immediately
  }
  /** The colour behind the scene. */
  void background(SkColor4f color) {
    if (spec) spec->background = color;
  }
  /** Declare the scene time a STILL of this sketch should be taken at —
   *  the moment the piece is most itself.
   *
   *      ctx.captureAt(7.2); // the hold at the end of the cycle */
  void captureAt(double seconds) {
    if (spec) spec->captureSeconds = seconds;
  }
  /** Declare how many device pixels per canvas pixel a PLATE of this
   *  sketch is taken at — a whole number, at least one, honoured
   *  exactly rather than fitted to the host's width budget. It changes
   *  nothing about the live window, which presents at the display's own
   *  scale.
   *
   *  Declare it on a pixel-exact reconstruction: one whose subject's
   *  pixel is a whole number of canvas pixels, so that downsampling the
   *  plate by that whole number lays it over the reference. A
   *  fractional scale spreads one source pixel over seven device pixels
   *  in one column and eight in the next, and no downsample recovers
   *  the reference from that.
   *
   *      ctx.oversample(2); // one 1994 pixel is 4 canvas px, so 8 here */
  void oversample(int perCanvasPixel) {
    if (spec) spec->oversample = std::max(1, perCanvasPixel);
  }
};

/** A SKETCH THAT DRAWS A COMPOSE ELEMENT TREE.
 *
 *  Retained-mode, not p5's redraw loop — three paths for motion:
 *   1. setup() DECLARES the scene once, wiring in its animation (bound
 *      Outputs, transitions, ticker steppables); the runtime then
 *      animates every frame without re-describing anything.
 *   2. custom() leaves with Cache::None are the immediate-mode floor:
 *      their paint program runs per frame with elapsedSeconds.
 *   3. update() is the DATA path: when state changes, describe again via
 *      composer.render(...) and the reconciler diffs it. Do not
 *      re-render every frame out of habit — bindings are cheaper.
 *
 *  Keep state in members: every reload constructs a fresh instance,
 *  while the shared clock keeps running, so elapsed time is continuous
 *  across an edit. */
class Sketch {
 public:
  virtual ~Sketch() = default;
  /** Called once per (re)load and again when an asset file changes.
   *  Declare the scene here, animation wiring included. */
  virtual void setup(SketchContext& ctx) = 0;
  /** Called every frame with the clock's elapsed seconds — react to DATA
   *  changes here by re-rendering a fresh description; leave per-frame
   *  motion to bindings and Cache::None paint programs. */
  virtual void update(double elapsed, SketchContext& ctx) {
    (void)elapsed;
    (void)ctx;
  }
};

/** THE 2D KIND: a compose Element tree, reconciled by a Composer and
 *  painted onto a canvas, driven by a clock the host owns. */
class CanvasKind final : public KindOps {
 public:
  using Factory = Sketch* (*)();
  explicit CanvasKind(Factory factory) : m_factory(factory) {}
  /** Written out rather than defaulted: the operations a kind answers
   *  are an abstract base, and a defaulted comparison would try to
   *  compare that. What identifies a kind is the body it opens. */
  bool operator==(const CanvasKind& other) const {
    return m_factory == other.m_factory;
  }

  [[nodiscard]] std::string_view runtime() const override { return "canvas"; }

  [[nodiscard]] std::unique_ptr<Session> open(
      weave::FontContext& fonts, Assets& assets,
      bool deterministic) const override;

 private:
  Factory m_factory;
};

/** The factory SIGIL_SKETCH takes the ADDRESS of, rather than a lambda
 *  whose body it would carry: taking a function's address cannot throw,
 *  which is what keeps a registration initializer non-throwing whatever
 *  the sketch's constructor does. */
template <class SketchType>
[[nodiscard]] Sketch* makeCanvasSketch() {
  return new SketchType();
}

/** The kind a 2D sketch draws through. SIGIL_SKETCH resolves this from
 *  the type it is handed, so a file that includes this header registers
 *  for the compose runtime and cannot register for another. */
template <class SketchType>
  requires std::derived_from<SketchType, Sketch>
[[nodiscard]] Kind kindOf() {
  return CanvasKind{&makeCanvasSketch<SketchType>};
}

}  // namespace sigil::sketch
