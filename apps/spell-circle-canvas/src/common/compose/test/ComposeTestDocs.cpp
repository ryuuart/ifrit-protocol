// The one profile claim the generated probe TU cannot make.
//
// ComposeApiDocProbes.cpp is generated from README.md, TYPOGRAPHY.md and the
// chapters under reference/ on
// every build and fails when a documented name no header declares: it
// proves every name the prose spells EXISTS. It cannot prove that a value
// an author meets in a profile answers the question the profile is read
// for, because no document spells the answer — the enumerator does.

#include "support/DocsTestSupport.h"

TEST(ComposeCache, EveryCacheStateAnswersWhetherItIsCached) {
  // Every CacheState answers `cached()`, and only Live answers false: the
  // profile switches over this exhaustively, so a value added without a
  // case is a build break for whoever adds it rather than a silent
  // mislabel under a `default:`.
  for (Composer::CacheState s :
       {Composer::CacheState::Live, Composer::CacheState::Picture,
        Composer::CacheState::Texture, Composer::CacheState::Group}) {
    Composer::NodeCost row;
    row.cacheState = s;
    EXPECT_EQ(row.cached(), s != Composer::CacheState::Live);
  }
}
