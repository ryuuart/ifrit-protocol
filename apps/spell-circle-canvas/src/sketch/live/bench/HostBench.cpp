/** @file
 * What the live host adds to a frame: the phase marks the crash reporter
 * reads and the rolling window a status bar shows, over the session's
 * own cost.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/live/Host.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <filesystem>
#include <fstream>

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

struct Square : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(320, 240);
    ctx.composer.render(
        box().width(80).height(80).fill(Fill::color({0, 1, 0, 1})));
  }
};

Kind squareKind() { return kindOf<Square>(); }
const Entry kEntry{"square", "square", "Bench", "", &squareKind};

Host::Options options() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "sigil_sketch_host_bench.cpp";
  std::ofstream(path) << "// watched, never built\n";
  Host::Options opts;
  opts.sketchPath = path;
  opts.assetsDir = std::filesystem::temp_directory_path();
  opts.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
  opts.compiledIn = &kEntry;
  return opts;
}

void Frame(benchmark::State& state) {
  Host host(options(), fonts());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320, 240));
  for (auto&& _ : state) host.frame(*surface->getCanvas(), 1.0 / 60.0);
}
BENCHMARK(Frame)->Unit(benchmark::kMicrosecond);

/** The watch itself: one filesystem stat per frame, which every live
 *  frame pays whether or not anything changed. */
void Poll(benchmark::State& state) {
  Host host(options(), fonts());
  for (auto&& _ : state) host.poll();
}
BENCHMARK(Poll)->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
