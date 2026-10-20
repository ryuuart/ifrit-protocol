#pragma once

/** @file
 * @ingroup sketch-scene2d
 *
 * The 2D sketch surface: a p5-style entry point over the full compose
 * API. Include this and SIGIL_SKETCH registers a sketch that draws a
 * compose Element tree.
 */

/** @defgroup sketch-scene2d Scene2D sketches
 *  The scene2d kind: what a scene2d sketch is handed every frame
 *  (`Scene2DContext`), what makes a type a scene2d sketch
 *  (`Scene2DSketch`), and the kind that opens one on the compose runtime
 *  (scene2d/). The first header a sketch author opens. */
/** @defgroup sketch-core Registry, session, assets, device
 *  What every kind of sketch shares: the registry a sketch is addressed
 *  in, the session a host drives, the canvas specification a sketch
 *  declares, hot-reloading asset access, the device seam, and the
 *  crash guard (core/). */
/** @defgroup sketch-kit The sheet a sketch stands on
 *  The theme, the page, and the furniture a specimen is built out of —
 *  headings, passages, panels, rows, cells, charts, legends, meters,
 *  tickers, scrollbars and the channel a live value arrives on (kit/).
 *  Each takes content and reads the theme; none of them decides how a
 *  study looks. */
/** @defgroup sketch-live The live host
 *  What drives a sketch while someone is watching it: the host, the
 *  bench cadence it is judged against, and the texture residency it
 *  reports (live/). */
/** @defgroup sketch-plate Plates and sweeps
 *  Rendering sketches to files and judging the results: the sweep over
 *  the registry, the per-frame statistics, the channel-by-channel
 *  comparison, the MP4 montage, and the thumbnails the browser shows
 *  (plate/). */
/** @defgroup sketch-scene3d Scene3D sketches
 *  The scene3d kind: a sketch whose subject is a lit scene in space
 *  rather than a flat surface (scene3d/). */
/** @defgroup sketch-scry Web leaves
 *  An HTML and CSS page rendered into a sketch, and the settling a
 *  deterministic capture of one needs (scry/). */
/** @defgroup sketch-publish Publication
 *  A sketch's canvas offered to other applications as a shared texture,
 *  and another application's offered back to it (publish/). */
/** @defgroup sketch-python The Python session
 *  A Python source file as a sketch kind (python/). */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Measure.h>
#include <sigilcompose/texture/SurfaceRange.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/time/Duration.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/CanvasSpecification.h>
#include <sigilsketch/core/Device.h>
#include <sigilsketch/core/Members.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/scene3d/OpensOnRuntime.h>
#include <sigilsketch/scene3d/Page.h>

#include <algorithm>
#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace sigil::compose {
class SurfaceScene;
class TextureScene;
}  // namespace sigil::compose
namespace sigil::world {
class Frame;
class Runtime;
}  // namespace sigil::world

/** EVERY RENDERABLE THING AS ONE SKETCH: a single file, addressed by its
 *  stem, that a live host opens, a sweep photographs and a browser lists.
 *
 *  A sketch is a plain type with no base class and nothing to inherit. A
 *  struct spelling `setup(Scene2DContext&)` is a scene2d sketch; spelling
 *  `update()` as well makes it one that reacts. The `SIGIL_SKETCH` macro
 *  registers it, and a `Kind` says what runtime it draws through — a
 *  compose Element tree, a lit 3D scene, or a Python source file.
 *
 *  A `Session` is what a host drives: it holds the composer, the clock
 *  and the assets, opens the body once, and steps it. `Scene2DContext` is
 *  the per-frame value the body is handed, and is rebuilt every frame
 *  deliberately, so nothing may capture it.
 *
 *  Around that: `kit` is the sheet a study stands on, `plate` renders and
 *  compares sketches as files, `live` is the host that shows one while it
 *  is edited, and `publish` offers the canvas to other applications.
 *
 *  It is the library the visual work of this tree is done in: a sketch is
 *  where a library's vocabulary is exercised, and its plate is what says
 *  the vocabulary still draws what it drew. */
