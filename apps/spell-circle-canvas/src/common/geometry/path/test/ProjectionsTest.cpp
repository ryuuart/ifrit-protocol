/** @file
 * The sphere laid onto a plane: that every scheme comes back from where it
 * lands, that the middle of the map is the same size whichever scheme is
 * chosen, that a stereographic really does carry circles to circles (and
 * says so rather than approximating when it cannot), what the plate of an
 * astrolabe is, how the two cylindrical forms space their parallels, and
 * the three-angle turn that carries a star from one epoch to the next.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <glm/geometric.hpp>
#include <vector>

#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Projection.h"

using namespace sigil::geometry::path;

namespace {

/** The five schemes, each at a scale and a centre that suit it, so one
 *  loop can ask the questions every map must answer. */
std::vector<Projection> everyScheme() {
  return {
      {.scheme = Scheme::Stereographic, .centre = {.latDeg = 90}, .scale = 120},
      {.scheme = Scheme::Orthographic,
       .centre = {.lonDeg = 40, .latDeg = 20},
       .scale = 300},
      {.scheme = Scheme::AzimuthalEquidistant,
       .centre = {.lonDeg = 200, .latDeg = -35},
       .scale = 90},
      {.scheme = Scheme::Equirectangular,
       .centre = {.lonDeg = 296},
       .scale = 7},
      {.scheme = Scheme::Mercator, .centre = {.lonDeg = 10}, .scale = 7},
  };
}

/** Chaucer's two constants, and the plate that follows from them: the
 *  sphere seen from the south pole onto the plane of the equator, in units
 *  of the Tropic of Capricorn's radius, with right ascension read off the
 *  paper as an ordinary angle from +x. */
constexpr float kObliquity = 23.0f + 50.0f / 60.0f;
constexpr float kLatitude = 51.0f + 50.0f / 60.0f;
const float kEquator = std::tan(radians((90.0f - kObliquity) * 0.5f));

const Projection kPlate{.scheme = Scheme::Stereographic,
                        .centre = {.latDeg = 90},
                        .scale = kEquator * 0.5f,
                        .rollDeg = 90};
/** The zenith of that latitude, on the plate's own longitude — where the
 *  almucantars and every azimuth circle stand. */
const Spherical kZenith{.lonDeg = 90, .latDeg = kLatitude};

}  // namespace

TEST(Projections, EveryMapComesBackFromThePlaneItLandsOn) {
  for (const Projection& map : everyScheme()) {
    for (float lat = -80; lat <= 80; lat += 20) {
      for (float lon = 0; lon < 360; lon += 30) {
        const Spherical sky{lon, lat};
        // An orthographic folds the far hemisphere onto the near one, so
        // only what it can actually show has a way home.
        if (map.scheme == Scheme::Orthographic && map.angleFrom(sky) > 85)
          continue;
        const Spherical back = map.from(map.at(sky));
        EXPECT_NEAR(angleBetween(sky, back), 0.0f, 2e-3f)
            << "lon " << lon << " lat " << lat;
      }
    }
  }
}

TEST(Projections, OneRadianAtTheCentreIsTheScaleWhicheverSchemeIsChosen) {
  for (const Projection& map : everyScheme()) {
    EXPECT_NEAR(glm::length(map.at(map.centre)), 0.0f, 1e-4f);
    // The derivative every one of them shares at its own centre: a
    // hundredth of a degree of arc costs the same on all five, which is
    // what makes the scheme a field a caller can swap.
    const float step = 0.01f;
    EXPECT_NEAR(map.radiusAt(step) / radians(step), map.scale,
                map.scale * 1e-3f);
    // And it is odd about the centre, so a southward arc reads below it.
    EXPECT_NEAR(map.radiusAt(-step), -map.radiusAt(step), map.scale * 1e-4f);
  }
}

TEST(Projections, AStereographicCarriesACircleOnTheSphereToACircleOnThePlane) {
  // Do not take the property on trust from the closed form: sample the
  // circle on the SPHERE and put every point through the raw map, then ask
  // how far each one stands from the centre the closed form answers.
  const Spherical pole{.lonDeg = 33, .latDeg = 41};
  for (float arc : {12.0f, 55.0f, 90.0f, 128.0f}) {
    const std::optional<PlaneCircle> image = kPlate.circleOf(pole, arc);
    ASSERT_TRUE(image.has_value()) << "arc " << arc;
    for (float bearing = 0; bearing < 360; bearing += 5) {
      const glm::vec2 p = kPlate.at(offsetFrom(pole, bearing, arc));
      // Relative, because a circle that reaches toward the point the
      // projection is taken from is enormous beside the plate itself, and
      // an absolute bar would be a bar on its size rather than on its
      // roundness.
      EXPECT_NEAR(glm::length(p - image->centre), image->radius,
                  image->radius * 2e-5f)
          << "arc " << arc << " bearing " << bearing;
    }
    // And the way a maker without the closed form strikes that circle —
    // through three of its points — reaches the same one.
    const std::optional<PlaneCircle> struck =
        circleThrough(kPlate.at(offsetFrom(pole, 0, arc)),
                      kPlate.at(offsetFrom(pole, 120, arc)),
                      kPlate.at(offsetFrom(pole, 240, arc)));
    ASSERT_TRUE(struck.has_value());
    EXPECT_NEAR(glm::length(struck->centre - image->centre), 0.0f,
                image->radius * 2e-5f);
    EXPECT_NEAR(struck->radius, image->radius, image->radius * 2e-5f);
  }
}

