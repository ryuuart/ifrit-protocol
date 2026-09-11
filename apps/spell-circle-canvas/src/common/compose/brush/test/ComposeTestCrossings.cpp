// Where strands cross: the crossing scan and the rule ladder that decides
// who passes over, the composites woven out of them and the cache behind
// the crossings, the strands themselves, and the names the brush kinds
// answer to.

#include <type_traits>

#include "support/BrushTestSupport.h"

namespace {

/** Two straight strands that cross once, as raw geometry. */
SkPath diagonal(SkPoint a, SkPoint b) {
  SkPathBuilder p;
  p.moveTo(a);
  p.lineTo(b);
  return p.detach();
}

}  // namespace

TEST(ComposeCrossings, CoincidentStrandsNeverCross) {
  // This is what layers() IS, so it has to be exact: N copies of one path
  // meet everywhere and cross nowhere.
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(80, 60));
  const SkPath rect = b.detach();
  EXPECT_TRUE(geometry::path::discoverCrossings({rect, rect}).empty());
  EXPECT_TRUE(geometry::path::discoverCrossings({rect, rect, rect}).empty());
}

TEST(ComposeCrossings, SharedCornersAreMeetingsNotCrossings) {
  // Two edges touching at a shared endpoint is a MEETING, not a crossing.
  // A discovery pass that counted them would put a knot at every corner of
  // every rectangular frame ever drawn.
  SkPathBuilder a;
  a.addRect(SkRect::MakeWH(80, 60));
  SkPathBuilder c;
  c.addRect(SkRect::MakeXYWH(80, 60, 80, 60));  // touches at one point only
  EXPECT_TRUE(
      geometry::path::discoverCrossings({a.detach(), c.detach()}).empty());
}

TEST(ComposeCrossings, ProperCrossingsAreFoundAndNumberedAlongTheBoundary) {
  const SkPath down = diagonal({0, 0}, {100, 100});
  const SkPath up = diagonal({0, 100}, {100, 0});
  const std::vector<geometry::path::Crossing> one =
      geometry::path::discoverCrossings({down, up});
  ASSERT_EQ(one.size(), 1u);
  EXPECT_EQ(one[0].a, 0u);
  EXPECT_EQ(one[0].b, 1u);
  EXPECT_NEAR(one[0].at.fX, 50.0f, 2.0f);
  EXPECT_NEAR(one[0].at.fY, 50.0f, 2.0f);
  EXPECT_EQ(one[0].index, 0u);

  // Numbering is positional along the lowest-indexed strand, so a horizontal
  // strand crossed by two verticals numbers them left to right.
  const SkPath across = diagonal({0, 50}, {100, 50});
  const std::vector<geometry::path::Crossing> two =
      geometry::path::discoverCrossings(
          {across, diagonal({70, 0}, {70, 100}), diagonal({30, 0}, {30, 100})});
  ASSERT_EQ(two.size(), 2u);
  EXPECT_LT(two[0].alongA, two[1].alongA);
  EXPECT_EQ(two[0].index, 0u);
  EXPECT_EQ(two[1].index, 1u);
  EXPECT_EQ(two[0].b, 2u) << "the x=30 strand is met first";
}

