// The paint binary's share of ComposeTestEngineDocs.cpp: the suites whose
// subjects are paint-tier values, cut from that file so each test binary links
// only the target it exercises.

#include <utility>

#include "support/PaintTestSupport.h"

TEST(ComposeBrushes, PatternCopyRebakesAllChangedArt) {
  auto art = [](Fill fill) {
    return box().width(12).height(12).fill(std::move(fill));
  };
  brush::Pattern base;
  base.side = box().width(16).height(4).fill(green());
  base.start = art(red());
  base.end = art(red());
  base.corner = brush::CornerArt{art(red()), brush::CornerAlign::Bisector};
  base.advance = 16;

  // Aggregate copies intentionally share the memoization cache. The cache
  // therefore has to key every art slot, not only the side tile.
  brush::Pattern variant = base;
  variant.start = art(blue());
  variant.end = art(blue());
  variant.corner = brush::CornerArt{art(blue()), brush::CornerAlign::Bisector};

  auto lRun = [](brush::Pattern brush) {
    return box().child(box()
                           .absolute()
                           .inset(30)
                           .shape([](SkSize size) {
                             SkPathBuilder path;
                             path.moveTo(0, 0);
                             path.lineTo(size.width(), 0);
                             path.lineTo(size.width(), size.height());
                             return path.detach();
                           })
                           .stroke(std::move(brush)));
  };

  Host donor;
  donor.composer.render(lRun(base));
  donor.frame();  // primes the shared cache with red start/end/corner art
  EXPECT_EQ(donor.pixel(38, 30), SK_ColorRED);
  EXPECT_EQ(donor.pixel(170, 162), SK_ColorRED);

  Host changed;
  changed.composer.render(lRun(variant));
  changed.frame();
  EXPECT_EQ(changed.pixel(38, 30), SK_ColorBLUE);    // changed start art
  EXPECT_EQ(changed.pixel(170, 32), SK_ColorBLUE);   // changed corner art
  EXPECT_EQ(changed.pixel(170, 162), SK_ColorBLUE);  // changed end art
}

namespace {

/** A corner tile whose EXTENT is symmetric but whose colour is not: a 24x8
 *  bar, red on its local -x half and green on its local +x half. The stamp
 *  centres on the art's cull rect, so the midpoint of the two blobs is the
 *  placement and the vector between them is the rotation — both readable
 *  off the pixels, neither inferred. */
Element directedCornerTile() {
  return box()
      .width(24)
      .height(8)
      .child(box().absolute().left(0).top(0).width(12).height(8).fill(
          Fill::color({1, 0, 0, 1})))
      .child(box().absolute().left(12).top(0).width(12).height(8).fill(
          Fill::color({0, 1, 0, 1})));
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

TEST(ComposeBrushes, PatternCornerLandsOnTheVertexAndFacesTheBisector) {
  // Corner placement has two independent ways to go subtly wrong, and a
  // large corner tile against short sides is what makes both visible.
  //
  //  a. The tangent scan STRADDLES the vertex — it compares the tangent at
  //     d − step with the tangent at d — so a bend is first detected one
  //     step AFTER it happens. Taking the midpoint of that bracket as the
  //     vertex puts the art up to half a step along the outgoing leg. The
  //     corner has to be recovered from the two legs, not from the scan
  //     position.
  //  b. A bisector built by re-probing tangents at d ± ε is wrong for the
  //     same reason: from a point already past the vertex, both probes land
  //     on the same leg and every corner ends up facing the outgoing
  //     tangent. The one exception is a closed contour's seam at d = 0,
  //     whose probes wrap onto both legs — so on a rectangle three corners
  //     agree with each other and the fourth sits 45 degrees off, which
  //     reads as "the seam is special" rather than as a general error.
  //
  // A rectangle's vertices are exact, so both claims here are arithmetic
  // rather than approximate.
  Host host(400, 340);
  brush::Pattern brush;
  brush.side = box().width(24).height(10).child(
      box().absolute().left(2).top(4).width(20).height(2).fill(
          Fill::color({0.35f, 0.45f, 0.95f, 1})));
  brush.corner =
      brush::CornerArt{directedCornerTile(), brush::CornerAlign::Bisector};
  brush.advance = 24;
  brush.cornerLength = 40;
  brush.reach = 40;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(0)
                                       .shape([](SkSize) {
                                         SkPathBuilder p;
                                         p.moveTo(100, 100);
                                         p.lineTo(300, 100);
                                         p.lineTo(300, 240);
                                         p.lineTo(100, 240);
                                         p.close();
                                         return p.detach();
                                       })
                                       .stroke(std::move(brush))));
  host.frame();

  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(400, 340));
  ASSERT_TRUE(host.surface->readPixels(bm.pixmap(), 0, 0));
  // Quadrant assignment about the rect's centre — unbiased, and every tile
  // sits within 30 px of its own vertex, nowhere near the middle.
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
  const SkPoint vertex[4] = {{100, 100}, {300, 100}, {100, 240}, {300, 240}};
  for (int q = 0; q < 4; ++q) {
    ASSERT_GT(red[q].n, 20u) << "no corner tile in quadrant " << q;
    ASSERT_GT(green[q].n, 20u) << "no corner tile in quadrant " << q;
    const SkPoint rc = red[q].centre(), gc = green[q].centre();
    const SkPoint place{(rc.x() + gc.x()) * 0.5f, (rc.y() + gc.y()) * 0.5f};
    EXPECT_NEAR(place.x(), vertex[q].x(), 1.5f)
        << "corner " << q << " landed off its vertex in x";
    EXPECT_NEAR(place.y(), vertex[q].y(), 1.5f)
        << "corner " << q << " landed off its vertex in y";
    // Every vertex of an axis-aligned rectangle bisects to a diagonal, so
    // |dx| == |dy|. The outgoing tangent is axis aligned and one of them
    // would be ~0.
    const float dx = std::abs(gc.x() - rc.x()), dy = std::abs(gc.y() - rc.y());
    EXPECT_NEAR(dx, dy, (dx + dy) * 0.2f)
        << "corner " << q << " faces (" << (gc.x() - rc.x()) << ", "
        << (gc.y() - rc.y()) << ") — not the bisector";
  }
}