TEST(Projections,
     ACircleThroughThePointTheProjectionIsTakenFromIsAStraightLine) {
  // The plate is taken from the south pole, so a great circle through the
  // celestial poles — the local meridian, whose pole stands due east on
  // the horizon — is a line and has no centre to answer with.
  const Spherical meridianPole = offsetFrom(kZenith, 90, 90);
  EXPECT_FALSE(kPlate.circleOf(meridianPole, 90).has_value());
  // Three points on one line decide no circle either.
  EXPECT_FALSE(circleThrough({0, 0}, {10, 5}, {40, 20}).has_value());
  EXPECT_TRUE(circleThrough({0, 0}, {10, 5}, {40, 21}).has_value());
  // Nor does a scheme that does not map circles to circles pretend to.
  const Projection globe{.scheme = Scheme::Orthographic, .scale = 200};
  EXPECT_FALSE(globe.circleOf({.latDeg = 30}, 20).has_value());
}

TEST(Projections, ThePlateOfAnAstrolabeStandsOnItsPoleAndItsHorizon) {
  // The pole is the middle of the plate, and a parallel of declination
  // stands at the radius the maker's own rule gives.
  EXPECT_NEAR(glm::length(kPlate.at({.lonDeg = 123, .latDeg = 90})), 0.0f,
              1e-5f);
  for (float dec : {-kObliquity, 0.0f, kObliquity, 66.5f}) {
    const float expected = kEquator * std::tan(radians((90.0f - dec) * 0.5f));
    EXPECT_NEAR(kPlate.radiusAt(90.0f - dec), expected, 1e-5f);
    // Right ascension is read off the plate as the angle itself, which is
    // what the roll is set for: at a pole north is not a direction.
    const glm::vec2 p = kPlate.at({.lonDeg = 30, .latDeg = dec});
    EXPECT_NEAR(std::atan2(p.y, p.x), radians(30.0f), 1e-4f);
    EXPECT_NEAR(glm::length(p), expected, 1e-5f);
  }
  // The horizon is the almucantar of altitude zero: a circle about the
  // zenith at a quarter turn, standing on the plate's own +y.
  const std::optional<PlaneCircle> horizon = kPlate.circleOf(kZenith, 90);
  ASSERT_TRUE(horizon.has_value());
  const float sinPhi = std::sin(radians(kLatitude));
  const float cosPhi = std::cos(radians(kLatitude));
  EXPECT_NEAR(horizon->centre.x, 0.0f, 1e-5f);
  EXPECT_NEAR(horizon->centre.y, kEquator * cosPhi / sinPhi, 1e-5f);
  EXPECT_NEAR(horizon->radius, kEquator / sinPhi, 1e-5f);
  // It crosses the equator due east and due west, which is what "the sun
  // rises east at the equinox" is on this instrument: the equator's own
  // side point is exactly the horizon's radius from its centre.
  EXPECT_NEAR(glm::length(glm::vec2{kEquator, 0} - horizon->centre),
              horizon->radius, 1e-5f);
  // The almucantars close in on the zenith as the altitude rises, and
  // every one of them stands on the same line — but NOT on the zenith's
  // own image: a conformal map carries a circle to a circle and does not
  // carry its centre to that circle's centre.
  const std::optional<PlaneCircle> high = kPlate.circleOf(kZenith, 20);
  ASSERT_TRUE(high.has_value());
  const glm::vec2 zenith = kPlate.at(kZenith);
  EXPECT_LT(high->radius, horizon->radius);
  EXPECT_NEAR(high->centre.x, 0.0f, 1e-5f);
  EXPECT_LT(glm::length(zenith - high->centre), high->radius);
  EXPECT_GT(glm::length(zenith - high->centre), 1e-3f);
  // The prime vertical — the azimuth circle through the east and west
  // points — passes through the equator's side point too.
  const std::optional<PlaneCircle> prime =
      kPlate.circleOf(offsetFrom(kZenith, 180, 90), 90);
  ASSERT_TRUE(prime.has_value());
  EXPECT_NEAR(prime->centre.x, 0.0f, 1e-5f);
  EXPECT_NEAR(kEquator * kEquator + prime->centre.y * prime->centre.y,
              prime->radius * prime->radius, 1e-5f);
}

