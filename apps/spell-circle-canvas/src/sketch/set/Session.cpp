/** @file
 * The 3D session: an engine, a retained Scene and one set body describing
 * a frame into them.
 */

#include <include/core/SkCanvas.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilio/advanced/Time.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmeasure/advanced/Laps.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <glm/geometric.hpp>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sigil::sketch {

namespace {

/** The runtime a session opens on when its kind states none. A device
 *  is one device and one queue for the whole run; an empty value is the
 *  CPU mesh executor. */
world::Runtime& processRuntime() {
  static world::Runtime runtime;
  return runtime;
}

/** HOW MANY CANVAS PIXELS ONE DECLARED UNIT COVERS.
 *
 *  A host hands over a canvas already fitted: a plate's canvas is the
 *  declared size and carries nothing, while a live window on a scaled
 *  screen carries the fit AND the screen's own scale. A drawn tree is
 *  resolution-independent and needs neither number; a lit set is formed
 *  at ONE resolution, so a set formed at its declared size and then
 *  fitted upward is a magnified picture of a smaller one rather than the
 *  picture at the size it is seen. Reading the number off the canvas is
 *  what keeps the two agreeing without a second place to state it. */
float pixelScale(const SkCanvas& canvas) {
  const float scale = canvas.getTotalMatrix().getMaxScale();
  return std::isfinite(scale) && scale > 0.0f ? scale : 1.0f;
}

/** WHERE A FRAME IS FORMED for one canvas: the whole declared canvas in
 *  the pixels that canvas has for it, and the part of that picture the
 *  canvas's clip leaves — the only part of it that can be seen. */
struct Window {
  SkISize whole{1, 1};
  SkIRect part = SkIRect::MakeWH(1, 1);

  /** Whether the part is less than the whole picture. */
  [[nodiscard]] bool partial() const {
    return part != SkIRect::MakeSize(whole);
  }
  /** Whether a frame formed over this window already holds every pixel
   *  @p wanted asks for. */
  [[nodiscard]] bool covers(const Window& wanted) const {
    return whole == wanted.whole && part.contains(wanted.part);
  }
};

/** @p frame without the readbacks it asked for: what forming a frame a
 *  second time over another part of the same picture runs, since a
 *  readback is owed once per frame described and not once per forming. */
world::Frame formedAgain(const world::Frame& frame) {
  world::Frame again(frame.scene());
  again.extent(frame.extent())
      .viewOffset(frame.viewOffset())
      .camera(frame.camera())
      .runtime(frame.runtime());
  for (const world::Pass& pass : frame.passes()) again.pass(pass);
  if (!frame.present().empty()) again.present(frame.present());
  return again;
}

/** A set about the SCENE, made into one a runtime can perform: one
 *  geometry pass clearing to the declared background and painting every
 *  body. A set that already declares passes is left alone — an executor
 *  is only reached through passes, and a set about the scene must be
 *  able to say what it looks like on a device too. */
void throughPasses(world::Frame& frame,
                   const sigil::material::Color& background) {
  if (!frame.passes().empty()) return;
  frame.pass(world::geometryPass("colour").writes("colour").clear(
      sigil::material::skia::toSkColor(background)));
}

/** ONE 3D SKETCH, RUNNING. */
class SetSession final : public Session {
 public:
  SetSession(std::unique_ptr<SetBody> set, weave::FontContext& fonts,
             Assets& assets, bool deterministic, world::Runtime runtime)
      : m_set(std::move(set)),
        m_assets(assets),
        m_scene(m_engine),
        m_runtime(std::move(runtime)) {
    m_specification.size = {900, 640};
    m_specification.background = {0.04f, 0.045f, 0.06f, 1.0f};
    m_specification.captureSeconds = 1.0;
    SetContext ctx{assets,    fonts,       &m_specification, &m_camera,
                   &m_scenes, &m_surfaces, deterministic};
    m_set->setup(ctx);
    m_declared = m_camera;
    m_window.whole = {(int)m_specification.size.width(),
                      (int)m_specification.size.height()};
    m_window.part = SkIRect::MakeSize(m_window.whole);
  }

  [[nodiscard]] const CanvasSpecification& canvas() const override {
    return m_specification;
  }

