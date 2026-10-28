// Where strands cross: the crossing scan and the rule ladder that decides
// who passes over, the composites woven out of them and the cache behind
// the crossings, the strands themselves, and the names the brush kinds
// answer to.

#include <sigilgeometry/advanced/Skia.h>

#include <type_traits>

#include "support/BrushTestSupport.h"
#include "support/Rows.h"

namespace {

/** Two straight strands that cross once, as raw geometry. */
SkPath diagonal(SkPoint a, SkPoint b) {
  SkPathBuilder p;
  p.moveTo(a);
  p.lineTo(b);
  return p.detach();
}

}  // namespace

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

// ---------------------------------------------------------------------------

TEST(ComposeComposites,
     TheCrossingCacheChangesWhenDiscoveryRunsAndNeverWhatIsDrawn) {
  sigil::compose::test::checkRows({
      // The crossing cache: discovery is memoized on the RESOLVED strand paths,
      // which are the function's entire input, and held on the Weave value
      // behind
      // a shared_ptr so copies share it. Three things must hold — staleness
      // through each of the two doors into the key, and byte identity between
      // the
      // cold and cached paints.
      //
      // `crossingCache->computes` counts DISCOVERIES, not paints. That is what
      // makes these cases readable: with Cache::None the node repaints every
      // frame, so a steady frame that does not move the counter proves a hit.
      {"AnAuthoredGeometryChangeDiscoversAgain",
       [&] {
         // Staleness through the AUTHORED door: warm the cache, then edit a
         // strand's path on a copy that shares it.
         //
         // THREE strands, not two, and that is load-bearing. With a lone
         // crossing the knot's territory has no neighbour to bound it, so a
         // stale repair — clipped only by the CURRENT tubes — still happens to
         // cover the new meeting, and the staleness is invisible in pixels. Two
         // knots bound each other's territory, so a stale answer mis-paints
         // somewhere readable.
         //
         // Red runs y=100; green verticals at x=60 and x=140. alternate() puts
         // red over at knot 0 (x=60) and leaves green over at knot 1 (x=140).
         // Moving the first vertical to x=180 makes (140,100) the new knot 0 —
         // red over — while (180,100) keeps green. A stale cache still numbers
         // (140,100) as knot 1 and aims its red repair at (60,100), where
         // nothing is left to repair.
         brush::Weave w =
             brush::weave({brush::Strand{strand::path(geometry::path::fromSk(
                                             diagonal({0, 100}, {200, 100}))),
                                         brush::solid(9, red())},
                           brush::Strand{strand::path(geometry::path::fromSk(
                                             diagonal({60, 0}, {60, 200}))),
                                         brush::solid(9, green())},
                           brush::Strand{strand::path(geometry::path::fromSk(
                                             diagonal({140, 0}, {140, 200}))),
                                         brush::solid(9, green())}},
                          geometry::path::crossing::alternate());
         Host host(240, 240);
         host.composer.render(
             stack().children({box().inset(0).stroke(w).cache(Cache::None)}));
         host.frame();
         EXPECT_EQ(host.pixel(60, 100), SK_ColorRED)
             << "knot 0: red repaired over";
         EXPECT_EQ(host.pixel(140, 100), SK_ColorGREEN) << "knot 1: list order";
         EXPECT_EQ(w.crossingCache->computes, 1)
             << "the cold paint discovers once";
         host.frame();
         EXPECT_EQ(w.crossingCache->computes, 1)
             << "a steady repaint must HIT, not rediscover";

         brush::Weave moved =
             w;  // shares the WARM cache — that is the scenario
         moved.strands[1].path = strand::path(
             geometry::path::fromSk(diagonal({180, 0}, {180, 200})));
         host.composer.render(stack().children(
             {box().inset(0).stroke(moved).cache(Cache::None)}));
         host.frame();
         EXPECT_EQ(host.pixel(140, 100), SK_ColorRED)
             << "stale crossings: (140,100) still numbered as the green knot";
         EXPECT_EQ(host.pixel(180, 100), SK_ColorGREEN)
             << "the new knot 1 keeps list order";
         EXPECT_EQ(w.crossingCache->computes, 2)
             << "the edit must land in the key and rediscover";
       }},
      {"ACachedPaintIsTheColdPaint",
       [&] {
         // The cache may change WHEN discovery runs and never what is drawn.
         // Frame 1 discovers cold, frame 2 hits, and a fresh equal value
         // discovers cold again — all three surfaces must be byte-equal.
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
               {brush::Strand{strand::path(geometry::path::fromSk(
                                  diagonal({20, 20}, {180, 180}))),
                              brush::solid(9, red())},
                brush::Strand{strand::path(geometry::path::fromSk(
                                  diagonal({20, 180}, {180, 20}))),
                              brush::solid(9, green())}},
               geometry::path::crossing::alternate());
         };
         brush::Weave w = weaveX();
         Host host(240, 240);
         host.composer.render(
             stack().children({box().inset(0).stroke(w).cache(Cache::None)}));
         host.frame();
         const std::vector<uint8_t> cold = bytes(host);
         EXPECT_EQ(w.crossingCache->computes, 1);
         host.frame();
         EXPECT_EQ(w.crossingCache->computes, 1) << "frame 2 must be a HIT";
         EXPECT_TRUE(cold == bytes(host))
             << "the cache-hit frame drew differently";

         brush::Weave fresh = weaveX();  // its own cold cache
         Host host2(240, 240);
         host2.composer.render(stack().children(
             {box().inset(0).stroke(fresh).cache(Cache::None)}));
         host2.frame();
         EXPECT_EQ(fresh.crossingCache->computes, 1);
         EXPECT_TRUE(cold == bytes(host2))
             << "cold and cached must be one picture, byte for byte";
       }},
  });
}

