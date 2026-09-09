/** @file
 * The cloud: a rate produces the count it promises over a span, a burst
 * arrives at once, every birth attribute lands inside the range it was
 * drawn from, a mouth puts its births where its shape says, lifetimes
 * expire and the set compacts, a consumer's attribute rides through a death
 * with the particle it belongs to, an attribute drifts at its own rate and
 * stops at its own bound, a weight is drawn from a range unless it is one
 * number and then costs no word, and the same seed twice is the same
 * cloud.
 */

#include <gtest/gtest.h>
#include <sigilmotion/physics/Physics.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "support/StandsAlone.h"

using namespace sigil::motion::physics;
namespace chance = sigil::core::chance;

namespace {

/** A mouth on a segment throwing a cone upwards, with the three attributes a
 *  drawing usually wants. */
Emitter fountain() {
  return Emitter{
      .from = EmitFrom::Segment,
      .at = {100.0f, 200.0f},
      .along = {1.0f, 0.0f},
      .size = {20.0f, 0.0f},
      .aim = {0.0f, -1.0f},
      .cone = 0.4f,
      .speed = {.mean = 60.0f, .variation = 12.0f},
      .attributes = {{std::string(kLife), {.mean = 3.0f, .variation = 1.0f}},
                     {"size", {.mean = 4.0f, .variation = 1.5f}},
                     {"heat", {.mean = 0.5f, .variation = 0.5f}}},
      .rate = 120.0f};
}

}  // namespace

TEST(Particles, ARateProducesTheCountItPromisesOverASpan) {
  Emitter mouth = fountain();
  chance::Stream stream = chance::Stream::pcg(7);
  Particles cloud;

  // Two hundred steps of a sixtieth at 120 a second is four hundred, and
  // the fraction each step leaves over is what makes it exact rather
  // than two per step by luck of the arithmetic.
  size_t born = 0;
  for (int step = 0; step < 200; ++step)
    born += mouth.emit(cloud, stream, 1.0f / 60.0f);
  EXPECT_EQ(born, 400u);
  EXPECT_EQ(cloud.size(), 400u);

  // A rate finer than one birth per step still arrives at that rate: one
  // every five steps, and none of them lost to the fraction.
  Emitter trickle = fountain();
  trickle.rate = 12.0f;
  Particles slow;
  size_t few = 0;
  for (int step = 0; step < 100; ++step)
    few += trickle.emit(slow, stream, 1.0f / 60.0f);
  EXPECT_EQ(few, 20u);
}

TEST(Particles, ABurstArrivesAtOnceWhateverTheRateSays) {
  const Emitter mouth = fountain();
  chance::Stream stream = chance::Stream::pcg(3);
  Particles cloud;

  EXPECT_EQ(mouth.burst(cloud, stream, 250), 250u);
  EXPECT_EQ(cloud.size(), 250u);
  // The rate said nothing and the carry is untouched: a burst is a
  // consumer's own law, not a span of time.
  EXPECT_EQ(mouth.carry, 0.0f);
  EXPECT_EQ(mouth.burst(cloud, stream, 0), 0u);
  EXPECT_EQ(cloud.size(), 250u);
}

