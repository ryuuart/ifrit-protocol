#pragma once

/** @file
 * @ingroup sketch-scene3d
 *
 * The scene3d kind: a lit scene in space, described as a function of the scene
 * time. Include this and SIGIL_SKETCH registers a sketch that draws a
 * world Frame.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmotion/time/Duration.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/CanvasSpecification.h>
#include <sigilsketch/core/Device.h>
#include <sigilsketch/core/Members.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/scene3d/Page.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Runtime.h>

#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace sigil::weave {
class FontContext;
}
namespace sigil::compose {
class SurfaceScene;
class TextureScene;
}  // namespace sigil::compose

namespace sigil::sketch {

/** WHAT A SCENE3D SKETCH IS HANDED when it declares itself: the plate it will
 *  be photographed onto, the camera it is seen through, what it may reach
 *  for, and the two doors a 2D picture comes in by — a screen that is its
 *  own light, and a page the scene lights. Handed once, at
 *  setup — a scene3d sketch's every frame is a function of the scene time and
 *  of nothing else. */
struct Scene3DContext {
  Assets& assets;
  weave::FontContext& fonts;
  CanvasSpecification* specification =
      nullptr;  ///< host-owned; written via the calls below
  geometry::mesh::camera::Camera* eye =
      nullptr;  ///< host-owned; the fallback viewpoint
  /** Host-owned: the texture scenes `textureScene()` handed out, kept
   *  for the session's life. */
  std::vector<std::shared_ptr<compose::TextureScene>>* scenes = nullptr;
  /** Host-owned: what the pages `page()` handed out paint into, kept for
   *  the session's life. */
  std::vector<std::shared_ptr<compose::SurfaceScene>>* pages = nullptr;

  /** A COMPOSE SCENE PAINTED INTO A TEXTURE, @p size pixels across and
   *  cleared to @p background — a 2D screen a body wears. Ask for it
   *  here, hold the pointer, and in `describe` hand it the tree at the
   *  scene time with `render()` and put `texture()` in a material's
   *  slot. Its own words are SigilCompose's, from
   *  `<sigilcompose/texture/Texture.h>`.
   *
   *  THE SESSION KEEPS IT for as long as it runs, which is what a body
   *  wearing it needs: a scene standing on a device destroys the texture
   *  it painted into when it goes, so a surface whose scene had been let
   *  go would be sampling a texture that is not there.
   *
   *  Nothing has to be remade when time moves. A session's clock only
   *  goes forward — a sweep that must photograph an earlier moment opens
   *  a second session rather than rewinding this one — so no run of the
   *  piece begins where an earlier one left off. */
  [[nodiscard]] std::shared_ptr<compose::TextureScene> textureScene(
      SkISize size, sigil::material::Color background = {0, 0, 0, 0});

  /** A PAGE THE SCENE LIGHTS, @p size pixels across: a compose tree
   *  painted as the maps of a surface by a pass on the frame. Ask for it
   *  here and hold it; in `describe`, put `page.render(tree)` on the frame
   *  and `page.surface()` on the body that shows it — the scene's own lights,
   *  environment and camera shade it, and the frame's time paints it. The
   *  session keeps what it paints into for as long as it runs, as it keeps
   *  a `textureScene()`, and for the same reason. */
  [[nodiscard]] Page page(glm::ivec2 size);

  /** Declare the plate's size in pixels. */
  void canvas(int width, int height) {
    if (specification) specification->size = {(float)width, (float)height};
  }
  /** The colour behind the scene — and, on a device, what the frame's own
   *  clear is written with. */
  void background(sigil::material::Color color) {
    if (specification) specification->background = color;
  }
  /** The scene time a still of this sketch is taken at. */
  void captureAt(double seconds) {
    if (specification) specification->captureSeconds = seconds;
  }
  /** The lens a tree holding no camera is seen through — a sketch that puts
   *  a camera on a rail says so in its description and leaves this
   *  alone. */
  void camera(const geometry::mesh::camera::Camera& lens) {
    if (eye) *eye = lens;
  }

  /** The host is taking a capture that will be DIFFED, so the scene has
   *  to stand complete before the frame does: a body that fetches
   *  something for a body to wear waits for it here, on this thread,
   *  and hands the same picture to every run. False is a window, where
   *  nothing may hold the thread that draws, and such a body starts
   *  what it needs and describes what has arrived. A scene3d sketch's own
   *  description is a function of the scene time either way — this
   *  decides when what it describes is THERE, not what it is. It also
   *  decides how many pixels are formed: a capture forms every pixel the
   *  canvas has for it, a live window no more than the sketch declared,
   *  magnified to the pane. */
  bool deterministic = false;
};

