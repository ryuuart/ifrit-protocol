// The brush engine: the layered stack, the geometry ops a brush is
// restyled with, the art warp, the hatch and the contour bands the gloss
// look is built from, and the pattern art a copy rebakes.

#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Contour.h>

#include "support/BrushTestSupport.h"
#include "support/Rows.h"

TEST(ComposeBrushEngine, ArtBendsAlongTheOutlineInABandOfItsHeight) {
  Host host;
  // A straight horizontal outline through the node's middle: the warped
  // ribbon must be a horizontal band of the art's height around it.
  auto lineOutline = skiaShape([](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() / 2);
    b.lineTo(s.width(), s.height() / 2);
    return b.detach();
  });
  brush::Art brush = brush::artAlong(
      box().width(40).height(20).fill(Fill::color({1, 1, 1, 1})), 20);
  host.composer.render(box().children(
      {box().absolute().inset(60, 20).shape(lineOutline).foreground(brush)}));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorWHITE);  // on the ribbon
  EXPECT_EQ(host.pixel(100, 130), SK_ColorBLACK);  // 30px off: outside height
}

TEST(ComposeBrushValues, WhatAMarkKeepsBetweenPaintsStandsOnTheInstance) {
  sigil::compose::test::checkRows({
      {"AContourWalkCopyBakesItsChangedStamp",
       [&] {
         ContourWalk base;
         base.spacing = 1000.0f;  // one stamp at the open route's first point
         base.stamp = box().width(12).height(12).fill(red());
         ContourWalk variant = base;
         variant.stamp = box().width(12).height(12).fill(blue());

         Host donor;
         donor.composer.render(straightRun(base));
         donor.frame();  // primes the shared cache with the red stamp
         EXPECT_EQ(donor.pixel(20, 100), SK_ColorRED);

         Host changed;
         changed.composer.render(straightRun(variant));
         changed.frame();
         EXPECT_EQ(changed.pixel(20, 100), SK_ColorBLUE);
       }},
      {"APatternCopyBakesAllItsChangedArt",
       [&] {
         auto art = [](Fill fill) {
           return box().width(12).height(12).fill(std::move(fill));
         };
         brush::Pattern base;
         base.side = box().width(16).height(4).fill(green());
         base.start = art(red());
         base.end = art(red());
         base.corner =
             brush::CornerArt{art(red()), brush::CornerAlign::Bisector};
         base.advance = 16;

         // Aggregate copies intentionally share the memoization cache. The
         // cache therefore has to key every art slot, not only the side tile.
         brush::Pattern variant = base;
         variant.start = art(blue());
         variant.end = art(blue());
         variant.corner =
             brush::CornerArt{art(blue()), brush::CornerAlign::Bisector};

         auto lRun = [](brush::Pattern brush) {
           return box().children({box()
                                      .absolute()
                                      .inset(30)
                                      .shape(skiaShape([](SkSize size) {
                                        SkPathBuilder path;
                                        path.moveTo(0, 0);
                                        path.lineTo(size.width(), 0);
                                        path.lineTo(size.width(),
                                                    size.height());
                                        return path.detach();
                                      }))
                                      .stroke(std::move(brush))});
         };

         Host donor;
         donor.composer.render(lRun(base));
         donor
             .frame();  // primes the shared cache with red start/end/corner art
         EXPECT_EQ(donor.pixel(38, 30), SK_ColorRED);
         EXPECT_EQ(donor.pixel(170, 162), SK_ColorRED);

         Host changed;
         changed.composer.render(lRun(variant));
         changed.frame();
         EXPECT_EQ(changed.pixel(38, 30), SK_ColorBLUE);   // changed start art
         EXPECT_EQ(changed.pixel(170, 32), SK_ColorBLUE);  // changed corner art
         EXPECT_EQ(changed.pixel(170, 162), SK_ColorBLUE);  // changed end art
       }},
  });
}

namespace {

/** A corner tile whose EXTENT is symmetric but whose colour is not: a 24x8
 *  bar, red on its local -x half and green on its local +x half. The stamp
 *  centres on the art's cull rect, so the midpoint of the two blobs is the
 *  placement and the vector between them is the rotation — both readable
 *  off the pixels, neither inferred. */
Element directedCornerTile() {
  return box().width(24).height(8).children(
      {box().absolute().left(0).top(0).width(12).height(8).fill(
           Fill::color({1, 0, 0, 1})),
       box().absolute().left(12).top(0).width(12).height(8).fill(
           Fill::color({0, 1, 0, 1}))});
}

struct Blob {
  double x = 0, y = 0;
  size_t n = 0;
  void add(int px, int py) {
    x += px;
    y += py;
    ++n;
  }
  SkPoint centre() const {
    return {(float)(x / (double)n), (float)(y / (double)n)};
  }
};

}  // namespace

