/** @file
 * The 3D session: an engine, a retained Scene and one scene3d body describing
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
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/advanced/Stats.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <glm/geometric.hpp>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::sketch {

namespace {

/** The runtime a session opens on when its kind states none. A device
 *  is one device and one queue for the whole run; an empty value is no
 *  device, and no scene3d sketch opens on it. */
world::Runtime& processRuntime() {
  static world::Runtime runtime;
  return runtime;
}

/** THE ONE MESSAGE a scene3d sketch is refused with, wherever it is refused. */
constexpr const char* kNoDevice =
    "a scene3d sketch is drawn on a device, and no device is available to it";

/** HOW MANY CANVAS PIXELS ONE DECLARED UNIT COVERS.
 *
 *  A host hands over a canvas already fitted: a plate's canvas is the
 *  declared size and carries nothing, while a live window on a scaled
 *  screen carries the fit AND the screen's own scale. A drawn tree is
 *  resolution-independent and needs neither number; a lit scene is formed
 *  at ONE resolution, so a scene3d sketch formed at its declared size and then
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
  again.environment(frame.environmentMap(), frame.environmentOptions())
      .extent(frame.extent())
      .viewOffset(frame.viewOffset())
      .defaultCamera(frame.defaultCamera())
      .runtime(frame.runtime())
      .standIns(frame.standIns())
      .timeStages(frame.timeStages());
  if (!frame.camera().empty()) again.camera(frame.camera());
  if (frame.cameraOverride()) again.cameraOverride(*frame.cameraOverride());
  for (const world::Pass& pass : frame.passes()) again.pass(pass);
  if (!frame.present().empty()) again.present(frame.present());
  return again;
}

}  // namespace

/** A scene3d sketch about the SCENE, made into one a runtime can perform: one
 *  geometry pass clearing to the declared background and painting every
 *  body into `colour`. A scene3d sketch whose passes already form a picture — a
 *  geometry pass, or a body doing its own work — is left alone, since an
 *  executor is only reached through passes; one whose passes only paint
 *  the textures its bodies wear, or only work on the picture — a bloom
 *  over `colour` — is still a scene3d sketch about the scene, and has the stock
 *  pass put FIRST, so a post pass reading `colour` reads what it painted. */
void throughPasses(world::Frame& frame,
                   const sigil::material::Color& background) {
  for (const world::Pass& pass : frame.passes())
    if (pass.stage() == world::Stage::Geometry || pass.body()) return;
  world::Pass stock = world::geometryPass("colour").writes("colour").clear(
      sigil::material::skia::toSkColor(background));
  if (frame.passes().empty()) {
    frame.pass(std::move(stock));
    return;
  }
  world::Frame first(frame.scene());
  first.environment(frame.environmentMap(), frame.environmentOptions())
      .extent(frame.extent())
      .viewOffset(frame.viewOffset())
      .defaultCamera(frame.defaultCamera())
      .runtime(frame.runtime())
      .standIns(frame.standIns())
      .timeStages(frame.timeStages());
  if (!frame.camera().empty()) first.camera(frame.camera());
  if (frame.cameraOverride()) first.cameraOverride(*frame.cameraOverride());
  first.pass(std::move(stock));
  for (const world::Pass& pass : frame.passes()) first.pass(pass);
  if (!frame.present().empty()) first.present(frame.present());
  for (const world::Readback& readback : frame.readbacks())
    first.readback(readback);
  frame = std::move(first);
}