/** A SCENE3D SKETCH: any type that can describe a world
 *  Frame at a scene time. There is no base class and nothing to inherit
 *  — a struct spelling `describe(motion::Duration now)` is a scene3d
 *  sketch, and `describe()` is accepted too, from one that does not move.
 * `setup(Scene3DContext&)` is optional beside it: declare the plate and the
 * viewpoint there, once per (re)load.
 *
 *  `describe` is a PURE FUNCTION of the scene time and nothing else,
 *  which is what makes a plate reproducible: a host steps from zero at a
 *  fixed rate and photographs the declared moment, so the image depends
 *  on the declaration and never on how fast the machine ran. Keep no
 *  state that a second run would start differently.
 *
 *  A scene3d sketch about the scene returns an Element and a Frame is made
 *  of it; one about the passes returns the Frame itself. The host writes the
 *  plate's size and the viewpoint into whichever it was handed, so a sketch
 *  states its subject and nothing about where it lands. */
template <class Scene3DType>
concept Scene3DSketch = core::PrefixCallable<detail::DescribeCall<Scene3DType>,
                                             world::Frame(motion::Duration)>;

/** WHAT A SESSION DRIVES: one scene3d sketch, behind the two calls a host
 *  makes of it. The sketch itself derives from nothing and declares no
 *  virtual of its own; this is the vtable, built out of the members the
 *  type spells, and the only thing that holds the sketch by pointer. */
class Scene3DBody {
 public:
  virtual ~Scene3DBody() = default;
  virtual void setup(Scene3DContext& ctx) = 0;
  virtual world::Frame describe(motion::Duration now) = 0;
};

/** Opens an owned body on the scene3d runtime — the door a host whose sketch
 *  is not a C++ type comes in by, where a registered sketch comes in
 *  through its kind. Setup runs before the session is returned. The
 *  body must not be null, and @p runtime is the one every frame is
 *  drawn through: `runtime()` below is the process's own, which is what
 *  a kind that stated none opens on.
 *  @throws std::runtime_error with the message `runtimeAvailable` writes
 *  when @p runtime is empty, before the body's setup runs. */
[[nodiscard]] std::unique_ptr<Session> openScene3D(
    std::unique_ptr<Scene3DBody> body, weave::FontContext& fonts,
    Assets& assets, bool deterministic, const world::Runtime& runtime);

/** WHAT OPENS ONE SCENE3D BODY when there is no C++ type to take the
 *  address of a factory for: a host whose scene3d sketches are written in
 *  another language names its own supplier, and two kinds are the same kind
 *  when they hold the same one. */
class Scene3DBodySource {
 public:
  virtual ~Scene3DBodySource() = default;
  /** One fresh body, for one session. Never null. */
  [[nodiscard]] virtual std::unique_ptr<Scene3DBody> open() const = 0;
};

/** ONE SCENE3D SKETCH OF TYPE @p Scene3DType, OWNED, answering those calls
 *  with the parameters it named — so one spelling no `setup` at all takes the
 *  plate and the viewpoint it was opened with, and one spelling
 *  `describe()` is photographed at every moment the same. */
template <Scene3DSketch Scene3DType>
class Scene3DBodyOf final : public Scene3DBody {
 public:
  void setup(Scene3DContext& ctx) override {
    if constexpr (core::PrefixCallable<detail::SetupCall<Scene3DType>,
                                       void(Scene3DContext&)>)
      core::callPrefix(detail::SetupCall<Scene3DType>{m_sketch}, ctx);
  }
  world::Frame describe(motion::Duration now) override {
    return core::callPrefix(detail::DescribeCall<Scene3DType>{m_sketch}, now);
  }

 private:
  Scene3DType m_sketch;
};

/** THE 3D KIND: a world Frame, reconciled onto a retained Scene and
 *  drawn on the device runtime the process brought up. A scene3d sketch is
 *  drawn on a device and nowhere else: where there is none, the kind is
 *  unavailable and says so, and no session opens. */
class Scene3DKind final : public KindOperations {
 public:
  using Factory = Scene3DBody* (*)();
  explicit Scene3DKind(Factory factory) : m_factory(factory) {}
  /** A kind whose bodies @p source makes, for a host holding scene3d sketches
   *  that are not C++ types. */
  explicit Scene3DKind(std::shared_ptr<const Scene3DBodySource> source)
      : m_source(std::move(source)) {}
  /** What identifies a kind is the body it opens and where it opens it;
   *  see the 2D kind for the first half. */
  bool operator==(const Scene3DKind& other) const {
    return m_factory == other.m_factory && m_source == other.m_source &&
           m_runtime == other.m_runtime;
  }

  /** THIS KIND, OPENING ITS SESSIONS ON @p runtime — an empty one being
   *  no device, which leaves the kind unavailable.
   *
   *  A session takes its runtime once, when it opens, and draws every
   *  frame through that one: what a host installed on the process is the
   *  DEFAULT a session takes, not a value it re-reads. So a host that
   *  must draw a scene3d sketch somewhere other than where the process's device
   *  stands — a background still on a thread that shares no queue with
   *  the one presenting — says so here and the sessions it opens are
   *  unaffected by what any other thread installed. */
  [[nodiscard]] Scene3DKind on(const world::Runtime& runtime) const {
    Scene3DKind stated = *this;
    stated.m_runtime = runtime;
    return stated;
  }

