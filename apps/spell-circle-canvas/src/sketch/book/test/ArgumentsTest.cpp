/** @file
 * Native launcher flags are parsed before a sketch or its Python environment
 * runs.
 */

#include <gtest/gtest.h>

#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "../Arguments.h"

namespace {

std::optional<Arguments> parse(std::initializer_list<const char*> words) {
  std::vector<std::string> owned{"Sketchbook"};
  owned.insert(owned.end(), words.begin(), words.end());
  std::vector<char*> argv;
  for (auto& word : owned) argv.push_back(word.data());
  return parseArguments((int)argv.size(), argv.data());
}

TEST(SketchbookArguments, CaptureRejectsInvalidTimingAndScale) {
  for (const char* value : {"-1", "nan", "inf"}) {
    EXPECT_FALSE(parse({"--at", value}));
    EXPECT_FALSE(parse({"--scale", value}));
    EXPECT_FALSE(parse({"--fps", value}));
  }
  EXPECT_FALSE(parse({"--scale", "0"}));
  EXPECT_FALSE(parse({"--fps", "0"}));
  EXPECT_FALSE(parse({"--fps", "1e20"}));
  EXPECT_TRUE(parse({"--at", "0"}));
  EXPECT_TRUE(parse({"--at", "0.025", "--fps", "60", "--scale", "0.5"}));
}

TEST(SketchbookArguments, PythonInfoIsAStandaloneQuery) {
  const auto args = parse({"--python-info"});
  ASSERT_TRUE(args);
  EXPECT_TRUE(args->pythonInfo);
  EXPECT_TRUE(args->sketchFile.empty());
  EXPECT_TRUE(args->pythonExecutable.empty());
  EXPECT_TRUE(args->pythonAbi.empty());
  EXPECT_FALSE(parse({"--python-info", "--list"}));
  EXPECT_FALSE(parse({"--python-info", "scene.py"}));
  EXPECT_FALSE(parse({"--python-info", "--python-info"}));
}

TEST(SketchbookArguments, PythonEnvironmentIsPairedWithItsAbiInEitherOrder) {
  const auto args = parse(
      {"--python-executable", "/tmp/a project/.venv/bin/python", "--python-abi",
       "cpython-314-darwin", "scene.py", "--frame", "/tmp/frame.png"});
  ASSERT_TRUE(args);
  EXPECT_EQ(args->pythonExecutable, "/tmp/a project/.venv/bin/python");
  EXPECT_EQ(args->pythonAbi, "cpython-314-darwin");
  EXPECT_EQ(args->sketchFile, "scene.py");
  EXPECT_EQ(args->capture.outputPath, "/tmp/frame.png");
  EXPECT_FALSE(args->pythonInfo);

  const auto reversed =
      parse({"--python-abi", "cpython-314-darwin", "--python-executable",
             ".venv/bin/python", "--catalog"});
  ASSERT_TRUE(reversed);
  EXPECT_TRUE(reversed->catalog);
  EXPECT_EQ(reversed->pythonExecutable, ".venv/bin/python");
  EXPECT_EQ(reversed->pythonAbi, args->pythonAbi);
}

TEST(SketchbookArguments, PythonEnvironmentRequiresBothNonemptyValues) {
  EXPECT_FALSE(parse({"--python-executable"}));
  EXPECT_FALSE(parse({"--python-abi"}));
  EXPECT_FALSE(parse({"--python-executable", ".venv/bin/python"}));
  EXPECT_FALSE(parse({"--python-abi", "cpython-314-darwin"}));
  EXPECT_FALSE(
      parse({"--python-executable", "", "--python-abi", "cpython-314-darwin"}));
  EXPECT_FALSE(
      parse({"--python-executable", ".venv/bin/python", "--python-abi", ""}));
  EXPECT_FALSE(
      parse({"--python-executable", "--python-abi", "cpython-314-darwin"}));
  EXPECT_FALSE(
      parse({"--python-abi", "--python-executable", ".venv/bin/python"}));
}

TEST(SketchbookArguments, DuplicateAndUnknownEnvironmentFlagsAreRejected) {
  EXPECT_FALSE(parse({"--python-executable", "first", "--python-executable",
                      "second", "--python-abi", "cpython-314-darwin"}));
  EXPECT_FALSE(parse({"--python-executable", "first", "--python-abi", "one",
                      "--python-abi", "two"}));
  EXPECT_FALSE(parse({"--python-env", ".venv"}));
  EXPECT_FALSE(parse(
      {"--python-executable=python", "--python-abi", "cpython-314-darwin"}));
  EXPECT_FALSE(parse({"--python-info", "--python-version"}));
  EXPECT_FALSE(parse({"--sketch", "hello", "--python-abi"}));
}

TEST(SketchbookArguments, PythonPathsAndPublishingKeepTheirOwnArguments) {
  const auto args =
      parse({"--publish", "scene.py", "--python-executable", ".venv/bin/python",
             "--python-abi", "cpython-314-darwin"});
  ASSERT_TRUE(args);
  EXPECT_TRUE(args->publish);
  EXPECT_TRUE(args->publishName.empty());
  EXPECT_EQ(args->sketchFile, "scene.py");
  const auto plain = parse({"scene.cpp", "--frame", "/tmp/frame.png"});
  ASSERT_TRUE(plain);
  EXPECT_TRUE(plain->pythonExecutable.empty());
  EXPECT_TRUE(plain->pythonAbi.empty());
}

TEST(SketchbookArguments, WorkspaceSelectionAndRestoreCanBeExplicit) {
  const auto args =
      parse({"--workspace", "/tmp/my sketches", "--no-restore", "scene.py"});
  ASSERT_TRUE(args);
  EXPECT_EQ(args->workspace, "/tmp/my sketches");
  EXPECT_TRUE(args->noRestore);
  EXPECT_EQ(args->sketchFile, "scene.py");
  EXPECT_FALSE(parse({"--workspace"}));
  EXPECT_FALSE(parse({"--workspace", ""}));
  EXPECT_FALSE(parse({"--workspace", "one", "--workspace", "two"}));
  EXPECT_FALSE(parse({"--workspace", "--no-restore"}));
}

TEST(SketchbookArguments, ExplicitPublicationNamesCannotBecomeSketchPaths) {
  for (const char* flag : {"--publish=scene.py", "--publish=--named"}) {
    const auto args = parse({"selected.py", flag});
    ASSERT_TRUE(args);
    EXPECT_TRUE(args->publish);
    EXPECT_EQ(args->publishName, std::string(flag).substr(10));
    EXPECT_EQ(args->sketchFile, "selected.py");
  }
  EXPECT_FALSE(parse({"selected.py", "--publish="}));
}

}  // namespace
