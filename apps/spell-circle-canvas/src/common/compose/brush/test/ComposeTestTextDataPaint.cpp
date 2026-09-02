// The paint binary's share of ComposeTestTextData.cpp: the suites whose
// subjects are paint-tier values, cut from that file so each test binary links
// only the target it exercises.

#include "support/PaintTestSupport.h"

TEST(ComposePattern, ARepeatCanBePanned) {
  // A repeat's PHASE is a defining property of a surprising number of
  // patterns — a twill advances one thread per pick — so Pattern exposes the
  // translation part of the matrix it already hands to Material::image.
  // Without it, a pattern can be scaled and rotated but not offset, which is
  // two thirds of a matrix its own backend takes whole.
  auto stripes = [](SkPoint pan) {
    Pattern p = Pattern::tile({8, 8}, [](SkCanvas& c, SkSize s, uint32_t) {
      SkPaint left;
      left.setColor4f({1, 0, 0, 1}, nullptr);
      c.drawRect(SkRect::MakeWH(s.width() * 0.5f, s.height()), left);
      SkPaint right;
      right.setColor4f({0, 1, 0, 1}, nullptr);
      c.drawRect(
          SkRect::MakeXYWH(s.width() * 0.5f, 0, s.width() * 0.5f, s.height()),
          right);
    });
    p.offset(pan).sampling(SkSamplingOptions(SkFilterMode::kNearest));
    return p.material();
  };
  auto colourAt = [](material::skia::Paint m, int x) {
    Host host(64, 64);
    host.composer.render(
        box().child(box().absolute().inset(0).fill(std::move(m))));
    host.frame();
    return host.pixel(x, 32);
  };

  // Unpanned: the left half of each 8px tile is red.
  const SkColor unpanned = colourAt(stripes({0, 0}), 1);
  EXPECT_GT(SkColorGetR(unpanned), 180);
  EXPECT_LT(SkColorGetG(unpanned), 80);

  // Panned by half a tile: the same pixel is now green. Nothing rebakes.
  const SkColor panned = colourAt(stripes({4, 0}), 1);
  EXPECT_GT(SkColorGetG(panned), 180);
  EXPECT_LT(SkColorGetR(panned), 80);
}

TEST(ComposePatterns, GridLinesTakeATwoAxisPitch) {
  // A lattice whose x and y pitch differ is not exotic — an X-COM control
  // panel's is 5 x 2 — and gridLines took one `spacing`.
  Host host(120, 120);
  host.composer.render(box().child(box().absolute().inset(0).fill(
      Pattern(material::pattern::gridLines(20.0f, 8.0f, 2.0f, {1, 1, 1, 1}))
          .material())));
  host.frame();
  auto rules = [&](bool vertical) {
    int runs = 0;
    bool on = false;
    for (int i = 0; i < 120; ++i) {
      // Sample mid-cell on the other axis: a column that lands ON a
      // vertical rule is lit for its whole length and counts one run.
      const SkColor c = vertical ? host.pixel(i, 63) : host.pixel(70, i);
      const bool lit = SkColorGetR(c) > 128;
      runs += lit && !on;
      on = lit;
    }
    return runs;
  };
  EXPECT_NEAR(rules(/*vertical=*/true), 6, 1);    // 120 / 20
  EXPECT_NEAR(rules(/*vertical=*/false), 15, 2);  // 120 / 8
}