TEST(ComposeCrossings, TheRuleLadderClimbs) {
  geometry::path::Crossing c0, c1, c2;
  c0.index = 0;
  c0.a = 0;
  c0.b = 1;
  c1.index = 1;
  c1.a = 0;
  c1.b = 1;
  c2.index = 2;
  c2.a = 1;
  c2.b = 2;

  // Rung 0 — list order: the later strand is on top, so `a` is under.
  EXPECT_EQ(geometry::path::CrossingRule{}.decide(c0),
            geometry::path::Order::Under);

  // Rung 1 — alternate IS sequence({Over, Under}).
  const geometry::path::CrossingRule alt =
      geometry::path::crossing::alternate();
  EXPECT_EQ(alt,
            geometry::path::crossing::sequence(
                {geometry::path::Order::Over, geometry::path::Order::Under}));
  EXPECT_EQ(alt.decide(c0), geometry::path::Order::Over);
  EXPECT_EQ(alt.decide(c1), geometry::path::Order::Under);

  // Rung 2 — a generic repeating pattern.
  const geometry::path::CrossingRule three = geometry::path::crossing::sequence(
      {geometry::path::Order::Over, geometry::path::Order::Over,
       geometry::path::Order::Under});
  EXPECT_EQ(three.decide(c0), geometry::path::Order::Over);
  EXPECT_EQ(three.decide(c1), geometry::path::Order::Over);
  EXPECT_EQ(three.decide(c2), geometry::path::Order::Under);

  // Rung 3 — strand dominance, including a CYCLE (the Penrose case:
  // 0 over 1, 1 over 2, 2 over 0, which no layer order can express).
  const geometry::path::CrossingRule cyclic =
      geometry::path::crossing::pairs({{0, 1}, {1, 2}, {2, 0}});
  EXPECT_EQ(cyclic.decide(c0), geometry::path::Order::Over);  // 0 over 1
  EXPECT_EQ(cyclic.decide(c2), geometry::path::Order::Over);  // 1 over 2
  geometry::path::Crossing c20;
  c20.a = 0;
  c20.b = 2;
  EXPECT_EQ(cyclic.decide(c20), geometry::path::Order::Under);  // 2 over 0
}

namespace {

/** Strand 0 over strand 1 at every crossing — used where the point is that
 *  the REPAIR works, not which rule chose it. */
struct EveryCrossingRedOnTop {
  bool operator==(const EveryCrossingRedOnTop&) const = default;
  geometry::path::Order decide(const geometry::path::Crossing&) const {
    return geometry::path::Order::Over;
  }
};

/** Rung 4: a user rule is a comparable value with the seam's one named
 *  member — never a bare lambda, because a rule is read live. */
struct EverySecondStrandWins {
  size_t winner = 1;
  bool operator==(const EverySecondStrandWins&) const = default;
  geometry::path::Order decide(const geometry::path::Crossing& c) const {
    return c.a == winner ? geometry::path::Order::Over
                         : geometry::path::Order::Under;
  }
};

}  // namespace

TEST(ComposeCrossings, CustomRulesAreComparableValues) {
  static_assert(geometry::path::CrossingScheme<EverySecondStrandWins>);
  const geometry::path::CrossingRule mine = EverySecondStrandWins{1};
  geometry::path::Crossing c;
  c.a = 1;
  c.b = 2;
  EXPECT_EQ(mine.decide(c), geometry::path::Order::Over);
  EXPECT_TRUE(mine == geometry::path::CrossingRule(EverySecondStrandWins{1}));
  EXPECT_FALSE(mine == geometry::path::CrossingRule(EverySecondStrandWins{2}));
  EXPECT_FALSE(mine == geometry::path::crossing::alternate());
}

TEST(ComposeCrossings, PinsComposeOntoTheBaseRule) {
  // One .crossing field: a pin layers over whatever rule is already there
  // rather than becoming a second entry.
  geometry::path::CrossingRule rule = geometry::path::crossing::alternate();
  rule.except(0, geometry::path::Order::Under)
      .except(3, geometry::path::Order::Over);
  geometry::path::Crossing c;
  c.a = 0;
  c.b = 1;
  c.index = 0;
  EXPECT_EQ(rule.decide(c), geometry::path::Order::Under)
      << "pinned against alternate";
  c.index = 1;
  EXPECT_EQ(rule.decide(c), geometry::path::Order::Under)
      << "base rule still runs";
  c.index = 2;
  EXPECT_EQ(rule.decide(c), geometry::path::Order::Over);
  c.index = 3;
  EXPECT_EQ(rule.decide(c), geometry::path::Order::Over) << "second pin";
  // Re-pinning the same index REPLACES it (one answer per crossing).
  rule.except(0, geometry::path::Order::Over);
  c.index = 0;
  EXPECT_EQ(rule.decide(c), geometry::path::Order::Over);
  // …and a pinned rule is still a comparable value.
  geometry::path::CrossingRule same = geometry::path::crossing::alternate();
  same.except(0, geometry::path::Order::Over)
      .except(3, geometry::path::Order::Over);
  EXPECT_TRUE(rule == same);
  EXPECT_FALSE(rule == geometry::path::crossing::alternate());
}