namespace {

/** ONE 3D SKETCH, RUNNING. */
class Scene3DSession final : public Session {
 public:
  Scene3DSession(std::unique_ptr<Scene3DBody> body, weave::FontContext& fonts,
                 Assets& assets, bool deterministic, world::Runtime runtime)
      : m_textures(fonts),
        m_body(std::move(body)),
        m_assets(assets),
        m_faceAdoption(assets.adoptFacesInto(fonts)),
        m_scene(m_engine),
        m_runtime(std::move(runtime)),
        m_deterministic(deterministic) {
    m_specification.size = {900, 640};
    m_specification.background = {0.04f, 0.045f, 0.06f, 1.0f};
    m_specification.captureSeconds = 1.0;
    Scene3DContext ctx{assets,    fonts,    &m_specification, &m_camera,
                       &m_scenes, &m_pages, deterministic,    &m_textures};
    m_body->setup(ctx);
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
    // feeds the sketch reads are moved by the same engine, and moved before
    // it describes: what a frame is described from is everything that had
    // arrived by the moment it draws.
    const double seconds = m_engine.elapsed().count();
    io::advance(m_assets.hub(), std::chrono::duration<double>(seconds));
    world::Frame frame = m_body->describe(m_engine.elapsed());
    // The plate's size and its viewpoint are the host's to state: a scene3d
    // sketch says what it is of, not where it lands. The picture is the
    // declared canvas in the pixels this canvas actually has, so the frame is
    // formed at the resolution it will be seen at, and only over the part
    // of it this canvas's clip leaves.
    m_window = windowOn(canvas);
    frame.defaultCamera(m_camera);
    over(frame, m_window);
    // A TREE'S OWN CAMERA WINS over the lens the sketch was handed at setup,
    // which is what lets a scene3d sketch put its camera on a rail and be
    // photographed from it. A host that has taken hold of the viewpoint
    // has to win over THAT, and says so as the frame's override: the
    // camera the tree chose still says how the picture is read.
    if (m_orbiting) frame.cameraOverride(m_orbit);
    frame.runtime(m_runtime);
    // A LIVE SESSION KEEPS PRESENTING while a program compiles, drawing
    // the body as a stand-in until it arrives; a capture that will be
    // diffed waits for every program, so its first frame is the real one.
    frame.standIns(!m_deterministic);
    frame.timeStages(m_timeDevice);
    throughPasses(frame, m_specification.background);
    m_scene.render(frame);
    // A frame formed over a part of its picture is kept, so a repaint
    // onto a canvas that shows more of the picture can form what the
    // part left out without describing the moment again.
    if (m_window.partial())
      m_standing = std::move(frame);
    else
      m_standing.reset();
    // WHAT THE SKETCH ITSELF DECLARED, read back after the describe that
    // said it, so a host asking where the sketch stands is told the
    // sketch's own answer and not the fallback the host handed in. It is
    // only read while the host has NOT taken hold: once it has, the
    // camera the tree carries is the host's own.
    if (!m_orbiting) {
      const std::optional<geometry::mesh::camera::Camera> declared =
          m_scene.camera();
      m_declared = declared ? *declared : m_camera;
    }
    m_timing.updateMilliseconds =
        measure::Milliseconds(m_laps.mark("update")).count();
    // The phase turns over where the sketch's own body ends and its
    // runtime's painting begins, so a fault reads the same whichever
    // host drove the frame: one call in, two phases.
    {
      PhaseMark mark(Phase::Draw);
      paint(canvas);
    }
    m_timing.drawMilliseconds =
        measure::Milliseconds(m_laps.mark("draw")).count();
    m_timing.totalMilliseconds = measure::Milliseconds(m_laps.total()).count();
    const world::SceneStats& stats = m_scene.stats();
    if (m_timeDevice) gatherDeviceTimes(stats);
    // `pending` counts the bodies the frame stood in for because their
    // program's pipeline was still building, so a host that wants every
    // body shaded draws until it reads zero.
    m_lanes = {LaneCost{"nodes", (double)stats.nodes},
               LaneCost{"drawn", (double)stats.drawn},
               LaneCost{"cooked", (double)stats.cooked},
               LaneCost{"passes", (double)stats.passes},
               LaneCost{"pending", (double)stats.programsPending}};
  }

  void repaint(SkCanvas& canvas) override { present(canvas); }

  /** The plate IS the frame just finished, put on the canvas the host
   *  sized. A scene3d sketch is formed at ONE resolution and this call
   *  describes nothing, so there is nothing here to form again larger: a bigger
   *  canvas magnifies the frame that stands rather than sharpening it,
   *  which is why a scene3d sketch asks for no oversample. */
  void still(SkCanvas& canvas) override { present(canvas); }

  [[nodiscard]] Timing timing() const override { return m_timing; }

  [[nodiscard]] std::span<const LaneCost> lanes() const override {
    return m_lanes;
  }

  void timeDevice(bool on) override {
    m_timeDevice = on;
    m_deviceSums.clear();
    m_deviceFrames = 0;
    m_deviceSeen = -1;
    m_deviceTiming = world::DeviceTiming::Off;
  }

