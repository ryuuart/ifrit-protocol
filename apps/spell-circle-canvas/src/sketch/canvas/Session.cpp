/** @file
 * The 2D session: a clock, a ticker and a Composer wired together, with
 * one sketch body describing into them.
 */

#include <sigilcompose/core/Composer.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/mesh/render/Runtime.h>
#include <sigilmeasure/time/Laps.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilsketch/canvas/Sketch.h>

#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace sigil::sketch {

namespace {

/** ONE 2D SKETCH, RUNNING.
 *
 *  The clock is the composer's, so a pause or a time scale reaches
 *  bindings, transitions and volatile leaves coherently. Everything a
 *  frame costs is measured around the two halves that answer different
 *  questions: what the BODY did, and what the runtime did with it. */
class CanvasSession final : public Session {
 public:
  CanvasSession(Sketch* sketch, weave::FontContext& fonts, Assets& assets,
                bool deterministic)
      : m_fonts(fonts),
        m_assets(assets),
        m_sketch(sketch),
        m_deterministic(deterministic) {
    m_composer = std::make_unique<compose::Composer>(m_ticker, m_fonts);
    m_composer->setClock(&m_clock);
    // TWO SIZINGS, deliberately: a sketch may lay out during setup, so
    // it needs a canvas before it runs, and it declares its own from
    // inside setup. The second call is a no-op when they agree.
    m_composer->setSize(m_spec.size);
    SketchContext ctx = context();
    m_sketch->setup(ctx);
    applySize();
  }

  ~CanvasSession() override {
    // Retained descriptions and running motions may point into
    // sketch-owned state, so the composer goes before its author.
    m_composer.reset();
    m_sketch.reset();
  }

  [[nodiscard]] const CanvasSpec& canvas() const override { return m_spec; }

  void frame(SkCanvas& canvas, double dt) override {
    m_laps.reset();
    // A STATED step and a wall-clock one are the same clock: `advance`
    // takes the caller's delta through the same pause, time scale and
    // stall clamp `tick` puts a wall reading through, so a stepped run
    // and a live one differ in where the number came from and in nothing
    // else. Coming back to wall time, the clock's raw reading is rebased
    // without advancing elapsed, or the first live frame after a sweep
    // injects one whole maxDelta.
    double step = 0.0;
    if (dt >= 0.0) {
      m_stepping = true;
      step = m_clock.advance(dt);
    } else {
      if (m_stepping) {
        const bool wasPaused = m_clock.paused();
        m_clock.setPaused(true);
        m_clock.tick();
        m_clock.setPaused(wasPaused);
        m_stepping = false;
      }
      step = m_clock.tick();
    }
    m_ticker.tick(step);
    {
      SketchContext ctx = context();
      m_sketch->update(m_clock.elapsed(), ctx);
    }
    applySize();  // a sketch may resize itself mid-run, p5 style
    m_timing.updateMs = m_laps.mark("update");
    m_composer->draw(canvas);
    m_timing.drawMs = m_laps.mark("draw");
    m_timing.totalMs = m_laps.totalMs();
    const compose::Composer::Stats& stats = m_composer->stats();
    m_lanes = {Lane{"recon", stats.reconcileMs}, Lane{"layout", stats.layoutMs},
               Lane{"volat", stats.volatileMs}, Lane{"paint", stats.paintMs}};
  }

  void repaint(SkCanvas& canvas) override { m_composer->draw(canvas); }

  /** One more stepped frame, at the capture's own scale: a bake re-runs
   *  at that scale instead of being upsampled. */
  void still(SkCanvas& canvas) override { frame(canvas, 1.0 / 60.0); }

  [[nodiscard]] float oversample() const override { return 2.0f; }

  void redeclare() override {
    // The body declares everything again, its texture scenes included,
    // so the ones it asked for last time are let go before it asks.
    m_scenes.clear();
    SketchContext ctx = context();
    m_sketch->setup(ctx);
    applySize();
  }

  [[nodiscard]] Timing timing() const override { return m_timing; }

