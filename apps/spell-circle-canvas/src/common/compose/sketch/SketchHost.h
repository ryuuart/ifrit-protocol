#pragma once
// The live-reload engine (Qt-free): watches the sketch source, rebuilds
// it into a versioned dylib with the compiler flags CMake captured at
// configure time, dlopens the result, and swaps the running sketch —
// keeping the previous one alive on compile errors, p5 style. The host
// executable exports the framework's symbols, so sketch dylibs link
// with `-undefined dynamic_lookup` and build in a couple of seconds.

#include "sigilsketch/Sketch.h"

#include <sigilmotion/FrameClock.h>

#include <include/core/SkRefCnt.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

class SkImageInfo;
class SkPixmap;
class SkSurface;

namespace sigil::weave {
class FontContext;
}

namespace sigil::compose::sketch {

class SketchHost {
public:
  struct Options {
    std::filesystem::path sketchPath;
    std::filesystem::path assetsDir;  // default: <sketch dir>/assets
    std::filesystem::path flagsFile;  // sketch_flags.rsp next to the exe
    std::string compiler = "clang++";
    /** --deterministic: pin anything a sketch measured about its own
     *  execution, so a capture can be diffed. Surfaced to sketches as
     *  SketchContext::deterministic / ctx.measured(). */
    bool deterministic = false;
  };

  SketchHost(Options options, sigil::weave::FontContext &fonts);
  ~SketchHost();

  /** Drives the reload machinery: source mtime, async compile results,
   *  asset changes. Call once per frame. */
  void poll();

  /** Ticks the clock/ticker, runs the sketch's update, draws. Returns
   *  false while no sketch has ever loaded. `fixedDt` < 0 uses wall
   *  time. */
  bool frame(SkCanvas &canvas, double fixedDt = -1.0);

  /** True once a compile is in flight. */
  bool compiling() const { return m_compile.valid(); }
  bool live() const { return m_sketch != nullptr; }
  int generation() const { return m_generation; }

  /** The lifecycle for status displays: Compiling wins even while a
   *  previous build keeps rendering underneath. */
  enum class State { Waiting, Compiling, Live, Failed };
  State state() const {
    if (m_compile.valid())
      return State::Compiling;
    if (!m_errorLog.empty())
      return State::Failed;
    return m_sketch ? State::Live : State::Waiting;
  }

  /** Honest frame metrics over a rolling window: the full frame body
   *  (tick + update + reconcile + draw) is timed, presentation is what
   *  the host reports via markPresented(). */
  double workMsAverage() const;
  double workMsP99() const;
  double presentedFps() const;
  void markPresented();

  /** The last frame(), split at the seam the perf question always asks
   *  about: `updateMs` is everything the SKETCH did (clock tick, ticker
   *  steppables, Sketch::update — which is where describe() and
   *  Composer::render() live), `drawMs` is Composer::draw() (layout +
   *  volatile walk + the paint traversal). Composer::stats() breaks
   *  drawMs down further. Zero until the first frame lands. */
  struct FrameTiming {
    double totalMs = 0;
    double updateMs = 0;
    double drawMs = 0;
  };
  const FrameTiming &lastFrameTiming() const { return m_lastTiming; }

  /** Renders the CURRENT state (clock untouched) into a PNG at
   *  `scale`× the sketch's canvas size. The capture path for both the
   *  windowed save command and headless asset generation. */
  bool capture(const std::filesystem::path &out, float scale = 1.0f);

  /** GPU hosts must route capture through their own backend: once live
   *  frames render on Graphite, the composer's caches hold GPU-backed
   *  images that cannot replay onto a raster canvas. makeSurface builds
   *  the capture target, readback fetches its pixels after the draw
   *  (both called on whichever thread calls capture()). Unset = the
   *  default CPU raster path. */
  struct CaptureBackend {
    std::function<sk_sp<SkSurface>(const SkImageInfo &)> makeSurface;
    std::function<bool(SkSurface &, const SkPixmap &)> readback;
  };
  void setCaptureBackend(CaptureBackend backend) {
    m_captureBackend = std::move(backend);
  }

  /** One-line state for a status bar. */
  const std::string &status() const { return m_status; }
  /** Full compiler/loader output of the most recent failure; empty
   *  when the latest build is good. */
  const std::string &errorLog() const { return m_errorLog; }

  const std::filesystem::path &sketchPath() const {
    return m_options.sketchPath;
  }
  sigil::motion::FrameClock &clock() { return m_clock; }
  Composer *composer() { return m_composer.get(); }

  /** The sketch-declared canvas (ctx.canvas()/ctx.background()); hosts
   *  letterbox to this size and clear with this color. */
  SkSize canvasSize() const { return m_canvasSpec.size; }
  SkColor4f background() const { return m_canvasSpec.background; }

private:
  struct CompileResult {
    bool ok = false;
    std::filesystem::path library;
    std::string output;
  };

  void startCompile();
  void adopt(const std::filesystem::path &library);
  SketchContext makeContext();
  void applyCanvasSpec();

  Options m_options;
  sigil::weave::FontContext &m_fonts;
  std::filesystem::path m_buildDir;

  sigil::motion::FrameClock m_clock;
  CanvasSpec m_canvasSpec;
  SkSize m_appliedSize = SkSize::MakeEmpty();
  Assets m_assets;
  std::unique_ptr<sigil::motion::Ticker> m_ticker;
  std::unique_ptr<Composer> m_composer;
  std::unique_ptr<Sketch> m_sketch;
  std::vector<void *> m_libraries; // never dlclosed (statics stay valid)

  std::future<CompileResult> m_compile;
  std::filesystem::file_time_type m_compiledMtime{};
  bool m_everCompiled = false;
  int m_generation = 0;
  int m_frameIndex = -1; // for the crash reporter's phase line
  double m_lastAssetPoll = 0.0;
  double m_syntheticNow = 0.0; // fixed-dt timeline for headless runs
  std::chrono::steady_clock::time_point m_compileStart{};
  std::chrono::steady_clock::time_point m_lastPresent{};
  std::vector<double> m_workMs;      // rolling frame-body cost window
  std::vector<double> m_presentMs;   // rolling present-interval window
  FrameTiming m_lastTiming;          // update/draw split of the last frame
  std::string m_status = "waiting for first build";
  std::string m_errorLog;
  CaptureBackend m_captureBackend;
};

} // namespace sigil::compose::sketch