TEST(ComposeComposites, LayersIsWeaveWithCoincidentSelfStrands) {
  // FORMALLY one machine. Same pixels from both spellings, and the layers
  // form really is a weave of self-strands.
  const brush::Weave stacked =
      brush::layers({brush::solid(8, red()), brush::solid(3, green())});
  ASSERT_EQ(stacked.strands.size(), 2u);
  EXPECT_EQ(stacked.strands[0].path,
            StrandPath(geometry::path::profile::self()));
  EXPECT_EQ(stacked.strands[1].path,
            StrandPath(geometry::path::profile::self()));

  const brush::Weave woven = brush::weave(
      {brush::Strand{geometry::path::profile::self(), brush::solid(8, red())},
       brush::Strand{geometry::path::profile::self(),
                     brush::solid(3, green())}},
      geometry::path::CrossingRule{});

  auto draw = [](const brush::Weave& w) {
    Host host(200, 200);
    host.composer.render(stack().child(
        box().rect(SkRect::MakeXYWH(40, 40, 100, 100)).stroke(w)));
    host.frame();
    std::vector<SkColor> out;
    for (int x = 30; x < 150; x += 3) out.push_back(host.pixel(x, 40));
    return out;
  };
  const std::vector<SkColor> layered = draw(stacked);
  EXPECT_EQ(layered, draw(woven));
  // Bottom-up: the last brush in the list is the one you see.
  EXPECT_EQ(layered[(layered.size() / 2)], SK_ColorGREEN);
}

TEST(ComposeComposites, WeaveRepairsTheCrossingsTheRuleDisagreesWith) {
  // Two authored strands crossing once. Under list order the second is on
  // top; alternate() says the FIRST passes over at crossing 0, so the
  // repair patch must put strand 0's colour at the meeting.
  auto draw = [](geometry::path::CrossingRule rule) {
    Host host(200, 200);
    brush::Weave w = brush::weave(
        {brush::Strand{strand::path(diagonal({20, 20}, {180, 180})),
                       brush::solid(9, red())},
         brush::Strand{strand::path(diagonal({20, 180}, {180, 20})),
                       brush::solid(9, green())}},
        std::move(rule));
    host.composer.render(stack().child(box().inset(0).stroke(w)));
    host.frame();
    return host.pixel(100, 100);
  };
  EXPECT_EQ(draw(geometry::path::CrossingRule{}), SK_ColorGREEN)
      << "list order: later on top";
  EXPECT_EQ(draw(geometry::path::crossing::alternate()), SK_ColorRED)
      << "rule flipped it";
  geometry::path::CrossingRule pinned = geometry::path::crossing::alternate();
  pinned.except(0, geometry::path::Order::Under);
  EXPECT_EQ(draw(pinned), SK_ColorGREEN) << "the pin overrode the rule";
}