namespace sigil::sketch {

/** HOW A 3D FRAME IS BAKED into a scene2d sketch's picture: what it is seen
 *  through, what is behind it, and when. */
struct BakeScene3DOptions {
  /** The lens a tree holding no camera is seen through. */
  geometry::mesh::camera::Camera camera;
  /** What the picture is cleared to behind the scene. */
  sigil::material::Color background{0, 0, 0, 0};
  /** The moment of the bake, on the baked scene's own clock. */
  motion::Duration at{};
};

/** WHAT A SKETCH IS HANDED every time it is asked to describe itself.
 *
 *  It is a PER-FRAME value the host rebuilds, and non-copyable
 *  deliberately: capturing it in a steppable by reference dangles next
 *  frame, and capturing a copy would hold a stale specification and a stale
 * size just as silently. Neither compiles. Capture `ctx.composer` (stable for
 * the sketch's life) or plain data instead. */
struct Scene2DContext {
  compose::Composer& composer;    ///< render()/renderSlot()/query surface
  sigil::motion::Engine& engine;  ///< animations, timelines and timers
  Assets& assets;                 ///< hot-reloading resource access
  SkSize size;                    ///< the current logical canvas size
  /// Host-owned; written through the declaration calls below.
  CanvasSpecification* specification = nullptr;
  sigil::weave::FontContext* fonts = nullptr;  ///< measure()/snapshot() fuel
  /** Host-owned: the texture scenes `textureScene()` handed out, kept
   *  for the session's life. */
  std::vector<std::shared_ptr<compose::TextureScene>>* scenes = nullptr;
  /** Host-owned: what the pages `page()` handed out paint into, kept for
   *  the session's life. */
  std::vector<std::shared_ptr<compose::SurfaceScene>>* pages = nullptr;
  /** The sketch's registry key, and the files beside it: `local("x.csv")`
   *  is the URI of the file `x.csv` in the directory the sketch's entry
   *  stands in, which `assets.hub().load<data::Table>()` and
   *  `assets.hub().load<media::Image>()` take as they take any URI. Empty
   *  when the session was opened without a key. */
  std::string_view key;
  /** Host-owned: the device runtime `bakeScene3D` draws on, where the
   *  session was opened on one of its own; null is the process's. */
  const sigil::world::Runtime* bakeRuntime = nullptr;
  [[nodiscard]] std::string local(std::string_view name) const {
    return "sketch://" + std::string(key) + "/" + std::string(name);
  }

  Scene2DContext(
      compose::Composer& composerIn, sigil::motion::Engine& engineIn,
      Assets& assetsIn, SkSize sizeIn,
      CanvasSpecification* specificationIn = nullptr,
      sigil::weave::FontContext* fontsIn = nullptr,
      bool deterministicIn = false,
      std::vector<std::shared_ptr<compose::TextureScene>>* scenesIn = nullptr,
      std::string_view keyIn = {},
      std::vector<std::shared_ptr<compose::SurfaceScene>>* pagesIn = nullptr,
      const sigil::world::Runtime* bakeRuntimeIn = nullptr)
      : composer(composerIn),
        engine(engineIn),
        assets(assetsIn),
        size(sizeIn),
        specification(specificationIn),
        fonts(fontsIn),
        scenes(scenesIn),
        pages(pagesIn),
        key(keyIn),
        bakeRuntime(bakeRuntimeIn),
        deterministic(deterministicIn) {}
  Scene2DContext(const Scene2DContext&) = delete;
  Scene2DContext& operator=(const Scene2DContext&) = delete;

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
      SkISize size, sigil::material::Color background = {0, 0, 0, 0});

  /** A PAGE A BAKED 3D FRAME LIGHTS, @p size pixels across: a compose
   *  tree painted as the maps of a surface by a pass on the frame. It is
   *  the page a scene3d sketch asks its own context for, and it is worn
   *  the same way:
   *
   *      world::Frame frame(world::group({…,
   *                                       world::mesh(plate).fill(page.surface())}));
   *      frame.pass(page.render(tree));
   *      auto still = ctx.bakeScene3D(frame, {640, 480}, options);
   *
   *  The bake runs the page's pass at the bake's moment before anything
   *  draws a body, so the tree a frame carries is the one its body wears.
   *  A tree that has not changed since the page last painted is not
   *  painted again, so a body that carries the same tree every frame pays
   *  for the paint once.
   *
   *  THE SESSION KEEPS what the page paints into, as it keeps a
   *  `textureScene()`, and lets go of it when the body declares again.
   *  Ask while declaring and hold the page; a body asking every frame
   *  holds every frame's page until the next declaration.
   *
   *  @p range states what the page's maps stand for at their white, as
   *  `Scene3DContext::page` takes it.
   *
   *  No page — its pass paints nothing and its surface is the stock one —
   *  where the host lent no fonts. */
  [[nodiscard]] Page page(glm::ivec2 size, compose::SurfaceRange range = {});

