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

TEST(SketchbookArguments, ASweepTakesItsMomentFromTheStillsTimeFlag) {
  // One word for scene time: the sweep reads `--at` as the still does,
  // and the sweep's own spelling of it is not understood.
  const auto args = parse({"--headless", "plates", "--at", "1.5"});
  ASSERT_TRUE(args);
  EXPECT_TRUE(args->headless);
  EXPECT_EQ(args->capture.at, 1.5);
  EXPECT_FALSE(parse({"--headless", "plates", "--capture-at", "1.5"}));
  EXPECT_FALSE(parse({"--headless", "plates", "--timing-json", "t.json"}));
}

TEST(SketchbookArguments, TheScaleFlagSetsBothStillAndSweepDensity) {
  const auto defaults = parse({"--headless"});
  ASSERT_TRUE(defaults);
  EXPECT_EQ(defaults->capture.scale, 1.0f);
  EXPECT_EQ(defaults->sweepOptions.density, 0.0f);
  for (const char* value : {"0.5", "2"}) {
    const auto args =
        parse({"--headless", "plates", "--gpu", "--scale", value});
    ASSERT_TRUE(args);
    EXPECT_TRUE(args->gpu);
    EXPECT_EQ(args->capture.scale, std::stof(value));
    EXPECT_EQ(args->sweepOptions.density, args->capture.scale);
    const auto reversed = parse({"--scale", value, "--headless", "plates"});
    ASSERT_TRUE(reversed);
    EXPECT_EQ(reversed->sweepOptions.density, args->capture.scale);
  }
}

TEST(SketchbookArguments, AStateRootIsOneDirectoryNamedOnce) {
  const auto args = parse({"--state", "scratch/state", "--sketch", "cascade"});
  ASSERT_TRUE(args);
  EXPECT_EQ(args->stateDirectory, "scratch/state");
  EXPECT_FALSE(parse({"--state"}));
  EXPECT_FALSE(parse({"--state", ""}));
  EXPECT_FALSE(parse({"--state", "--sketch", "cascade"}));
  EXPECT_FALSE(parse({"--state", "one", "--state", "two"}));
  // The thumbnail store stands under the root; it has no flag of its own.
  EXPECT_FALSE(parse({"--thumbnails-dir", "thumbnails"}));
}

TEST(SketchbookArguments, InspectTakesAnOptionalPortOnce) {
  const auto any = parse({"--inspect"});
  ASSERT_TRUE(any);
  EXPECT_EQ(any->inspectPort, std::optional<uint16_t>(0));
  const auto stated = parse({"--inspect=9222"});
  ASSERT_TRUE(stated);
  EXPECT_EQ(stated->inspectPort, std::optional<uint16_t>(9222));
  EXPECT_FALSE(parse({})->inspectPort);
  EXPECT_FALSE(parse({"--inspect=", "--sketch", "cascade"}));
  EXPECT_FALSE(parse({"--inspect=port"}));
  EXPECT_FALSE(parse({"--inspect=70000"}));
  EXPECT_FALSE(parse({"--inspect", "--inspect=1"}));
}

TEST(SketchbookArguments, TheDeterministicFlagsNameAClockPolicy) {
  EXPECT_EQ(parse({"--deterministic"})->clockPolicy,
            sigil::motion::ClockPolicy::Advance);
  EXPECT_EQ(parse({"--no-deterministic"})->clockPolicy,
            sigil::motion::ClockPolicy::Wall);
  EXPECT_FALSE(parse({})->clockPolicy);
}

TEST(SketchbookArguments, AHeadlessRunSaysWhetherItNamedADirectory) {
  EXPECT_FALSE(parse({"--headless", "--inspect"})->headlessDirectoryNamed);
  EXPECT_TRUE(parse({"--headless", "plates"})->headlessDirectoryNamed);
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

TEST(SketchbookArguments, ExamplesCanBeOpenedExplicitly) {
  const auto args = parse({"--examples"});
  ASSERT_TRUE(args);
  EXPECT_TRUE(args->noRestore);
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

TEST(SketchbookArguments, ASketchIsASourceFileAndNeverALibrary) {
  for (const char* library : {"scene.dylib", "scene.so", "scene.bundle"})
    EXPECT_FALSE(parse({library})) << library;
  const auto source = parse({"scene.cpp"});
  ASSERT_TRUE(source);
  EXPECT_EQ(source->sketchFile, "scene.cpp");
}

TEST(SketchbookArguments, AHeadlessScaleIsThePlateDensity) {
  const auto args = parse({"--headless", "plates", "--scale", "0.5"});
  ASSERT_TRUE(args);
  EXPECT_TRUE(args->headless);
  EXPECT_EQ(args->sweepOptions.density, 0.5f);
}

}  // namespace
