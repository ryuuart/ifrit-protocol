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

TEST(SketchSources, PythonKeysResolveBareAndDirectoryEntries) {
  const sigil::test::ScratchDir scratch("sigil_sketch_python_keys");
  scratch.write("orbits.py", "pass\n");
  scratch.write("rain.py", "pass\n");
  scratch.write("rain/rain.py", "pass\n");
  EXPECT_EQ(sourceOf(scratch.path, "orbits"), scratch.path / "orbits.py");
  EXPECT_EQ(sourceOf(scratch.path, "rain"), scratch.path / "rain/rain.py");
  EXPECT_EQ(sourceOf(scratch.path, "absent"), scratch.path / "absent.cpp");
}

TEST(SketchSources, CppEntriesTakePrecedenceOverPythonEntries) {
  const sigil::test::ScratchDir scratch("sigil_sketch_mixed_keys");
  scratch.write("rain.py", "pass\n");
  scratch.write("rain/rain.py", "pass\n");
  scratch.write("rain.cpp", "// bare entry\n");
  EXPECT_EQ(sourceOf(scratch.path, "rain"), scratch.path / "rain.cpp");
  scratch.write("rain/rain.cpp", "// directory entry\n");
  EXPECT_EQ(sourceOf(scratch.path, "rain"), scratch.path / "rain/rain.cpp");
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

TEST(SketchSources, TagsArePathsAndDoNotBecomeSubjectOrKnobs) {
  const sigil::test::ScratchDir scratch("sigil_sketch_metadata_tags");
  scratch.write("example.cpp",
                "/** @file\n * A title\n *\n * A paragraph about the drawing.\n"
                " * TAGS: Typography / Paragraph, Motion/Text, , / /\n"
                " *\n * EDIT THESE FIRST\n *   size — width\n */\n"
                "// TAGS: Typography/Paragraph, Drawing//Brushes/\n"
                "#include <vector>\n// TAGS: Ignored\n");
  const SourceMetadata metadata = sourceMetadata(scratch.path / "example.cpp");
  EXPECT_EQ(metadata.subject, "A paragraph about the drawing.");
  EXPECT_EQ(metadata.editFirst, "size — width");
  EXPECT_EQ(metadata.tags,
            (std::vector<std::string>{"Typography/Paragraph", "Motion/Text",
                                      "Drawing/Brushes"}));
  EXPECT_TRUE(sourceMetadata(scratch.path / "absent.cpp").tags.empty());
}

TEST(SketchSources, PythonDocstringsSupplySubjectTagsAndKnobs) {
  const sigil::test::ScratchDir scratch("sigil_sketch_python_metadata");
  scratch.write("example.py",
                "#!/usr/bin/env python3\n# -*- coding: utf-8 -*-\n"
                "# TAGS: Motion / Animation\n"
                "r\"\"\"A retained view.\n\n"
                "EDIT THESE FIRST\n  spacing — width\n    in pixels\n"
                "  ink — color\n"
                "TAGS: Typography / Interface, Motion/Animation\n\"\"\"\n"
                "from math import sin\n# TAGS: Ignored\n"
                "\"\"\"An unrelated literal.\"\"\"\n");
  const SourceMetadata metadata = sourceMetadata(scratch.path / "example.py");
  EXPECT_EQ(metadata.subject, "A retained view.");
  EXPECT_EQ(metadata.editFirst, "spacing — width in pixels\nink — color");
  EXPECT_EQ(metadata.tags, (std::vector<std::string>{"Motion/Animation",
                                                     "Typography/Interface"}));
  EXPECT_EQ(metadata.lines, 14);
}

TEST(SketchSources, PythonDocstringUsesItsOpeningParagraphWithoutATitle) {
  const sigil::test::ScratchDir scratch("sigil_sketch_python_summary");
  for (const std::string quote : {"\"\"\"", "'''"}) {
    scratch.write("example.py", quote + "A compact summary" + quote +
                                    "\n# TAGS: Drawing/Pen\nimport math\n");
    auto metadata = sourceMetadata(scratch.path / "example.py");
    EXPECT_EQ(metadata.subject, "A compact summary");
    EXPECT_EQ(metadata.tags, (std::vector<std::string>{"Drawing/Pen"}));
    scratch.write("example.py", "# A header before the module\n" + quote +
                                    "A summary spread\nover two lines\n\n"
                                    "A longer explanation.\n" +
                                    quote + "\npass\n");
    metadata = sourceMetadata(scratch.path / "example.py");
    EXPECT_EQ(metadata.subject, "A summary spread over two lines");
  }
}

TEST(SketchSources, PythonCommentHeadersKeepTheTitleAndSubjectConvention) {
  const sigil::test::ScratchDir scratch("sigil_sketch_python_comments");
  scratch.write("example.py",
                "# example — a title\n# A coding study\n#\n"
                "# A shared description.\n# TAGS: Drawing/Pen\n#\n"
                "# EDIT THESE FIRST\n#   radius — size\n"
                "radius = 12\n# TAGS: Ignored\n");
  const SourceMetadata metadata = sourceMetadata(scratch.path / "example.py");
  EXPECT_EQ(metadata.subject, "A shared description.");
  EXPECT_EQ(metadata.editFirst, "radius — size");
  EXPECT_EQ(metadata.tags, (std::vector<std::string>{"Drawing/Pen"}));
  EXPECT_EQ(metadata.lines, 10);
}

TEST(SketchSources, PythonCodePreventsLaterCommentsAndStringsBecomingMetadata) {
  const sigil::test::ScratchDir scratch("sigil_sketch_python_boundary");
  for (const std::string code :
       {"import math", "REQUIRES = ('numpy',)", "def draw(pen):", "@sketch()",
        "f\"\"\"An expression.\"\"\"", "b\"\"\"Bytes.\"\"\""}) {
    SCOPED_TRACE(code);
    scratch.write("example.py", code +
                                    "\n\"\"\"A later string.\n"
                                    "TAGS: Ignored\n\"\"\"\n"
                                    "# TAGS: Also/Ignored\n");
    const SourceMetadata metadata = sourceMetadata(scratch.path / "example.py");
    EXPECT_TRUE(metadata.subject.empty());
    EXPECT_TRUE(metadata.tags.empty());
    EXPECT_EQ(metadata.lines, 5);
  }
}

TEST(SketchSources, PythonCodeAfterClosingQuotesEndsTheHeader) {
  const sigil::test::ScratchDir scratch("sigil_sketch_python_inline_code");
  scratch.write("example.py",
                "\"\"\"The module summary.\"\"\"; value = 1\n"
                "# TAGS: Ignored\n\"\"\"Another literal.\"\"\"\n");
  const SourceMetadata metadata = sourceMetadata(scratch.path / "example.py");
  EXPECT_EQ(metadata.subject, "The module summary.");
  EXPECT_TRUE(metadata.tags.empty());
}

TEST(SketchSources, EscapedPythonQuotesDoNotEndTheDocstring) {
  const sigil::test::ScratchDir scratch("sigil_sketch_python_escaped_quotes");
  scratch.write("example.py", R"py("""A quoted \"\"\" marker.
TAGS: Drawing/Pen
"""
import math
)py");
  const SourceMetadata metadata = sourceMetadata(scratch.path / "example.py");
  EXPECT_EQ(metadata.subject, R"(A quoted \"\"\" marker.)");
  EXPECT_EQ(metadata.tags, (std::vector<std::string>{"Drawing/Pen"}));
}

TEST(SketchSources, OnlyTheFirstPythonStringIsAModuleDocstring) {
  const sigil::test::ScratchDir scratch("sigil_sketch_python_later_string");
  scratch.write("example.py",
                "u'''The module summary.'''\n\n"
                "'''A second string.\nTAGS: Ignored\n'''\n"
                "# TAGS: Also/Ignored\n");
  const SourceMetadata metadata = sourceMetadata(scratch.path / "example.py");
  EXPECT_EQ(metadata.subject, "The module summary.");
  EXPECT_TRUE(metadata.tags.empty());
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

TEST(SketchSources, PythonReloadWatchesSiblingsAndRegularPackages) {
  const sigil::test::ScratchDir scratch("sigil_sketch_python_sources");
  scratch.write("entry.py", "pass\n");
  scratch.write("palette.py", "ink = 'red'\n");
  scratch.write("shapes/__init__.py", "\n");
  scratch.write("shapes/circles.py", "\n");
  scratch.write("shapes/detail/__init__.py", "\n");
  scratch.write("shapes/detail/arcs.py", "\n");
  scratch.write("assets/helper.py", "\n");
  scratch.write(".hidden/__init__.py", "\n");
  scratch.write("other.cpp", "\n");
  const std::vector<fs::path> expected{
      scratch.path / "entry.py",
      scratch.path / "palette.py",
      scratch.path / "shapes/__init__.py",
      scratch.path / "shapes/circles.py",
      scratch.path / "shapes/detail/__init__.py",
      scratch.path / "shapes/detail/arcs.py"};
  EXPECT_EQ(pythonSourcesOf(scratch.path / "entry.py"), expected);
}

TEST(SketchSources, PythonReloadKeepsTheEntryWhenItIsMissing) {
  const sigil::test::ScratchDir scratch("sigil_sketch_python_missing");
  const fs::path entry = scratch.path / "absent.py";
  EXPECT_EQ(pythonSourcesOf(entry), std::vector<fs::path>{entry});
}

}  // namespace
