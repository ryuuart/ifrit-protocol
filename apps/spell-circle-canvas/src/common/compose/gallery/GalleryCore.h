#pragma once
// Split from GalleryScenes.h — see that header for the registry.

#include <sigilcompose/Compose.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Util.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

namespace compose_gallery {

using namespace sigil::compose;
using sigil::compose::util::toU8;
using namespace std::chrono_literals;

inline sigil::weave::FontContext &fonts() {
  static auto *context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

inline sigil::weave::TextStyle styleAt(float size,
                                       SkColor color = SK_ColorWHITE) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.paint.foreground.setColor(color);
  return s;
}

constexpr SkSize kSceneSize = {900, 640};

// ---------------------------------------------------------------------------
// Frame statistics (the FPS measurement)

struct FrameStats {
  std::deque<double> frameMs;   // rolling window of FULL frame work
  std::deque<double> presentMs; // wall deltas between presented frames
  static constexpr size_t kWindow = 120;

  void add(double ms) {
    frameMs.push_back(ms);
    if (frameMs.size() > kWindow)
      frameMs.pop_front();
  }
  void addPresent(double ms) {
    presentMs.push_back(ms);
    if (presentMs.size() > kWindow)
      presentMs.pop_front();
  }
  double presentedFps() const {
    if (presentMs.empty())
      return 0;
    const double avg =
        std::accumulate(presentMs.begin(), presentMs.end(), 0.0) /
        (double)presentMs.size();
    return avg > 0 ? 1000.0 / avg : 0;
  }
  double average() const {
    if (frameMs.empty())
      return 0;
    return std::accumulate(frameMs.begin(), frameMs.end(), 0.0) /
           (double)frameMs.size();
  }
  double percentile(double p) const {
    if (frameMs.empty())
      return 0;
    std::vector<double> sorted(frameMs.begin(), frameMs.end());
    std::sort(sorted.begin(), sorted.end());
    return sorted[std::min(sorted.size() - 1,
                           (size_t)((double)sorted.size() * p))];
  }
  double fps() const {
    const double avg = average();
    return avg > 0 ? 1000.0 / avg : 0;
  }
};

// ---------------------------------------------------------------------------

struct Scene {
  virtual ~Scene() = default;
  virtual const char *name() const = 0;
  virtual void setup(Composer &composer, sigil::motion::Ticker &ticker) = 0;
  virtual void update(double, Composer &) {}

  /** The scene's own logical canvas; hosts letterbox to it. The catalog
   *  scenes were all authored against kSceneSize, but the studies were
   *  authored against the thing they rebuild — a 320x200 Battlescape at 4x
   *  and a 640x800 web page do not share an aspect ratio, let alone a size,
   *  and squeezing either into a common frame would falsify it. Read only
   *  AFTER setup(): a sketch declares its canvas from inside setup, the way
   *  p5's createCanvas does. */
  virtual SkSize canvasSize() const { return kSceneSize; }
  /** What the host clears to behind the scene. Studies that rebuild paper
   *  or a lit desk need it; the catalog scenes are all built on black. */
  virtual SkColor4f background() const { return {0, 0, 0, 1}; }
};

// ---------------------------------------------------------------------------
// Stage + FPS overlay (itself an SigilCompose composer)

struct GalleryStage {
  sigil::motion::FrameClock clock;
  std::unique_ptr<sigil::motion::Ticker> ticker;
  std::unique_ptr<Scene> scene;
  // Declared after scene/ticker so reverse destruction releases retained
  // descriptions (which may point at scene-owned Outputs) before either owner.
  std::unique_ptr<Composer> composer;

  // Overlay: a second composer layered over the scene.
  sigil::motion::Ticker overlayTicker;
  Composer overlay{overlayTicker, fonts()};
  FrameStats stats;
  bool showStats = true;

  /// The active scene's canvas — what a host letterboxes to and what the
  /// headless sweep allocates. Resolved after setup(), because that is when
  /// a study has declared it.
  SkSize sceneSize = kSceneSize;
  SkColor4f sceneBackground = {0, 0, 0, 1};

  GalleryStage() {
    overlay.setSize(kSceneSize);
    reset();
  }

  void reset() {
    composer.reset();
    scene.reset();
    ticker.reset();
    ticker = std::make_unique<sigil::motion::Ticker>();
    composer = std::make_unique<Composer>(*ticker, fonts());
    composer->setClock(&clock);
    composer->setSize(kSceneSize);
  }

  void activate(std::unique_ptr<Scene> next) {
    const bool wasPaused = clock.paused();
    const double previousTimeScale = clock.timeScale();
    clock = sigil::motion::FrameClock{};
    clock.setPaused(wasPaused);
    clock.setTimeScale(previousTimeScale);
    reset();
    stats = {};
    pendingOverlayMs = 0.0;
    resetPresentationCadence();
    fixedNowSeconds = 0.0;
    fixedClockInitialized = false;
    scene = std::move(next);
    sceneSize = kSceneSize;
    sceneBackground = {0, 0, 0, 1};
    if (scene) {
      // Two sizings, deliberately: a scene may lay out during setup(), so it
      // needs a canvas before it runs, and a study only declares its own from
      // inside setup(). The second call is a no-op when they agree.
      composer->setSize(scene->canvasSize());
      scene->setup(*composer, *ticker);
      sceneSize = scene->canvasSize();
      sceneBackground = scene->background();
      composer->setSize(sceneSize);
      overlay.setSize(sceneSize);
    }
  }

