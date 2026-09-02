/** @file
 * The 3D session: a ticker, a retained Scene and one set body describing
 * a frame into them.
 */

#include <include/core/SkCanvas.h>
#include <sigilmeasure/time/Laps.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <glm/geometric.hpp>
#include <optional>

namespace sigil::sketch {

namespace {

/** The process's runtime. A device is one device and one queue for the
 *  whole run; an empty value is the CPU mesh executor. */
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

/** A set about the SCENE, made into one a runtime can perform: one
 *  geometry pass clearing to the declared background and painting every
 *  body. A set that already declares passes is left alone — an executor
 *  is only reached through passes, and a set about the scene must be
 *  able to say what it looks like on a device too. */
void throughPasses(world::Frame& frame, const SkColor4f& background) {
  if (!frame.passes().empty()) return;
  frame.pass(world::geometryPass("colour").writes("colour").clear(background));
}

/** ONE 3D SKETCH, RUNNING. */
class SetSession final : public Session {
 public:
  SetSession(Set* set, weave::FontContext& fonts, Assets& assets)
      : m_set(set), m_scene(m_ticker) {
    m_spec.size = {900, 640};
    m_spec.background = {0.04f, 0.045f, 0.06f, 1.0f};
    m_spec.captureSeconds = 1.0;
    SetContext ctx{assets, fonts, &m_spec, &m_camera};
    m_set->setup(ctx);
    m_declared = m_camera;
    m_extent = {(int)m_spec.size.width(), (int)m_spec.size.height()};
  }

  [[nodiscard]] const CanvasSpec& canvas() const override { return m_spec; }

  void frame(SkCanvas& canvas, double dt) override {
    m_laps.reset();
    // ONE CLOCK, whether the step is stated or read off the wall: a
    // stated delta goes through `advance`, a live frame through `tick`,
    // and both take the same pause, time scale and stall clamp. A host
    // that kept its own accumulator here would drift from the ticker the
    // first time either was paused.
    const double step =
        dt >= 0.0 ? m_clock.advance(dt) : m_clock.tick();
    m_ticker.tick(step);
    world::Frame frame = m_set->describe((float)m_clock.elapsed());
    // The plate's size and its viewpoint are the host's to state: a set
    // says what it is of, not where it lands. The size is the declared
    // canvas in the pixels this canvas actually has, so the frame is
    // formed at the resolution it will be seen at.
    m_extent = extentOn(canvas);
    frame.extent(m_extent).camera(viewing());
    if (m_orbiting) {
      // A TREE'S OWN LENS WINS over the frame's, which is what lets a
      // set put its camera on a rail and be photographed from it. A host
      // that has taken hold of the viewpoint has to win over THAT, and
      // the first camera in tree order is the one a frame is seen from,
      // so the described tree is hung under one node carrying the host's
      // camera — which stands before whatever the set declared.
      frame.scene(world::Element().camera(m_orbit).child(frame.scene()));
    }
    if (processRuntime()) {
      frame.runtime(processRuntime());
      throughPasses(frame, m_spec.background);
    }
    m_scene.render(frame);
    // WHAT THE SET ITSELF DECLARED, read back after the describe that
    // said it, so a host asking where the sketch stands is told the
    // set's own answer and not the fallback the host handed in. It is
    // only read while the host has NOT taken hold: once it has, the
    // camera the tree carries is the host's own.
    if (!m_orbiting) {
      const std::optional<world::Camera> declared = m_scene.camera();
      m_declared = declared ? *declared : m_camera;
    }
    m_timing.updateMs = m_laps.mark("update");
    paint(canvas);
    m_timing.drawMs = m_laps.mark("draw");
    m_timing.totalMs = m_laps.totalMs();
    const world::SceneStats& stats = m_scene.stats();
    m_lanes = {Lane{"nodes", (double)stats.nodes},
               Lane{"drawn", (double)stats.drawn},
               Lane{"cooked", (double)stats.cooked},
               Lane{"passes", (double)stats.passes}};
  }

  void repaint(SkCanvas& canvas) override { paint(canvas); }

  /** The plate IS the frame just finished, put on the canvas the host
   *  sized. A set is formed at ONE resolution and this call describes
   *  nothing, so there is nothing here to form again larger: a bigger
   *  canvas magnifies the frame that stands rather than sharpening it,
   *  which is why a set asks for no oversample. */
  void still(SkCanvas& canvas) override { paint(canvas); }

  [[nodiscard]] Timing timing() const override { return m_timing; }

