/** @file
 * The 3D session: a ticker, a retained Scene and one set body describing
 * a frame into them.
 */

#include <include/core/SkCanvas.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <optional>

namespace sigil::sketch {

namespace {

double millisSince(std::chrono::steady_clock::time_point from,
                   std::chrono::steady_clock::time_point to) {
  return std::chrono::duration<double, std::milli>(to - from).count();
}

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
    m_extent = {(int)m_spec.size.width(), (int)m_spec.size.height()};
  }

  [[nodiscard]] const CanvasSpec& canvas() const override { return m_spec; }

  void frame(SkCanvas& canvas, double dt) override {
    const auto start = std::chrono::steady_clock::now();
    const double step = dt >= 0.0 ? dt : 1.0 / 60.0;
    m_ticker.tick(step);
    m_seconds += step;
    world::Frame frame = m_set->describe((float)m_seconds);
    // The plate's size and its viewpoint are the host's to state: a set
    // says what it is of, not where it lands. The size is the declared
    // canvas in the pixels this canvas actually has, so the frame is
    // formed at the resolution it will be seen at.
    m_extent = extentOn(canvas);
    frame.extent(m_extent).camera(viewing());
    if (processRuntime()) {
      frame.runtime(processRuntime());
      throughPasses(frame, m_spec.background);
    }
    m_scene.render(frame);
    const auto described = std::chrono::steady_clock::now();
    paint(canvas);
    const auto drawn = std::chrono::steady_clock::now();
    m_timing.updateMs = millisSince(start, described);
    m_timing.drawMs = millisSince(described, drawn);
    m_timing.totalMs = millisSince(start, drawn);
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

  /** ORBIT: yaw and pitch about the declared target, at a distance from
   *  it. The set's own camera is the pivot — a set that puts its lens on
   *  a rail declares that in its description, and a host taking hold of
   *  the viewpoint replaces it rather than composing with it. */
  [[nodiscard]] bool hasViewpoint() const override { return true; }

  void viewpoint(float yawDeg, float pitchDeg, float distance) override {
    constexpr float kToRadians = 3.14159265358979f / 180.0f;
    const float yaw = yawDeg * kToRadians;
    const float pitch = pitchDeg * kToRadians;
    m_orbit = m_camera;
    m_orbit.eye =
        m_camera.target + glm::vec3{distance * std::cos(pitch) * std::sin(yaw),
                                    distance * std::sin(pitch),
                                    distance * std::cos(pitch) * std::cos(yaw)};
    m_orbiting = true;
  }

 private:
  /** The viewpoint a frame is described with. */
  [[nodiscard]] const world::Camera& viewing() const {
    return m_orbiting ? m_orbit : m_camera;
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
    // A tree that declares its own lens wins, unless a host has taken
    // hold of the viewpoint.
    const std::optional<world::Camera> declared = m_scene.camera();
    m_scene.draw(canvas,
                 m_orbiting ? m_orbit : (declared ? *declared : m_camera));
  }

  std::unique_ptr<Set> m_set;
  motion::Ticker m_ticker;
  world::Scene m_scene;
  CanvasSpec m_spec;
  world::Camera m_camera;
  world::Camera m_orbit;
  bool m_orbiting = false;
  SkISize m_extent{1, 1};  // the pixels the frame standing was formed at
  Timing m_timing;
  std::array<Lane, 4> m_lanes{};
  double m_seconds = 0.0;
};

}  // namespace

std::unique_ptr<Session> SetKind::open(weave::FontContext& fonts,
                                       Assets& assets,
                                       bool deterministic) const {
  (void)deterministic;
  return std::make_unique<SetSession>(m_factory(), fonts, assets);
}

void useRuntime(const world::Runtime& runtime) { processRuntime() = runtime; }

const world::Runtime& runtime() { return processRuntime(); }

}  // namespace sigil::sketch