  /** A LIT 3D SCENE RENDERED ONCE INTO AN IMAGE @p size pixels across — the
   *  picture INSIDE a page. A scene2d plate is
   *  re-rendered at the capture scale, so a document that wanted a 3D scene
   *  in one of its panels cannot wear it as a texture the way a body
   *  does: it bakes the scene at the pixels the panel will have and paints
   *  the image. The frame's own words are SigilWorld's, from
   *  `<sigilworld/frame/Frame.h>`.
   *
   *  SEEN THROUGH `BakeScene3DOptions::camera` — unless the tree carries a
   *  camera of its own, which wins here exactly as it wins in the scene3d
   *  runtime, so a scene that put its camera on a rail is photographed from
   *  that rail wherever it is photographed.
   *
   *  It draws on the device runtime the process installed — a 3D scene is
   *  drawn on a device and nowhere else — and answers NULL where there
   *  is none, so a sketch that bakes a 3D scene declares `needsDevice` and an
   *  `available()` probe over `runtimeAvailable`. A frame that declares no
   *  passes is drawn through one geometry pass clearing to the options'
   *  background. The picture is a function of the frame, the camera, the
   *  size and the moment; the scene it stands the frame in
   *  lives for this call alone, so what comes back is an image and not a
   *  view onto something still standing.
   *
   *  `BakeScene3DOptions::at` IS THE MOMENT OF THE BAKE, on the baked scene's
   *  own clock, which starts when the scene mounts. It is what a scene with an
   *  ENTRANCE is photographed at: a `staggerChildren` cascade is a
   *  schedule of transitions that begin at the mount, so at zero every
   *  one of them is still at its start pose and the picture is the scene
   *  before it arrived — which is a still no author ever wanted.
   *
   *  The clock is the BAKE'S, not the sketch's. Reaching the moment on
   *  the sketch's own engine would step the sketch, and a document that
   *  photographed a 3D scene in one of its panels would move everything else
   *  on the page to do it. The moment is reached in one step, so a scene
   *  whose motion is a fixed-rate steppable rather than a transition is
   *  not what this is for. */
  [[nodiscard]] sk_sp<SkImage> bakeScene3D(const world::Frame& frame,
                                           SkISize size,
                                           BakeScene3DOptions options = {});

  /** WHETHER THE CLOCK THIS SESSION IS DRAWN AT IS NOT THE WALL'S: the
   *  host is taking a capture that will be DIFFED, or a client is
   *  stepping it, so anything the sketch measured about its own execution
   *  must be pinned. It is the one reading of the clock a sketch gets,
   *  and never says which client drives it. See `measured()`; read the
   *  flag directly only when you need to suppress a whole panel rather
   *  than one number. */
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
   *      std::snprintf(buf, n, "BUILD %.2f ms",
   * ctx.measured(buildMilliseconds)); */
  [[nodiscard]] double measured(double value, double pinned = 0.0) const {
    return deterministic ? pinned : value;
  }

  /** WHERE THE POINTER STANDS over the canvas, in the canvas units the
   *  sketch declared, and whether its button is down: what the host fed
   *  the session, read when it is asked, so `update` sees a pointer the
   *  host moved before the step. A paint program reads the same pointer
   *  mapped into its own node's box as `PaintContext::pointer`. The
   *  origin with the button up where nobody points.
   *
   *      if (ctx.pointer().pressed) drag(ctx.pointer().at); */
  [[nodiscard]] compose::PaintContext::Pointer pointer() const {
    return composer.pointer();
  }
  /** THE KEYBOARD as the host fed the session: the last key to go down by
   *  name and code, whether any key is down, and every key that is. */
  [[nodiscard]] const compose::KeyState& keys() const {
    return composer.keys();
  }

  /** One-shot intrinsic measurement with the host's fonts: size marquee
   *  strips, tooltips and badges from their content. */
  SkSize measure(const compose::Element& element,
                 SkSize maxSize = SkSize::MakeEmpty()) {
    return fonts ? compose::intrinsicSize(element, *fonts, maxSize)
                 : SkSize::MakeEmpty();
  }