// ---------------------------------------------------------------------------
// The crossing cache: discovery is memoized on the RESOLVED strand paths,
// which are the function's entire input, and held on the Weave value behind
// a shared_ptr so copies share it. Three things must hold — staleness
// through each of the two doors into the key, and byte identity between the
// cold and cached paints.
//
// `crossingCache->computes` counts DISCOVERIES, not paints. That is what
// makes these cases readable: with Cache::None the node repaints every
// frame, so a steady frame that does not move the counter proves a hit.
TEST(ComposeComposites, CrossingCacheRecomputesWhenAuthoredGeometryChanges) {
  // Staleness through the AUTHORED door: warm the cache, then edit a
  // strand's path on a copy that shares it.
  //
  // THREE strands, not two, and that is load-bearing. With a lone crossing
  // the knot's territory has no neighbour to bound it, so a stale repair —
  // clipped only by the CURRENT tubes — still happens to cover the new
  // meeting, and the staleness is invisible in pixels. Two knots bound each
  // other's territory, so a stale answer mis-paints somewhere readable.
  //
  // Red runs y=100; green verticals at x=60 and x=140. alternate() puts red
  // over at knot 0 (x=60) and leaves green over at knot 1 (x=140). Moving
  // the first vertical to x=180 makes (140,100) the new knot 0 — red over —
  // while (180,100) keeps green. A stale cache still numbers (140,100) as
  // knot 1 and aims its red repair at (60,100), where nothing is left to
  // repair.
  brush::Weave w =
      brush::weave({brush::Strand{strand::path(diagonal({0, 100}, {200, 100})),
                                  brush::solid(9, red())},
                    brush::Strand{strand::path(diagonal({60, 0}, {60, 200})),
                                  brush::solid(9, green())},
                    brush::Strand{strand::path(diagonal({140, 0}, {140, 200})),
                                  brush::solid(9, green())}},
                   geometry::path::crossing::alternate());
  Host host(240, 240);
  host.composer.render(
      stack().child(box().inset(0).stroke(w).cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(60, 100), SK_ColorRED) << "knot 0: red repaired over";
  EXPECT_EQ(host.pixel(140, 100), SK_ColorGREEN) << "knot 1: list order";
  EXPECT_EQ(w.crossingCache->computes, 1) << "the cold paint discovers once";
  host.frame();
  EXPECT_EQ(w.crossingCache->computes, 1)
      << "a steady repaint must HIT, not rediscover";

  brush::Weave moved = w;  // shares the WARM cache — that is the scenario
  moved.strands[1].path = strand::path(diagonal({180, 0}, {180, 200}));
  host.composer.render(
      stack().child(box().inset(0).stroke(moved).cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(140, 100), SK_ColorRED)
      << "stale crossings: (140,100) still numbered as the green knot";
  EXPECT_EQ(host.pixel(180, 100), SK_ColorGREEN)
      << "the new knot 1 keeps list order";
  EXPECT_EQ(w.crossingCache->computes, 2)
      << "the edit must land in the key and rediscover";
}

TEST(ComposeComposites, CrossingCacheFollowsTheOutlineUnderRelativeStrands) {
  // Staleness through the OUTLINE door. A relative strand resolves against
  // ctx.outline, so the SAME weave value over a changed shape must
  // rediscover: the key is the resolved paths, not the value's own fields.
  // A self-strand ring crossed by an authored line, red always on top;
  // when the ring grows, the knots move outward along the line, and only
  // a fresh discovery repairs them at the new radius. The line runs
  // VERTICALLY so both knots sit a quarter turn from the ring's contour
  // seam (addCircle starts at 3 o'clock, and a knot AT the seam is
  // rejected by the transversality walk — a discovery property, not the
  // cache's).
  brush::Weave w = brush::weave(
      {brush::Strand{geometry::path::profile::self(), brush::solid(6, red())},
       brush::Strand{strand::path(diagonal({100, 0}, {100, 200})),
                     brush::solid(6, green())}},
      geometry::path::CrossingRule(EveryCrossingRedOnTop{}));
  auto ring = [](float radius) {
    return [radius](SkSize) {
      SkPathBuilder p;
      p.addCircle(100, 100, radius);
      return p.detach();
    };
  };
  Host host(200, 200);
  host.composer.render(stack().child(
      box().inset(0).shape(ring(60.0f)).stroke(w).cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 160), SK_ColorRED) << "the r=60 knot repairs";
  EXPECT_EQ(w.crossingCache->computes, 1);
  host.frame();
  EXPECT_EQ(w.crossingCache->computes, 1) << "steady outline: a hit";

  host.composer.render(stack().child(
      box().inset(0).shape(ring(85.0f)).stroke(w).cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 185), SK_ColorRED)
      << "stale crossings: the repair stayed at the old radius";
  EXPECT_EQ(w.crossingCache->computes, 2)
      << "the outline change must land in the key and rediscover";
}

TEST(ComposeComposites, CrossingCacheIsByteNeutral) {
  // The cache may change WHEN discovery runs and never what is drawn.
  // Frame 1 discovers cold, frame 2 hits, and a fresh equal value discovers
  // cold again — all three surfaces must be byte-equal.
  auto bytes = [](Host& host) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(host.surface->width(),
                                              host.surface->height()));
    host.surface->readPixels(bm.pixmap(), 0, 0);
    const uint8_t* p = (const uint8_t*)bm.getPixels();
    return std::vector<uint8_t>(p, p + bm.computeByteSize());
  };
  auto weaveX = [] {
    return brush::weave(
        {brush::Strand{strand::path(diagonal({20, 20}, {180, 180})),
                       brush::solid(9, red())},
         brush::Strand{strand::path(diagonal({20, 180}, {180, 20})),
                       brush::solid(9, green())}},
        geometry::path::crossing::alternate());
  };
  brush::Weave w = weaveX();
  Host host(240, 240);
  host.composer.render(
      stack().child(box().inset(0).stroke(w).cache(Cache::None)));
  host.frame();
  const std::vector<uint8_t> cold = bytes(host);
  EXPECT_EQ(w.crossingCache->computes, 1);
  host.frame();
  EXPECT_EQ(w.crossingCache->computes, 1) << "frame 2 must be a HIT";
  EXPECT_TRUE(cold == bytes(host)) << "the cache-hit frame drew differently";

  brush::Weave fresh = weaveX();  // its own cold cache
  Host host2(240, 240);
  host2.composer.render(
      stack().child(box().inset(0).stroke(fresh).cache(Cache::None)));
  host2.frame();
  EXPECT_EQ(fresh.crossingCache->computes, 1);
  EXPECT_TRUE(cold == bytes(host2))
      << "cold and cached must be one picture, byte for byte";
}

