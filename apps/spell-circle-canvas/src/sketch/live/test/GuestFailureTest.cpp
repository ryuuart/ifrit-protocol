/** @file
 * A guest the live host built and loaded that throws: from its kind
 * factory or its ABI query while it is being adopted, or from a frame
 * once it runs. None of it escapes the host, a refused build leaves the
 * accepted session running, and a frame that throws fails the session
 * until it is opened again.
 */

#include <gtest/gtest.h>
#include <sigilsketch/live/Host.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "Fixture.h"
#include "ScratchDir.h"
#include "support/Fixtures.h"

#ifdef SIGIL_SKETCH_FRAME_GUEST

namespace {

namespace sketch = sigil::sketch;

/** A sketch directory whose stub compiler links whichever guest image the
 *  case names last: an object is an empty file, and the library is a copy
 *  of the image named in `guest` beside the script. */
struct GuestSketch {
  explicit GuestSketch(const char* name) : files(name) {
    files.write("guest_sketch/guest_sketch.cpp",
                "// adopted from a guest image, never compiled\n");
    entry = files.path / "guest_sketch" / "guest_sketch.cpp";
    std::ofstream(files.path / "compiler.sh")
        << "prev=\n"
           "for arg in \"$@\"; do\n"
           "  if [ \"$prev\" = \"-o\" ]; then\n"
           "    case \"$arg\" in\n"
           "      *.dylib) cp \"$(cat \"$(dirname \"$0\")/guest\")\" \"$arg\" "
           ";;\n"
           "      *) : > \"$arg\" ;;\n"
           "    esac\n"
           "  fi\n"
           "  prev=$arg\n"
           "done\n"
           "exit 0\n";
  }

  /** The image the next build links, and an edit that asks for it. */
  void link(std::string_view guest) {
    std::ofstream(files.path / "guest") << guest;
    const auto now = std::filesystem::file_time_type::clock::now();
    std::filesystem::last_write_time(entry,
                                     now + std::chrono::seconds(++edits));
  }

  sketch::Host::Options options() const {
    sketch::Host::Options result;
    result.sketchPath = entry;
    result.assetsDirectory = std::filesystem::temp_directory_path();
    result.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
    result.compiler = "/bin/sh " + (files.path / "compiler.sh").string();
    result.compiledIn = nullptr;
    result.clock = sigil::motion::ClockPolicy::Advance;
    return result;
  }

  sigil::test::ScratchDir files;
  std::filesystem::path entry;
  int edits = 0;
};

/** The libraries a host has linked into its build directory so far. */
int linkedLibraries(const sketch::Host& host) {
  int count = 0;
  for (const auto& file :
       std::filesystem::directory_iterator(host.buildDirectory()))
    if (file.path().extension() == ".dylib") ++count;
  return count;
}

TEST(SketchGuestFailure, AThrowingFactoryOrAbiQueryKeepsTheAcceptedSession) {
  GuestSketch sketch("sigil_sketch_guest_failure_adoption");
  sketch.link(SIGIL_SKETCH_IMAGE_GUEST);
  sketch::Host host(sketch.options(), sketch::test::fonts());
  ASSERT_TRUE(sketch::test::buildOnce(host)) << "the build never finished";
  ASSERT_TRUE(host.live()) << host.errorLog();
  for (const char* guest :
       {SIGIL_SKETCH_FACTORY_GUEST, SIGIL_SKETCH_METADATA_GUEST}) {
    auto* accepted = host.session();
    const int linked = linkedLibraries(host);
    sketch.link(guest);
    EXPECT_NO_THROW(EXPECT_TRUE(sketch::test::buildOnce(host)));
    EXPECT_EQ(host.session(), accepted);
    EXPECT_NE(host.errorLog().find("the guest callback failed"),
              std::string::npos)
        << host.errorLog();
    EXPECT_NE(host.status().find("keeping previous sketch"), std::string::npos)
        << host.status();
    // The refused build's library stays where the build put it.
    EXPECT_EQ(linkedLibraries(host), linked + 1);
    EXPECT_TRUE(host.restartSession());
  }
}

TEST(SketchGuestFailure, AFrameThrowingAnythingFailsTheSessionWithoutEscaping) {
  GuestSketch sketch("sigil_sketch_guest_failure_frame");
  sketch.link(SIGIL_SKETCH_FRAME_GUEST);
  sketch::Host host(sketch.options(), sketch::test::fonts());
  ASSERT_TRUE(sketch::test::buildOnce(host)) << "the build never finished";
  ASSERT_TRUE(host.live()) << host.errorLog();
  bool advanced = true;
  EXPECT_NO_THROW(advanced = host.frame(1.0 / 60.0));
  EXPECT_FALSE(advanced);
  EXPECT_NE(host.errorLog().find("unknown exception"), std::string::npos)
      << host.errorLog();
  EXPECT_FALSE(host.frame(1.0 / 60.0));
  ASSERT_TRUE(host.restartSession());
  EXPECT_NO_THROW(advanced = host.frame(1.0 / 60.0));
  EXPECT_FALSE(advanced);
}

}  // namespace

#endif