TEST(Particles, EveryBirthAttributeLandsInsideItsOwnRange) {
  const Emitter mouth = fountain();
  chance::Stream stream = chance::Stream::pcg(11);
  Particles cloud;
  mouth.burst(cloud, stream, 4000);

  const std::vector<float>& size = cloud.attribute("size").values;
  const std::vector<float>& heat = cloud.attribute("heat").values;
  float slowest = 1e9f, fastest = 0.0f, widest = 0.0f;
  for (size_t i = 0; i < cloud.size(); ++i) {
    // The mouth is a segment twenty long about x = 100, across nothing:
    // every birth is on it.
    EXPECT_GE(cloud.points.position[i].x, 80.0f);
    EXPECT_LE(cloud.points.position[i].x, 120.0f);
    EXPECT_FLOAT_EQ(cloud.points.position[i].y, 200.0f);

    EXPECT_GE(cloud.life[i], 2.0f);
    EXPECT_LE(cloud.life[i], 4.0f);
    EXPECT_GE(size[i], 2.5f);
    EXPECT_LE(size[i], 5.5f);
    EXPECT_GE(heat[i], 0.0f);
    EXPECT_LE(heat[i], 1.0f);
    EXPECT_EQ(cloud.age[i], 0.0f);

    const float speed = cloud.points.velocity[i].length();
    slowest = std::min(slowest, speed);
    fastest = std::max(fastest, speed);
    // Every birth is inside the cone: the angle off the aim, which
    // points along negative y.
    widest = std::max(
        widest, std::acos(std::clamp(-cloud.points.velocity[i].y / speed, -1.0f,
                                     1.0f)));
  }
  EXPECT_GE(slowest, 48.0f);
  EXPECT_LE(fastest, 72.0f);
  EXPECT_LE(widest, 0.4f);
  // The cone is thrown to BOTH sides and the whole of it is used: over
  // four thousand draws the widest is within a hundredth of the half
  // angle, and the mean heading is the aim.
  EXPECT_GT(widest, 0.39f);
  float sideways = 0.0f;
  for (size_t i = 0; i < cloud.size(); ++i)
    sideways += cloud.points.velocity[i].x;
  EXPECT_LT(std::abs(sideways / (float)cloud.size()), 1.0f);
}

TEST(Particles, EachMouthPutsItsBirthsWhereItsShapeSays) {
  chance::Stream stream = chance::Stream::pcg(5);

  Emitter spot{.at = {10.0f, -4.0f}};
  Particles one;
  spot.burst(one, stream, 50);
  for (size_t i = 0; i < one.size(); ++i) {
    EXPECT_FLOAT_EQ(one.points.position[i].x, 10.0f);
    EXPECT_FLOAT_EQ(one.points.position[i].y, -4.0f);
  }

  // A box, in a frame turned a quarter turn: its first half extent
  // measures along `along`, which is straight down.
  Emitter box{
      .from = EmitFrom::Box, .along = {0.0f, 1.0f}, .size = {30.0f, 5.0f}};
  Particles rectangle;
  box.burst(rectangle, stream, 2000);
  float tallest = 0.0f, broadest = 0.0f;
  for (size_t i = 0; i < rectangle.size(); ++i) {
    tallest = std::max(tallest, std::abs(rectangle.points.position[i].y));
    broadest = std::max(broadest, std::abs(rectangle.points.position[i].x));
  }
  EXPECT_LE(tallest, 30.0f);
  EXPECT_GT(tallest, 29.0f);
  EXPECT_LE(broadest, 5.0f);
  EXPECT_GT(broadest, 4.9f);

  // A ring is the shell alone; a disc fills it, and evenly over the AREA
  // — half of a disc's births stand outside the radius that halves it,
  // which is the radius over root two.
  Emitter ring{.from = EmitFrom::Ring, .size = {40.0f, 0.0f}};
  Particles shell;
  ring.burst(shell, stream, 500);
  for (size_t i = 0; i < shell.size(); ++i)
    EXPECT_NEAR(shell.points.position[i].length(), 40.0f, 0.01f);

  Emitter disc{.from = EmitFrom::Disc, .size = {40.0f, 0.0f}};
  Particles filled;
  disc.burst(filled, stream, 4000);
  size_t outer = 0;
  for (size_t i = 0; i < filled.size(); ++i) {
    const float reach = filled.points.position[i].length();
    EXPECT_LE(reach, 40.0f);
    if (reach > 40.0f / std::sqrt(2.0f)) ++outer;
  }
  EXPECT_NEAR((double)outer / (double)filled.size(), 0.5, 0.03);
}