TEST(ComposeComposites,
     AWeavePaintsInListOrderAndRepairsEachCrossingByItsRule) {
  sigil::compose::test::checkRows({
      {"TheRuleDecidesEachCrossing",
       [&] {
         // Two authored strands crossing once. Under list order the second is
         // on top; alternate() says the FIRST passes over at crossing 0, so the
         // repair patch must put strand 0's colour at the meeting.
         auto draw = [](geometry::path::CrossingRule rule) {
           Host host(200, 200);
           brush::Weave w =
               brush::weave({brush::Strand{strand::path(geometry::path::fromSk(
                                               diagonal({20, 20}, {180, 180}))),
                                           brush::solid(9, red())},
                             brush::Strand{strand::path(geometry::path::fromSk(
                                               diagonal({20, 180}, {180, 20}))),
                                           brush::solid(9, green())}},
                            std::move(rule));
           host.composer.render(stack().children({box().inset(0).stroke(w)}));
           host.frame();
           return host.pixel(100, 100);
         };
         EXPECT_EQ(draw(geometry::path::CrossingRule{}), SK_ColorGREEN)
             << "list order: later on top";
         EXPECT_EQ(draw(geometry::path::crossing::alternate()), SK_ColorRED)
             << "rule flipped it";
         geometry::path::CrossingRule pinned =
             geometry::path::crossing::alternate();
         pinned.except(0, geometry::path::Order::Under);
         EXPECT_EQ(draw(pinned), SK_ColorGREEN) << "the pin overrode the rule";
       }},
      {"TheRepairCoversShallowCrossings",
       [&] {
         // A crossing repair sized as a disc under-covers at SHALLOW angles:
         // two marks overlap in a lens whose extent grows as reach/sin(theta),
         // so a radius chosen for the perpendicular case leaves the
         // under-strand showing straight across the over-strand. The repair has
         // to follow the lens.
         //
         // Checked by sampling ALONG the over-strand through the meeting: every
         // sample must be the over-strand's colour.
         //
         // The two strands are one segment rotated by ± half the crossing angle
         // about the centre. Building them instead from a shared dx with
         // dy = dx·tan(angle) sends the coordinates to infinity at 90 degrees.
         //
         // Align::Inner is not exercised here and cannot be: an OPEN strand has
         // no inside. It is covered by
         // ReachReportsTheMarkWhereBleedReportsNothing below, on closed
         // strands.
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
               {brush::Strand{
                    strand::path(geometry::path::fromSk(through(dirA))),
                    stroke(9, red())},
                brush::Strand{
                    strand::path(geometry::path::fromSk(through(dirB))),
                    stroke(9, green())}},
               geometry::path::crossing::alternate());  // strand 0 (red) passes
                                                        // OVER at crossing 0
           host.composer.render(stack().children({box().inset(0).stroke(w)}));
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
       }},
      {"TheRepairIsSizedByReachNotBleed",
       [&] {
         // bleed() and reach() are different numbers and a crossing repair
         // needs the second one. bleed() answers "how far outside the node does
         // this escape", which for an Align::Inner stroke is zero, while the
         // MARK it paints is still `width` wide. Sizing a repair from bleed()
         // gives a zero-sized repair for every inner stroke.
         const Decoration inner = stroke(9, red(), PathFormat::Align::Inner);
         EXPECT_EQ(inner.bleed({100, 100}), 0.0f)
             << "unchanged: it escapes nothing";
         EXPECT_EQ(inner.reach({100, 100}), 9.0f)
             << "…but the mark is 9px wide";
         // A centred stroke escapes by its half width, and by four half-widths
         // at a mitred corner, the join it meets its corners with by default.
         const Decoration centred = stroke(9, red());
         EXPECT_EQ(centred.bleed({100, 100}), 18.0f);
         EXPECT_EQ(centred.reach({100, 100}), 9.0f);
         PathFormat rounded = stroke(9, red());
         rounded.join = geometry::path::Join::Round;
         EXPECT_EQ(Decoration(rounded).bleed({100, 100}), 4.5f);

         // And it repairs: two CLOSED strands (where Inner is meaningful) — two
         // overlapping circles, which meet at TWO points — stroked Inner, with
         // the rule against list order.
         auto circle = [](float cx, float cy, float r) {
           SkPathBuilder p;
           p.addCircle(cx, cy, r);
           return p.detach();
         };
         Host host(400, 400);
         brush::Weave w = brush::weave(
             {brush::Strand{
                  strand::path(geometry::path::fromSk(circle(160, 200, 90))),
                  stroke(9, red(), PathFormat::Align::Inner)},
              brush::Strand{
                  strand::path(geometry::path::fromSk(circle(240, 200, 90))),
                  stroke(9, green(), PathFormat::Align::Inner)}},
             geometry::path::CrossingRule(EveryCrossingRedOnTop{}));
         host.composer.render(stack().children({box().inset(0).stroke(w)}));
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
       }},
      {"AbsoluteStrandsLeaveTheBoundaryUnpainted",
       [&] {
         // "With only absolute strands the boundary is an unpainted host."
         Host host(200, 200);
         host.composer.render(stack().children(
             {box()
                  .rect(40, 40, 100, 100)
                  .stroke(brush::weave(
                      {brush::Strand{strand::path(geometry::path::fromSk(
                                         diagonal({0, 0}, {100, 0}))),
                                     brush::solid(6, red())}},
                      geometry::path::CrossingRule{}))}));
         host.frame();
         EXPECT_EQ(host.pixel(90, 40), SK_ColorRED)
             << "the authored strand paints";
         EXPECT_EQ(host.pixel(140, 90), SK_ColorBLACK)
             << "the boundary itself is only a host";
       }},
      {"ABorrowedStrandRidesTheDerivePass",
       [&] {
         Host host(200, 200);
         host.composer.render(stack().children(
             {box().key("guide").rect(60, 20, 80, 40),
              box()
                  .rect(20, 20, 160, 160)
                  .stroke(brush::weave({brush::Strand{strand::from("guide"),
                                                      brush::solid(6, red())}},
                                       geometry::path::CrossingRule{}))}));
         host.frame();
         host.frame();  // derive resolves against the first layout
         // The guide's own box outline, painted in the host's local space.
         EXPECT_EQ(host.pixel(100, 20), SK_ColorRED);
         EXPECT_EQ(host.pixel(100, 60), SK_ColorRED);
         EXPECT_EQ(host.pixel(100, 120), SK_ColorBLACK) << "nothing else moved";
       }},
  });
}

