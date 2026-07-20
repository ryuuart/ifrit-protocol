#pragma once
// The live-reload engine (Qt-free): watches the sketch source, rebuilds
// it into a versioned dylib with the compiler flags CMake captured at
// configure time, dlopens the result, and swaps the running sketch —
// keeping the previous one alive on compile errors, p5 style. The host
// executable exports the framework's symbols, so sketch dylibs link
// with `-undefined dynamic_lookup` and build in a couple of seconds.

#include "ifritsketch/Sketch.h"

#include <ifrittick/FrameClock.h>

#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace textflow {
class FontContext;
}

namespace ifrit::compose::sketch {

class SketchHost {
public:
  struct Options {
    std::filesystem::path sketchPath;
    std::filesystem::path assetsDir;  // default: <sketch dir>/assets
    std::filesystem::path flagsFile;  // sketch_flags.rsp next to the exe
    std::string compiler = "clang++";
  };

  SketchHost(Options options, textflow::FontContext &fonts);
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

  /** One-line state for a status bar. */
  const std::string &status() const { return m_status; }
  /** Full compiler/loader output of the most recent failure; empty
   *  when the latest build is good. */
  const std::string &errorLog() const { return m_errorLog; }

  ifrit::tick::FrameClock &clock() { return m_clock; }
  Composer *composer() { return m_composer.get(); }

  static constexpr SkSize kCanvasSize = {900, 640};

private:
  struct CompileResult {
    bool ok = false;
    std::filesystem::path library;
    std::string output;
  };

  void startCompile();
  void adopt(const std::filesystem::path &library);

  Options m_options;
  textflow::FontContext &m_fonts;
  std::filesystem::path m_buildDir;

  ifrit::tick::FrameClock m_clock;
  Assets m_assets;
  std::unique_ptr<ifrit::tick::Ticker> m_ticker;
  std::unique_ptr<Composer> m_composer;
  std::unique_ptr<Sketch> m_sketch;
  std::vector<void *> m_libraries; // never dlclosed (statics stay valid)

  std::future<CompileResult> m_compile;
  std::filesystem::file_time_type m_compiledMtime{};
  bool m_everCompiled = false;
  int m_generation = 0;
  double m_lastAssetPoll = 0.0;
  double m_syntheticNow = 0.0; // fixed-dt timeline for headless runs
  std::string m_status = "waiting for first build";
  std::string m_errorLog;
};

} // namespace ifrit::compose::sketch
