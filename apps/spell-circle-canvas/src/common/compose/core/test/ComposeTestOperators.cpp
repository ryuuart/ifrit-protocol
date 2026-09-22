// The facts a node states and the operators that read them: a fact read
// back in the type it was written in and in no other, a table equal in any
// order, an arranging operator placing children by a fact rather than an
// index, a second operator in the list nudging what the first left, a turn
// that reaches the paint, a scheme of the older shape applied through the
// same list, a point that takes no room, and the prune a comparable
// operator keeps and a fact change breaks.

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include "support/CoreTestSupport.h"

namespace {

constexpr float kRadiansPerDegree = std::numbers::pi_v<float> / 180.0f;

/** Each child at its own hour on a ring, facing outward. */
struct AroundRing {
  float radiusFraction = 0.8f;
  bool operator==(const AroundRing&) const = default;

  void arrange(Arrangement& arrangement) const {
    const SkPoint centre = arrangement.box.center();
    const float radius =
        std::min(centre.x(), centre.y()) * radiusFraction;
    for (Arrangement::Child& child : arrangement.children) {
      const int hour = child.attribute<int>("hour").value_or(0);
      const float degrees = (float)hour * 30.0f - 90.0f;
      const float radians = degrees * kRadiansPerDegree;
      child.centreAt({centre.x() + radius * std::cos(radians),
                      centre.y() + radius * std::sin(radians)});
      child.turn(degrees + 90.0f);
    }
  }
};

/** Every child moved by the same offset from wherever it stands. */
struct Nudge {
  SkVector by = {0, 0};
  bool operator==(const Nudge&) const = default;

  void arrange(Arrangement& arrangement) const {
    for (Arrangement::Child& child : arrangement.children)
      child.place(child.rect.makeOffset(by));
  }
};

/** A scheme of the older shape: every child at the same corner. */
struct AllAtCorner {
  float inset = 0.0f;
  bool operator==(const AllAtCorner&) const = default;

  std::vector<SkRect> place(const LayoutInput& in) const {
    std::vector<SkRect> rects;
    for (SkSize size : in.childSizes)
      rects.push_back(
          SkRect::MakeXYWH(inset, inset, size.width(), size.height()));
    return rects;
  }
};

Element dot(int hour) {
  return box()
      .key("h" + std::to_string(hour))
      .attribute("hour", hour)
      .width(10)
      .height(10)
      .fill(red());
}

}  // namespace

TEST(ComposeOperators, AFactReadsBackInTheTypeItWasWrittenIn) {
  Attributes facts;
  facts.set("tier", 2);
  facts.set("weight", 1.5f);
  facts.set("name", "gateway");
  facts.set("calls", std::vector<std::string>{"ledger", "cache"});
  EXPECT_EQ(facts.get<int>("tier"), 2);
  EXPECT_FALSE(facts.get<float>("tier").has_value());  // an int is not a float
  EXPECT_EQ(facts.get<float>("weight"), 1.5f);
  EXPECT_EQ(facts.get<std::string>("name"), "gateway");  // a literal is a string
  ASSERT_TRUE(facts.get<std::vector<std::string>>("calls").has_value());
  EXPECT_EQ(facts.get<std::vector<std::string>>("calls")->size(), 2u);
  EXPECT_FALSE(facts.has("missing"));
  EXPECT_FALSE(facts.get<int>("missing").has_value());
  facts.set("tier", 3);  // a later fact under the same name replaces
  EXPECT_EQ(facts.get<int>("tier"), 3);
  EXPECT_EQ(facts.size(), 4u);
}

TEST(ComposeOperators, TwoTablesAreEqualInAnyOrder) {
  Attributes a, b, c;
  a.set("tier", 2);
  a.set("name", "gateway");
  b.set("name", "gateway");
  b.set("tier", 2);
  c.set("tier", 2);
  c.set("name", "ledger");
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
  // Copies share storage and a write to one leaves the other standing.
  Attributes copy = a;
  copy.set("tier", 9);
  EXPECT_EQ(a.get<int>("tier"), 2);
  EXPECT_EQ(copy.get<int>("tier"), 9);
}

TEST(ComposeOperators, AnArrangingOperatorPlacesByAFactNotByIndex) {
  // Written out of order and with hours missing: 3 lands at three o'clock
  // whatever its position in the list, which layouts::Radial — told only
  // the index — could not do.
  Host host;
  host.composer.render(box().width(200).height(200).operators({AroundRing{}})
                           .children({dot(6), dot(3), dot(12)}));
  host.frame();
  auto centre = [&](const char* key) {
    auto rect = host.composer.bounds(key);
    return SkPoint{rect->centerX(), rect->centerY()};
  };
  EXPECT_NEAR(centre("h12").x(), 100, 1);
  EXPECT_NEAR(centre("h12").y(), 20, 1);
  EXPECT_NEAR(centre("h3").x(), 180, 1);
  EXPECT_NEAR(centre("h3").y(), 100, 1);
  EXPECT_NEAR(centre("h6").x(), 100, 1);
  EXPECT_NEAR(centre("h6").y(), 180, 1);
}

TEST(ComposeOperators, ALaterOperatorSeesWhereTheEarlierOneLeftEachChild) {
  Host host;
  host.composer.render(
      box()
          .width(200)
          .height(200)
          .operators({AroundRing{}, Nudge{.by = {7, -3}}})
          .children({dot(3)}));
  host.frame();
  auto rect = host.composer.bounds("h3");
  ASSERT_TRUE(rect.has_value());
  EXPECT_NEAR(rect->centerX(), 187, 1);
  EXPECT_NEAR(rect->centerY(), 97, 1);
}