TEST(ComposeComposites, TheRepairCoversShallowCrossings) {
  // A crossing repair sized as a disc under-covers at SHALLOW angles: two
  // marks overlap in a lens whose extent grows as reach/sin(theta), so a
  // radius chosen for the perpendicular case leaves the under-strand showing
  // straight across the over-strand. The repair has to follow the lens.
  //
  // Checked by sampling ALONG the over-strand through the meeting: every
  // sample must be the over-strand's colour.
  //
  // The two strands are one segment rotated by ± half the crossing angle
  // about the centre. Building them instead from a shared dx with
  // dy = dx·tan(angle) sends the coordinates to infinity at 90 degrees.
  //
  // Align::Inner is not exercised here and cannot be: an OPEN strand has no
  // inside. It is covered by ReachReportsTheMarkWhereBleedReportsNothing
  // below, on closed strands.
  auto interruptions = [](float degrees) {
    Host host(400, 400);
    const float half = degrees * 0.5f * 3.14159265f / 180.0f;
    const float len = 180.0f;
    const SkPoint mid{200, 200};
    const SkVector dirA{std::cos(half), std::sin(half)};
    const SkVector dirB{std::cos(-half), std::sin(-half)};
    const auto through = [&](SkVector d) {
      return diagonal({mid.fX - d.x() * len, mid.fY - d.y() * len},
                      {mid.fX + d.x() * len, mid.fY + d.y() * len});
    };
    brush::Weave w = brush::weave(
        {brush::Strand{strand::path(through(dirA)), stroke(9, red())},
         brush::Strand{strand::path(through(dirB)), stroke(9, green())}},
        geometry::path::crossing::alternate());  // strand 0 (red) passes OVER
                                                 // at crossing 0
    host.composer.render(stack().child(box().inset(0).stroke(w)));
    host.frame();
    int wrong = 0;
    for (int i = -40; i <= 40; ++i) {
      const int x = (int)std::lround(mid.fX + dirA.x() * (float)i);
      const int y = (int)std::lround(mid.fY + dirA.y() * (float)i);
      if (host.pixel(x, y) != SK_ColorRED) ++wrong;
    }
    return wrong;
  };
  EXPECT_EQ(interruptions(90.0f), 0)
      << "even the perpendicular case exceeded the radius bound";
  EXPECT_EQ(interruptions(45.0f), 0);
  EXPECT_EQ(interruptions(12.5f), 0)
      << "12.5 degrees: the disc's measured failure";
}

