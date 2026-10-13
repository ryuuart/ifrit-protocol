/** @file
 * The 2D session: an engine and a Composer wired together, with
 * one sketch body describing into them.
 */

#include <sigilcompose/core/Composer.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/mesh/render/Runtime.h>
#include <sigilio/advanced/Time.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmeasure/advanced/Laps.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilweave/paint/Paint.h>

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

/** The process's mesh painter. It starts as the CPU executor rather than
 *  as nothing, so a sketch that hands it to a style draws on a machine
 *  where no host ever installed one — an empty runtime draws no mesh at
 *  all, which would read as a bug in the sketch. */
geometry::mesh::render::Runtime& processPainter() {
  static geometry::mesh::render::Runtime painter =
      geometry::mesh::render::Runtime::cpu();
  return painter;
}

/** THE PROCESS'S TEXT MATERIAL RESOLVER, for the same reason the painter
 *  above starts as the CPU executor. A text pass carrying a material is
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

/** The painter the calling thread is drawing a session through, or null
 *  where no session is drawing on it. */
const geometry::mesh::render::Runtime*& threadPainter() {
  thread_local const geometry::mesh::render::Runtime* painter = nullptr;
  return painter;
}

/** THE SESSION'S PAINTER, FOR AS LONG AS THIS STANDS. A sketch body asks
 *  for the painter from inside its own describe, so the answer has to be
 *  the one the session opened with rather than whatever the process last
 *  installed — a still drawn on a worker beside a window that is
 *  presenting is the case that makes the difference. */
class PainterScope {
 public:
  explicit PainterScope(const geometry::mesh::render::Runtime& painter)
      : m_was(std::exchange(threadPainter(), &painter)) {}
  ~PainterScope() { threadPainter() = m_was; }
  PainterScope(const PainterScope&) = delete;
  PainterScope& operator=(const PainterScope&) = delete;

 private:
  const geometry::mesh::render::Runtime* m_was;
};

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
                 Assets& assets, bool deterministic,
                 const geometry::mesh::render::Runtime& painter,
                 std::string_view key = {})
      : m_fonts(fonts),
        m_assets(assets),
        m_sketch(std::move(sketch)),
        m_painter(painter ? painter : geometry::mesh::render::Runtime::cpu()),
        m_key(key),
        m_deterministic(deterministic) {
    // Before the body describes anything: a material on a text pass shades
    // in every session this runtime opens, whoever opened it.
    installTextMaterialResolver();
    // Setup declares too, so the painter is the session's from the first
    // line of the body onward.
    const PainterScope on(m_painter);
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
    const PainterScope on(m_painter);
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
    const PainterScope on(m_painter);
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
      std::snprintf(line, sizeof line, "%8.2f ms  %-40s %s", row.selfMs,
                    row.label.c_str(), state);
      lines.emplace_back(line);
    }
    return lines;
  }

 private:
  void step(SkCanvas& canvas, double dt, bool discarded) {
    const PainterScope on(m_painter);
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
    m_timing.updateMs = measure::Milliseconds(m_laps.mark("update")).count();
    if (discarded && !m_specification.paintDiscardedFrames) {
      m_paintSkipped = true;
      m_timing.drawMs = 0;
      m_timing.totalMs = measure::Milliseconds(m_laps.total()).count();
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
    m_timing.drawMs = measure::Milliseconds(m_laps.mark("draw")).count();
    m_timing.totalMs = measure::Milliseconds(m_laps.total()).count();
    const compose::Composer::Stats& stats = m_composer->stats();
    m_lanes = {LaneCost{"recon", stats.reconcileMs},
               LaneCost{"layout", stats.layoutMs},
               LaneCost{"volat", stats.volatileMs},
               LaneCost{"paint", stats.paintMs}};
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
                          &m_pages};
  }

  weave::FontContext& m_fonts;
  Assets& m_assets;
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
  geometry::mesh::render::Runtime m_painter;
  bool m_deterministic;
};

}  // namespace

std::unique_ptr<Session> Scene2DKind::open(weave::FontContext& fonts,
                                           Assets& assets, bool deterministic,
                                           std::string_view key) const {
  return std::make_unique<Scene2DSession>(
      std::unique_ptr<Scene2DBody>(m_factory()), fonts, assets, deterministic,
      m_painter ? *m_painter : painterRuntime(), key);
}

std::unique_ptr<Session> openScene2D(std::unique_ptr<Scene2DBody> body,
                                     weave::FontContext& fonts, Assets& assets,
                                     bool deterministic, std::string_view key) {
  return std::make_unique<Scene2DSession>(std::move(body), fonts, assets,
                                          deterministic, painterRuntime(), key);
}

Kind onPainterRuntime(const Kind& kind,
                      const geometry::mesh::render::Runtime& painter) {
  // The painter is a 2D kind's own vocabulary, so only a 2D kind answers
  // to it and everything else comes back as it went in.
  if (const auto* canvas = dynamic_cast<const Scene2DKind*>(kind.get()))
    return canvas->on(painter);
  return kind;
}

void usePainterRuntime(const geometry::mesh::render::Runtime& runtime) {
  processPainter() = runtime ? runtime : geometry::mesh::render::Runtime::cpu();
}

const geometry::mesh::render::Runtime& painterRuntime() {
  const geometry::mesh::render::Runtime* const session = threadPainter();
  return session ? *session : processPainter();
}

}  // namespace sigil::sketch