TEST(ComposeOperators, ATurnReachesThePaint) {
  // A tall bar turned a quarter turn about its centre paints wide: the
  // pixel two-thirds of the way along its former height is background,
  // and the one two-thirds of the way along its new width is the bar.
  Host host;
  struct Quarter {
    bool operator==(const Quarter&) const = default;
    void arrange(Arrangement& arrangement) const {
      for (Arrangement::Child& child : arrangement.children) child.turn(90);
    }
  };
  host.composer.render(
      box().width(200).height(200).operators({Quarter{}}).children(
          {box().key("bar").left(95).top(40).width(10).height(120).fill(
              red())}));
  host.frame();
  EXPECT_EQ(host.pixel(100, 60), SK_ColorBLACK);  // where the bar stood
  EXPECT_EQ(host.pixel(60, 100), SK_ColorRED);    // where it turned to
  EXPECT_EQ(host.pixel(140, 100), SK_ColorRED);
}

TEST(ComposeOperators, ASchemeOfTheOlderShapeRunsThroughTheSameList) {
  Host host;
  host.composer.render(
      box().width(200).height(200).operators({AllAtCorner{.inset = 30}})
          .children({dot(1), dot(2)}));
  host.frame();
  for (const char* key : {"h1", "h2"}) {
    auto rect = host.composer.bounds(key);
    ASSERT_TRUE(rect.has_value());
    EXPECT_NEAR(rect->left(), 30, 0.5f);
    EXPECT_NEAR(rect->top(), 30, 0.5f);
  }
  // And layout(scheme) is that same node under the shorter spelling.
  host.composer.render(layout(AllAtCorner{.inset = 30})
                           .width(200)
                           .height(200)
                           .children({dot(1)}));
  host.frame();
  EXPECT_NEAR(host.composer.bounds("h1")->left(), 30, 0.5f);
}

TEST(ComposeOperators, AFactReachesASchemeOfTheOlderShape) {
  struct ByTier {
    bool operator==(const ByTier&) const = default;
    std::vector<SkRect> place(const LayoutInput& in) const {
      std::vector<SkRect> rects;
      for (size_t i = 0; i < in.childSizes.size(); ++i) {
        const int tier = in.attribute<int>(i, "tier").value_or(0);
        rects.push_back(SkRect::MakeXYWH(0, (float)tier * 50.0f,
                                         in.childSizes[i].width(),
                                         in.childSizes[i].height()));
      }
      return rects;
    }
  };
  Host host;
  host.composer.render(
      layout(ByTier{}).width(200).height(200).children(
          {box().key("a").attribute("tier", 2).width(10).height(10),
           box().key("b").attribute("tier", 0).width(10).height(10)}));
  host.frame();
  EXPECT_NEAR(host.composer.bounds("a")->top(), 100, 0.5f);
  EXPECT_NEAR(host.composer.bounds("b")->top(), 0, 0.5f);
}

TEST(ComposeOperators, APointTakesNoRoomAndStandsWhereItIsPinned) {
  Host host;
  host.composer.render(box().row().gap(0).children(
      {box().key("first").width(40).height(40).fill(red()),
       point().key("port").left(pct(100)).top(pct(50)),
       box().key("second").width(40).height(40).fill(green())}));
  host.frame();
  // The point sits between the boxes in the list and moves neither.
  EXPECT_NEAR(host.composer.bounds("second")->left(), 40, 0.5f);
  auto port = host.composer.bounds("port");
  ASSERT_TRUE(port.has_value());
  EXPECT_NEAR(port->left(), 200, 0.5f);
  EXPECT_NEAR(port->top(), 100, 0.5f);
  EXPECT_NEAR(port->width(), 0, 0.5f);
  EXPECT_NEAR(port->height(), 0, 0.5f);
  // …and it answers no hit: the box under it does.
  host.composer.render(box().children(
      {box().key("under").width(200).height(200).fill(red()),
       point().key("port").left(100).top(100)}));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({100, 100}), std::optional<std::string>("under"));
}

TEST(ComposeOperators, AComparableOperatorPrunesAndAFactChangeDoesNot) {
  Host host;
  auto tree = [](int hour) {
    return box().width(200).height(200).operators({AroundRing{}}).children(
        {dot(hour)});
  };
  host.composer.render(tree(3));
  host.frame();
  // The same description again: the operator compares equal, the fact is
  // unchanged, nothing is patched.
  host.composer.render(tree(3));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  // The fact moved: the child is patched and lands at its new hour.
  host.composer.render(tree(6));
  EXPECT_GT(host.composer.stats().patchedNodes, 0u);
  host.frame();
  auto rect = host.composer.bounds("h6");
  ASSERT_TRUE(rect.has_value());
  EXPECT_NEAR(rect->centerY(), 180, 1);
}

TEST(ComposeOperators, AnOperatorWithNoEqualityNeverPrunes) {
  struct Opaque {
    void arrange(Arrangement&) const {}
  };
  Operator held = Opaque{};
  EXPECT_FALSE(held.comparable());
  EXPECT_FALSE(held == Operator(Opaque{}));
  Operator copy = held;
  EXPECT_TRUE(held == copy);  // copies of ONE value share state
  Operator ring = AroundRing{};
  EXPECT_TRUE(ring.comparable());
  EXPECT_TRUE(ring == Operator(AroundRing{}));
  EXPECT_FALSE(ring == Operator(AroundRing{.radiusFraction = 0.5f}));
}
