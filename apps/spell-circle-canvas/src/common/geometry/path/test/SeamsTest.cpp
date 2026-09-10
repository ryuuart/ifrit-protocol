/** @file
 * The two comparable seams a mark is deviated and widened through — the
 * shaper that bends one and the profile that says how wide it is at each
 * point along it — and the band a width law cuts.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
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

TEST(Profile, ThePresetsAreTheLawsEveryOtherIsDefinedAgainst) {
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

TEST(Profile, ATaperRunsLinearlyBetweenTwoSignedEnds) {
  const Profile p = profile::taper(10.0f, 2.0f);
  EXPECT_FLOAT_EQ(p.across(0.0f), 10.0f);
  EXPECT_FLOAT_EQ(p.across(0.5f), 6.0f);
  EXPECT_FLOAT_EQ(p.across(1.0f), 2.0f);
  // Clamped past its own ends, so a caller off [0,1] cannot widen it.
  EXPECT_FLOAT_EQ(p.across(-1.0f), 10.0f);
  EXPECT_FLOAT_EQ(p.across(2.0f), 2.0f);
  // max() is the widest reach, which is what every cull is sized from —
  // and a signed taper reaches on BOTH sides.
  EXPECT_FLOAT_EQ(p.max(), 10.0f);
  EXPECT_FLOAT_EQ(profile::taper(-8.0f, 3.0f).max(), 8.0f);
  EXPECT_TRUE(profile::taper(4, 0) == profile::taper(4, 0));
  EXPECT_FALSE(profile::taper(4, 0) == profile::taper(4, 1));
  EXPECT_FALSE(profile::taper(6, 6) == profile::offset(6));
}

TEST(Profile, StepsHoldOneWidthPerSpanAndDoNotInterpolate) {
  // Three widths against two boundaries: the last width holds to the end.
  const Profile p = profile::spans({0.25f, 0.75f}, {3.0f, 9.0f, 5.0f});
  EXPECT_FLOAT_EQ(p.across(0.0f), 3.0f);
  EXPECT_FLOAT_EQ(p.across(0.2f), 3.0f);
  EXPECT_FLOAT_EQ(p.across(0.25f), 9.0f);  // the boundary opens the next span
  EXPECT_FLOAT_EQ(p.across(0.5f), 9.0f);
  EXPECT_FLOAT_EQ(p.across(0.9f), 5.0f);
  EXPECT_FLOAT_EQ(p.across(1.0f), 5.0f);
  EXPECT_FLOAT_EQ(p.max(), 9.0f);
  // A short table reads the last width there is rather than running off.
  EXPECT_FLOAT_EQ(profile::spans({0.5f}, {2.0f}).across(0.9f), 2.0f);
  EXPECT_FLOAT_EQ(profile::spans({}, {}).across(0.5f), 0.0f);
  EXPECT_TRUE(profile::spans({0.5f}, {1, 2}) == profile::spans({0.5f}, {1, 2}));
  EXPECT_FALSE(profile::spans({0.5f}, {1, 2}) ==
               profile::spans({0.6f}, {1, 2}));
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

// ---------------------------------------------------------------------------
// The band SWEPT rather than zipped.

/** A right angle, so a join has something to close. */
SkPath elbow() {
  SkPathBuilder b;
  b.moveTo(40, 40);
  b.lineTo(160, 40);
  b.lineTo(160, 160);
  return b.detach();
}

/** How many pixels of a 300 x 300 field @p path inks, aliased. */
int inked(const SkPath& path) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(300, 300));
  bm.eraseColor(SK_ColorBLACK);
  SkCanvas canvas(bm);
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setColor(SK_ColorWHITE);
  canvas.drawPath(path, paint);
  int on = 0;
  for (int y = 0; y < 300; ++y)
    for (int x = 0; x < 300; ++x)
      if (bm.getColor(x, y) == SK_ColorWHITE) ++on;
  return on;
}

TEST(Band, ASweptBandClosesItsTurnTheWayItsJoinSays) {
  const SweepWidth constant = [](const SweepStation&) { return 24.0f; };
  const SkPath spine = elbow();
  const SkPath mitre = sweptRegion(spine, constant, {.join = SweepJoin::Miter});
  const SkPath round = sweptRegion(spine, constant, {.join = SweepJoin::Round});
  const SkPath bevel = sweptRegion(spine, constant, {.join = SweepJoin::Bevel});
  ASSERT_FALSE(mitre.isEmpty());
  // The point reaches furthest, the chord least, the arc between them —
  // the same ordering a stroke's three joins have, because it is the same
  // decision. Nothing else about the band differs, so the counts differ by
  // the corner alone.
  const int m = inked(mitre), r = inked(round), b = inked(bevel);
  EXPECT_GT(m, r);
  EXPECT_GT(r, b);
  // …and none of them punches a hole on the INSIDE of the bend, which is
  // what a reversed piece under the winding fill would do.
  for (const SkPath& band : {mitre, round, bevel}) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(300, 300));
    bm.eraseColor(SK_ColorBLACK);
    SkCanvas canvas(bm);
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);
    canvas.drawPath(band, paint);
    EXPECT_EQ(bm.getColor(152, 48), SK_ColorWHITE) << "a hole inside the bend";
  }
  // A run with no turn has no join to make, so all three are one band.
  const SkPath straight = SkPath::Line({40, 40}, {200, 40});
  EXPECT_EQ(inked(sweptRegion(straight, constant, {.join = SweepJoin::Miter})),
            inked(sweptRegion(straight, constant, {.join = SweepJoin::Bevel})));
}

TEST(Band, ASweptWidthMayBeKeyedOnDirectionWhereAProfileCannot) {
  // The reason a band is swept at all: a pen NIB is widest where the spine
  // crosses it and thinnest along it, which is a function of the TANGENT —
  // no profile keyed on arc length can say it. The elbow's two legs run at
  // right angles, so one is fat and the other thin.
  const SweepWidth nib = [](const SweepStation& at) {
    const float a = std::atan2(at.tangent.y(), at.tangent.x());
    return 4.0f + 20.0f * std::abs(std::sin(a));
  };
  const SkPath band = sweptRegion(elbow(), nib);
  ASSERT_FALSE(band.isEmpty());
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(300, 300));
  bm.eraseColor(SK_ColorBLACK);
  SkCanvas canvas(bm);
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setColor(SK_ColorWHITE);
  canvas.drawPath(band, paint);
  int wide = 0, thin = 0;
  for (int y = 0; y < 300; ++y)
    if (bm.getColor(100, y) == SK_ColorWHITE) ++thin;  // across the flat leg
  for (int x = 0; x < 300; ++x)
    if (bm.getColor(x, 100) == SK_ColorWHITE) ++wide;  // across the upright
  EXPECT_LE(thin, 6);
  EXPECT_GE(wide, 20);

  // A law that answers nothing sweeps nothing, and a non-finite answer
  // pinches to the spine rather than deleting the whole mark.
  EXPECT_TRUE(sweptRegion(elbow(), {}).isEmpty());
  const SkPath pinched = sweptRegion(elbow(), [](const SweepStation& at) {
    return at.fraction < 0.5f ? 20.0f : std::nanf("");
  });
  EXPECT_FALSE(pinched.isEmpty());
  EXPECT_TRUE(pinched.isFinite());
}

}  // namespace