// ---------------------------------------------------------------------------
// The numbers a brush is asked for, and the ones it refuses

TEST(ComposeBrushValues, AStampedBrushPlacesItsArtWhereItsPlacementSays) {
  sigil::compose::test::checkRows({
      {"VertexAndSegmentCentrePlacement",
       [&] {
         // Vertex family reads the path's actual verbs — stamps sit ON the
         // bends.
         Host host;
         brush::Scatter b;
         b.art = box().width(8).height(8).fill(red());
         b.place = {brush::Placement::Mode::InnerVertices};
         b.alignToPath = false;
         host.composer.render(box().children(
             {box()
                  .absolute()
                  .inset(40)
                  .shape(skiaShape([](SkSize s) {
                    SkPathBuilder p;  // three segments, two bends
                    p.moveTo(0, s.height());
                    p.lineTo(60, s.height());
                    p.lineTo(60, 0);
                    p.lineTo(s.width(), 0);
                    return p.detach();
                  }))
                  .stroke(std::move(b))}));
         host.frame();
         EXPECT_EQ(host.pixel(100, 160), SK_ColorRED);   // bend 1 (60,120)+40
         EXPECT_EQ(host.pixel(100, 40), SK_ColorRED);    // bend 2 (60,0)+40
         EXPECT_EQ(host.pixel(40, 160), SK_ColorBLACK);  // endpoints excluded

         Host centers;
         brush::Scatter c;
         c.art = box().width(8).height(8).fill(blue());
         c.place = {brush::Placement::Mode::SegmentCenter};
         c.alignToPath = false;
         centers.composer.render(
             box().children({box()
                                 .absolute()
                                 .inset(40)
                                 .shape(skiaShape([](SkSize s) {
                                   SkPathBuilder p;
                                   p.moveTo(0, 0);
                                   p.lineTo(s.width(), 0);
                                   return p.detach();
                                 }))
                                 .stroke(std::move(c))}));
         centers.frame();
         EXPECT_EQ(centers.pixel(100, 40),
                   SK_ColorBLUE);  // the segment midpoint
       }},
      {"ACornerTileLandsOnItsVertexFacingTheBisector",
       [&] {
         // Corner placement has two independent ways to go subtly wrong, and a
         // large corner tile against short sides is what makes both visible.
         //
         //  a. The tangent scan STRADDLES the vertex — it compares the tangent
         //  at
         //     d − step with the tangent at d — so a bend is first detected one
         //     step AFTER it happens. Taking the midpoint of that bracket as
         //     the vertex puts the art up to half a step along the outgoing
         //     leg. The corner has to be recovered from the two legs, not from
         //     the scan position.
         //  b. A bisector built by re-probing tangents at d ± ε is wrong for
         //  the
         //     same reason: from a point already past the vertex, both probes
         //     land on the same leg and every corner ends up facing the
         //     outgoing tangent. The one exception is a closed contour's seam
         //     at d = 0, whose probes wrap onto both legs — so on a rectangle
         //     three corners agree with each other and the fourth sits 45
         //     degrees off, which reads as "the seam is special" rather than as
         //     a general error.
         //
         // A rectangle's vertices are exact, so both claims here are arithmetic
         // rather than approximate.
         Host host(400, 340);
         brush::Pattern brush;
         brush.side = box().width(24).height(10).children(
             {box().absolute().left(2).top(4).width(20).height(2).fill(
                 Fill::color({0.35f, 0.45f, 0.95f, 1}))});
         brush.corner = brush::CornerArt{directedCornerTile(),
                                         brush::CornerAlign::Bisector};
         brush.advance = 24;
         brush.cornerLength = 40;
         brush.bleedPx = 40;
         host.composer.render(box().children({box()
                                                  .absolute()
                                                  .inset(0)
                                                  .shape(skiaShape([] {
                                                    SkPathBuilder p;
                                                    p.moveTo(100, 100);
                                                    p.lineTo(300, 100);
                                                    p.lineTo(300, 240);
                                                    p.lineTo(100, 240);
                                                    p.close();
                                                    return p.detach();
                                                  }))
                                                  .stroke(std::move(brush))}));
         host.frame();

         SkBitmap bm;
         bm.allocPixels(SkImageInfo::MakeN32Premul(400, 340));
         ASSERT_TRUE(host.surface->readPixels(bm.pixmap(), 0, 0));
         // Quadrant assignment about the rect's centre — unbiased, and every
         // tile sits within 30 px of its own vertex, nowhere near the middle.
         Blob red[4], green[4];
         for (int y = 0; y < 340; ++y)
           for (int x = 0; x < 400; ++x) {
             const SkColor c = bm.getColor(x, y);
             const int r = (int)SkColorGetR(c), g = (int)SkColorGetG(c),
                       b = (int)SkColorGetB(c);
             const int q = (x > 200 ? 1 : 0) + (y > 170 ? 2 : 0);
             if (r > 150 && r > 2 * g && r > 2 * b)
               red[q].add(x, y);
             else if (g > 150 && g > 2 * r && g > 2 * b)
               green[q].add(x, y);
           }

         // quadrant order: 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = BR
         const SkPoint vertex[4] = {
             {100, 100}, {300, 100}, {100, 240}, {300, 240}};
         for (int q = 0; q < 4; ++q) {
           ASSERT_GT(red[q].n, 20u) << "no corner tile in quadrant " << q;
           ASSERT_GT(green[q].n, 20u) << "no corner tile in quadrant " << q;
           const SkPoint rc = red[q].centre(), gc = green[q].centre();
           const SkPoint place{(rc.x() + gc.x()) * 0.5f,
                               (rc.y() + gc.y()) * 0.5f};
           EXPECT_NEAR(place.x(), vertex[q].x(), 1.5f)
               << "corner " << q << " landed off its vertex in x";
           EXPECT_NEAR(place.y(), vertex[q].y(), 1.5f)
               << "corner " << q << " landed off its vertex in y";
           // Every vertex of an axis-aligned rectangle bisects to a diagonal,
           // so |dx| == |dy|. The outgoing tangent is axis aligned and one of
           // them would be ~0.
           const float dx = std::abs(gc.x() - rc.x()),
                       dy = std::abs(gc.y() - rc.y());
           EXPECT_NEAR(dx, dy, (dx + dy) * 0.2f)
               << "corner " << q << " faces (" << (gc.x() - rc.x()) << ", "
               << (gc.y() - rc.y()) << ") — not the bisector";
         }
       }},
      {"AContourOfNoLengthIsSkipped",
       [&] {
         // A contour of no length gives a fractional interval a step of zero,
         // and an offset before its start then never reaches its end: the walk
         // stands still. The degenerate contour is skipped and the real one is
         // stamped.
         brush::Scatter s;
         s.art = box().width(6).height(6).fill(red());
         s.place = {.mode = brush::Placement::Mode::Interval,
                    .interval = 0.25f,
                    .offset = -5.0f};
         Host host;
         host.composer.render(
             box().children({box()
                                 .absolute()
                                 .inset(80, 20)
                                 .shape(skiaShape([](SkSize s) {
                                   SkPathBuilder b;
                                   b.moveTo(0, s.height() / 2);
                                   b.close();  // no length at all
                                   b.moveTo(0, s.height() / 2);
                                   b.lineTo(s.width(), s.height() / 2);
                                   return b.detach();
                                 }))
                                 .stroke(Decoration(s))}));
         host.frame();
         SUCCEED();  // it finished
       }},
  });
}

