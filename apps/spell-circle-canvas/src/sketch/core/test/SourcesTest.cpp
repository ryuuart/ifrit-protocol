/** @file
 * Where a sketch stands on disk: a bare file, or a directory carrying
 * its own stem, and what each is built from.
 */

#include <gtest/gtest.h>
#include <sigilsketch/core/Sources.h>

#include <filesystem>
#include <vector>

#include "ScratchDir.h"

namespace {

using namespace sigil::sketch;
namespace fs = std::filesystem;

TEST(SketchSources, ABareFileIsItsOwnOnlyUnit) {
  const sigil::test::ScratchDir scratch("sigil_sketch_sources_bare");
  scratch.write("hello.cpp", "// a sketch\n");
  // A source beside a bare sketch is another sketch, not a unit of this
  // one: a directory of bare sketches shares nothing but its headers.
  scratch.write("other.cpp", "// another sketch\n");
  const fs::path entry = scratch.path / "hello.cpp";
  EXPECT_FALSE(directorySketch(entry));
  EXPECT_EQ(unitsOf(entry), std::vector<fs::path>{entry});
}

TEST(SketchSources, ADirectorySketchIsEveryUnitBesideItsEntry) {
  const sigil::test::ScratchDir scratch("sigil_sketch_sources_directory");
  scratch.write("rain/rain.cpp", "// the entry\n");
  scratch.write("rain/tables.cpp", "// a unit\n");
  scratch.write("rain/brush.cpp", "// another unit\n");
  scratch.write("rain/notes.h", "// a header, reached by include\n");
  const fs::path dir = scratch.path / "rain";
  const fs::path entry = dir / "rain.cpp";
  EXPECT_TRUE(directorySketch(entry));
  EXPECT_FALSE(directorySketch(dir / "tables.cpp"));
  // The entry first, then the rest in name order.
  const std::vector<fs::path> expected{entry, dir / "brush.cpp",
                                       dir / "tables.cpp"};
  EXPECT_EQ(unitsOf(entry), expected);
}

TEST(SketchSources, AKeyNamesTheDirectoryFormWhenItStands) {
  const sigil::test::ScratchDir scratch("sigil_sketch_sources_key");
  scratch.write("rain/rain.cpp", "// the entry\n");
  scratch.write("hello.cpp", "// a bare sketch\n");
  EXPECT_EQ(sourceOf(scratch.path, "rain"), scratch.path / "rain" / "rain.cpp");
  EXPECT_EQ(sourceOf(scratch.path, "hello"), scratch.path / "hello.cpp");
  // A key nothing on disk answers to still names the bare file: the
  // caller asks for a file that is not there and finds out for itself.
  EXPECT_EQ(sourceOf(scratch.path, "absent"), scratch.path / "absent.cpp");
}

TEST(SketchSources, AHelperDirectoryWithoutAnEntryIsNotASketch) {
  const sigil::test::ScratchDir scratch("sigil_sketch_sources_helper");
  scratch.write("helpers/palette.cpp", "// a module\n");
  scratch.write("helpers/palette.h", "// its header\n");
  // A directory with no entry of its own name holds units of nothing.
  EXPECT_FALSE(directorySketch(scratch.path / "helpers" / "palette.cpp"));
  EXPECT_EQ(sourcesUnder(scratch.path / "helpers"),
            std::vector<fs::path>{scratch.path / "helpers" / "palette.cpp"});
  EXPECT_TRUE(sourcesUnder(scratch.path / "nowhere").empty());
}

TEST(SketchSources, SourceMetadataPreservesKnobsAndStopsAtCode) {
  const sigil::test::ScratchDir scratch("sigil_sketch_metadata");
  scratch.write("example.cpp",
                "// example — a title\n//\n// A reusable component.\n//\n"
                "// EDIT THESE FIRST\n//   size — width\n//     in pixels\n"
                "//   ink — color\nint main() {}\n// unrelated comment\n");
  const SourceMetadata metadata = sourceMetadata(scratch.path / "example.cpp");
  EXPECT_EQ(metadata.subject, "A reusable component.");
  EXPECT_EQ(metadata.editFirst, "size — width in pixels\nink — color");
  EXPECT_EQ(metadata.lines, 10);
  EXPECT_TRUE(sourceMetadata(scratch.path / "absent.cpp").subject.empty());
}

TEST(SketchSources, LocalHeadersFollowOwnersAcrossDirectoriesAndCycles) {
  const sigil::test::ScratchDir scratch("sigil_sketch_header_owners");
  scratch.write("rain/rain.cpp", "#include \"../cloud/Palette.h\"\n");
  scratch.write("rain/drops.cpp", "# include \"Drops.h\"\n");
  scratch.write("rain/Drops.h", "#include \"../cloud/Palette.h\"\n");
  scratch.write("cloud/Palette.h",
                "#include \"../rain/Drops.h\"\n"
                "#include <vector>\n"
                "#include \"Missing.h\"\n");
  const std::vector<fs::path> expected{scratch.path / "cloud/Missing.h",
                                       scratch.path / "cloud/Palette.h",
                                       scratch.path / "rain/Drops.h"};
  EXPECT_EQ(headersOf(scratch.path / "rain/rain.cpp"), expected);
}

}  // namespace