TEST(Projections, AnOrthographicFoldsTheFarSideOntoTheNearAndEndsAtItsLimb) {
  const Projection globe{.scheme = Scheme::Orthographic, .scale = 200};
  EXPECT_NEAR(globe.radiusAt(90), 200.0f, 1e-3f);
  // The far side lands on top of the near side, which is what a globe
  // drawn as a disc does and why a caller culls by the angle from the
  // centre rather than by the point.
  const Spherical near{.lonDeg = 70, .latDeg = 10};
  const Spherical far{.lonDeg = 110, .latDeg = -10};
  EXPECT_LT(globe.angleFrom(near), 90.0f);
  EXPECT_GT(globe.angleFrom(far), 90.0f);
  EXPECT_NEAR(glm::length(globe.at({.lonDeg = 60}) - globe.at({.lonDeg = 120})),
              0.0f, 1e-3f);
  // Nothing stands outside the limb, however far round the sphere it is.
  for (float lon = 0; lon < 360; lon += 15)
    for (float lat = -90; lat <= 90; lat += 15)
      EXPECT_LE(glm::length(globe.at({lon, lat})), 200.0f + 1e-3f);
}

TEST(Projections, TheCylindricalFormsSpaceTheirParallelsByTheirOwnRule) {
  const Projection plate{.scheme = Scheme::Equirectangular, .scale = 100};
  const Projection chart{.scheme = Scheme::Mercator, .scale = 100};
  // Both lay their meridians down at one spacing: the abscissa is the same
  // ruler on either, and only the ordinate tells them apart.
  for (float lon = 10; lon <= 90; lon += 10) {
    EXPECT_NEAR(plate.at({.lonDeg = lon}).x, chart.at({.lonDeg = lon}).x,
                1e-3f);
    EXPECT_NEAR(plate.at({.lonDeg = lon}).x, radians(lon) * 100.0f, 1e-3f);
  }
  // The plate carrée's parallels are a ruler: every ten degrees is the
  // same distance, anywhere on the map.
  const float step = plate.radiusAt(10);
  for (int i = 1; i <= 8; ++i)
    EXPECT_NEAR(plate.radiusAt(10.0f * (float)i), step * (float)i, 1e-3f);
  // Mercator's are not: each step is longer than the one below it, and the
  // rate the ordinate grows at is the secant of the latitude it is read
  // at, which is the whole of what the scheme is.
  float previous = 0;
  for (int i = 1; i <= 8; ++i) {
    const float lat = 10.0f * (float)i;
    const float rise = chart.radiusAt(lat) - chart.radiusAt(lat - 10.0f);
    EXPECT_GT(rise, previous);
    const float rate =
        (chart.radiusAt(lat + 0.05f) - chart.radiusAt(lat - 0.05f)) /
        radians(0.1f);
    EXPECT_NEAR(rate, 100.0f / std::cos(radians(lat)), 0.5f);
    previous = rise;
  }
  // A chart's ordinate about a standard parallel of its own is measured
  // from that parallel and nowhere else.
  const Projection offset = chart.centredOn({.latDeg = 30});
  EXPECT_NEAR(offset.at({.latDeg = 30}).y, 0.0f, 1e-3f);
  EXPECT_NEAR(offset.at({.latDeg = 50}).y,
              chart.radiusAt(50) - chart.radiusAt(30), 1e-2f);
}

TEST(Projections, AMirroredMapIsTheSameMapReadTheOtherWayRound) {
  Projection outside = kPlate;
  outside.vantage = Vantage::Outside;
  for (float lon = 0; lon < 360; lon += 45) {
    const Spherical sky{lon, 20};
    const glm::vec2 a = kPlate.at(sky), b = outside.at(sky);
    // The mirror is taken about the map's own north line BEFORE the roll
    // turns it, so on this plate — whose roll stands north along +x — the
    // two readings are either side of the horizontal.
    EXPECT_NEAR(a.x, b.x, 1e-5f);
    EXPECT_NEAR(a.y, -b.y, 1e-5f);
    EXPECT_NEAR(angleBetween(sky, outside.from(b)), 0.0f, 2e-3f);
  }
}

