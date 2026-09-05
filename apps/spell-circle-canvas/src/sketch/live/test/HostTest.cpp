/** @file
 * The live host: what it does before any compiler runs.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/live/Host.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "Fixture.h"
#include "support/Fixtures.h"

namespace {

using namespace sigil::sketch;
using sigil::sketch::test::fonts;
using sigil::sketch::test::kSquare;
using sigil::sketch::test::Watched;

/** A body whose setup count makes a runtime-session restart observable. */
struct Restarted : Sketch {
  static inline int setups = 0;

  void setup(SketchContext& ctx) override {
    ++setups;
    ctx.canvas(120, 90);
    ctx.composer.render(sigil::compose::box().width(20).height(20));
  }
};

Kind restartedKind() { return kindOf<Restarted>(); }
const Entry kRestarted{"restarted", "restarted", "Test", "", &restartedKind};

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

TEST(SketchHost, RestartsTheRuntimeSessionWithoutBuildingAgain) {
  const Watched file("sigil_sketch_host_restart");
  Host::Options opts = options(file.path);
  opts.compiledIn = &kRestarted;
  Restarted::setups = 0;
  Host host(std::move(opts), fonts());
  ASSERT_TRUE(host.live());
  EXPECT_EQ(Restarted::setups, 1);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 90));
  host.frame(*surface->getCanvas(), 1.0 / 60.0);
  EXPECT_GT(host.workMsAverage(), 0.0);

  EXPECT_TRUE(host.restartSession());
  EXPECT_EQ(Restarted::setups, 2);
  EXPECT_EQ(host.workMsAverage(), 0.0);
  EXPECT_FALSE(host.compiling());
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

/** ONE EDIT, and whether it is an edit to the sketch.
 *
 *  A sketch is one translation unit and more than one file, and which
 *  files those are depends on the shape it stands in. `written` is
 *  relative to the watched directory. */
struct Edit {
  const char* what;
  /** The entry stands in a directory named for it, so the sources
   *  beside it are units of it rather than other sketches. */
  bool inADirectoryOfItsOwn;
  /** A shared layer is configured beside it. Its sources are units of
   *  every sketch and its headers may be included by any. */
  bool sharedLayer;
  const char* written;
  bool rebuilds;
};

class EditedFile : public ::testing::TestWithParam<Edit> {};

TEST_P(EditedFile, StartsABuildExactlyWhenWhatWasEditedIsPartOfTheSketch) {
  // What stays on screen after an edit the host did not see is the code
  // that stood before it, with nothing to say so; what a rebuild of
  // every open sketch costs is every save in a directory of them.
  const Edit& edit = GetParam();
  const Watched file(std::string("sigil_sketch_host_edit_") + edit.what,
                     edit.inADirectoryOfItsOwn);
  Host::Options opts = options(file.path);
  // The files beside a sketch are read on their own cadence; asking for
  // every poll is what lets the edit below be seen the moment it is made
  // rather than whenever the cadence next comes round.
  opts.siblingScanInterval = std::chrono::milliseconds(0);
  if (edit.sharedLayer) {
    opts.sharedDir = file.dir.path / "shared";
    std::filesystem::create_directories(opts.sharedDir);
  }
  Host host(std::move(opts), fonts());
  ASSERT_FALSE(host.compiling());

  const std::filesystem::path written = file.dir.path / edit.written;
  std::filesystem::create_directories(written.parent_path());
  std::ofstream(written) << "// edited\n";
  host.poll();
  EXPECT_EQ(host.compiling(), edit.rebuilds);
}

INSTANTIATE_TEST_SUITE_P(
    EditedFiles, EditedFile,
    ::testing::Values(
        // A helper beside a bare sketch is reached by a quoted include,
        // which needs no include path, so an edit to it is an edit here.
        Edit{"AHeaderBesideABareSketch", false, false, "palette.h", true},
        // A sketch that is a directory is every source beside its entry.
        Edit{"AUnitBesideADirectorySketch", true, false, "rain/tables.cpp",
             true},
        // Beside a bare sketch the other sources are other sketches, and
        // an edit to one of them is nothing to this one.
        Edit{"AnotherBareSketchBesideIt", false, false, "other.cpp", false},
        // The shared layer's sources are units of every sketch.
        Edit{"AModuleInTheSharedLayer", false, true, "shared/palette.cpp",
             true}),
    [](const ::testing::TestParamInfo<Edit>& row) { return row.param.what; });