TEST(Particles, LifetimesExpireAndTheSetCompacts) {
  Particles cloud;
  for (int i = 0; i < 10; ++i) cloud.add({(float)i, 0}, {}, 1.0f + (float)i);

  EXPECT_EQ(cloud.size(), 10u);
  cloud.live(0.5f);
  EXPECT_EQ(cloud.reap(), 0u);

  // Five and a half of the way through: the five whose lifetimes are at
  // or under it are gone, and every attribute is the length of what is left.
  cloud.live(5.0f);
  EXPECT_EQ(cloud.reap(), 5u);
  EXPECT_EQ(cloud.size(), 5u);
  EXPECT_EQ(cloud.age.size(), 5u);
  EXPECT_EQ(cloud.life.size(), 5u);
  EXPECT_EQ(cloud.points.position.size(), 5u);
  EXPECT_EQ(cloud.points.velocity.size(), 5u);
  for (size_t i = 0; i < cloud.size(); ++i) EXPECT_GT(cloud.life[i], 5.5f);

  // A lifetime of nothing never expires, which is the cloud nobody gave
  // one to.
  Particles forever;
  forever.add({0, 0});
  forever.live(1.0e6f);
  EXPECT_EQ(forever.reap(), 0u);
  EXPECT_FALSE(forever.expired(0));

  // A consumer's own rule is spelled beside the age one rather than
  // instead of it.
  Particles fallen;
  for (int i = 0; i < 6; ++i) fallen.add({0, (float)i}, {}, 10.0f);
  fallen.live(11.0f);
  fallen.add({0, -1.0f}, {}, 100.0f);
  EXPECT_EQ(fallen.reap([&](size_t i) {
    return fallen.expired(i) || fallen.points.position[i].y > 3.0f;
  }),
            6u);
  EXPECT_EQ(fallen.size(), 1u);
  EXPECT_FLOAT_EQ(fallen.points.position[0].y, -1.0f);
}

TEST(Particles, ANamedLaneRidesThroughADeathWithItsParticle) {
  Particles cloud;
  std::vector<float>& tag = cloud.attribute("tag").values;
  for (int i = 0; i < 8; ++i) {
    const size_t index = cloud.add({(float)i, 0}, {}, i % 2 == 0 ? 1.0f : 9.0f);
    // The attribute says what its particle's position says, so an attribute
    // that came loose from its point set is readable as a mismatch rather than
    // as a plausible number.
    cloud.attribute("tag").values[index] = (float)i * 10.0f;
  }
  EXPECT_EQ(tag.size(), 8u);

  cloud.live(2.0f);
  EXPECT_EQ(cloud.reap(), 4u);
  EXPECT_EQ(cloud.size(), 4u);

  const std::vector<float>& kept = cloud.attribute("tag").values;
  EXPECT_EQ(kept.size(), 4u);
  for (size_t i = 0; i < cloud.size(); ++i)
    EXPECT_FLOAT_EQ(kept[i], cloud.points.position[i].x * 10.0f);

  // An attribute asked for after the set is grown is the set's length, at
  // zero, and one nobody named at all is nothing.
  EXPECT_EQ(cloud.attribute("late").values.size(), 4u);
  EXPECT_EQ(cloud.attribute("late").values[0], 0.0f);
  const Particles& reading = cloud;
  EXPECT_EQ(reading.attribute("nobody"), nullptr);
  EXPECT_NE(reading.attribute("tag"), nullptr);

  // Clearing keeps the attributes themselves: a cloud emptied and regrown is
  // the same cloud, with the same rates and the same bounds on it.
  cloud.attribute("tag").rate = -1.0f;
  cloud.clear();
  EXPECT_EQ(cloud.size(), 0u);
  EXPECT_EQ(cloud.attribute("tag").values.size(), 0u);
  EXPECT_FLOAT_EQ(cloud.attribute("tag").rate, -1.0f);
}