TEST(Projections, ARollTurnsTheMapAndMovesNothingOnIt) {
  Projection turned = kPlate;
  turned.rollDeg = kPlate.rollDeg + 37.0f;
  const Spherical a{.lonDeg = 15, .latDeg = 40};
  const Spherical b{.lonDeg = 200, .latDeg = -10};
  EXPECT_NEAR(glm::length(kPlate.at(a) - kPlate.at(b)),
              glm::length(turned.at(a) - turned.at(b)), 1e-5f);
  EXPECT_NEAR(glm::length(turned.at(a)), glm::length(kPlate.at(a)), 1e-5f);
  const float before = std::atan2(kPlate.at(a).y, kPlate.at(a).x);
  const float after = std::atan2(turned.at(a).y, turned.at(a).x);
  EXPECT_NEAR(wrap(degrees(after - before), 360.0f), 37.0f, 1e-3f);
}

TEST(Projections, AStepAlongAGreatCircleLandsWhereItsBearingSaysItDoes) {
  // A quarter turn from the pole in any direction lands on the equator, at
  // the meridian the bearing names.
  for (float bearing = 0; bearing < 360; bearing += 30) {
    const Spherical p = offsetFrom({.latDeg = 90}, bearing, 90);
    EXPECT_NEAR(p.latDeg, 0.0f, 1e-3f);
    EXPECT_NEAR(angleBetween({.latDeg = 90}, p), 90.0f, 1e-3f);
  }
  // Due north is due north, and the distance is the arc that was asked
  // for however far round the sphere the start is.
  const Spherical from{.lonDeg = 217, .latDeg = -13};
  const Spherical north = offsetFrom(from, 0, 25);
  EXPECT_NEAR(north.lonDeg, from.lonDeg, 1e-3f);
  EXPECT_NEAR(north.latDeg, from.latDeg + 25.0f, 1e-3f);
  EXPECT_NEAR(angleBetween(from, offsetFrom(from, 123, 7)), 7.0f, 1e-3f);
}

TEST(Rotations, ATurnComposesUndoesAndHoldsItsOwnAxis) {
  const Rotation a = Rotation::aboutZ(37);
  const Rotation b = Rotation::aboutY(-14);
  const Spherical star{.lonDeg = 88, .latDeg = 24};
  // then() is "this, and then that", whichever way the matrices multiply.
  EXPECT_NEAR(angleBetween(a.then(b)(star), b(a(star))), 0.0f, 1e-3f);
  EXPECT_NEAR(angleBetween(a.then(a.inverse())(star), star), 0.0f, 1e-3f);
  // A turn about an axis leaves the two directions on it where they are,
  // and turns everything else by the angle it was given.
  EXPECT_NEAR(angleBetween(a({.latDeg = 90}), {.latDeg = 90}), 0.0f, 1e-3f);
  EXPECT_NEAR(a({.lonDeg = 10}).lonDeg, 47.0f, 1e-3f);
  EXPECT_NEAR(angleBetween(b({.lonDeg = 90}), {.lonDeg = 90}), 0.0f, 1e-3f);
}

TEST(Rotations, TheThreeAngleFormCarriesAStarFromOneEpochToTheNext) {
  // An epoch-to-epoch precession is published as three angles and applied
  // as one turn of the sphere. These are the angles a fourteenth-century
  // astrolabe's rete was cut to, and the twelve stars on it: each J2000
  // position, turned, lands where the maker put it.
  const Rotation toEpoch = Rotation::zyz(-4.3055f, 3.7543f, -4.3155f);
  const struct {
    Spherical j2000, cut;
  } stars[] = {
      {{74.248f, 33.166f}, {63.397f, 31.809f}},
      {{81.573f, 28.608f}, {70.999f, 27.717f}},
      {{101.287f, -16.716f}, {93.767f, -16.224f}},
      {{206.885f, 49.313f}, {200.116f, 52.757f}},
      {{310.358f, 45.280f}, {304.629f, 42.994f}},
      {{2.097f, 29.090f}, {353.549f, 25.339f}},
  };
  for (const auto& star : stars) {
    EXPECT_NEAR(angleBetween(toEpoch(star.j2000), star.cut), 0.0f, 2e-3f)
        << "star at " << star.j2000.lonDeg;
    // And the turn undoes: the catalogue position comes back from the
    // cut one, which is what a chart stepping through epochs relies on.
    EXPECT_NEAR(angleBetween(toEpoch.inverse()(star.cut), star.j2000), 0.0f,
                2e-3f);
  }
  // Nothing about the turn is in the projection: the same plate reads
  // either epoch, and what moved is the sky.
  const glm::vec2 then = kPlate.at(toEpoch({74.248f, 33.166f}));
  const glm::vec2 now = kPlate.at({74.248f, 33.166f});
  EXPECT_GT(glm::length(then - now), 0.01f);
}
