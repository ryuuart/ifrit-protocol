#include <gtest/gtest.h>

#include <QtCore/QSettings>
#include <QtCore/QTemporaryDir>
#include <QtCore/QUrl>
#include <filesystem>
#include <fstream>
#include <string_view>

#include "../Workspace.h"

namespace {

namespace fs = std::filesystem;
using sketchbook::workspaceFiles;
using sketchbook::WorkspaceHistory;

class SketchbookWorkspace : public testing::Test {
 protected:
  QTemporaryDir directory;
  fs::path root = directory.path().toStdString();

  fs::path write(const fs::path& relative, std::string_view source) {
    const fs::path path = root / relative;
    fs::create_directories(path.parent_path());
    std::ofstream(path) << source;
    return fs::canonical(path);
  }

  QString settingsPath() const {
    return QString::fromStdString((root / "history.ini").string());
  }

  void SetUp() override {
    ASSERT_TRUE(directory.isValid());
    root = fs::canonical(root);
  }
};

TEST_F(SketchbookWorkspace,
       FindsDeclarationsAndDirectoryEntriesInCanonicalOrder) {
  const auto python =
      write("zebra.py", "@sigil.sketch(size=(10, 10))\nclass Zebra: pass\n");
  const auto cpp =
      write("alpha.cpp",
            "constexpr int pixels = 1'000;\nSIGIL_SKETCH_AS(Alpha, \"alpha\", "
            "\"Art\", \"A\")\n");
  const auto bare = write("bare.py", "@sketch\nclass Bare: pass\n");
  const auto pythonDirectory = write("plant/plant.py", "class Plant: pass\n");
  const auto cppDirectory = write("room/room.cpp", "struct Room {};\n");
  write("plant/palette.py", "COLORS = ['red', 'green']\n");
  write("room/Geometry.cpp", "void geometry() {}\n");
  write("helper.cpp", "void helper() {}\n");
  write("__init__.py", "from .plant import Plant\n");
  write("notes.txt", "@sketch\nSIGIL_SKETCH(Notes)\n");
  const std::vector<fs::path> expected{cpp, bare, pythonDirectory, cppDirectory,
                                       python};
  EXPECT_EQ(workspaceFiles(root / "plant" / ".."), expected);
  EXPECT_TRUE(workspaceFiles(root / "missing").empty());
  EXPECT_TRUE(workspaceFiles(python).empty());
  EXPECT_TRUE(workspaceFiles({}).empty());
}

TEST_F(SketchbookWorkspace, CommentsAndStringsDoNotRegisterHelpers) {
  write("comment.cpp", "// SIGIL_SKETCH(No)\n/* SIGIL_SKETCH(No) */\n");
  write("literal.cpp", R"source(
const char* plain = "SIGIL_SKETCH(No)";
const char* raw = R"example(
SIGIL_SKETCH(No)
)example";
)source");
  write("comment.py", "# @sketch\nclass Helper: pass\n");
  write("literal.py", "'''\n@sigil.sketch()\n'''\nexample = '@sketch()'\n");
  write("other.py", "@sketch_helper()\nclass Helper: pass\n");
  const auto actual = write("real.py",
                            "'''An example says @sketch.'''\n@api . sketch(\n "
                            "size=(10, 10),\n)\nclass Real: pass\n");
  EXPECT_EQ(workspaceFiles(root), std::vector<fs::path>{actual});
}

TEST_F(SketchbookWorkspace, SkipsBuildDependencyHiddenAndSymlinkTrees) {
  const auto actual =
      write("scenes/actual.py", "@sketch()\nclass Actual: pass\n");
  for (const auto* ignored :
       {".git", ".venv", ".hidden", "build", "build-release", "build_debug",
        "cmake-build-debug", "__pycache__", "vendor", "third_party",
        "node_modules", "venv", "vcpkg_installed", "site-packages", "cache"})
    write(fs::path(ignored) / "hidden.py", "@sketch()\nclass Hidden: pass\n");
  write(".hidden.py", "@sketch()\nclass Hidden: pass\n");
  fs::create_directory_symlink(root / "scenes", root / "alias");
  fs::create_directory_symlink(root, root / "scenes" / "loop");
  fs::create_symlink(actual, root / "copy.py");
  EXPECT_EQ(workspaceFiles(root), std::vector<fs::path>{actual});
}

TEST_F(SketchbookWorkspace, BoundsDirectoryDepth) {
  fs::path nested;
  for (int i = 0; i < 13; ++i) nested /= "level" + std::to_string(i);
  write(nested / "too_deep.py", "@sketch()\nclass Deep: pass\n");
  const auto visible = write(nested.parent_path() / "visible.py",
                             "@sketch()\nclass Visible: pass\n");
  EXPECT_EQ(workspaceFiles(root), std::vector<fs::path>{visible});
}