  [[nodiscard]] std::string deviceTimes() const override {
    switch (m_deviceTiming) {
      case world::DeviceTiming::Off:
        return {};
      case world::DeviceTiming::Unavailable:
        return "unavailable";
      case world::DeviceTiming::Pending:
        return "pending";
      case world::DeviceTiming::Measured:
        break;
    }
    if (m_deviceFrames == 0) return "pending";
    double whole = 0.0;
    for (const auto& [stage, sum] : m_deviceSums) whole += sum;
    const auto token = [&](std::string_view stage, double sum) {
      std::string name(stage);
      for (char& c : name)
        if (c == ' ' || c == ',' || c == ':') c = '_';
      char milliseconds[32];
      std::snprintf(milliseconds, sizeof milliseconds, ":%.2f",
                    sum / (double)m_deviceFrames);
      return name + milliseconds;
    };
    std::string line = token("frame", whole);
    for (const auto& [stage, sum] : m_deviceSums)
      line += "," + token(stage, sum);
    return line;
  }

  [[nodiscard]] std::string counters() const override {
    const world::SceneStats& stats = m_scene.stats();
    char line[224];
    std::snprintf(line, sizeof line,
                  "nodes %lld   drawn %lld   resources %lld   passes %lld"
                  "   readbacks %lld",
                  (long long)stats.nodes, (long long)stats.drawn,
                  (long long)stats.resources, (long long)stats.passes,
                  (long long)stats.readbacks);
    // …and the screens, pages and textures the sketch asked for at setup,
    // which no counter of the retained scene's can see.
    const size_t screens = m_scenes.size() + m_pages.size() + m_textures.kept();
    if (screens == 0) return line;
    char held[48];
    std::snprintf(held, sizeof held, "   screens %zu", screens);
    return std::string(line) + held;
  }

  /** ORBIT: yaw and pitch about the viewpoint's own target, at a
   *  distance from it. THE SKETCH'S OWN CAMERA IS THE PIVOT — its target,
   *  its up axis and its lens are kept and only the eye is moved — so a
   *  sketch that put its camera somewhere particular is orbited around what
   *  it was looking at rather than around a point the host chose. Until
   *  this is called the sketch is seen from exactly the camera it declared,
   *  which is what makes the live picture and the plate the same
   *  picture. */
  [[nodiscard]] bool hasViewpoint() const override { return true; }

  [[nodiscard]] std::optional<geometry::mesh::camera::Orbit> orbit()
      const override {
    return geometry::mesh::camera::orbitOf(viewing());
  }

  void viewpoint(float yawDegrees, float pitchDegrees,
                 float distance) override {
    m_orbit = geometry::mesh::camera::cameraAt(
        m_declared, {yawDegrees, pitchDegrees, distance});
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
    // A LIVE WINDOW FORMS NO MORE PIXELS THAN THE SKETCH DECLARED. The
    // lit draw costs by the pixel, and a pane on a dense screen has four
    // times the pixels of the same pane on a plain one; the declared
    // canvas is the resolution the sketch was made at, so a live session
    // forms that many and the pane magnifies the picture. A deterministic
    // session — a plate, a still — forms every pixel the canvas has for
    // it, so a capture is the picture at the size it is taken.
    const float scale = m_deterministic ? pixelScale(canvas)
                                        : std::min(pixelScale(canvas), 1.0f);
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
    // was formed at — a presented resource standing where its part of
    // the picture does — and is put back on the declared canvas here.
    // It is read off the frame rather than off this canvas because a
    // repaint may arrive on a canvas fitted differently from the one the
    // frame was formed for, and the picture that exists is the one that
    // has to land. On a canvas at the declared size it is the identity
    // and the bytes are the plate's.
    SkAutoCanvasRestore restore(&canvas, true);
    canvas.scale(
        m_specification.size.width() / (float)m_window.whole.width(),
        m_specification.size.height() / (float)m_window.whole.height());
    // A LIVE PICTURE IS SAMPLED WHERE THE DEVICE DREW IT, when the
    // canvas's queue reaches that device; a capture that will be diffed
    // is read back, which is the same bytes onto every canvas.
    world::draw(m_scene, canvas,
                m_deterministic ? world::Presentation::Readback
                                : world::Presentation::OnDevice);
  }

