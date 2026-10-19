/** @file
 * The 2D session: an engine and a Composer wired together, with
 * one sketch body describing into them.
 */

#include <sigilcompose/core/Composer.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilio/advanced/Time.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmeasure/advanced/Laps.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilweave/paint/Paint.h>
#include <sigilworld/advanced/Runtime.h>

#include <array>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace sigil::sketch {

namespace {

namespace {

/** THE PROCESS'S TEXT MATERIAL RESOLVER. A text pass carrying a material is
 *  shaded through a resolver the paint feature holds and does not supply,
 *  because that feature links no renderer; with none installed the pass
 *  draws its plain paint, which reads as a bug in the sketch rather than
 *  as a seam nobody closed. The scene2d runtime closes it with
 *  SigilMaterial's Skia backend, over the bounds of what the pass covers.
 *
 *  It is a DEFAULT and never an override: a host that shades somewhere
 *  else installs its own resolver and this leaves it standing, at
 *  whatever point it was installed. Sessions open on worker threads — a
 *  thumbnail, a still drawn beside a presenting window — and the slot is
 *  a plain function with no synchronisation of its own, so the ask and
 *  the install are one step here.
 *
 *  There is no clock here on purpose: a text paint carries its time in
 *  the parameters its material was built with, so a moving pass is a new
 *  material rather than a new frame. */
void installTextMaterialResolver() {
  static std::mutex installing;
  const std::lock_guard held(installing);
  if (weave::paint::hasMaterialResolver()) return;
  weave::paint::setMaterialResolver(
      [](const material::Material& shading, const SkRect& bounds) {
        return material::skia::shader(
            shading, {.resolution = {bounds.width(), bounds.height()}});
      });
}

}  // namespace

/** ONE 2D SKETCH, RUNNING.
 *
 *  The clock is the composer's, so a pause or a time scale reaches
 *  bindings, transitions and volatile leaves coherently. Everything a
 *  frame costs is measured around the two halves that answer different
 *  questions: what the BODY did, and what the runtime did with it. */
class Scene2DSession final : public Session {
 public:
  Scene2DSession(std::unique_ptr<Scene2DBody> sketch, weave::FontContext& fonts,
                 Assets& assets, bool deterministic, std::string_view key = {},
                 std::shared_ptr<const world::Runtime> runtime = nullptr)
      : m_fonts(fonts),
        m_assets(assets),
        m_faceAdoption(assets.adoptFacesInto(fonts)),
        m_sketch(std::move(sketch)),
        m_runtime(std::move(runtime)),
        m_key(key),
        m_deterministic(deterministic) {
    // Before the body describes anything: a material on a text pass shades
    // in every session this runtime opens, whoever opened it.
    installTextMaterialResolver();
    m_composer = std::make_unique<compose::Composer>(m_engine, m_fonts);
    // TWO SIZINGS, deliberately: a sketch may lay out during setup, so
    // it needs a canvas before it runs, and it declares its own from
    // inside setup. The second call is a no-op when they agree.
    m_composer->setSize(
        glm::vec2{m_specification.size.width(), m_specification.size.height()});
    Scene2DContext ctx = context();
    m_sketch->setup(ctx);
    applySize();
  }

  ~Scene2DSession() override {
    // Retained descriptions and running motions may point into
    // sketch-owned state, so the composer goes before its author.
    m_composer.reset();
    m_sketch.reset();
  }

  [[nodiscard]] const CanvasSpecification& canvas() const override {
    return m_specification;
  }

  void frame(SkCanvas& canvas, double dt) override { step(canvas, dt, false); }

  void discardedFrame(SkCanvas& canvas, double dt) override {
    step(canvas, dt, true);
  }

  void repaint(SkCanvas& canvas) override {
    m_composer->draw(canvas);
    m_paintSkipped = false;
  }

  /** One more frame at the capture's own scale and at the moment the
   *  scene stands at: a bake re-runs at that scale instead of being
   *  upsampled, and the clock does not move, so the still shows the
   *  moment it was asked for. */
  void still(SkCanvas& canvas) override { frame(canvas, 0.0); }

  [[nodiscard]] float oversample() const override { return 2.0f; }

  void redeclare() override {
    // The body declares everything again, its texture scenes and pages
    // included, so the ones it asked for last time are let go before it
    // asks.
    m_scenes.clear();
    m_pages.clear();
    Scene2DContext ctx = context();
    m_sketch->setup(ctx);
    applySize();
  }

  [[nodiscard]] Timing timing() const override { return m_timing; }

  [[nodiscard]] std::span<const LaneCost> lanes() const override {
    return m_lanes;
  }