TEST(Particles, AnAttributeFilledWithANumberCostsNoDraw) {
  // Which of many mouths threw a particle is the case this is for: two
  // emitters into one cloud, each stamping its own number, and neither
  // spending a word of the stream on it.
  Emitter first = fountain();
  first.fixed = {{"mouth", 1.0f}};
  Emitter second = fountain();
  second.at = {400.0f, 200.0f};
  second.fixed = {{"mouth", 2.0f}};

  chance::Stream stream = chance::Stream::xorshift(0x1982u);
  Particles cloud;
  first.burst(cloud, stream, 300);
  second.burst(cloud, stream, 200);
  EXPECT_EQ(stream.drawn(), 500u * 7u);

  const std::vector<float>& mouth = cloud.attribute("mouth").values;
  for (size_t i = 0; i < cloud.size(); ++i)
    EXPECT_FLOAT_EQ(mouth[i],
                    cloud.points.position[i].x < 300.0f ? 1.0f : 2.0f);

  // And it rides through a death like any other attribute.
  cloud.live(10.0f);
  EXPECT_EQ(cloud.reap([&](size_t i) { return mouth[i] == 1.0f; }), 300u);
  EXPECT_EQ(cloud.size(), 200u);
  for (size_t i = 0; i < cloud.size(); ++i) EXPECT_FLOAT_EQ(mouth[i], 2.0f);
}

TEST(Particles, ALaneDriftsAtItsOwnRateAndStopsAtItsOwnBound) {
  Particles cloud;
  Attribute& fade = cloud.attribute("red");
  fade.rate = -0.25f;
  fade.least = 0.0f;
  Attribute& grow = cloud.attribute("size");
  grow.rate = 2.0f;
  grow.most = 5.0f;

  for (int i = 0; i < 3; ++i) cloud.add({0, 0}, {}, 100.0f);
  for (size_t i = 0; i < cloud.size(); ++i) {
    cloud.attribute("red").values[i] = 1.0f;
    cloud.attribute("size").values[i] = 1.0f;
  }

  cloud.live(2.0f);
  EXPECT_FLOAT_EQ(cloud.attribute("red").values[0], 0.5f);
  EXPECT_FLOAT_EQ(cloud.attribute("size").values[0], 5.0f);
  EXPECT_FLOAT_EQ(cloud.age[0], 2.0f);

  // Past the bound and it stops there rather than going through: a
  // channel that ran negative is a number no renderer can read.
  cloud.live(10.0f);
  EXPECT_FLOAT_EQ(cloud.attribute("red").values[0], 0.0f);
  EXPECT_FLOAT_EQ(cloud.attribute("size").values[0], 5.0f);
  EXPECT_FLOAT_EQ(cloud.age[0], 12.0f);
}

TEST(Particles, TheSameSeedTwiceIsTheSameCloud) {
  const Emitter mouth = fountain();
  auto run = [&](uint64_t seed) {
    chance::Stream stream = chance::Stream::xorshift(seed);
    Particles cloud;
    mouth.burst(cloud, stream, 500);
    return cloud;
  };
  const Particles first = run(0x1982u);
  const Particles again = run(0x1982u);
  const Particles other = run(0x1983u);

  EXPECT_EQ(first.points.position, again.points.position);
  EXPECT_EQ(first.points.velocity, again.points.velocity);
  EXPECT_EQ(first.life, again.life);
  EXPECT_EQ(first.attribute("size")->values, again.attribute("size")->values);
  EXPECT_NE(first.points.position, other.points.position);

  // A birth costs the same number of words whatever the numbers on it
  // are: a place on the segment, the angle off the aim, the side of it,
  // the speed and the mouth's three attributes are seven draws, and a
  // variation of nothing still spends one.
  chance::Stream counting = chance::Stream::xorshift(0x1982u);
  Particles counted;
  mouth.burst(counted, counting, 500);
  EXPECT_EQ(counting.drawn(), 500u * 7u);

  Emitter steady = mouth;
  for (BirthAttribute& birth : steady.attributes) birth.drawn.variation = 0.0f;
  steady.cone = 0.0f;
  steady.speed.variation = 0.0f;
  chance::Stream constant = chance::Stream::xorshift(0x1982u);
  Particles same;
  steady.burst(same, constant, 500);
  EXPECT_EQ(constant.drawn(), 500u * 7u);
  for (size_t i = 0; i < same.size(); ++i) {
    EXPECT_FLOAT_EQ(same.life[i], 3.0f);
    EXPECT_FLOAT_EQ(same.points.velocity[i].x, 0.0f);
    EXPECT_FLOAT_EQ(same.points.velocity[i].y, -60.0f);
  }
}

