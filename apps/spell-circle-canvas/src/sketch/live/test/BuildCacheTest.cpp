#include <gtest/gtest.h>

#include <fstream>

#include "../BuildCache.h"
#include "ScratchDir.h"

namespace {
using namespace sigil::sketch;

TEST(SketchBuildCache, RestoresIntoIndependentRuntimeFiles) {
  const sigil::test::ScratchDir scratch("sigil_build_cache");
  const auto cache = scratch.path / "cache";
  const auto library = scratch.path / "built.dylib";
  std::ofstream(library) << "native image";
  const auto key = buildDigest("source and native build");
  storeBuild(cache, key, library);
  std::filesystem::remove(library);
  const auto first = scratch.path / "session_one.dylib";
  const auto second = scratch.path / "session_two.dylib";
  ASSERT_TRUE(restoreBuild(cache, key, first));
  ASSERT_TRUE(restoreBuild(cache, key, second));
  std::filesystem::remove(first);
  EXPECT_TRUE(std::filesystem::exists(second));
  EXPECT_TRUE(restoreBuild(cache, key, first));
  EXPECT_FALSE(restoreBuild(cache, buildDigest("changed source"), first));
}

TEST(SketchBuildCache, FailedPublicationPreservesExistingArtifact) {
  const sigil::test::ScratchDir scratch("sigil_build_cache_failure");
  const auto cache = scratch.path / "cache";
  const auto library = scratch.path / "built.dylib";
  std::ofstream(library) << "good";
  const auto key = buildDigest("inputs");
  storeBuild(cache, key, library);
  storeBuild(cache, key, scratch.path / "absent");
  ASSERT_TRUE(restoreBuild(cache, key, scratch.path / "loaded"));
  std::ifstream input(scratch.path / "loaded");
  std::string bytes;
  input >> bytes;
  EXPECT_EQ(bytes, "good");
  EXPECT_FALSE(restoreBuild({}, key, scratch.path / "disabled"));
  std::ofstream(cache / (key + ".bin"), std::ios::app) << "corrupted";
  EXPECT_FALSE(restoreBuild(cache, key, scratch.path / "corrupt"));
}
}  // namespace