  /** What the runtime is HOLDING, and the one split that is easy to
   *  read wrongly: `picturesRecorded` counts pixel bakes too, so the
   *  recordings are the difference against the bakes — and a recording
   *  is not a pixel cache. */
  [[nodiscard]] std::string counters() const override {
    const compose::Composer::Stats& stats = m_composer->stats();
    char line[256];
    std::snprintf(line, sizeof line,
                  "painted %zu   cache writes %zu (%zu recordings, %zu bakes)"
                  "   pictures %zu   textures %zu   instances %zu",
                  stats.nodesPainted, stats.picturesRecorded,
                  stats.picturesRecorded - stats.texturesBaked,
                  stats.texturesBaked, stats.picturesLive, stats.texturesLive,
                  stats.instances);
    // …and what the BODY is holding through the context, which the
    // composer's own counters cannot see: a scene it asked for once is a
    // fixture, and a count that climbs frame by frame is a body asking
    // for one every frame.
    const size_t scenes = m_scenes.size() + m_pages.size();
    if (scenes == 0) return line;
    char held[48];
    std::snprintf(held, sizeof held, "   scenes %zu", scenes);
    return std::string(line) + held;
  }

  void setProfiling(bool on) override { m_composer->setProfiling(on); }

  void pointer(float x, float y, bool pressed) override {
    m_composer->setPointer({x, y}, pressed);
  }

  void key(std::string_view name, int code, bool pressed) override {
    m_composer->setKey(name, code, pressed);
  }

  void setBakeDensity(float devicePixelsPerUnit) override {
    m_composer->setBakeDensity(devicePixelsPerUnit);
  }

  /** The per-node attribution, written out: what each node cost and how
   *  it produced its pixels. */
  [[nodiscard]] std::vector<std::string> costs(size_t limit) const override {
    if (m_paintSkipped) return {};
    using Cache = compose::Composer::CacheState;
    std::vector<std::string> lines;
    const auto& rows = m_composer->profile();
    for (size_t i = 0; i < rows.size() && lines.size() < limit; ++i) {
      const auto& row = rows[i];
      const char* state = "live paint";
      switch (row.cacheState) {
        case Cache::Live:
          break;
        case Cache::Picture:
          state =
              "[PICTURE — a replay still re-runs every shader, every pixel]";
          break;
        case Cache::Texture:
          state = "[texture — you asked for it]";
          break;
        case Cache::Group:
          state = "[group — subtree baked whole while its bindings hold still]";
          break;
      }
      char line[320];
      std::snprintf(line, sizeof line, "%8.2f ms  %-40s %s",
                    row.selfMilliseconds, row.label.c_str(), state);
      lines.emplace_back(line);
    }
    return lines;
  }

 private:
  void step(SkCanvas& canvas, double dt, bool discarded) {
    m_laps.reset();
    // A STATED step and a wall-clock frame move the same engine, so a
    // stepped run and a live one differ in where the number came from and
    // in nothing else; coming back to the wall after a sweep, the engine
    // counts from its next reading rather than catching up on the sweep.
    if (dt >= 0.0)
      m_engine.advance(m_engine.elapsed() + motion::Duration(dt));
    else
      m_engine.advance();
    // A recording plays back as a function of the scene time, so the
    // feeds the body reads are moved by the same engine, and moved before
    // the body reads them: what a frame sees is everything that had
    // arrived by the moment it draws.
    const double seconds = m_engine.elapsed().count();
    io::advance(m_assets.hub(), std::chrono::duration<double>(seconds));
    {
      Scene2DContext ctx = context();
      m_sketch->update(seconds, ctx);
    }
    applySize();  // a sketch may resize itself mid-run, p5 style
    m_timing.updateMilliseconds =
        measure::Milliseconds(m_laps.mark("update")).count();
    if (discarded && !m_specification.paintDiscardedFrames) {
      m_paintSkipped = true;
      m_timing.drawMilliseconds = 0;
      m_timing.totalMilliseconds =
          measure::Milliseconds(m_laps.total()).count();
      m_lanes = {LaneCost{"recon", 0}, LaneCost{"layout", 0},
                 LaneCost{"volat", 0}, LaneCost{"paint", 0}};
      return;
    }
    // The phase turns over where the sketch's own body ends and its
    // runtime's painting begins, so a fault reads the same whichever
    // host drove the frame: one call in, two phases.
    {
      PhaseMark mark(Phase::Draw);
      m_composer->draw(canvas);
    }
    m_paintSkipped = false;
    m_timing.drawMilliseconds =
        measure::Milliseconds(m_laps.mark("draw")).count();
    m_timing.totalMilliseconds = measure::Milliseconds(m_laps.total()).count();
    const compose::Composer::Stats& stats = m_composer->stats();
    m_lanes = {LaneCost{"recon", stats.reconcileMilliseconds},
               LaneCost{"layout", stats.layoutMilliseconds},
               LaneCost{"volat", stats.volatileMilliseconds},
               LaneCost{"paint", stats.paintMilliseconds}};
  }