TEST(SketchHost, RefusesToBuildAgainstAFrameworkHeaderNewerThanTheHost) {
  // A sketch dylib compiled against a framework header the host binary
  // predates has ONE side's idea of a layout: the sketch fills a pool the
  // host then resizes, and the corruption surfaces wherever the object is
  // next touched rather than where it was caused. Every public header of a
  // framework library is such a header, whatever it happens to declare, so
  // the build is refused rather than risked.
  const Watched file("sigil_sketch_host_skew");
  const std::filesystem::path root =
      file.dir.path / "src" / "common" / "compose" / "include";
  const std::filesystem::path header =
      root / "sigilcompose" / "core" / "Instances.h";
  std::filesystem::create_directories(header.parent_path());
  std::ofstream(header) << "#pragma once\n";
  std::filesystem::last_write_time(
      header,
      std::filesystem::file_time_type::clock::now() + std::chrono::hours(1));
  const std::filesystem::path flags = file.dir.path / "flags.rsp";
  std::ofstream(flags) << "-I" << root.generic_string() << "\n";

  Host::Options opts = options(file.path);
  opts.flagsFile = flags;
  opts.siblingScanInterval = std::chrono::milliseconds(0);
  Host host(std::move(opts), fonts());
  ASSERT_FALSE(host.compiling());
  std::ofstream(file.dir.path / "palette.h") << "// a helper\n";
  host.poll();
  EXPECT_FALSE(host.compiling());
  EXPECT_NE(host.errorLog().find("Instances.h"), std::string::npos)
      << host.errorLog();
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

// ---- the build directory --------------------------------------------------

/** A PID NOBODY HOLDS, which is what a run that ended looks like from
 *  outside. Zero when every number tried was in use. */
pid_t unusedPid() {
  for (pid_t pid = 90000; pid < 99999; ++pid)
    if (::kill(pid, 0) == -1 && errno == ESRCH) return pid;
  return 0;
}

TEST(SketchHostBuildDir, StandsWhileTheHostLivesAndGoesWithIt) {
  // The objects and the dylibs a run compiles serve nobody once it ends:
  // the table that decides a rebuild is in memory, so no later run reads
  // a byte of them, and the directory's name carries a pid no later run
  // can reuse.
  const Watched file("sigil_sketch_host_build_dir");
  std::filesystem::path dir;
  {
    Host host(options(file.path), fonts());
    dir = host.buildDir();
    ASSERT_FALSE(dir.empty());
    EXPECT_TRUE(std::filesystem::is_directory(dir));
  }
  EXPECT_FALSE(std::filesystem::exists(dir));
}

TEST(SketchHostBuildDir, SweepsAGoneProcessAndLeavesALiveOneStanding) {
  // A run that was killed or that faulted never reached the removal
  // above. The pid in the name is what says which is which, and only
  // "nobody holds it" removes anything — a directory belonging to a
  // process that is still running, this one's own above all, is one a
  // live host is writing into.
  const std::filesystem::path root = std::filesystem::temp_directory_path();
  const pid_t gone = unusedPid();
  ASSERT_NE(gone, 0);
  const std::filesystem::path abandoned =
      root / ("sigil_sketch_" + std::to_string(gone));
  const std::filesystem::path live =
      root / ("sigil_sketch_" + std::to_string(::getpid()));
  std::filesystem::create_directories(abandoned);
  std::ofstream(abandoned / "sketch_1.dylib") << "a library nobody holds\n";
  std::filesystem::create_directories(live);

  Host::sweepAbandonedBuildDirs();

  EXPECT_FALSE(std::filesystem::exists(abandoned));
  EXPECT_TRUE(std::filesystem::is_directory(live));
  std::error_code ec;
  std::filesystem::remove_all(live, ec);
}

/** A COMPILER THAT ONLY MAKES THE FILE IT IS ASKED FOR, so that a case
 *  about which PATH a build names does not pay for a real one. It is
 *  spelled as a command prefix, which is all the host ever does with the
 *  compiler it is given. */
std::string stubCompiler(const std::filesystem::path& script) {
  std::ofstream(script) << "prev=\n"
                           "for arg in \"$@\"; do\n"
                           "  if [ \"$prev\" = \"-o\" ]; then\n"
                           "    : > \"$arg\"\n"
                           "  fi\n"
                           "  prev=$arg\n"
                           "done\n"
                           "exit 0\n";
  return "/bin/sh " + script.string();
}

/** Runs one build of @p host to completion, and says whether it got
 *  there.
 *
 *  The wait is a COUNT OF TURNS rather than an open loop: a compiler
 *  that never exits, or a build that never starts, would otherwise hang
 *  the whole run, and a hung run reports nothing at all where a failed
 *  one names the claim that broke. */
[[nodiscard]] bool buildOnce(Host& host) {
  constexpr int kTurns = 20000;
  host.poll();
  for (int turn = 0; turn < kTurns; ++turn) {
    if (!host.compiling()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    host.poll();
  }
  return false;
}

TEST(SketchHostBuildDir, TwoHostsInOneProcessNeverLinkOverEachOther) {
  // Every host in a process links into ONE directory, and the window
  // keeps three sketches resident, each with a host of its own. A build
  // named by its generation alone would have all of them writing
  // sketch_1.dylib: two hosts building at once race for the path, and
  // the file standing there when one of them dlopens is whichever link
  // finished last, so a host adopts a sketch it did not build. Two hosts
  // over ONE source, two builds each, must therefore name four files and
  // not two.
  const Watched file("sigil_sketch_host_two_hosts");
  const std::string compiler = stubCompiler(file.dir.path / "compiler.sh");

  Host::Options first = options(file.path);
  first.compiler = compiler;
  Host::Options second = options(file.path);
  second.compiler = compiler;
  second.compiledIn = &sigil::sketch::test::kWide;

  Host square(std::move(first), fonts());
  Host wide(std::move(second), fonts());
  ASSERT_TRUE(square.live());
  ASSERT_TRUE(wide.live());

  for (int build = 1; build <= 2; ++build) {
    // One edit, seen by both: the same source at the same generation is
    // the collision this is about.
    std::filesystem::last_write_time(
        file.path,
        std::filesystem::file_time_type::clock::now() +
            std::chrono::seconds(build));
    ASSERT_TRUE(buildOnce(square)) << "the square's build never finished";
    ASSERT_TRUE(buildOnce(wide)) << "the wide sketch's build never finished";
  }

  std::vector<std::string> libraries;
  std::error_code ec;
  for (auto it = std::filesystem::directory_iterator(square.buildDir(), ec);
       !ec && it != std::filesystem::directory_iterator(); it.increment(ec))
    if (it->path().extension() == ".dylib")
      libraries.push_back(it->path().filename().string());
  std::sort(libraries.begin(), libraries.end());
  EXPECT_EQ(libraries.size(), 4u) << "four builds, four files";
  EXPECT_EQ(std::unique(libraries.begin(), libraries.end()) - libraries.begin(),
            4);

  // And each host still answers with the sketch IT opened. Nothing is
  // dlopened here — the stub's output is not a library, so every adopt
  // fails and each host keeps the session it started with, which is
  // exactly the state a host beside a building one is in. That a real
  // guest loads and draws its own file is what the sketch_reload_* ctest
  // entries put through a whole Sketchbook.
  EXPECT_EQ(square.canvasSize(), SkSize::Make(120, 90));
  EXPECT_EQ(wide.canvasSize(), SkSize::Make(200, 100));
}

}  // namespace
