/** @file
 * The live host's build directory: it stands while the host lives and goes
 * with it, an abandoned one is swept, a compiler of several words reaches
 * the shell whole, and two hosts in one process never link over each other.
 */

#include <gtest/gtest.h>
#include <sigilsketch/live/Host.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "HostTestSupport.h"
#include "support/Fixtures.h"

namespace {

using namespace sigil::sketch;
using sigil::sketch::test::buildOnce;
using sigil::sketch::test::fonts;
using sigil::sketch::test::options;
using sigil::sketch::test::Watched;

/** A PID NOBODY HOLDS, which is what a run that ended looks like from
 *  outside. Zero when every number tried was in use. */
pid_t unusedPid() {
  for (pid_t pid = 90000; pid < 99999; ++pid)
    if (::kill(pid, 0) == -1 && errno == ESRCH) return pid;
  return 0;
}

TEST(SketchHostBuildDirectory, StandsWhileTheHostLivesAndGoesWithIt) {
  // The objects and the dylibs a run compiles serve nobody once it ends:
  // the table that decides a rebuild is in memory, so no later run reads
  // a byte of them, and the directory's name carries a pid no later run
  // can reuse.
  const Watched file("sigil_sketch_host_build_dir");
  std::filesystem::path dir;
  {
    Host host(options(file.path), fonts());
    dir = host.buildDirectory();
    ASSERT_FALSE(dir.empty());
    EXPECT_TRUE(std::filesystem::is_directory(dir));
  }
  EXPECT_FALSE(std::filesystem::exists(dir));
}

TEST(SketchHostBuildDirectory, AThrowingFactoryReturnsItsDirectoryReference) {
  const Watched file("sigil_sketch_host_throwing_factory");
  const Entry throwing{
      "throwing", "throwing", "Test", "", []() -> Kind {
        throw std::runtime_error("the built-in factory failed");
      }};
  auto failed = options(file.path);
  failed.compiledIn = &throwing;
  EXPECT_THROW(Host(std::move(failed), fonts()), std::runtime_error);

  std::filesystem::path directory;
  {
    Host host(options(file.path), fonts());
    directory = host.buildDirectory();
    ASSERT_TRUE(std::filesystem::is_directory(directory));
  }
  EXPECT_FALSE(std::filesystem::exists(directory));
}

TEST(SketchHostBuildDirectory, SweepsAGoneProcessAndLeavesALiveOneStanding) {
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

  Host::sweepAbandonedBuildDirectories();

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

/** Bumps @p path past now, so the next poll sees an edit. */
void touch(const std::filesystem::path& path, int seconds) {
  std::filesystem::last_write_time(
      path, std::filesystem::file_time_type::clock::now() +
                std::chrono::seconds(seconds));
}

TEST(SketchHostBuildDirectory, CompilesThroughACompilerOfSeveralWords) {
  // The compiler option is a COMMAND PREFIX: a launcher in front of a
  // compiler, or an interpreter in front of a script, is spelled with
  // spaces in it, and each word must reach the shell as an argument of
  // its own. Quoted whole, the prefix names one file that is not there,
  // and the build fails before the compiler is reached at all.
  const Watched file("sigil_sketch_host_prefix_compiler");
  Host::Options opts = options(file.path);
  opts.compiler = stubCompiler(file.dir.path / "compiler.sh");

  Host host(std::move(opts), fonts());
  ASSERT_TRUE(host.live());
  touch(file.path, 1);
  ASSERT_TRUE(buildOnce(host)) << "the build never finished";

  // One unit compiled and linked: the object the compile names and the
  // library the link names both stand. Nothing is dlopened — the stub
  // writes empty files — so the adopt after this fails and the host
  // keeps the session it opened with.
  int objects = 0;
  int libraries = 0;
  std::error_code ec;
  for (auto it = std::filesystem::directory_iterator(host.buildDirectory(), ec);
       !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
    if (it->path().extension() == ".o") ++objects;
    if (it->path().extension() == ".dylib") ++libraries;
  }
  EXPECT_EQ(objects, 1) << "the compile never ran: " << host.errorLog();
  EXPECT_EQ(libraries, 1) << "the link never ran: " << host.errorLog();
}

TEST(SketchHostBuildDirectory, TwoHostsInOneProcessNeverLinkOverEachOther) {
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
    touch(file.path, build);
    ASSERT_TRUE(buildOnce(square)) << "the square's build never finished";
    ASSERT_TRUE(buildOnce(wide)) << "the wide sketch's build never finished";
  }

  std::vector<std::string> libraries;
  std::error_code ec;
  for (auto it =
           std::filesystem::directory_iterator(square.buildDirectory(), ec);
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
  // entries put through a whole host.
  EXPECT_EQ(square.canvasSize(), SkSize::Make(120, 90));
  EXPECT_EQ(wide.canvasSize(), SkSize::Make(200, 100));
}

}  // namespace