  void frame(SkCanvas& canvas, double dt) override {
    m_laps.reset();
    // ONE ENGINE, whether the step is stated or read off the wall. A host
    // that kept its own accumulator here would drift from the engine the
    // first time either was paused.
    if (dt >= 0.0)
      m_engine.advance(m_engine.elapsed() + motion::Duration(dt));
    else
      m_engine.advance();
    // A recording plays back as a function of the scene time, so the
    // feeds the set reads are moved by the same engine, and moved before
    // it describes: what a frame is described from is everything that had
    // arrived by the moment it draws.
    const double seconds = m_engine.elapsed().count();
    io::advance(m_assets.hub(), std::chrono::duration<double>(seconds));
    world::Frame frame = m_set->describe((float)seconds);
    // The plate's size and its viewpoint are the host's to state: a set
    // says what it is of, not where it lands. The picture is the declared
    // canvas in the pixels this canvas actually has, so the frame is
    // formed at the resolution it will be seen at, and only over the part
    // of it this canvas's clip leaves.
    m_window = windowOn(canvas);
    frame.camera(viewing());
    over(frame, m_window);
    if (m_orbiting) {
      // A TREE'S OWN LENS WINS over the frame's, which is what lets a
      // set put its camera on a rail and be photographed from it. A host
      // that has taken hold of the viewpoint has to win over THAT, and
      // the first camera in tree order is the one a frame is seen from,
      // so the described tree is hung under one node carrying the host's
      // camera — which stands before whatever the set declared.
      frame.scene(world::Element().camera(m_orbit).children({frame.scene()}));
    }
    if (m_runtime) {
      frame.runtime(m_runtime);
      throughPasses(frame, m_specification.background);
    }
    m_scene.render(frame);
    // A frame formed over a part of its picture is kept, so a repaint
    // onto a canvas that shows more of the picture can form what the
    // part left out without describing the moment again. A frame with
    // no passes forms no targets and is drawn over the whole picture
    // whatever the part, so there is nothing of it to form again.
    if (m_window.partial() && !frame.passes().empty())
      m_standing = std::move(frame);
    else
      m_standing.reset();
    // WHAT THE SET ITSELF DECLARED, read back after the describe that
    // said it, so a host asking where the sketch stands is told the
    // set's own answer and not the fallback the host handed in. It is
    // only read while the host has NOT taken hold: once it has, the
    // camera the tree carries is the host's own.
    if (!m_orbiting) {
      const std::optional<geometry::mesh::camera::Camera> declared =
          m_scene.camera();
      m_declared = declared ? *declared : m_camera;
    }
    m_timing.updateMs = measure::Milliseconds(m_laps.mark("update")).count();
    // The phase turns over where the sketch's own body ends and its
    // runtime's painting begins, so a fault reads the same whichever
    // host drove the frame: one call in, two phases.
    {
      PhaseMark mark(Phase::Draw);
      paint(canvas);
    }
    m_timing.drawMs = measure::Milliseconds(m_laps.mark("draw")).count();
    m_timing.totalMs = measure::Milliseconds(m_laps.total()).count();
    const world::SceneStats& stats = m_scene.stats();
    m_lanes = {LaneCost{"nodes", (double)stats.nodes},
               LaneCost{"drawn", (double)stats.drawn},
               LaneCost{"cooked", (double)stats.cooked},
               LaneCost{"passes", (double)stats.passes}};
  }

  void repaint(SkCanvas& canvas) override { present(canvas); }

  /** The plate IS the frame just finished, put on the canvas the host
   *  sized. A set is formed at ONE resolution and this call describes
   *  nothing, so there is nothing here to form again larger: a bigger
   *  canvas magnifies the frame that stands rather than sharpening it,
   *  which is why a set asks for no oversample. */
  void still(SkCanvas& canvas) override { present(canvas); }

  [[nodiscard]] Timing timing() const override { return m_timing; }

  [[nodiscard]] std::span<const LaneCost> lanes() const override {
    return m_lanes;
  }

  [[nodiscard]] std::string counters() const override {
    const world::SceneStats& stats = m_scene.stats();
    char line[192];
    std::snprintf(line, sizeof line,
                  "nodes %lld   drawn %lld   resources %lld   passes %lld",
                  (long long)stats.nodes, (long long)stats.drawn,
                  (long long)stats.resources, (long long)stats.passes);
    // …and the screens the set asked for at setup, which no counter of
    // the retained scene's can see.
    const size_t screens = m_scenes.size() + m_surfaces.size();
    if (screens == 0) return line;
    char held[48];
    std::snprintf(held, sizeof held, "   screens %zu", screens);
    return std::string(line) + held;
  }