TEST(ComposeComposites, ReachReportsTheMarkWhereBleedReportsNothing) {
  // bleed() and reach() are different numbers and a crossing repair needs
  // the second one. bleed() answers "how far outside the node does this
  // escape", which for an Align::Inner stroke is zero, while the MARK it
  // paints is still `width` wide. Sizing a repair from bleed() gives a
  // zero-sized repair for every inner stroke.
  const Decoration inner = stroke(9, red(), PathFormat::Align::Inner);
  EXPECT_EQ(inner.bleed({100, 100}), 0.0f) << "unchanged: it escapes nothing";
  EXPECT_EQ(inner.reach({100, 100}), 9.0f) << "…but the mark is 9px wide";
  const Decoration centred = stroke(9, red());
  EXPECT_EQ(centred.bleed({100, 100}), 4.5f);
  EXPECT_EQ(centred.reach({100, 100}), 9.0f);

  // And it repairs: two CLOSED strands (where Inner is meaningful) — two
  // overlapping circles, which meet at TWO points — stroked Inner, with the
  // rule against list order.
  auto circle = [](float cx, float cy, float r) {
    SkPathBuilder p;
    p.addCircle(cx, cy, r);
    return p.detach();
  };
  Host host(400, 400);
  brush::Weave w = brush::weave(
      {brush::Strand{strand::path(circle(160, 200, 90)),
                     stroke(9, red(), PathFormat::Align::Inner)},
       brush::Strand{strand::path(circle(240, 200, 90)),
                     stroke(9, green(), PathFormat::Align::Inner)}},
      geometry::path::CrossingRule(EveryCrossingRedOnTop{}));
  host.composer.render(stack().child(box().inset(0).stroke(w)));
  host.frame();
  // Walk the red circle's stroke band through the upper crossing region.
  int red = 0, green = 0;
  for (int i = 0; i < 360; ++i) {
    const float a = (float)i * 3.14159265f / 180.0f;
    const int x = (int)std::lround(160 + std::cos(a) * 86.0f);
    const int y = (int)std::lround(200 + std::sin(a) * 86.0f);
    const SkColor c = host.pixel(x, y);
    if (c == SK_ColorRED)
      ++red;
    else if (c == SK_ColorGREEN)
      ++green;
  }
  EXPECT_GT(red, 300) << "the over-strand should own its whole ring";
  EXPECT_EQ(green, 0) << "the under-strand still shows through the mark";
}

TEST(ComposeStrands, AbsoluteOnlyLeavesTheBoundaryUnpainted) {
  // "With only absolute strands the boundary is an unpainted host."
  Host host(200, 200);
  host.composer.render(stack().child(
      box()
          .rect(SkRect::MakeXYWH(40, 40, 100, 100))
          .stroke(brush::weave(
              {brush::Strand{strand::path(diagonal({0, 0}, {100, 0})),
                             brush::solid(6, red())}},
              geometry::path::CrossingRule{}))));
  host.frame();
  EXPECT_EQ(host.pixel(90, 40), SK_ColorRED) << "the authored strand paints";
  EXPECT_EQ(host.pixel(140, 90), SK_ColorBLACK)
      << "the boundary itself is only a host";
}

