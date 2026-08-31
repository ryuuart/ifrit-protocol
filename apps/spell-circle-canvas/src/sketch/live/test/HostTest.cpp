/** @file
 * The live host: what it does before any compiler runs.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
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
    ctx.canvas(120, 90);
    ctx.background({0, 0, 0, 1});
    ctx.composer.render(
        box().width(40).height(40).fill(Fill::color({0, 1, 0, 1})));
  }
};

Kind squareKind() { return kindOf<Square>(); }

/** A file for the host to watch. It is never compiled here: what is
 *  under test is that a sketch this binary already carries opens without
 *  a build, which is what makes selecting one in a host instant. */
std::filesystem::path watched() {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "sigil_sketch_host_test.cpp";
  std::ofstream(path) << "// watched, never built\n";
  return path;
}

Host::Options options(const Entry& entry, const std::filesystem::path& path) {
  Host::Options opts;
  opts.sketchPath = path;
  opts.assetsDir = std::filesystem::temp_directory_path();
  opts.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
  opts.compiledIn = &entry;
  return opts;
}

TEST(SketchHost, OpensACompiledInSketchWithoutBuildingIt) {
  const Entry entry{"square", "square", "Test", "", &squareKind};
  const std::filesystem::path path = watched();
  Host host(options(entry, path), fonts());
  EXPECT_TRUE(host.live());
  EXPECT_FALSE(host.compiling());
  EXPECT_EQ(host.canvasSize(), SkSize::Make(120, 90));
  // Polling an unedited file must not kick a build: the sketch is
  // already here, and rebuilding it would only make selecting one slow.
  host.poll();
  EXPECT_FALSE(host.compiling());
  EXPECT_TRUE(host.errorLog().empty());
}

TEST(SketchHost, DrawsTheSketchItOpened) {
  const Entry entry{"square", "square", "Test", "", &squareKind};
  Host host(options(entry, watched()), fonts());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  surface->getCanvas()->clear(SK_ColorBLACK);
  EXPECT_TRUE(host.frame(*surface->getCanvas(), 1.0 / 60.0));
  SkBitmap bitmap;
  bitmap.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
  EXPECT_EQ(bitmap.getColor(5, 5), SK_ColorGREEN);
}

TEST(SketchHost, WritesACaptureAtTheScaleItIsAsked) {
  const Entry entry{"square", "square", "Test", "", &squareKind};
  Host host(options(entry, watched()), fonts());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  host.frame(*surface->getCanvas(), 1.0 / 60.0);
  const std::filesystem::path out =
      std::filesystem::temp_directory_path() / "sigil_sketch_host_test.png";
  std::filesystem::remove(out);
  EXPECT_TRUE(host.capture(out, 2.0f));
  EXPECT_TRUE(std::filesystem::exists(out));
}

TEST(SketchHost, ReportsWaitingWhenNothingHasLoaded) {
  Host::Options opts;
  opts.sketchPath = std::filesystem::temp_directory_path() / "absent.cpp";
  opts.assetsDir = std::filesystem::temp_directory_path();
  opts.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
  Host host(std::move(opts), fonts());
  EXPECT_FALSE(host.live());
  EXPECT_EQ(host.state(), Host::State::Waiting);
  // A source file that is not there cannot be built, and the host must
  // say so by standing still rather than by spawning a compiler.
  host.poll();
  EXPECT_FALSE(host.compiling());
}

}  // namespace