TEST(Particles, AWeightIsDrawnLikeEveryOtherBirthAttribute) {
  Emitter heavy = fountain();
  heavy.mass = {.mean = 4.0f, .variation = 1.5f};

  chance::Stream stream = chance::Stream::pcg(9001);
  Particles cloud;
  heavy.burst(cloud, stream, 4000);

  double total = 0.0;
  float lightest = 1e9f, heaviest = -1e9f;
  for (const float weight : cloud.points.mass) {
    EXPECT_GE(weight, 2.5f);
    EXPECT_LE(weight, 5.5f);
    total += weight;
    lightest = std::min(lightest, weight);
    heaviest = std::max(heaviest, weight);
  }
  EXPECT_NEAR(total / (double)cloud.size(), 4.0, 0.1);
  // The range is USED and not merely respected: a draw lands close to
  // each end of it.
  EXPECT_LT(lightest, 2.6f);
  EXPECT_GT(heaviest, 5.4f);

  // A weight that varies costs one word beside the seven a birth already
  // spends, and it is spent between the speed and the attributes.
  EXPECT_EQ(stream.drawn(), 4000u * 8u);

  // The scale multiplies the whole draw, as it does everywhere else.
  Emitter scaled = fountain();
  scaled.mass = {.mean = 4.0f, .variation = 1.5f, .scale = 0.25f};
  chance::Stream same = chance::Stream::pcg(9001);
  Particles smaller;
  scaled.burst(smaller, same, 100);
  for (size_t i = 0; i < smaller.size(); ++i)
    EXPECT_FLOAT_EQ(smaller.points.mass[i], cloud.points.mass[i] * 0.25f);
}

TEST(Particles, AWeightThatDoesNotVaryCostsNoWordAndMovesNoCloud) {
  const Emitter plain = fountain();
  auto run = [](const Emitter& mouth) {
    chance::Stream stream = chance::Stream::xorshift(0x1982u);
    Particles cloud;
    mouth.burst(cloud, stream, 500);
    return std::pair<Particles, uint64_t>{cloud, stream.drawn()};
  };

  // The weight nobody stated: one for every birth, and seven words a
  // birth exactly as the attribute list alone asks for.
  const auto [ones, wordsForOnes] = run(plain);
  EXPECT_EQ(wordsForOnes, 500u * 7u);
  for (const float weight : ones.points.mass) EXPECT_FLOAT_EQ(weight, 1.0f);

  // A weight stated as a middle with no raggedness: still no word, so
  // every position, velocity and attribute of the cloud is where it was.
  Emitter stated = plain;
  stated.mass = {.mean = 3.0f};
  const auto [threes, wordsForThrees] = run(stated);
  EXPECT_EQ(wordsForThrees, 500u * 7u);
  EXPECT_EQ(threes.points.position, ones.points.position);
  EXPECT_EQ(threes.points.velocity, ones.points.velocity);
  EXPECT_EQ(threes.attribute("size")->values, ones.attribute("size")->values);
  for (const float weight : threes.points.mass) EXPECT_FLOAT_EQ(weight, 3.0f);

  // And the raggedness is what costs the word: the same emitter with a
  // variation on it spends one more and no longer replays that cloud.
  Emitter ragged = stated;
  ragged.mass.variation = 0.5f;
  const auto [drawn, wordsForDrawn] = run(ragged);
  EXPECT_EQ(wordsForDrawn, 500u * 8u);
  EXPECT_NE(drawn.points.position, ones.points.position);
}