// ---- one namespace, one name per brush kind -------------------------------

// ---- cornerAlign is a required argument ------------------------------------
// --------------------------------
TEST(ComposeBrushKinds, EveryBrushKindIsOneValueUnderOneName) {
  sigil::compose::test::checkRows({
      {"SolidIsPathFormat",
       [&] {
         // brush::Solid is not a wrapper or a subclass — it is an alias for
         // PathFormat, so the two names build one value and a decoration built
         // through either compares equal to the other.
         static_assert(std::is_same_v<brush::Solid, PathFormat>);
         const brush::Solid a = brush::solid(2, red());
         const PathFormat b = stroke(2, red());
         EXPECT_TRUE(a == b) << "one value, two spellings";
       }},
      {"EveryKindAnswersToOneName",
       [&] {
         // Every brush kind answers to exactly one name, under `brush::`, with
         // no suffix and no second namespace.
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
       }},
      {"CornerArtNeedsItsAlignment",
       [&] {
         // Corner alignment has no defensible default — bisector and outgoing
         // are both right for different marks — so it is required by the type
         // system rather than defaulted and warned about. There is no way to
         // describe corner art without stating it.
         static_assert(!std::is_default_constructible_v<brush::CornerArt>);
         static_assert(!std::is_constructible_v<brush::CornerArt, Element>);
         static_assert(std::is_constructible_v<brush::CornerArt, Element,
                                               brush::CornerAlign>);
         // And the alignment participates in equality, so two brushes that
         // differ only in how their corners face do not prune into each other.
         const Element art = box().width(10).height(10).fill(red());
         brush::Pattern a, b;
         a.side = box().width(10).height(2).fill(red());
         b.side = a.side;
         a.corner = brush::CornerArt{art, brush::CornerAlign::Bisector};
         b.corner = brush::CornerArt{art, brush::CornerAlign::Outgoing};
         EXPECT_FALSE(a == b);
         b.corner = brush::CornerArt{art, brush::CornerAlign::Bisector};
         EXPECT_TRUE(a == b);
       }},
  });
}

TEST(ComposeDecorations, CompositeBoundsFollowTheResolvedSize) {
  struct RelativeMark {
    bool operator==(const RelativeMark&) const = default;
    float bleed(glm::vec2 size) const { return size.y / 2; }
    float reach(glm::vec2 size) const { return size.y; }
    void paint(sigil::draw::Pen& pen, const PaintContext&) const {}
  };
  const Decoration mark = RelativeMark{};
  const Decoration stack = Brush{}.layer(inset(-4, mark));
  const Decoration edge = onEdges(geometry::path::Edge::Top, stack);
  const Decoration woven = brush::layers({edge});
  for (float height : {20.0f, 100.0f, 40.0f}) {
    const glm::vec2 size{80, height};
    EXPECT_FLOAT_EQ(woven.bleed(size), height / 2 + 4);
    EXPECT_FLOAT_EQ(woven.reach(size), height);
  }
}