TEST_F(SketchbookWorkspace, NestedPythonProjectsHaveIndependentWorkspaces) {
  write("pyproject.toml", "[project]\nname = 'parent'\n");
  const auto parent = write("scene.py", "@sketch()\nclass Parent: pass\n");
  write("child/pyproject.toml", "[project]\nname = 'child'\n");
  const auto child = write("child/scene.py", "@sketch()\nclass Child: pass\n");
  write("isolated/.venv/pyvenv.cfg", "home = /usr/bin\n");
  const auto isolated =
      write("isolated/scene.py", "@sketch()\nclass Isolated: pass\n");
  EXPECT_EQ(workspaceFiles(root), std::vector<fs::path>{parent});
  EXPECT_EQ(workspaceFiles(child.parent_path()), std::vector<fs::path>{child});
  EXPECT_EQ(workspaceFiles(isolated.parent_path()),
            std::vector<fs::path>{isolated});
}

TEST_F(SketchbookWorkspace, PersistsFilesFoldersAndEachFoldersSelection) {
  const auto first = write("project/first.py", "");
  const auto next = write("project/next.cpp", "");
  const auto other = write("other/other.py", "");
  const auto standalone = write("standalone.py", "");
  const auto project = first.parent_path();
  {
    QSettings settings(settingsPath(), QSettings::IniFormat);
    WorkspaceHistory history(settings);
    history.remember({project, first});
    history.remember({other.parent_path(), other});
    history.remember({project, "next.cpp"});
    history.remember({project, {}});
    EXPECT_EQ(history.restore().root, project);
    EXPECT_EQ(history.restore().file, next);
    history.remember({{}, standalone});
    EXPECT_EQ(settings.status(), QSettings::NoError);
  }
  QSettings settings(settingsPath(), QSettings::IniFormat);
  WorkspaceHistory history(settings);
  EXPECT_TRUE(history.restore().root.empty());
  EXPECT_EQ(history.restore().file, standalone);
  EXPECT_EQ(history.forRoot(project / "."), next);
  EXPECT_EQ(history.forRoot(other.parent_path()), other);
  EXPECT_TRUE(history.forRoot(root / "missing").empty());
  const QVariantList rows = history.recent();
  ASSERT_EQ(rows.size(), 3);
  EXPECT_EQ(rows[0].toMap().value("kind").toString(), "file");
  EXPECT_EQ(rows[0].toMap().value("name").toString(), "standalone.py");
  EXPECT_EQ(rows[0].toMap().value("url").toUrl().toLocalFile().toStdString(),
            standalone.string());
  EXPECT_EQ(rows[1].toMap().value("kind").toString(), "folder");
  EXPECT_EQ(rows[1].toMap().value("path").toString().toStdString(),
            project.string());
  EXPECT_TRUE(rows[1].toMap().value("exists").toBool());
}

TEST_F(SketchbookWorkspace,
       DeduplicatesCanonicalAliasesAndRetainsMissingLocations) {
  const auto source = write("project/scene.py", "");
  const auto project = source.parent_path();
  fs::create_directory_symlink(project, root / "alias");
  QSettings settings(settingsPath(), QSettings::IniFormat);
  WorkspaceHistory history(settings);
  history.remember({project, source});
  history.remember({root / "alias", "scene.py"});
  ASSERT_EQ(history.recent().size(), 1);
  EXPECT_EQ(history.restore().root, project);
  EXPECT_EQ(history.restore().file, source);
  history.remember({{}, source});
  fs::remove(source);
  ASSERT_EQ(history.recent().size(), 2);
  EXPECT_FALSE(history.recent()[0].toMap().value("exists").toBool());
  EXPECT_EQ(history.restore().file, source);
  fs::remove(project);
  EXPECT_FALSE(history.recent()[1].toMap().value("exists").toBool());
  history.remember({{}, root / "missing" / ".." / "new.py"});
  EXPECT_EQ(history.restore().file, root / "new.py");
  EXPECT_FALSE(history.recent()[0].toMap().value("exists").toBool());
}

TEST_F(SketchbookWorkspace, CapsMruAndClearsOnlyItsOwnSettings) {
  QSettings settings(settingsPath(), QSettings::IniFormat);
  settings.setValue("window/width", 1400);
  WorkspaceHistory history(settings);
  history.remember({});
  EXPECT_TRUE(history.restore().root.empty());
  EXPECT_TRUE(history.restore().file.empty());
  for (int i = 0; i < 14; ++i)
    history.remember({{}, root / (std::to_string(i) + ".py")});
  ASSERT_EQ(history.recent().size(), 12);
  EXPECT_EQ(history.recent().front().toMap().value("name").toString(), "13.py");
  EXPECT_EQ(history.recent().back().toMap().value("name").toString(), "2.py");
  history.remember({{}, root / "6.py"});
  ASSERT_EQ(history.recent().size(), 12);
  EXPECT_EQ(history.recent().front().toMap().value("name").toString(), "6.py");
  history.clear();
  EXPECT_TRUE(history.recent().empty());
  EXPECT_TRUE(history.restore().file.empty());
  EXPECT_EQ(settings.value("window/width").toInt(), 1400);
}

}  // namespace
