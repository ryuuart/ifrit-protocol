#pragma once

/** @file
 * The 3D sketch surface: a lit set, described as a function of the scene
 * time. Include this and SIGIL_SKETCH registers a sketch that draws a
 * world Frame.
 */

#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/CanvasSpec.h>
#include <sigilsketch/core/Device.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/frame/Runtime.h>

#include <concepts>
#include <memory>
#include <optional>
#include <vector>

namespace sigil::weave {
class FontContext;
}
namespace sigil::compose {
class TextureScene;
}

namespace sigil::sketch {

/** WHAT A SET IS HANDED when it declares itself: the plate it will be
 *  photographed onto, the viewpoint it is seen from, what it may reach
 *  for, and the one door a 2D picture comes in by. Handed once, at
 *  setup — a set's every frame is a function of the scene time and of
 *  nothing else. */
struct SetContext {
  Assets& assets;
  weave::FontContext& fonts;
  CanvasSpec* spec = nullptr;  ///< host-owned; written via the calls below
  geometry::mesh::camera::Camera* eye =
      nullptr;  ///< host-owned; the fallback viewpoint
  /** Host-owned: the texture scenes `textureScene()` handed out, kept
   *  for the session's life. */
  std::vector<std::shared_ptr<compose::TextureScene>>* scenes = nullptr;

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
      SkISize size, SkColor4f background = {0, 0, 0, 0});

  /** Declare the plate's size in pixels. */
  void canvas(int width, int height) {
    if (spec) spec->size = {(float)width, (float)height};
  }
  /** The colour behind the set — and, on a device, what the frame's own
   *  clear is written with. */
  void background(SkColor4f color) {
    if (spec) spec->background = color;
  }
  /** The scene time a still of this set is taken at. */
  void captureAt(double seconds) {
    if (spec) spec->captureSeconds = seconds;
  }
  /** The viewpoint, unless the tree declares one of its own — a set that
   *  puts a camera on a rail says so in its description and leaves this
   *  alone. */
  void camera(const geometry::mesh::camera::Camera& lens) {
    if (eye) *eye = lens;
  }
};

/** A SKETCH THAT DRESSES A SET: a world Frame, lit and photographed.
 *
 *  `describe` is a PURE FUNCTION of the scene time and nothing else,
 *  which is what makes a plate reproducible: a host steps from zero at a
 *  fixed rate and photographs the declared moment, so the image depends
 *  on the declaration and never on how fast the machine ran. Keep no
 *  state that a second run would start differently.
 *
 *  A set about the scene returns an Element and a Frame is made of it; a
 *  set about the passes returns the Frame itself. The host writes the
 *  plate's size and the viewpoint into whichever it was handed, so a set
 *  states its subject and nothing about where it lands. */
class Set {
 public:
  virtual ~Set() = default;
  /** Declare the plate and the viewpoint. Called once per (re)load. */
  virtual void setup(SetContext& ctx) { (void)ctx; }
  /** THE FRAME at scene time @p seconds. */
  virtual world::Frame describe(float seconds) = 0;
};

/** THE 3D KIND: a world Frame, reconciled onto a retained Scene and
 *  drawn through whichever runtime the process brought up. */
class SetKind final : public KindOps {
 public:
  using Factory = Set* (*)();
  explicit SetKind(Factory factory) : m_factory(factory) {}
  /** What identifies a kind is the body it opens and where it opens it;
   *  see the 2D kind for the first half. */
  bool operator==(const SetKind& other) const {
    return m_factory == other.m_factory && m_runtime == other.m_runtime;
  }

  /** THIS KIND, OPENING ITS SESSIONS ON @p runtime — an empty one being
   *  the CPU mesh executor.
   *
   *  A session takes its runtime once, when it opens, and draws every
   *  frame through that one: what a host installed on the process is the
   *  DEFAULT a session takes, not a value it re-reads. So a host that
   *  must draw a set somewhere other than where the process's device
   *  stands — a background still on a thread that shares no queue with
   *  the one presenting — says so here and the sessions it opens are
   *  unaffected by what any other thread installed. */
  [[nodiscard]] SetKind on(const world::Runtime& runtime) const {
    SetKind stated = *this;
    stated.m_runtime = runtime;
    return stated;
  }

  [[nodiscard]] std::string_view runtime() const override { return "set"; }

  /** A set is lit by the device renderer: its materials run their own
   *  bodies there, and there is no CPU tier that draws the same picture. */
  [[nodiscard]] bool needsDevice() const override { return true; }

  /** A set's every frame is a pure function of the scene time, so there
   *  is nothing a set could have measured about its own execution and
   *  the determinism answer has nothing to pin. */
  [[nodiscard]] std::unique_ptr<Session> open(
      weave::FontContext& fonts, Assets& assets,
      bool deterministic) const override;

 private:
  Factory m_factory;
  /** Unset is the process's own — the runtime a host installed once. */
  std::optional<world::Runtime> m_runtime;
};

/** @p kind WITH THE RUNTIME ITS SESSIONS DRAW THROUGH STATED, rather
 *  than read off the process when each opens.
 *
 *  A kind that draws through no such runtime — a 2D canvas, a pen — is
 *  returned unchanged, so a host may say this about whatever it is
 *  holding. An empty runtime is the CPU mesh executor, which is what a
 *  caller that must not reach the process's device asks for. */
[[nodiscard]] Kind onRuntime(const Kind& kind, const world::Runtime& runtime);

/** The factory SIGIL_SKETCH takes the ADDRESS of; see the 2D one for why
 *  it is a named template rather than a lambda. */
template <class SetType>
[[nodiscard]] Set* makeSet() {
  return new SetType();
}

/** The kind a 3D sketch draws through. */
template <class SetType>
  requires std::derived_from<SetType, Set>
[[nodiscard]] Kind kindOf() {
  return SetKind{&makeSet<SetType>};
}

/** THE RUNTIME A SET SESSION DRAWS THROUGH UNLESS ITS KIND STATED ONE,
 *  for this process.
 *
 *  An empty runtime is the CPU mesh executor: it needs no device, it is
 *  what a machine with no Vulkan runtime renders on, and it is what a
 *  byte-identity plate is hashed from. A host that brought a device up
 *  says so ONCE — one device, one queue — because a device is a property
 *  of the process and not of a sketch. It is READ AT OPEN: a session
 *  keeps the runtime it opened with, so installing another does not
 *  reach into a session already running, and `onRuntime` above is how a
 *  host opens one somewhere else.
 *
 *  A call that takes the DEVICE itself rather than a runtime over it
 *  reaches `sketch::device()`, from `<sigilsketch/core/Device.h>`. */
void useRuntime(const world::Runtime& runtime);
[[nodiscard]] const world::Runtime& runtime();

}  // namespace sigil::sketch