  [[nodiscard]] std::span<const Lane> lanes() const override { return m_lanes; }

  [[nodiscard]] std::string counters() const override {
    const world::SceneStats& stats = m_scene.stats();
    char line[192];
    std::snprintf(line, sizeof line,
                  "nodes %lld   drawn %lld   resources %lld   passes %lld",
                  (long long)stats.nodes, (long long)stats.drawn,
                  (long long)stats.resources, (long long)stats.passes);
    return line;
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

  [[nodiscard]] std::optional<Orbit> orbit() const override {
    return orbitOf(viewing());
  }

  void viewpoint(float yawDeg, float pitchDeg, float distance) override {
    m_orbit = cameraAt(m_declared, {yawDeg, pitchDeg, distance});
    m_orbiting = true;
  }

 private:
  /** The viewpoint a frame is described with. */
  [[nodiscard]] const world::Camera& viewing() const {
    return m_orbiting ? m_orbit : m_declared;
  }

  /** The declared canvas in the pixels @p canvas has for it. */
  [[nodiscard]] SkISize extentOn(const SkCanvas& canvas) const {
    const float scale = pixelScale(canvas);
    return {std::max(1, (int)std::lround(m_spec.size.width() * scale)),
            std::max(1, (int)std::lround(m_spec.size.height() * scale))};
  }

  void paint(SkCanvas& canvas) {
    canvas.clear(m_spec.background.toSkColor());
    // The picture arrives as many pixels across as the frame STANDING
    // was formed at — as a presented resource, or as bodies projected
    // into that extent — and is put back on the declared canvas here.
    // It is read off the frame rather than off this canvas because a
    // repaint may arrive on a canvas fitted differently from the one the
    // frame was formed for, and the picture that exists is the one that
    // has to land. On a canvas at the declared size it is the identity
    // and the bytes are the plate's.
    SkAutoCanvasRestore restore(&canvas, true);
    canvas.scale(m_spec.size.width() / (float)m_extent.width(),
                 m_spec.size.height() / (float)m_extent.height());
    m_scene.draw(canvas, viewing());
  }

  std::unique_ptr<Set> m_set;
  motion::Ticker m_ticker;
  world::Scene m_scene;
  CanvasSpec m_spec;
  /** The fallback the set was handed at setup, for a tree declaring no
   *  camera of its own. */
  world::Camera m_camera;
  /** The viewpoint the last describe put the set at — the tree's own, or
   *  the fallback where it declared none. */
  world::Camera m_declared;
  world::Camera m_orbit;
  bool m_orbiting = false;
  SkISize m_extent{1, 1};  // the pixels the frame standing was formed at
  Timing m_timing;
  // Reset per frame rather than built per frame, so the laps a frame
  // lays cost no allocation inside the span they are timing.
  measure::Laps m_laps;
  std::array<Lane, 4> m_lanes{};
  motion::FrameClock m_clock;
};

}  // namespace

Orbit orbitOf(const world::Camera& camera) {
  constexpr float kToDegrees = 180.0f / 3.14159265358979f;
  const glm::vec3 out = camera.eye - camera.target;
  const float distance = glm::length(out);
  if (!(distance > 0.0f)) return {};
  // Yaw from the +z axis toward +x and pitch off the ground plane, which
  // is the pair `cameraAt` puts the eye back at.
  return {std::atan2(out.x, out.z) * kToDegrees,
          std::asin(std::clamp(out.y / distance, -1.0f, 1.0f)) * kToDegrees,
          distance};
}

world::Camera cameraAt(const world::Camera& pivot, Orbit orbit) {
  constexpr float kToRadians = 3.14159265358979f / 180.0f;
  const float yaw = orbit.yawDeg * kToRadians;
  const float pitch = orbit.pitchDeg * kToRadians;
  world::Camera out = pivot;
  out.eye = pivot.target +
            glm::vec3{orbit.distance * std::cos(pitch) * std::sin(yaw),
                      orbit.distance * std::sin(pitch),
                      orbit.distance * std::cos(pitch) * std::cos(yaw)};
  return out;
}

std::unique_ptr<Session> SetKind::open(weave::FontContext& fonts,
                                       Assets& assets,
                                       bool deterministic) const {
  (void)deterministic;
  return std::make_unique<SetSession>(m_factory(), fonts, assets);
}

void useRuntime(const world::Runtime& runtime) { processRuntime() = runtime; }

const world::Runtime& runtime() { return processRuntime(); }

}  // namespace sigil::sketch