  /** ORBIT: yaw and pitch about the viewpoint's own target, at a
   *  distance from it. THE SET'S OWN CAMERA IS THE PIVOT — its target,
   *  its up axis and its lens are kept and only the eye is moved — so a
   *  set that put its camera somewhere particular is orbited around what
   *  it was looking at rather than around a point the host chose. Until
   *  this is called the set is seen from exactly the camera it declared,
   *  which is what makes the live picture and the plate the same
   *  picture. */
  [[nodiscard]] bool hasViewpoint() const override { return true; }

  [[nodiscard]] std::optional<geometry::mesh::camera::Orbit> orbit()
      const override {
    return geometry::mesh::camera::orbitOf(viewing());
  }

  void viewpoint(float yawDeg, float pitchDeg, float distance) override {
    m_orbit = geometry::mesh::camera::cameraAt(m_declared,
                                               {yawDeg, pitchDeg, distance});
    m_orbiting = true;
  }

 private:
  /** The viewpoint a frame is described with. */
  [[nodiscard]] const geometry::mesh::camera::Camera& viewing() const {
    return m_orbiting ? m_orbit : m_declared;
  }

  /** The declared canvas in the pixels @p canvas has for it, and the
   *  part of that picture @p canvas's clip leaves.
   *
   *  THE PART IS NEVER LARGER THAN THE CLIP, whatever the scale: a host
   *  showing a magnified piece of the canvas through a pane forms the
   *  pane's pixels, not the whole canvas's at the magnification. It is
   *  the clip's device bounds carried back through the canvas's matrix
   *  and rounded out to the picture's pixels, so a clip whose edge falls
   *  inside one of them takes the whole of that pixel.
   *  A canvas whose clip is the whole picture, as a plate's is, forms
   *  the whole picture exactly as a frame with no part does. */
  [[nodiscard]] Window windowOn(const SkCanvas& canvas) const {
    const float scale = pixelScale(canvas);
    const SkSize size = m_specification.size;
    Window window;
    window.whole = {std::max(1, (int)std::lround(size.width() * scale)),
                    std::max(1, (int)std::lround(size.height() * scale))};
    const SkIRect all = SkIRect::MakeSize(window.whole);
    window.part = all;
    SkMatrix toCanvas;
    if (!canvas.getTotalMatrix().invert(&toCanvas)) return window;
    const SkIRect device = canvas.getDeviceClipBounds();
    if (device.isEmpty()) {
      // Nothing can be seen through an empty clip, so the least there is
      // to form is formed.
      window.part = SkIRect::MakeWH(1, 1);
      return window;
    }
    const SkRect clip = toCanvas.mapRect(SkRect::Make(device));
    const float across = (float)window.whole.width() / size.width();
    const float down = (float)window.whole.height() / size.height();
    // A clip edge the matrix carries onto a pixel boundary lands a
    // rounding error either side of it, and is read as on it.
    constexpr float kOnTheEdge = 1.0f / 256.0f;
    const SkRect pixels = SkRect::MakeLTRB(
        clip.left() * across + kOnTheEdge, clip.top() * down + kOnTheEdge,
        clip.right() * across - kOnTheEdge, clip.bottom() * down - kOnTheEdge);
    if (!pixels.isFinite()) return window;
    SkIRect part = pixels.roundOut();
    if (!part.intersect(all)) {
      window.part = SkIRect::MakeWH(1, 1);
      return window;
    }
    // A part within a pixel of an edge of the picture is taken to that
    // edge: a canvas whose pixels are a rounding short of the picture —
    // a plate at a fractional density — is still shown the whole of it,
    // and forms it through the same projection a frame with no part has.
    if (part.left() <= 1) part.fLeft = 0;
    if (part.top() <= 1) part.fTop = 0;
    if (part.right() >= all.right() - 1) part.fRight = all.right();
    if (part.bottom() >= all.bottom() - 1) part.fBottom = all.bottom();
    window.part = part;
    return window;
  }

  /** @p frame made to form @p window: its targets the part's size, and —
   *  where the part is less than the picture — standing at the part's
   *  corner of it. */
  static void over(world::Frame& frame, const Window& window) {
    frame.extent(glm::ivec2(window.part.width(), window.part.height()));
    frame.viewOffset(
        window.partial()
            ? world::ViewOffset{{window.whole.width(), window.whole.height()},
                                {window.part.left(), window.part.top()}}
            : world::ViewOffset{});
  }