  [[nodiscard]] std::span<const Lane> lanes() const override { return m_lanes; }

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
    if (m_scenes.empty()) return line;
    char held[48];
    std::snprintf(held, sizeof held, "   scenes %zu", m_scenes.size());
    return std::string(line) + held;
  }

  void setAutoPromotion(bool on) override {
    m_composer->setAutoTexturePromotion(on);
  }

  void setProfiling(bool on) override { m_composer->setProfiling(on); }

  /** The per-node attribution, written out. An expensive node reported
   *  as live paint with nothing beside it gives an author no next move:
   *  each refusal to bake is individually correct and individually
   *  invisible, so the reason is printed where the cost is — and every
   *  other reason after it, because a first-match verdict costs an
   *  author one iteration per hidden refusal. */
  [[nodiscard]] std::vector<std::string> costs(size_t limit) const override {
    using Cache = compose::Composer::CacheState;
    using Prom = compose::Composer::Promotion;
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
        case Cache::Promoted:
          state = "[TEXTURE, promoted by the library — not by you]";
          break;
        case Cache::SplitOwn:
          state = "[OWN PAINT baked, live children over the blit]";
          break;
        case Cache::Group:
          state = "[group — subtree baked whole while its bindings hold still]";
          break;
      }
      char line[320];
      std::snprintf(line, sizeof line, "%8.2f ms  %-40s %s", row.selfMs,
                    row.label.c_str(), state);
      lines.emplace_back(line);
      if (row.cacheState != Cache::Live || row.selfMs < 1.0) continue;
      lines.emplace_back(std::string("          not baked: ") +
                         compose::Composer::promotionReason(row.promotion));
      for (Prom also : {Prom::Volatile, Prom::Composited, Prom::Transformed,
                        Prom::Filtered, Prom::ReadsBackdrop, Prom::TooBig})
        if (also != row.promotion && row.refused(also))
          lines.emplace_back(std::string("                …and: ") +
                             compose::Composer::promotionReason(also));
    }
    return lines;
  }

 private:
  /** The declared canvas, applied when it moved. Tracked here because a
   *  composer is told its size and never asked for it. */
  void applySize() {
    if (m_applied == m_spec.size) return;
    m_composer->setSize(m_spec.size);
    m_applied = m_spec.size;
  }

  SketchContext context() {
    // A prvalue: SketchContext is non-copyable, so guaranteed elision is
    // the only way it travels.
    return SketchContext{*m_composer, m_ticker, m_assets,        m_spec.size,
                         &m_spec,     &m_fonts, m_deterministic, &m_scenes};
  }

  weave::FontContext& m_fonts;
  Assets& m_assets;
  motion::FrameClock m_clock;
  motion::Ticker m_ticker;
  CanvasSpec m_spec;
  /** The texture scenes the context handed out. Before the sketch and
   *  the composer, so they outlive both: an image a sketch took from one
   *  and a texture a retained tree holds are still standing when their
   *  owners go. */
  std::vector<std::shared_ptr<compose::TextureScene>> m_scenes;
  std::unique_ptr<Sketch> m_sketch;
  // After the sketch: reverse destruction releases retained descriptions
  // (which may point at sketch-owned Outputs) before their owner.
  std::unique_ptr<compose::Composer> m_composer;
  Timing m_timing;
  // Reset per frame rather than built per frame, so the laps a frame
  // lays cost no allocation inside the span they are timing.
  measure::Laps m_laps;
  std::array<Lane, 4> m_lanes{};
  SkSize m_applied = m_spec.size;  // what the composer was last told
  bool m_stepping = false;         // the last frame took a stated step
  bool m_deterministic;
};

}  // namespace

std::unique_ptr<Session> CanvasKind::open(weave::FontContext& fonts,
                                          Assets& assets,
                                          bool deterministic) const {
  return std::make_unique<CanvasSession>(m_factory(), fonts, assets,
                                         deterministic);
}

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

}  // namespace

void usePainterRuntime(const geometry::mesh::render::Runtime& runtime) {
  processPainter() = runtime ? runtime : geometry::mesh::render::Runtime::cpu();
}

const geometry::mesh::render::Runtime& painterRuntime() {
  return processPainter();
}

}  // namespace sigil::sketch