  /** DECLARE THE WHOLE CANVAS AT ONCE, from the one value a host reads
   *  back afterwards. The setters below write that value a field at a
   *  time, so a sketch that declares more than its size says the same
   *  thing in three or four calls; this says it in one, and a caller
   *  that already holds a `CanvasSpecification` — a kit component handed a
   * stage, a host replaying a declaration — hands it over rather than taking it
   * apart.
   *
   *      ctx.canvas({.size = {1080, 430}, .background = ground,
   *                  .captureSeconds = 2.78});
   *
   *  Every field is declared, defaults included: this is the value, not
   *  a patch over the one already there. */
  void canvas(const CanvasSpecification& declared) {
    if (specification) *specification = declared;
    size = declared.size;  // visible immediately
  }
  /** Declare the logical canvas size. Usually in setup(); calling later
   *  resizes live, applied on the next frame. */
  void canvas(float width, float height) {
    if (specification) specification->size = {width, height};
    size = {width, height};  // visible immediately
  }
  /** The colour behind the scene. */
  void background(sigil::material::Color color) {
    if (specification) specification->background = color;
  }
  /** Declare the scene time a STILL of this sketch should be taken at —
   *  the moment the piece is most itself.
   *
   *      ctx.captureAt(7.2); // the hold at the end of the cycle */
  void captureAt(double seconds) {
    if (specification) specification->captureSeconds = seconds;
  }
  /** Whether unobserved capture steps paint. False requires capture
   *  state to advance entirely through the engine, feeds and update(). */
  void paintDiscardedFrames(bool paint) {
    if (specification) specification->paintDiscardedFrames = paint;
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
    if (specification) specification->oversample = std::max(1, perCanvasPixel);
  }
  /** DECLARE THIS SKETCH A PLATE, not a live scene. Its subject is the
   *  size of the sheet it draws rather than a frame rate held at that
   *  size, so a background still stands it down unless it was asked to
   *  walk plates. Nothing else changes: the plate sweep steps and
   *  captures it exactly as before, and this is never a timeout override.
   *
   *      ctx.plate(); // a 2400×1600 sheet, photographed as a plate */
  void plate() {
    if (specification) specification->plateOnly = true;
  }
};

/** A SKETCH THAT DRAWS A COMPOSE ELEMENT TREE: any type that can be
 *  asked to set itself up. There is no base class and nothing to
 *  inherit — a struct spelling `setup(Scene2DContext&)` is a sketch, and
 *  `setup()` is accepted too, from a body that reads nothing off the
 *  context. `setup` is called once per (re)load and again when an asset
 *  file changes; declare the scene there, animation wiring included.
 *
 *  A sketch may also spell `update`, which is called every frame with
 *  the clock's elapsed seconds and the context — or with the seconds
 *  alone, or with neither, since a body names the prefix of that offer
 *  it reads. React to DATA changes there by rendering a fresh
 *  description; leave per-frame motion to bindings and Cache::None paint
 *  programs.
 *
 *  Retained-mode, not p5's redraw loop — three paths for motion:
 *   1. setup() DECLARES the scene once, wiring in its animation (live
 *      values, tweens, engine timers); the runtime then
 *      animates every frame without re-describing anything.
 *   2. custom() leaves with Cache::None are the immediate-mode floor:
 *      their paint program runs per frame with elapsedSeconds.
 *   3. update() is the DATA path: when state changes, describe again via
 *      composer.render(...) and the reconciler diffs it. Do not
 *      re-render every frame out of habit — bindings are cheaper.
 *
 *  Keep state in members: every reload constructs a fresh instance and
 *  opens a fresh session, which owns a fresh clock, so elapsed time
 *  starts at zero again and the piece plays from its beginning — the
 *  entrance being edited runs again on every save. */
template <class SketchType>
concept Scene2DSketch =
    core::PrefixCallable<detail::SetupCall<SketchType>, void(Scene2DContext&)>;

/** WHAT A SESSION DRIVES: one scene2d sketch, behind the two calls a host
 *  makes of it. The sketch itself derives from nothing and declares no
 *  virtual of its own; this is the vtable, built out of the members the
 *  type spells, and the only thing that holds a sketch by pointer. */
class Scene2DBody {
 public:
  virtual ~Scene2DBody() = default;
  virtual void setup(Scene2DContext& ctx) = 0;
  virtual void update(double elapsed, Scene2DContext& ctx) = 0;
};

/** Opens an owned body on the native scene2d runtime. Setup runs before the
 *  session is returned; a failed setup destroys the candidate body and its
 *  retained descriptions. The body must not be null. */
