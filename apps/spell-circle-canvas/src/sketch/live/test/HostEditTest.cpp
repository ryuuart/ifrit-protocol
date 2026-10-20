/** @file
 * Which edits the live host builds on: an edit to a file that is part of
 * the sketch starts a build and one beside it does not, and a build against
 * a framework header newer than the running host is refused.
 */

#include <gtest/gtest.h>
#include <sigilsketch/live/Host.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "HostTestSupport.h"
#include "support/Fixtures.h"

namespace {

using namespace sigil::sketch;
using sigil::sketch::test::fonts;
using sigil::sketch::test::options;
using sigil::sketch::test::Watched;

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
  /** The entry includes a helper owned by a neighbouring sketch. */
  bool includesNeighbour;
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
  if (edit.includesNeighbour) {
    std::ofstream(file.path) << "#include \"owner/Palette.h\"\n";
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
        // An included helper remains watched in its owner's directory.
        Edit{"AHeaderOwnedByAnotherSketch", false, true, "owner/Palette.h",
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

/** THE GUARD READS THE IMAGE THAT IS RUNNING, not the file on disk.
 *
 *  Rebuilding the host replaces that file underneath the process that
 *  mapped it, and a stamp read after the replacement postdates every
 *  header — so a guard that re-stated the question at every compile would
 *  wave through exactly the dylib it exists to refuse: one built against
 *  the new headers, loaded into the old image still running. */
TEST(SketchHost, KeepsRefusingAfterTheBinaryOnDiskIsReplaced) {
  const Watched file("sigil_sketch_host_rebuilt");
  const std::filesystem::path root =
      file.dir.path / "src" / "common" / "compose" / "include";
  const std::filesystem::path header =
      root / "sigilcompose" / "core" / "Instances.h";
  std::filesystem::create_directories(header.parent_path());
  std::ofstream(header) << "#pragma once\n";
  const auto now = std::filesystem::file_time_type::clock::now();
  std::filesystem::last_write_time(header, now + std::chrono::hours(1));
  const std::filesystem::path flags = file.dir.path / "flags.rsp";
  std::ofstream(flags) << "-I" << root.generic_string() << "\n";

  // The image this host is part of, as it stood when the process started.
  const std::filesystem::path binary = file.dir.path / "host";
  std::ofstream(binary) << "the host\n";
  std::filesystem::last_write_time(binary, now);

  Host::Options opts = options(file.path);
  opts.flagsFile = flags;
  opts.siblingScanInterval = std::chrono::milliseconds(0);
  opts.hostStamp = std::filesystem::last_write_time(binary);
  Host host(std::move(opts), fonts());

  // The rebuild lands: the file on disk is now newer than the header,
  // while the process still running is the old one.
  std::filesystem::last_write_time(binary, now + std::chrono::hours(2));

  std::ofstream(file.dir.path / "palette.h") << "// a helper\n";
  host.poll();
  EXPECT_FALSE(host.compiling());
  EXPECT_NE(host.errorLog().find("Instances.h"), std::string::npos)
      << host.errorLog();
}

}  // namespace
