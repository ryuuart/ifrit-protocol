/** @file
 * The live host: what it does before any compiler runs.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/live/Host.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <utility>

#include "Fixture.h"
#include "Support.h"

namespace {

using namespace sigil::sketch;
using sigil::sketch::test::fonts;
using sigil::sketch::test::kSquare;
using sigil::sketch::test::Watched;
using sigil::sketch::test::WatchedDirectory;

Host::Options options(const std::filesystem::path& path) {
  Host::Options opts;
  opts.sketchPath = path;
  opts.assetsDir = std::filesystem::temp_directory_path();
  opts.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
  opts.compiledIn = &kSquare;
  return opts;
}

TEST(SketchHost, OpensACompiledInSketchWithoutBuildingIt) {
  const Watched file("sigil_sketch_host_unedited");
  Host host(options(file.path), fonts());
  EXPECT_TRUE(host.live());
  EXPECT_FALSE(host.compiling());
  EXPECT_EQ(host.canvasSize(), SkSize::Make(120, 90));
  // Polling an unedited file must not kick a build: the sketch is
  // already here, and rebuilding it would only make selecting one slow.
  host.poll();
  EXPECT_FALSE(host.compiling());
  EXPECT_TRUE(host.errorLog().empty());
}

TEST(SketchHost, ReportsTheMomentTheSketchDeclared) {
  // A capture with no moment named on the command line steps to this
  // number, so a still is the frame the author chose rather than the
  // one a default happened to reach.
  const Watched file("sigil_sketch_host_moment");
  Host host(options(file.path), fonts());
  EXPECT_EQ(host.captureSeconds(), sigil::sketch::test::Square::kMoment);
}

TEST(SketchHost, DrawsTheSketchItOpened) {
  const Watched file("sigil_sketch_host_draws");
  Host host(options(file.path), fonts());
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
  const Watched file("sigil_sketch_host_capture");
  Host host(options(file.path), fonts());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  host.frame(*surface->getCanvas(), 1.0 / 60.0);
  const std::filesystem::path out = file.dir.path / "capture.png";
  EXPECT_TRUE(host.capture(out, 2.0f));
  EXPECT_TRUE(std::filesystem::exists(out));
}

TEST(SketchHost, RebuildsWhenAHeaderBesideTheSketchIsEdited) {
  // A sketch is one translation unit and more than one file: a helper
  // beside it is reached by a quoted include, which needs no include
  // path, so an edit to one has to rebuild the sketch. Otherwise what
  // stays on screen is the code that stood before the edit, and nothing
  // says so.
  const Watched file("sigil_sketch_host_sibling");
  Host::Options opts = options(file.path);
  // The headers beside the sketch are read on their own cadence; asking
  // for every poll is what lets the edit below be seen the moment it is
  // made rather than whenever the cadence next comes round.
  opts.siblingScanInterval = std::chrono::milliseconds(0);
  Host host(std::move(opts), fonts());
  ASSERT_FALSE(host.compiling());
  std::ofstream(file.dir.path / "palette.h") << "// a helper\n";
  host.poll();
  EXPECT_TRUE(host.compiling());
}

TEST(SketchHost, RebuildsWhenAUnitBesideADirectorySketchIsEdited) {
  // A sketch that is a directory is every source beside its entry, so a
  // unit written there is an edit to the sketch.
  const WatchedDirectory file("sigil_sketch_host_unit");
  Host::Options opts = options(file.path);
  opts.siblingScanInterval = std::chrono::milliseconds(0);
  Host host(std::move(opts), fonts());
  ASSERT_FALSE(host.compiling());
  std::ofstream(file.path.parent_path() / "tables.cpp") << "// a unit\n";
  host.poll();
  EXPECT_TRUE(host.compiling());
}

TEST(SketchHost, LeavesABareSketchAloneWhenAnotherSketchBesideItIsEdited) {
  // Beside a bare sketch the other sources are other sketches. An edit
  // to one of them is nothing to this one — a directory of sketches
  // must not rebuild every open one whenever any of them is saved.
  const Watched file("sigil_sketch_host_neighbour");
  Host::Options opts = options(file.path);
  opts.siblingScanInterval = std::chrono::milliseconds(0);
  Host host(std::move(opts), fonts());
  ASSERT_FALSE(host.compiling());
  std::ofstream(file.dir.path / "other.cpp") << "// another sketch\n";
  host.poll();
  EXPECT_FALSE(host.compiling());
}

TEST(SketchHost, RebuildsWhenTheSharedLayerIsEdited) {
  // The shared layer's sources are units of every sketch and its
  // headers may be included by any, so an edit there rebuilds a bare
  // sketch as surely as an edit to the sketch itself.
  const Watched file("sigil_sketch_host_shared");
  Host::Options opts = options(file.path);
  opts.siblingScanInterval = std::chrono::milliseconds(0);
  opts.sharedDir = file.dir.path / "shared";
  std::filesystem::create_directories(opts.sharedDir);
  Host host(std::move(opts), fonts());
  ASSERT_FALSE(host.compiling());
  std::ofstream(file.dir.path / "shared" / "palette.cpp") << "// a module\n";
  host.poll();
  EXPECT_TRUE(host.compiling());
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