  /** The frame standing, onto a canvas that did not step it. Where the
   *  standing frame was formed over a part of its picture and @p canvas
   *  shows pixels the part left out, the same description is formed
   *  again over what @p canvas shows — the moment is not stepped, and the
   *  part it leaves out is the picture's and not the ground's. */
  void present(SkCanvas& canvas) {
    if (m_standing) {
      const Window wanted = windowOn(canvas);
      if (!m_window.covers(wanted)) {
        world::Frame again = formedAgain(*m_standing);
        over(again, wanted);
        m_scene.render(again);
        m_window = wanted;
        if (!wanted.partial()) m_standing.reset();
      }
    }
    paint(canvas);
  }

  void paint(SkCanvas& canvas) {
    canvas.clear(
        material::skia::toSkColor(m_specification.background).toSkColor());
    // The picture arrives as many pixels across as the frame STANDING
    // was formed at — as a presented resource standing where its part of
    // the picture does, or as bodies projected into the whole picture —
    // and is put back on the declared canvas here.
    // It is read off the frame rather than off this canvas because a
    // repaint may arrive on a canvas fitted differently from the one the
    // frame was formed for, and the picture that exists is the one that
    // has to land. On a canvas at the declared size it is the identity
    // and the bytes are the plate's.
    SkAutoCanvasRestore restore(&canvas, true);
    canvas.scale(
        m_specification.size.width() / (float)m_window.whole.width(),
        m_specification.size.height() / (float)m_window.whole.height());
    world::draw(m_scene, canvas, viewing());
  }

  /** The texture scenes the context handed out. Before the set and the
   *  retained scene, so they outlive both: a texture a body wears is
   *  still standing when its wearer goes. */
  std::vector<std::shared_ptr<compose::TextureScene>> m_scenes;
  std::vector<std::shared_ptr<compose::SurfaceScene>> m_surfaces;
  std::unique_ptr<SetBody> m_set;
  /** What the set reaches for that it did not generate. Held for the
   *  session's life rather than only for the setup that declared it,
   *  because the feeds it opened are moved forward every frame. */
  Assets& m_assets;
  /** Taken once, when this session opened: every frame it draws goes
   *  through this one, whatever the process installed after. */
  world::Runtime m_runtime;
  motion::Engine m_engine;
  world::Scene m_scene;
  CanvasSpecification m_specification;
  /** The fallback the set was handed at setup, for a tree declaring no
   *  camera of its own. */
  geometry::mesh::camera::Camera m_camera;
  /** The viewpoint the last describe put the set at — the tree's own, or
   *  the fallback where it declared none. */
  geometry::mesh::camera::Camera m_declared;
  geometry::mesh::camera::Camera m_orbit;
  bool m_orbiting = false;
  /** The picture the frame standing was formed for, and the part of it
   *  that was formed. */
  Window m_window;
  /** The description standing, kept only while it was formed over a part
   *  of its picture — the one case a repaint may have to form more of. */
  std::optional<world::Frame> m_standing;
  Timing m_timing;
  // Reset per frame rather than built per frame, so the laps a frame
  // lays cost no allocation inside the span they are timing.
  measure::Laps m_laps;
  std::array<LaneCost, 4> m_lanes{};
};

}  // namespace

std::unique_ptr<Session> openSet(std::unique_ptr<SetBody> body,
                                 weave::FontContext& fonts, Assets& assets,
                                 bool deterministic,
                                 const world::Runtime& runtime) {
  return std::make_unique<SetSession>(std::move(body), fonts, assets,
                                      deterministic, runtime);
}

std::unique_ptr<Session> SetKind::open(weave::FontContext& fonts,
                                       Assets& assets, bool deterministic,
                                       std::string_view key) const {
  (void)key;
  return openSet(
      m_source ? m_source->open() : std::unique_ptr<SetBody>(m_factory()),
      fonts, assets, deterministic, m_runtime ? *m_runtime : processRuntime());
}

Kind onRuntime(const Kind& kind, const world::Runtime& runtime) {
  // The concrete kind is asked for by type because the runtime is a set's
  // own vocabulary: a canvas and a pen have no frame to run through one,
  // and a host holding a mixed selection says this about every kind it
  // holds rather than sorting them first.
  if (const auto* set = dynamic_cast<const SetKind*>(kind.get()))
    return set->on(runtime);
  return kind;
}

void useRuntime(const world::Runtime& runtime) { processRuntime() = runtime; }

const world::Runtime& runtime() { return processRuntime(); }

}  // namespace sigil::sketch