TEST(ComposeStrands, RelativeStrandsRideTheBandsFrame) {
  // A relative strand is a displacement in the (along, across) frame the
  // BAND owns — positive across is LEFT of travel, i.e. outside a
  // clockwise path. Same convention, one body (profileOffset).
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(100, 100));
  const SkPath rect = b.detach();
  const SkPath out =
      geometry::path::profileOffset(rect, geometry::path::profile::offset(10));
  const SkPath in =
      geometry::path::profileOffset(rect, geometry::path::profile::offset(-10));
  EXPECT_GT(out.getBounds().width(), rect.getBounds().width());
  EXPECT_LT(in.getBounds().width(), rect.getBounds().width());
  // self() is the boundary itself.
  EXPECT_EQ(geometry::path::profile::self().max(), 0.0f);
}

TEST(ComposeStrands, BorrowedStrandsRideTheDerivePass) {
  Host host(200, 200);
  host.composer.render(
      stack()
          .child(box().key("guide").rect(SkRect::MakeXYWH(60, 20, 80, 40)))
          .child(
              box()
                  .rect(SkRect::MakeXYWH(20, 20, 160, 160))
                  .stroke(brush::weave({brush::Strand{strand::from("guide"),
                                                      brush::solid(6, red())}},
                                       geometry::path::CrossingRule{}))));
  host.frame();
  host.frame();  // derive resolves against the first layout
  // The guide's own box outline, painted in the host's local space.
  EXPECT_EQ(host.pixel(100, 20), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 60), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 120), SK_ColorBLACK) << "nothing else moved";
}

TEST(ComposeBrushKinds, SolidIsPathFormatUnderItsTaughtName) {
  // brush::Solid is not a wrapper or a subclass — it is an alias for
  // PathFormat, so the two names build one value and a decoration built
  // through either compares equal to the other.
  static_assert(std::is_same_v<brush::Solid, PathFormat>);
  const brush::Solid a = brush::solid(2, red());
  const PathFormat b = stroke(2, red());
  EXPECT_TRUE(a == b) << "one value, two spellings";
}

TEST(ComposeComposites, ClosedStrandsWrapAtTheirSeam) {
  // A CYCLE has no far end: two knots at along 0.02 and 0.98 are 4% apart,
  // not 96%. Measure the distance between them linearly and crossings that
  // straddle the seam read as maximally distant, which removes the bound on
  // each knot's territory and lets the two repair lenses merge — both knots
  // of two overlapping rings then come out in ONE colour.
  //
  // The geometry here is chosen so BOTH knots are neighbours across their
  // own strand's seam: a large ring crossed by a small one near its edge.
  auto circle = [](float cx, float cy, float r) {
    SkPathBuilder p;
    p.addCircle(cx, cy, r);
    return p.detach();
  };
  const SkPath big = circle(200, 200, 100);
  const SkPath small = circle(288, 200, 13);

  Host host(400, 400);
  host.composer.render(stack().child(box().inset(0).stroke(
      brush::weave({brush::Strand{strand::path(big), stroke(6, red())},
                    brush::Strand{strand::path(small), stroke(6, green())}},
                   geometry::path::crossing::alternate()))));
  host.frame();

  const std::vector<geometry::path::Crossing> knots =
      geometry::path::discoverCrossings({big, small});
  ASSERT_EQ(knots.size(), 2u) << "the two rings meet twice";
  // alternate(): ordinal 0 puts strand 0 (red) over, ordinal 1 puts strand
  // 1 (green) over. Both knots one colour is the defect.
  EXPECT_EQ(host.pixel((int)std::lround(knots[0].at.fX),
                       (int)std::lround(knots[0].at.fY)),
            SK_ColorRED);
  EXPECT_EQ(host.pixel((int)std::lround(knots[1].at.fX),
                       (int)std::lround(knots[1].at.fY)),
            SK_ColorGREEN)
      << "the second knot was swallowed by the first knot's patch";
}

