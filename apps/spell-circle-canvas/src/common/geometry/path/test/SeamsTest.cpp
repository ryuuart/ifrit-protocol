/** @file
 * The two comparable seams a mark is deviated and widened through — the
 * shaper that bends one and the profile that says how wide it is at each
 * point along it — and the band a width law cuts.
 */

#include <gtest/gtest.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>

#include <cmath>

#include "sigilgeometry/path/Band.h"
#include "sigilgeometry/path/Profile.h"
#include "sigilgeometry/path/Shaper.h"

using namespace sigil::geometry::path;

namespace {

// ---------------------------------------------------------------------------
// The shaper seam.

namespace {
struct NudgeX {
  float dx = 0;
  float bleed() const { return std::abs(dx); }
  SkPath shape(const SkPath& p) const {
    return p.makeTransform(SkMatrix::Translate(dx, 0));
  }
  bool operator==(const NudgeX&) const = default;
};
struct Identity {
  SkPath shape(const SkPath& p) const { return p; }
  bool operator==(const Identity&) const = default;
};
}  // namespace

TEST(PathShaper, ComparesByTheHeldSchemeAndItsParameters) {
  EXPECT_TRUE(Shaper(NudgeX{3}) == Shaper(NudgeX{3}));
  EXPECT_FALSE(Shaper(NudgeX{3}) == Shaper(NudgeX{4}));
  // Two different schemes are never equal, whatever they do.
  EXPECT_FALSE(Shaper(NudgeX{0}) == Shaper(Identity{}));
  // The empty shaper is reflexive, or every holder patches forever.
  EXPECT_TRUE(Shaper() == Shaper());
  EXPECT_FALSE(Shaper() == Shaper(Identity{}));
}

TEST(PathShaper, BleedIsReadOffTheSchemeAndIsZeroWhenNotDeclared) {
  EXPECT_FLOAT_EQ(Shaper(NudgeX{-5}).bleed(), 5.0f);
  EXPECT_FLOAT_EQ(Shaper(Identity{}).bleed(), 0.0f);
  // An empty shaper passes its path through untouched.
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(10, 10));
  const SkPath src = b.detach();
  EXPECT_EQ(Shaper().shape(src), src);
  EXPECT_EQ(Shaper(NudgeX{2}).shape(src).getBounds().left(), 2.0f);
}

// ---------------------------------------------------------------------------
// The profile seam.

namespace {
struct Taper {
  float peak = 10;
  float across(float along) const { return peak * (1.0f - along); }
  float max() const { return peak; }
  bool operator==(const Taper&) const = default;
};
struct PxTaper {
  static constexpr bool alongIsPx = true;
  float across(float alongPx) const { return alongPx * 0.1f; }
  float max() const { return 100.0f; }
  bool operator==(const PxTaper&) const = default;
};
}  // namespace

TEST(Profile, TheTwoPresetsAreTheLawsEveryOtherIsDefinedAgainst) {
  EXPECT_FLOAT_EQ(profile::self().across(0.5f), 0.0f);
  EXPECT_FLOAT_EQ(profile::self().max(), 0.0f);
  EXPECT_FLOAT_EQ(profile::offset(-7.0f).across(0.5f), -7.0f);
  // max() is a REACH, so it is the magnitude — cull is sized from it.
  EXPECT_FLOAT_EQ(profile::offset(-7.0f).max(), 7.0f);
  EXPECT_TRUE(profile::offset(4) == profile::offset(4));
  EXPECT_FALSE(profile::offset(4) == profile::offset(5));
  EXPECT_FALSE(profile::offset(0) == profile::self());
  EXPECT_TRUE(Profile() == Profile());
  EXPECT_FALSE(Profile() == profile::self());
}

TEST(Profile, APxKeyedLawIsConvertedOnceByTheSeam) {
  const Profile fraction = Taper{10};
  const Profile px = PxTaper{};
  EXPECT_FALSE(fraction.keyedInPx());
  EXPECT_TRUE(px.keyedInPx());
  // acrossAt is the one call a measured consumer makes: a fraction-keyed
  // law ignores the length, a px-keyed one is handed along * length.
  EXPECT_FLOAT_EQ(fraction.acrossAt(0.25f, 200.0f), 7.5f);
  EXPECT_FLOAT_EQ(px.acrossAt(0.25f, 200.0f), 5.0f);
}

// ---------------------------------------------------------------------------
// The band a width law cuts.

TEST(Band, AConstantProfileRidesParallelsCornerRepair) {
  SkPathBuilder b;
  b.addRect(SkRect::MakeXYWH(0, 0, 100, 60));
  const SkPath spine = b.detach();
  // Positive across is LEFT of travel, which on Skia's clockwise rect is
  // outside it: the rail's bounds grow by the offset on every side.
  const SkPath out = profileOffset(spine, profile::offset(6.0f));
  EXPECT_FALSE(out.isEmpty());
  EXPECT_LE(out.getBounds().left(), -5.0f);
  EXPECT_GE(out.getBounds().right(), 105.0f);
  // A zero profile is the boundary itself, handed back untouched.
  EXPECT_EQ(profileOffset(spine, profile::self()), spine);
}

TEST(Band, TheRegionIsBoundedByTheWidthAndEmptyWithoutOne) {
  SkPathBuilder b;
  b.moveTo(0, 50);
  b.lineTo(200, 50);
  const SkPath spine = b.detach();
  const SkPath centred = bandRegion(spine, profile::offset(20.0f));
  ASSERT_FALSE(centred.isEmpty());
  // Centred: half the width each side of the spine.
  EXPECT_NEAR(centred.getBounds().top(), 40.0f, 1.0f);
  EXPECT_NEAR(centred.getBounds().bottom(), 60.0f, 1.0f);
  // Outward puts the whole width on one side.
  const SkPath outward =
      bandRegion(spine, profile::offset(20.0f), Formation::Outward);
  ASSERT_FALSE(outward.isEmpty());
  EXPECT_NEAR(outward.getBounds().height(), 20.0f, 1.0f);
  // A profile that is zero everywhere sweeps nothing.
  EXPECT_TRUE(bandRegion(spine, profile::self()).isEmpty());
}

}  // namespace