TEST(Particles, AHeavierParticleMovesLessUnderTheSamePush) {
  Emitter mouth = fountain();
  mouth.speed = {};
  mouth.cone = 0.0f;
  mouth.from = EmitFrom::Point;
  mouth.mass = {.mean = 3.0f, .variation = 2.0f};

  chance::Stream stream = chance::Stream::pcg(5);
  Particles cloud;
  mouth.burst(cloud, stream, 200);

  // The same PUSH on every one of them — a force lane the caller loads,
  // not an acceleration, since an acceleration is what weight is taken
  // out of.
  for (Vec2& push : cloud.points.force) push = {0.0f, 600.0f};
  const Verlet stepper{.dt = 1.0f / 60.0f};
  stepper.step(cloud.points, {});

  size_t lightest = 0, heaviest = 0;
  for (size_t i = 0; i < cloud.size(); ++i) {
    if (cloud.points.mass[i] < cloud.points.mass[lightest]) lightest = i;
    if (cloud.points.mass[i] > cloud.points.mass[heaviest]) heaviest = i;
  }
  ASSERT_GT(cloud.points.mass[heaviest], cloud.points.mass[lightest]);

  const float lightMoved = cloud.points.velocity[lightest].y;
  const float heavyMoved = cloud.points.velocity[heaviest].y;
  EXPECT_GT(lightMoved, heavyMoved);
  // And by the ratio of the two weights, since a step divides the push it
  // was given by what it is pushing. The slack is the step's own: the
  // velocity a step answers is recovered from where the point ended up
  // rather than from the number that moved it.
  const float lightPush = lightMoved * cloud.points.mass[lightest];
  const float heavyPush = heavyMoved * cloud.points.mass[heaviest];
  EXPECT_NEAR(lightPush, heavyPush, std::abs(heavyPush) * 1e-3f);
}

TEST(Particles, ASpreadIsAMiddleARaggednessAndAScale) {
  chance::Stream stream = chance::Stream::pcg(19);
  const Roughly half{.mean = 4.0f, .variation = 2.0f, .scale = 0.5f};
  float least = 1e9f, most = -1e9f, total = 0.0f;
  constexpr int kDraws = 20000;
  for (int i = 0; i < kDraws; ++i) {
    const float drawn = stream.sample(half);
    least = std::min(least, drawn);
    most = std::max(most, drawn);
    total += drawn;
  }
  EXPECT_GE(least, 1.0f);
  EXPECT_LE(most, 3.0f);
  EXPECT_NEAR(total / (float)kDraws, 2.0f, 0.02f);
  // The scale multiplies the whole draw rather than the middle alone.
  EXPECT_EQ(half, (Roughly{.mean = 4.0f, .variation = 2.0f, .scale = 0.5f}));
  EXPECT_NE(half, (Roughly{.mean = 2.0f, .variation = 1.0f}));
}

TEST(Particles, ACloudIsSteppedByWhatStepsAPointSet) {
  // The whole claim of putting this over the point set: the stepper, the
  // forces and the constraints do not know a particle from a point.
  Emitter mouth = fountain();
  mouth.speed = {.mean = 0.0f};
  mouth.cone = 0.0f;
  chance::Stream stream = chance::Stream::pcg(23);
  Particles cloud;
  mouth.burst(cloud, stream, 64);

  const std::vector<Force> forces{gravity({0, 100.0f}), drag(0.5f)};
  const Verlet stepper{.dt = 1.0f / 60.0f};
  for (int frame = 0; frame < 60; ++frame) {
    stepper.step(cloud.points, forces);
    cloud.live(stepper.dt);
    cloud.reap();
  }
  EXPECT_EQ(cloud.size(), cloud.age.size());
  for (size_t i = 0; i < cloud.size(); ++i) {
    EXPECT_GT(cloud.points.position[i].y, 200.0f);
    EXPECT_LT(cloud.age[i], cloud.life[i]);
  }
}
