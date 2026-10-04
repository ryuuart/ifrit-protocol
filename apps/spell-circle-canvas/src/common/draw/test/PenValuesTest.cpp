/** @file
 * The numbers a pen answers rather than draws — the seeded random stream,
 * the noise field, the math vocabulary — and the retained guest a caller
 * draws through it.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Angle.h>
#include <sigildraw/Color.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Math.h>
#include <sigildraw/Noise.h>
#include <sigildraw/Pen.h>
#include <sigildraw/Retained.h>
#include <sigilmaterial/color/Color.h>

#include <any>
#include <cmath>
#include <memory>
#include <source_location>
#include <vector>

#include "support/Paper.h"

namespace probe {

struct Card {
  int id = 0;
};

struct Seen {
  int paints = 0;
  sigil::geometry::path::Rect box;
};

Seen& cardState(sigil::draw::Pen& pen, sigil::draw::Slot slot) {
  return pen.retained().get<Seen>(slot,
                                  [] { return std::make_shared<Seen>(); });
}

void paintRetained(sigil::draw::Pen& pen, const Card& card,
                   const sigil::geometry::path::Rect& box,
                   sigil::draw::Slot slot) {
  Seen& seen = cardState(pen, slot);
  ++seen.paints;
  seen.box = box;
  (void)card;
}

void cardAt(sigil::draw::Pen& pen, const Card& card, int index = 0,
            std::source_location where = std::source_location::current()) {
  pen.element(card, 10, 10, 30, 20, index, where);
}

}  // namespace probe

namespace {

using namespace sigil::draw;
using sigil::draw::testing::Paper;

std::shared_ptr<int> trackedValue(std::weak_ptr<int>& lifetime, int value) {
  auto resource = std::make_shared<int>(value);
  lifetime = resource;
  return resource;
}

// The pen reads a colour's text through the library that owns colour, so
// a palette in a words file means one colour to a pen and to a material.
TEST(Pen, AColourStringReadsAsTheMaterialLibraryReadsIt) {
  for (const char* css : {"#e3d7b6", "#e3d7b6ff", "#ed7", "#3a3125b8",
                          "steelblue", "transparent", "not a colour"})
    EXPECT_EQ(parseColor(css), sigil::material::parseColor(css)) << css;
}

TEST(Pen, OneSeedGivesOneSequenceOnEveryPen) {
  Pen a;
  Pen b;
  for (int i = 0; i < 16; ++i) EXPECT_EQ(a.random(), b.random());
  a.randomSeed(42);
  std::vector<float> first;
  for (int i = 0; i < 16; ++i) first.push_back(a.random(-3, 3));
  a.randomSeed(42);
  for (int i = 0; i < 16; ++i) EXPECT_EQ(first[(size_t)i], a.random(-3, 3));
}

TEST(Pen, ARandomDrawLandsInsideTheRangeItWasAskedFor) {
  Pen pen;
  pen.randomSeed(42);
  for (int i = 0; i < 256; ++i) {
    const float signed_ = pen.random(-3, 3);
    EXPECT_GE(signed_, -3.0f);
    EXPECT_LT(signed_, 3.0f);
    const float capped = pen.random(10);
    EXPECT_GE(capped, 0.0f);
    EXPECT_LT(capped, 10.0f);
  }
}

TEST(Pen, NoiseIsCoresLatticeAtTheCorners) {
  Pen pen;
  pen.noiseSeed(7);
  pen.noiseDetail(1, 0.5f);
  // One octave at an integer position IS the corner value, at the half
  // weight p5's first octave carries. The corner is asked for by name
  // rather than recomputed here, so this says what the pen promises
  // instead of restating how the field is built.
  const float corner = NoiseField::corner(7, 3, 4, 5);
  EXPECT_FLOAT_EQ(pen.noise(3, 4, 5), 0.5f * corner);

  pen.noiseDetail(4, 0.5f);
  float previous = pen.noise(0.0f, 0.37f);
  for (int i = 1; i < 2000; ++i) {
    const float v = pen.noise((float)i * 0.01f, 0.37f);
    EXPECT_GE(v, 0.0f);
    EXPECT_LT(v, 1.0f);
    EXPECT_LT(std::fabs(v - previous), 0.05f);  // continuous
    previous = v;
  }
  const float here = pen.noise(1.5f, 2.5f);
  pen.noiseSeed(8);
  EXPECT_NE(here, pen.noise(1.5f, 2.5f));
  pen.noiseSeed(7);
  EXPECT_EQ(here, pen.noise(1.5f, 2.5f));
}

TEST(Pen, TheMathVocabularyComputesWhatItsNamesPromise) {
  EXPECT_FLOAT_EQ(map(5, 0, 10, 0, 100), 50.0f);
  EXPECT_FLOAT_EQ(map(15, 0, 10, 0, 100, true), 100.0f);
  EXPECT_FLOAT_EQ(lerp(0, 10, 0.25f), 2.5f);
  EXPECT_FLOAT_EQ(constrain(11, 0, 10), 10.0f);
  EXPECT_FLOAT_EQ(dist(0, 0, 3, 4), 5.0f);
  EXPECT_FLOAT_EQ(norm(5, 0, 10), 0.5f);
  EXPECT_FLOAT_EQ(radians(180), PI);
  EXPECT_FLOAT_EQ(degrees(PI), 180.0f);
  // p5's two spellings, scaling by the float every other library here
  // scales an angle by: a pen that rounded its own way would place a
  // mark where no contour does.
  EXPECT_EQ(radians(37.5f), sigil::core::angle::radians(37.5f));
  EXPECT_EQ(degrees(0.6545f), sigil::core::angle::degrees(0.6545f));
}

TEST(Pen, AGuestIsRetainedPerCallSite) {
  Paper paper;
  const probe::Card card{1};
  for (int frame = 1; frame <= 3; ++frame) {
    paper.begin(frame);
    paper.pen.element(card, 10, 10, 30, 20);
    for (int i = 0; i < 2; ++i) paper.pen.element(card, 0, 0, 5, 5, i);
    paper.end();
  }
  // One slot for the single call, one per index for the loop.
  EXPECT_EQ(paper.pen.retained().size(), 3u);
}

TEST(Pen, TheRetainedStoreKeepsAValueByItsSlot) {
  Retained store;
  const sigil::draw::Slot slot =
      sigil::draw::Slot::at(std::source_location::current(), 0);
  int& first = store.get<int>(slot, [] { return std::make_shared<int>(5); });
  first = 9;
  int& again = store.get<int>(slot, [] { return std::make_shared<int>(1); });
  EXPECT_EQ(&first, &again);
  EXPECT_EQ(again, 9);
  const sigil::draw::Slot other =
      sigil::draw::Slot::at(std::source_location::current(), 1);
  EXPECT_NE(&store.get<int>(other, [] { return std::make_shared<int>(2); }),
            &first);
  EXPECT_EQ(store.size(), 2u);
}

TEST(Pen, RetiringAStoredGuestReleasesOnlyThatGuest) {
  Retained store;
  const auto where = std::source_location::current();
  const Slot retired = Slot::at(where, 0);
  const Slot kept = Slot::at(where, 1);
  std::weak_ptr<int> retiredResource;
  std::weak_ptr<int> keptResource;
  store.get<int>(retired, [&] { return trackedValue(retiredResource, 5); });
  int& survivor =
      store.get<int>(kept, [&] { return trackedValue(keptResource, 9); });
  store.host() = 42;

  EXPECT_TRUE(store.erase(retired));
  EXPECT_TRUE(retiredResource.expired());
  ASSERT_FALSE(keptResource.expired());
  EXPECT_EQ(store.size(), 1u);
  EXPECT_EQ(&survivor, keptResource.lock().get());
  EXPECT_EQ(survivor, 9);
  EXPECT_EQ(std::any_cast<int>(store.host()), 42);
  EXPECT_FALSE(store.erase(retired));
  EXPECT_EQ(store.size(), 1u);
}

TEST(Pen, RepeatedGuestRetirementKeepsTheStoreBounded) {
  Retained store;
  const auto where = std::source_location::current();
  const Slot kept = Slot::at(where, -1);
  std::weak_ptr<int> keptResource;
  int& survivor =
      store.get<int>(kept, [&] { return trackedValue(keptResource, 9); });
  std::weak_ptr<int> transientResource;

  for (int index = 0; index < 1000; ++index) {
    const Slot transient = Slot::at(where, index);
    store.get<int>(transient,
                   [&] { return trackedValue(transientResource, index); });
    ASSERT_EQ(store.size(), 2u);
    EXPECT_FALSE(transientResource.expired());
    EXPECT_TRUE(store.erase(transient));
    EXPECT_TRUE(transientResource.expired());
    ASSERT_EQ(store.size(), 1u);
  }
  EXPECT_EQ(&survivor, keptResource.lock().get());
  EXPECT_EQ(survivor, 9);
}

TEST(Pen, RetiringAForwardedGuestPreservesAHiddenPeer) {
  Paper paper;
  const probe::Card card;
  const auto where = std::source_location::current();
  const Slot retired = Slot::at(where, 0);
  const Slot hidden = Slot::at(where, 1);
  paper.begin(1);
  probe::cardAt(paper.pen, card, 0, where);
  probe::cardAt(paper.pen, card, 1, where);
  paper.end();
  const probe::Seen* survivor = &probe::cardState(paper.pen, hidden);
  EXPECT_EQ(survivor->paints, 1);
  EXPECT_EQ(paper.pen.retained().size(), 2u);

  paper.begin(2);
  EXPECT_TRUE(paper.pen.retained().erase(retired));
  paper.end();
  EXPECT_EQ(paper.pen.retained().size(), 1u);
  EXPECT_EQ(probe::cardState(paper.pen, hidden).paints, 1);

  paper.begin(3);
  probe::cardAt(paper.pen, card, 0, where);
  probe::cardAt(paper.pen, card, 1, where);
  paper.end();
  EXPECT_EQ(probe::cardState(paper.pen, retired).paints, 1);
  EXPECT_EQ(&probe::cardState(paper.pen, hidden), survivor);
  EXPECT_EQ(probe::cardState(paper.pen, hidden).paints, 2);
  EXPECT_EQ(paper.pen.retained().size(), 2u);
}

}  // namespace
