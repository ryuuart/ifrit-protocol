/** @file
 * The state root: a process starts with none, a root names where each
 * kind of state stands, and a test's own root is put back as it found it.
 */

#include <gtest/gtest.h>
#include <sigilsketch/core/State.h>

#include "support/StateRoot.h"

namespace {

using namespace sigil::sketch;

TEST(SketchState, EachKindStandsInADirectoryNamedForIt) {
  const test::StateRoot root("sigil_state_kinds");
  EXPECT_EQ(stateDirectory(), root.path());
  EXPECT_EQ(stateLocation("builds"), root.path() / "builds");
  EXPECT_EQ(stateLocation("thumbnails"), root.path() / "thumbnails");
}

TEST(SketchState, ATestsRootIsPutBackAsItWasFound) {
  const std::filesystem::path before = stateDirectory();
  {
    const test::StateRoot outer("sigil_state_outer");
    {
      const test::StateRoot inner("sigil_state_inner");
      EXPECT_EQ(stateDirectory(), inner.path());
    }
    EXPECT_EQ(stateDirectory(), outer.path());
  }
  EXPECT_EQ(stateDirectory(), before);
}

TEST(SketchState, WithoutARootEveryKindIsLeftToThePlatform) {
  const std::filesystem::path before = stateDirectory();
  setStateDirectory({});
  EXPECT_TRUE(stateLocation("builds").empty());
  setStateDirectory(before);
}

}  // namespace