TEST(ComposeBrushes, PatternCornerAlignOutgoingIsStillAvailable) {
  // The other alignment is what a directional marker wants — an arrow that
  // turns a corner should keep pointing the way it is going.
  Host host(400, 340);
  brush::Pattern brush;
  brush.side =
      box().width(24).height(2).fill(Fill::color({0.2f, 0.2f, 0.6f, 1}));
  brush.corner =
      brush::CornerArt{directedCornerTile(), brush::CornerAlign::Outgoing};
  brush.advance = 24;
  brush.cornerLength = 40;
  brush.reach = 40;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(0)
                                       .shape([](SkSize) {
                                         SkPathBuilder p;
                                         p.moveTo(100, 100);
                                         p.lineTo(300, 100);
                                         p.lineTo(300, 240);
                                         p.lineTo(100, 240);
                                         p.close();
                                         return p.detach();
                                       })
                                       .stroke(std::move(brush))));
  host.frame();
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(400, 340));
  ASSERT_TRUE(host.surface->readPixels(bm.pixmap(), 0, 0));
  Blob red, green;
  for (int y = 0; y < 170; ++y)
    for (int x = 201; x < 400; ++x) {  // the top-right vertex only
      const SkColor c = bm.getColor(x, y);
      const int r = (int)SkColorGetR(c), g = (int)SkColorGetG(c),
                b = (int)SkColorGetB(c);
      if (r > 150 && r > 2 * g && r > 2 * b)
        red.add(x, y);
      else if (g > 150 && g > 2 * r && g > 2 * b)
        green.add(x, y);
    }
  ASSERT_GT(red.n, 20u);
  ASSERT_GT(green.n, 20u);
  // Leaving (300,100) the contour heads straight DOWN: dx ~ 0, dy > 0.
  const SkPoint rc = red.centre(), gc = green.centre();
  EXPECT_NEAR(gc.x() - rc.x(), 0.0f, 2.0f);
  EXPECT_GT(gc.y() - rc.y(), 6.0f);
}

TEST(ComposeBrushTail, GlossContourBandsInsideTheShape) {
  Host plain, glossed;
  auto shape = [] {
    return box()
        .absolute()
        .inset(50, 50, 50, 50)
        .corners({24})
        .fill(Fill::color({0.2f, 0.3f, 0.5f, 1}));
  };
  plain.composer.render(box().child(shape()));
  plain.frame();
  glossed.composer.render(
      box().child(shape().foreground(styles::gloss({1, 1, 1, 1}, 8, {0, -4}))));
  glossed.frame();
  // The band brightens SOME interior pixels but not the deep center
  // (table peaks at mid-coverage, so the middle of the shape stays fill).
  int changed = 0;
  for (int y = 52; y < 148; y += 2)
    for (int x = 52; x < 148; x += 2)
      if (plain.pixel(x, y) != glossed.pixel(x, y)) ++changed;
  EXPECT_GT(changed, 40);  // a real band appeared
  EXPECT_EQ(plain.pixel(100, 100), glossed.pixel(100, 100));  // center: fill
  EXPECT_EQ(plain.pixel(30, 30), glossed.pixel(30, 30));      // outside: clip
}