  [[nodiscard]] std::string_view runtime() const override { return "scene3d"; }

  /** A scene3d sketch is lit by the device renderer: its materials run their
   *  own bodies there, and nothing else draws one. */
  [[nodiscard]] bool needsDevice() const override { return true; }

  /** False where the runtime this kind opens on — the one it states, or
   *  the process's — is empty, with the one message `runtimeAvailable`
   *  writes. */
  [[nodiscard]] bool available(std::string* why) const override;

  /** A scene3d sketch's every frame is a pure function of the scene time, so
   *  there is nothing it could have measured about its own execution to
   *  pin; what @p deterministic reaches is the sketch's own setup, which a
   *  capture may hold and a window may not. */
  [[nodiscard]] std::unique_ptr<Session> open(
      weave::FontContext& fonts, Assets& assets, bool deterministic,
      std::string_view key) const override;

 private:
  /** Null where a supplier makes the bodies instead. */
  Factory m_factory = nullptr;
  std::shared_ptr<const Scene3DBodySource> m_source;
  /** Unset is the process's own — the runtime a host installed once. */
  std::optional<world::Runtime> m_runtime;

  /** The runtime a session of this kind opens on. */
  [[nodiscard]] const world::Runtime& openingRuntime() const;
};

/** @p kind WITH THE RUNTIME ITS SESSIONS DRAW THROUGH STATED, rather
 *  than read off the process when each opens.
 *
 *  A kind that draws through no such runtime — scene2d, a pen — is
 *  returned unchanged, so a host may say this about whatever it is
 *  holding. An empty runtime is no device: a scene3d kind stated on one is
 *  unavailable, which is what a caller that must not reach the process's
 *  device — a background still on a worker — asks for. */
[[nodiscard]] Kind onRuntime(const Kind& kind, const world::Runtime& runtime);

/** The factory SIGIL_SKETCH takes the ADDRESS of; see the 2D one for why
 *  it is a named template rather than a lambda. */
template <class Scene3DType>
[[nodiscard]] Scene3DBody* makeScene3DBody() {
  return new Scene3DBodyOf<Scene3DType>();
}

/** The kind a 3D sketch draws through. */
template <class Scene3DType>
  requires Scene3DSketch<Scene3DType>
[[nodiscard]] Kind kindOf() {
  return Scene3DKind{&makeScene3DBody<Scene3DType>};
}

/** THE RUNTIME A SCENE3D SESSION DRAWS THROUGH UNLESS ITS KIND STATED ONE,
 *  for this process.
 *
 *  A host that brought a device up installs the runtime over it ONCE —
 *  one device, one queue — because a device is a property of the process
 *  and not of a sketch. Empty until then, and the scene3d kind is unavailable
 *  while it is: a machine with no Vulkan runtime, or a host that brought no
 *  device up, draws no scene3d sketch. It is READ AT OPEN: a session keeps the
 *  runtime it opened with, so installing another does not reach into a
 *  session already running, and `onRuntime` above is how a host opens one
 *  somewhere else.
 *
 *  A call that takes the DEVICE itself rather than a runtime over it
 *  reaches `sketch::device()`, from `<sigilsketch/core/Device.h>`. */
void useRuntime(const world::Runtime& runtime);
[[nodiscard]] const world::Runtime& runtime();

/** WHETHER A SCENE3D SKETCH CAN BE DRAWN on @p runtime — the process's when
 *  none is named — and, where it cannot, the one message that says why,
 *  written into @p why. Every door a scene3d sketch is refused at speaks
 *  through this, so a listing, a sweep, a still and a scene2d sketch baking a
 *  3D frame all say the same thing. */
[[nodiscard]] bool runtimeAvailable(std::string* why = nullptr);
[[nodiscard]] bool runtimeAvailable(const world::Runtime& runtime,
                                    std::string* why);

/** A FRAME ABOUT THE SCENE, made into one a runtime can perform: one
 *  geometry pass clearing to @p background and painting every body into
 *  `colour`. A frame whose passes already form a picture — a geometry
 *  pass, or a body doing its own work — is left alone, since an executor
 *  is only reached through passes; one whose passes only paint the
 *  textures its bodies wear, or only work on the picture — a bloom over
 *  `colour` — is still a frame about the scene, and has the stock pass
 *  put FIRST, so a post pass reading `colour` reads what it painted. A
 *  scene3d session and a scene2d bake fill a frame in through this one
 *  rule. */
void throughPasses(world::Frame& frame, const material::Color& background);

}  // namespace sigil::sketch