  double pendingOverlayMs = 0.0; // charged to the next frame's sample
  // Invoked INSIDE the timed frame window, after the scene draw. The GPU
  // headless sweep sets this to snap+submit(SyncToCpu) so "work ms" is the
  // honest serialized CPU+GPU cost of a frame, not just command recording.
  std::function<void()> flushHook;
  std::chrono::steady_clock::time_point lastPresent{};
  double fixedNowSeconds = 0.0;
  bool fixedClockInitialized = false;

  /** One full frame of work: tick (step + scene update + reconcile) and
   *  the scene draw are measured together — reconcile and relayout are
   *  real per-frame costs, not free. Returns nothing; stats accumulate. */
  void frame(SkCanvas &canvas, double fixedDt = -1.0) {
    if (!scene || !composer || !ticker)
      return;
    const auto start = std::chrono::steady_clock::now();
    double dt = 0.0;
    if (fixedDt >= 0.0) {
      if (!fixedClockInitialized) {
        clock.tick(0.0); // seed so the first synthetic step advances elapsed
        fixedClockInitialized = true;
      }
      fixedNowSeconds += fixedDt;
      dt = clock.tick(fixedNowSeconds);
    } else {
      if (fixedClockInitialized) {
        // Rebase the clock's raw time without advancing elapsed; otherwise a
        // synthetic-time test followed by a live frame would inject maxDelta.
        const bool wasPaused = clock.paused();
        clock.setPaused(true);
        clock.tick();
        clock.setPaused(wasPaused);
        fixedClockInitialized = false;
      }
      dt = clock.tick();
    }
    ticker->tick(dt);
    scene->update(clock.elapsed(), *composer);
    // A study may resize itself mid-run (p5's resizeCanvas); the host reads
    // the declaration back after update, exactly as ComposeSketch does.
    if (scene->canvasSize() != sceneSize) {
      sceneSize = scene->canvasSize();
      composer->setSize(sceneSize);
      overlay.setSize(sceneSize);
    }
    composer->draw(canvas);
    if (flushHook)
      flushHook();
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - start)
                          .count();
    stats.add(ms + pendingOverlayMs);
    pendingOverlayMs = 0.0;
    drawOverlay(canvas);
  }

  void markPresented() {
    const auto now = std::chrono::steady_clock::now();
    if (lastPresent.time_since_epoch().count() != 0)
      stats.addPresent(
          std::chrono::duration<double, std::milli>(now - lastPresent).count());
    lastPresent = now;
  }

  void resetPresentationCadence() {
    lastPresent = {};
    stats.presentMs.clear();
  }

  void drawOverlay(SkCanvas &canvas) {
    if (!showStats)
      return;
    const auto overlayStart = std::chrono::steady_clock::now();
    const Composer::Stats &cs = composer->stats();
    char line[160];
    const double presented = stats.presentedFps();
    if (presented > 0)
      std::snprintf(line, sizeof(line),
                    "%s   work %5.2f ms (p99 %5.2f)   headroom ~%.0f fps"
                    "   presented %.1f fps",
                    scene->name(), stats.average(), stats.percentile(0.99),
                    stats.fps(), presented);
    else
      std::snprintf(line, sizeof(line),
                    "%s   work %5.2f ms (p99 %5.2f)   headroom ~%.0f fps",
                    scene->name(), stats.average(), stats.percentile(0.99),
                    stats.fps());
    char line2[160];
    std::snprintf(line2, sizeof(line2),
                  "instances %zu   pictures %zu   textures %zu   live %zu",
                  cs.instances, cs.picturesLive, cs.texturesLive,
                  cs.nodesPainted);
    char line3[160];
    std::snprintf(line3, sizeof(line3),
                  "rec %.2f   layout %.2f   volatile %.2f   paint %.2f ms",
                  cs.reconcileMs, cs.layoutMs, cs.volatileMs, cs.paintMs);
    overlay.render(box().child(
        box()
            .row()
            .inset(12, sceneSize.height() - 80, 12, 12)
            .corners({8})
            .padding(10, 6)
            .fill(Fill::color({0, 0, 0, 0.55f}))
            .child(box()
                       .column()
                       .gap(2)
                       .child(text(toU8(line), styleAt(15, 0xff7ee8ff)))
                       .child(text(toU8(line2), styleAt(13, 0xff9aa4bb)))
                       .child(text(toU8(line3), styleAt(13, 0xff9aa4bb))))));
    overlay.draw(canvas);
    pendingOverlayMs = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - overlayStart)
                           .count();
  }
};

} // namespace compose_gallery