[[nodiscard]] std::unique_ptr<Session> openScene2D(
    std::unique_ptr<Scene2DBody> body, weave::FontContext& fonts,
    Assets& assets, bool deterministic = false, std::string_view key = {});

/** …with the world frames it bakes drawn on @p runtime — null being the
 *  process's own and an empty one no device. This is how a host that was
 *  told where a kind opens reaches a body it makes itself. */
[[nodiscard]] std::unique_ptr<Session> openScene2D(
    std::unique_ptr<Scene2DBody> body, weave::FontContext& fonts,
    Assets& assets, bool deterministic, std::string_view key,
    std::shared_ptr<const world::Runtime> runtime);

/** ONE SKETCH OF TYPE @p SketchType, OWNED, answering those calls with
 *  the parameters it named — so a body spelling `setup()` is set up with
 *  nothing, and one spelling no `update` at all is stepped by doing
 *  nothing. */
template <Scene2DSketch SketchType>
class Scene2DBodyOf final : public Scene2DBody {
 public:
  void setup(Scene2DContext& ctx) override {
    core::callPrefix(detail::SetupCall<SketchType>{m_sketch}, ctx);
  }
  void update(double elapsed, Scene2DContext& ctx) override {
    if constexpr (core::PrefixCallable<detail::UpdateCall<SketchType>,
                                       void(double, Scene2DContext&)>)
      core::callPrefix(detail::UpdateCall<SketchType>{m_sketch}, elapsed, ctx);
  }

 private:
  SketchType m_sketch;
};

/** THE 2D KIND: a compose Element tree, reconciled by a Composer and
 *  painted onto a canvas, driven by a clock the host owns. */
class Scene2DKind final : public KindOperations, public OpensOnRuntime {
 public:
  using Factory = Scene2DBody* (*)();
  /** @p needsDevice declares body work that uses the host's device, such as
   *  compute, foreign textures or a baked world frame. */
  explicit Scene2DKind(Factory factory, bool needsDevice = false)
      : m_factory(factory), m_needsDevice(needsDevice) {}
  /** Written out rather than defaulted: the operations a kind answers
   *  are an abstract base, and a defaulted comparison would try to
   *  compare that. What identifies a kind is the body it opens and the
   *  runtime it opens it on, including its device requirement. */
  bool operator==(const Scene2DKind& other) const;

  /** THIS KIND, ITS SESSIONS BAKING WORLD FRAMES ON @p runtime — an empty
   *  one being no device, which leaves a kind that declared it needs a
   *  device unavailable and draws every bake as none. */
  [[nodiscard]] Kind opensOn(const world::Runtime& runtime) const override;

  [[nodiscard]] std::string_view runtime() const override { return "scene2d"; }

  [[nodiscard]] bool needsDevice() const override { return m_needsDevice; }

  /** False where the kind declared it needs a device and was told a
   *  runtime that is none, with the one message `runtimeAvailable`
   *  writes; a kind told nothing answers as it is listed. */
  [[nodiscard]] bool available(std::string* why) const override;

  [[nodiscard]] std::unique_ptr<Session> open(
      weave::FontContext& fonts, Assets& assets, bool deterministic,
      std::string_view key) const override;

 private:
  Factory m_factory;
  bool m_needsDevice = false;
  /** Null is the process's own — the runtime a host installed once. */
  std::shared_ptr<const world::Runtime> m_runtime;
};

/** The factory SIGIL_SKETCH takes the ADDRESS of, rather than a lambda
 *  whose body it would carry: taking a function's address cannot throw,
 *  which is what keeps a registration initializer non-throwing whatever
 *  the sketch's constructor does. */
template <class SketchType>
[[nodiscard]] Scene2DBody* makeScene2DBody() {
  return new Scene2DBodyOf<SketchType>();
}

/** The kind a 2D sketch draws through. SIGIL_SKETCH resolves this from
 *  the type it is handed, so a file that includes this header registers
 *  for the compose runtime and cannot register for another. A body that
 *  uses the host's device declares `static constexpr bool needsDevice = true`.
 *  Ordinary 2D drawing needs no device executor. */
template <class SketchType>
  requires Scene2DSketch<SketchType>
[[nodiscard]] Kind kindOf() {
  if constexpr (requires {
                  { SketchType::needsDevice } -> std::convertible_to<bool>;
                })
    return Scene2DKind{&makeScene2DBody<SketchType>, SketchType::needsDevice};
  else
    return Scene2DKind{&makeScene2DBody<SketchType>};
}

}  // namespace sigil::sketch
