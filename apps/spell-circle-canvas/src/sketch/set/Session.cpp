/** @file
 * The 3D session: a ticker, a retained Scene and one set body describing
 * a frame into them.
 */

#include <include/core/SkCanvas.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/scene/Scene.h>

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
  }

  [[nodiscard]] const CanvasSpec& canvas() const override { return m_spec; }

  void frame(SkCanvas& canvas, double dt) override {
    const auto start = std::chrono::steady_clock::now();
    const double step = dt >= 0.0 ? dt : 1.0 / 60.0;
    m_ticker.tick(step);
    m_seconds += step;
    world::Frame frame = m_set->describe((float)m_seconds);
    // The plate's size and its viewpoint are the host's to state: a set
    // says what it is of, not where it lands.
    frame.extent({(int)m_spec.size.width(), (int)m_spec.size.height()})
        .camera(viewing());
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

  /** The plate IS the frame just finished: a set is drawn from shaded
   *  vertices, so a larger canvas would be a different picture rather
   *  than a sharper one. */
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

  void paint(SkCanvas& canvas) {
    canvas.clear(m_spec.background.toSkColor());
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