  /** The declared canvas, applied when it moved. Tracked here because a
   *  composer is told its size and never asked for it. */
  void applySize() {
    if (m_applied == m_specification.size) return;
    m_composer->setSize(
        glm::vec2{m_specification.size.width(), m_specification.size.height()});
    m_applied = m_specification.size;
  }

  Scene2DContext context() {
    // A prvalue: Scene2DContext is non-copyable, so guaranteed elision is
    // the only way it travels.
    return Scene2DContext{*m_composer,
                          m_engine,
                          m_assets,
                          m_specification.size,
                          &m_specification,
                          &m_fonts,
                          m_deterministic,
                          &m_scenes,
                          m_key,
                          &m_pages,
                          m_runtime.get()};
  }

  weave::FontContext& m_fonts;
  Assets& m_assets;
  /** Every font the hub loads is known to the font context by its family
   *  while the session stands, so the sketch names a bundled family as it
   *  names an installed one. */
  Assets::FaceAdoption m_faceAdoption;
  std::string m_key;  // the sketch's registry key, for the context's local()
  motion::Engine m_engine;
  CanvasSpecification m_specification;
  /** The texture scenes the context handed out. Before the sketch and
   *  the composer, so they outlive both: an image a sketch took from one
   *  and a texture a retained tree holds are still standing when their
   *  owners go. */
  std::vector<std::shared_ptr<compose::TextureScene>> m_scenes;
  /** What the pages the context handed out paint into, kept beside the
   *  texture scenes for the same reason. */
  std::vector<std::shared_ptr<compose::SurfaceScene>> m_pages;
  std::unique_ptr<Scene2DBody> m_sketch;
  // After the sketch: reverse destruction releases retained descriptions
  // (which may point at sketch-owned Outputs) before their owner.
  std::unique_ptr<compose::Composer> m_composer;
  Timing m_timing;
  // Reset per frame rather than built per frame, so the laps a frame
  // lays cost no allocation inside the span they are timing.
  measure::Laps m_laps;
  std::array<LaneCost, 4> m_lanes{};
  bool m_paintSkipped = false;
  SkSize m_applied = m_specification.size;  // what the composer was last told
  /** Where this session's bakes draw a world frame; null is the
   *  process's own. */
  std::shared_ptr<const world::Runtime> m_runtime;
  bool m_deterministic;
};

}  // namespace

bool Scene2DKind::operator==(const Scene2DKind& other) const {
  const bool sameRuntime =
      m_runtime == other.m_runtime ||
      (m_runtime && other.m_runtime && *m_runtime == *other.m_runtime);
  return m_factory == other.m_factory && m_needsDevice == other.m_needsDevice &&
         sameRuntime;
}

Kind Scene2DKind::opensOn(const world::Runtime& runtime) const {
  Scene2DKind stated = *this;
  stated.m_runtime = std::make_shared<const world::Runtime>(runtime);
  return Kind{std::move(stated)};
}

bool Scene2DKind::available(std::string* why) const {
  // Only a runtime this kind was TOLD decides it: listed against the
  // process's own, a scene2d sketch draws whatever its body can without
  // one, and a bake on no device draws as none.
  if (m_needsDevice && m_runtime) return runtimeAvailable(*m_runtime, why);
  return true;
}

std::unique_ptr<Session> Scene2DKind::open(weave::FontContext& fonts,
                                           Assets& assets, bool deterministic,
                                           std::string_view key) const {
  return std::make_unique<Scene2DSession>(
      std::unique_ptr<Scene2DBody>(m_factory()), fonts, assets, deterministic,
      key, m_runtime);
}

std::unique_ptr<Session> openScene2D(std::unique_ptr<Scene2DBody> body,
                                     weave::FontContext& fonts, Assets& assets,
                                     bool deterministic, std::string_view key) {
  return std::make_unique<Scene2DSession>(std::move(body), fonts, assets,
                                          deterministic, key);
}

std::unique_ptr<Session> openScene2D(
    std::unique_ptr<Scene2DBody> body, weave::FontContext& fonts,
    Assets& assets, bool deterministic, std::string_view key,
    std::shared_ptr<const world::Runtime> runtime) {
  return std::make_unique<Scene2DSession>(
      std::move(body), fonts, assets, deterministic, key, std::move(runtime));
}

}  // namespace sigil::sketch