TEST(ComposeComposites, CompositesNest) {
  // "Composites NEST": a strand painted by layers, a whole weave used as
  // one strand of a bigger one. No new vocabulary needed for either.
  Host host(200, 200);
  const brush::Weave inner =
      brush::layers({brush::solid(9, red()), brush::solid(3, green())});
  host.composer.render(stack().child(
      box()
          .rect(SkRect::MakeXYWH(40, 40, 100, 100))
          .stroke(brush::weave(
              {brush::Strand{geometry::path::profile::self(), inner},
               brush::Strand{geometry::path::profile::offset(12),
                             brush::solid(2, blue())}},
              geometry::path::CrossingRule{}))));
  host.frame();
  EXPECT_EQ(host.pixel(90, 40), SK_ColorGREEN) << "the nested layers' top";
  EXPECT_EQ(host.pixel(90, 28), SK_ColorBLUE) << "the offset strand, outside";
}

// ---- one namespace, one name per brush kind -------------------------------
TEST(ComposeBrushKinds, EveryKindAnswersToOneNameUnderOneNamespace) {
  // Every brush kind answers to exactly one name, under `brush::`, with no
  // suffix and no second namespace.
  const brush::Ribbon taught = brush::presets::taper(10, 2, red());
  EXPECT_FLOAT_EQ(taught.widthStart, 10.0f);
  // The taught constructor is the PROFILE one.
  const brush::Ribbon profiled =
      brush::ribbon(geometry::path::profile::offset(9.0f), red());
  EXPECT_TRUE(profiled.hasProfile());
  EXPECT_FLOAT_EQ(profiled.bleed(), 9.0f);
  // The kinds are values under the taught spelling, nothing else.
  static_assert(std::is_default_constructible_v<brush::Pattern>);
  static_assert(std::is_default_constructible_v<brush::Scatter>);
  static_assert(std::is_default_constructible_v<brush::Art>);
}

// ---- cornerAlign is a required argument ------------------------------------
// --------------------------------
TEST(ComposeBrushKinds, CornerArtCannotBeBuiltWithoutItsAlignment) {
  // Corner alignment has no defensible default — bisector and outgoing are
  // both right for different marks — so it is required by the type system
  // rather than defaulted and warned about. There is no way to describe
  // corner art without stating it.
  static_assert(!std::is_default_constructible_v<brush::CornerArt>);
  static_assert(!std::is_constructible_v<brush::CornerArt, Element>);
  static_assert(
      std::is_constructible_v<brush::CornerArt, Element, brush::CornerAlign>);
  // And the alignment participates in equality, so two brushes that differ
  // only in how their corners face do not prune into each other.
  const Element art = box().width(10).height(10).fill(red());
  brush::Pattern a, b;
  a.side = box().width(10).height(2).fill(red());
  b.side = a.side;
  a.corner = brush::CornerArt{art, brush::CornerAlign::Bisector};
  b.corner = brush::CornerArt{art, brush::CornerAlign::Outgoing};
  EXPECT_FALSE(a == b);
  b.corner = brush::CornerArt{art, brush::CornerAlign::Bisector};
  EXPECT_TRUE(a == b);
}

TEST(ComposeDecorations, CompositeBoundsFollowTheResolvedSize) {
  struct RelativeMark {
    bool operator==(const RelativeMark&) const = default;
    float bleed(SkSize size) const { return size.height() / 2; }
    float reach(SkSize size) const { return size.height(); }
    void paint(SkCanvas&, const PaintContext&) const {}
  };
  const Decoration mark = RelativeMark{};
  const Decoration stack = Brush{}.layer(inset(-4, mark));
  const Decoration edge = onEdges(geometry::path::Edge::Top, stack);
  const Decoration woven = brush::layers({edge});
  for (float height : {20.0f, 100.0f, 40.0f}) {
    const SkSize size{80, height};
    EXPECT_FLOAT_EQ(woven.bleed(size), height / 2 + 4);
    EXPECT_FLOAT_EQ(woven.reach(size), height);
  }
}