TEST(ComposeBrushEngine, AnEqualDescriptionPrunesAndAChangedOnePatches) {
  sigil::compose::test::checkRows({
      {"ABrushPrunesAsOneValue",
       [&] {
         Host host;
         auto tree = [] {
           Brush b;
           b.shaped(geometry::shapers::Rounded{6})
               .shaped(
                   geometry::shapers::Wave{.amplitude = 3, .wavelength = 30})
               .layer(lines::presets::cased(3, Fill::color({0, 1, 0, 1}), 5));
           return box().children(
               {box().absolute().inset(40).stroke(std::move(b))});
         };
         host.composer.render(tree());
         host.frame();
         host.composer.render(
             tree());  // fresh Elements, identical brush values
         EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
         host.frame();
         EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
       }},
      {"ARestyleComparesByTheShaperItHolds",
       [&] {
         // The shaper a restyle carries is comparable when it was built from a
         // comparable shaper value, and never when it was built from a raw
         // callable — which is the whole difference between the two doors.
         const auto carried = [](geometry::path::Shaper shaper) {
           return brush::restyle(std::move(shaper),
                                 brush::solid(1.0f, Fill::color({1, 1, 1, 1})))
               .operation;
         };
         const geometry::path::Shaper wave =
             carried(geometry::shapers::Wave{.amplitude = 8, .wavelength = 24});
         const geometry::path::Shaper same =
             carried(geometry::shapers::Wave{.amplitude = 8, .wavelength = 24});
         const geometry::path::Shaper wider = carried(
             geometry::shapers::Wave{.amplitude = 12, .wavelength = 24});
         const geometry::path::Shaper raw =
             carried(geometry::path::Shaper::incomparable(
                 [](const geometry::path::Outline& outline) { return outline; },
                 3.0f));
         EXPECT_TRUE(wave == same);
         EXPECT_FALSE(wave == wider);
         EXPECT_FALSE(raw == raw);
         EXPECT_FALSE(raw.comparable());
         EXPECT_FLOAT_EQ(raw.bleed(), 3.0f);
       }},
  });
}