  /** The texture scenes the context handed out. Before the sketch and the
   *  retained scene, so they outlive both: a texture a body wears is
   *  still standing when its wearer goes. */
  std::vector<std::shared_ptr<compose::TextureScene>> m_scenes;
  /** What the pages the context handed out paint into. */
  std::vector<std::shared_ptr<compose::SurfaceScene>> m_pages;
  /** The trees the context painted as textures, kept by value. */
  compose::Textures m_textures;
  std::unique_ptr<Scene3DBody> m_body;
  /** What the sketch reaches for that it did not generate. Held for the
   *  session's life rather than only for the setup that declared it,
   *  because the feeds it opened are moved forward every frame. */
  Assets& m_assets;
  /** Every font the hub loads is known to the font context by its family
   *  while the session stands. */
  Assets::FaceAdoption m_faceAdoption;
  /** Taken once, when this session opened: every frame it draws goes
   *  through this one, whatever the process installed after. */
  world::Runtime m_runtime;
  /** A capture that will be diffed: its picture is read back rather than
   *  sampled in place. */
  bool m_deterministic = false;
  motion::Engine m_engine;
  world::Scene m_scene;
  CanvasSpecification m_specification;
  /** The fallback the sketch was handed at setup, for a tree declaring no
   *  camera of its own. */
  geometry::mesh::camera::Camera m_camera;
  /** The viewpoint the last describe put the sketch at — the tree's own, or
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
  std::array<LaneCost, 5> m_lanes{};
  /** THE DEVICE'S STAGE TIMES, summed over every timed frame read since
   *  `timeDevice(true)` — each frame once, by the number the scene gives
   *  it — in the order the stages first ran. */
  bool m_timeDevice = false;
  std::vector<std::pair<std::string, double>> m_deviceSums;
  int64_t m_deviceFrames = 0;
  int64_t m_deviceSeen = -1;
  world::DeviceTiming m_deviceTiming = world::DeviceTiming::Off;

  void gatherDeviceTimes(const world::SceneStats& stats) {
    if (stats.deviceTiming != world::DeviceTiming::Measured) {
      if (m_deviceFrames == 0) m_deviceTiming = stats.deviceTiming;
      return;
    }
    m_deviceTiming = world::DeviceTiming::Measured;
    if (stats.deviceStagesFrame == m_deviceSeen) return;
    m_deviceSeen = stats.deviceStagesFrame;
    ++m_deviceFrames;
    for (const world::DeviceStage& stage : stats.deviceStages) {
      auto same = std::find_if(
          m_deviceSums.begin(), m_deviceSums.end(),
          [&](const auto& sum) { return sum.first == stage.stage; });
      if (same == m_deviceSums.end())
        m_deviceSums.emplace_back(stage.stage, stage.milliseconds);
      else
        same->second += stage.milliseconds;
    }
  }
};

}  // namespace

std::unique_ptr<Session> openScene3D(std::unique_ptr<Scene3DBody> body,
                                     weave::FontContext& fonts, Assets& assets,
                                     bool deterministic,
                                     const world::Runtime& runtime) {
  std::string why;
  if (!runtimeAvailable(runtime, &why)) throw std::runtime_error(why);
  return std::make_unique<Scene3DSession>(std::move(body), fonts, assets,
                                          deterministic, runtime);
}

const world::Runtime& Scene3DKind::openingRuntime() const {
  return m_runtime ? *m_runtime : processRuntime();
}

bool Scene3DKind::available(std::string* why) const {
  return runtimeAvailable(openingRuntime(), why);
}

std::unique_ptr<Session> Scene3DKind::open(weave::FontContext& fonts,
                                           Assets& assets, bool deterministic,
                                           std::string_view key) const {
  (void)key;
  // Refused BEFORE a body is made, so a scene3d sketch that cannot be drawn
  // runs none of its own setup.
  const world::Runtime& runtime = openingRuntime();
  std::string why;
  if (!runtimeAvailable(runtime, &why)) throw std::runtime_error(why);
  return openScene3D(
      m_source ? m_source->open() : std::unique_ptr<Scene3DBody>(m_factory()),
      fonts, assets, deterministic, runtime);
}

Kind onRuntime(const Kind& kind, const world::Runtime& runtime) {
  // Asked of the kind through the one interface every kind that draws
  // through a runtime answers, whoever made it: a host holding a mixed
  // selection says this about every kind it holds rather than sorting
  // them first, and a kind that draws through none comes back as it went.
  if (const auto* opens = dynamic_cast<const OpensOnRuntime*>(kind.get()))
    return opens->opensOn(runtime);
  return kind;
}

void useRuntime(const world::Runtime& runtime) { processRuntime() = runtime; }

const world::Runtime& runtime() { return processRuntime(); }

bool runtimeAvailable(const world::Runtime& runtime, std::string* why) {
  if (runtime) return true;
  if (why) *why = kNoDevice;
  return false;
}

bool runtimeAvailable(std::string* why) {
  return runtimeAvailable(processRuntime(), why);
}

}  // namespace sigil::sketch
