// The one documentation claim the generated probe TU cannot make.
//
// ComposeApiDocProbes.cpp is generated from README.md and TYPOGRAPHY.md on
// every build and fails when a documented name no header declares: it
// proves every name the prose spells EXISTS. It cannot prove that a value
// an author meets in a profile reads as words rather than as blank space,
// because no document spells the phrase — the enumerator does.

#include "support/DocsTestSupport.h"

TEST(ComposeCache, EveryPromotionAndCacheStateReachesAnAuthorAsWords) {
  // An author reads these exactly when a scene is too slow, so a value
  // with no phrase turns "I do not know why this is slow" into blank
  // space under the node that is costing the frame.
  for (Composer::Promotion p :
       {Composer::Promotion::Cheap, Composer::Promotion::Warming,
        Composer::Promotion::Promoted, Composer::Promotion::AskedFor,
        Composer::Promotion::OptedOut, Composer::Promotion::Volatile,
        Composer::Promotion::Composited, Composer::Promotion::Transformed,
        Composer::Promotion::Filtered, Composer::Promotion::ReadsBackdrop,
        Composer::Promotion::TooBig, Composer::Promotion::SplitBaked})
    EXPECT_STRNE(Composer::promotionReason(p), "");

  // Every CacheState answers `cached()`, and only Live answers false: the
  // profile switches over this exhaustively, so a value added without a
  // case is a build break for whoever adds it rather than a silent
  // mislabel under a `default:`.
  for (Composer::CacheState s :
       {Composer::CacheState::Live, Composer::CacheState::Picture,
        Composer::CacheState::Texture, Composer::CacheState::Promoted,
        Composer::CacheState::SplitOwn, Composer::CacheState::Group}) {
    Composer::NodeCost row;
    row.cacheState = s;
    EXPECT_EQ(row.cached(), s != Composer::CacheState::Live);
  }

  // A fresh row refuses nothing: the mask is additive, so a reader who
  // sees a set bit knows the promoter set it.
  Composer::NodeCost row;
  EXPECT_EQ(row.refusals, 0u);
  EXPECT_FALSE(row.refused(Composer::Promotion::Volatile));
}
