// The box a node is given and the order siblings paint in: flex sizing and
// fills, a user layout scheme placing its own cells, z-index and the
// composite an opacity or blend implies, per-axis scale about a transform
// origin, and the child-bounds union a parent reports.

#include "support/CoreTestSupport.h"

TEST(ComposeLayout, FlexRowPositionsAndFills) {
  Host host;
  host.composer.render(box()
                           .row()
                           .gap(20)
                           .child(box().width(50).height(50).fill(red()))
                           .child(box().width(50).height(50).fill(green())));
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);    // first child
  EXPECT_EQ(host.pixel(60, 25), SK_ColorBLACK);  // the gap
  EXPECT_EQ(host.pixel(95, 25), SK_ColorGREEN);  // second child at 70..120
}

TEST(ComposeLayout, TextSizesItselfInFlex) {
  Host host(400, 200);
  host.composer.render(
      box().row().padding(10).child(text(u8"Hello compose", styleAt(24))));
  host.frame();
  auto rect = host.composer.bounds("t");
  (void)rect;
  // The text node measured to a plausible single-line extent.
  host.composer.render(box().row().padding(10).child(
      text(u8"Hello compose", styleAt(24)).key("t")));
  host.frame();
  auto measured = host.composer.bounds("t");
  ASSERT_TRUE(measured.has_value());
  EXPECT_GT(measured->width(), 60.0f);
  EXPECT_LT(measured->width(), 380.0f);
  EXPECT_GT(measured->height(), 10.0f);
  EXPECT_LT(measured->height(), 60.0f);
  EXPECT_EQ(measured->left(), 10.0f);
  ASSERT_NE(host.composer.paragraphLayout("t"), nullptr);
}

TEST(ComposeStacking, ZIndexReordersSiblings) {
  Host host;
  // Later sibling has LOWER zIndex → paints first → red wins on top.
  host.composer.render(stack()
                           .child(box().inset(0).fill(red()).zIndex(1))
                           .child(box().inset(0).fill(green()).zIndex(0)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
}

TEST(ComposeStacking, OpacityAndBlendComposite) {
  Host host;
  host.composer.render(stack()
                           .child(box().inset(0).fill(red()))
                           .child(box().inset(0).fill(blue()).opacity(0.5f)));
  host.frame();
  SkColor c = host.pixel(100, 100);
  // Half blue over red: both channels present.
  EXPECT_NEAR(SkColorGetR(c), 128, 10);
  EXPECT_NEAR(SkColorGetB(c), 128, 10);
}

namespace {

/** A lightweight grid, ~20 lines of user code. */
struct Grid {
  int columns = 2;
  float gap = 8;
  float cellHeight = 40;

  std::vector<SkRect> place(const LayoutInput& in) const {
    std::vector<SkRect> rects;
    const float cellWidth =
        (in.container.width() - gap * (float)(columns - 1)) / (float)columns;
    for (size_t i = 0; i < in.childSizes.size(); ++i) {
      const int col = (int)i % columns;
      const int row = (int)i / columns;
      rects.push_back(SkRect::MakeXYWH((cellWidth + gap) * (float)col,
                                       (cellHeight + gap) * (float)row,
                                       cellWidth, cellHeight));
    }
    return rects;
  }
};

}  // namespace

TEST(ComposeLayoutScheme, GridPlacesAndSizesCells) {
  Host host(200, 200);
  auto grid = layout(Grid{.columns = 2, .gap = 10, .cellHeight = 30})
                  .width(190)
                  .height(190);
  for (int i = 0; i < 4; ++i)
    grid.child(
        box().key("cell" + std::to_string(i)).fill(i % 2 ? green() : red()));
  host.composer.render(box().child(std::move(grid)));
  host.frame();

  auto c0 = host.composer.bounds("cell0");
  auto c1 = host.composer.bounds("cell1");
  auto c3 = host.composer.bounds("cell3");
  ASSERT_TRUE(c0 && c1 && c3);
  EXPECT_EQ(c0->left(), 0.0f);
  EXPECT_EQ(c0->width(), 90.0f);  // (190 - 10) / 2
  EXPECT_EQ(c0->height(), 30.0f);
  EXPECT_EQ(c1->left(), 100.0f);  // second column
  EXPECT_EQ(c3->top(), 40.0f);    // second row
  EXPECT_EQ(host.pixel(45, 15), SK_ColorRED);
  EXPECT_EQ(host.pixel(145, 15), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(145, 55), SK_ColorGREEN);
}

TEST(ComposeTransform, ScaleXGrowsFromItsOrigin) {
  // The bar primitive. transformOrigin pins the LEFT edge, scaleX carries
  // the fraction, and the fill grows rightward — no clip, no counter-
  // translation, and correct for any fill (the translate-inside-a-clip
  // workaround is only correct for gradients along the other axis).
  Host host(200, 40);
  choreograph::Output<float> fraction{0.25f};
  host.composer.render(
      box().child(box()
                      .width(200)
                      .height(40)
                      .absolute()
                      .left(0)
                      .top(0)
                      .transformOrigin(0.0f, 0.5f)
                      .scaleX(&fraction)
                      .fill(material::skia::Paint::solid({1, 0, 0, 1}))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(20, 20)), 200u);  // inside the quarter
  EXPECT_LT(SkColorGetR(host.pixel(80, 20)), 60u);   // past it
  fraction = 0.75f;  // bound value moves — no re-render
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(80, 20)), 200u);
  EXPECT_LT(SkColorGetR(host.pixel(180, 20)), 60u);
}

TEST(ComposeTransform, ScaleYIsIndependentOfScaleX) {
  Host host(200, 200);
  host.composer.render(
      box().child(box()
                      .width(200)
                      .height(200)
                      .absolute()
                      .left(0)
                      .top(0)
                      .transformOrigin(0.0f, 0.0f)
                      .scaleX(0.25f)
                      .scaleY(0.75f)
                      .fill(material::skia::Paint::solid({0, 1, 0, 1}))));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(10, 10)), 200u);   // inside both
  EXPECT_LT(SkColorGetG(host.pixel(90, 10)), 60u);    // past x, inside y
  EXPECT_GT(SkColorGetG(host.pixel(10, 140)), 200u);  // inside x, inside y
  EXPECT_LT(SkColorGetG(host.pixel(10, 190)), 60u);   // past y
}

TEST(ComposePaintBounds, PerAxisScaleReachesTheParentsChildBoundsUnion) {
  // `recordBounds()` decides whether a child's transform widens the parent's
  // bounds, and it must recognise exactly the transforms `NodeTransform`
  // applies for paint() and hitInstance() — per-axis scale included. Miss one
  // and a child whose ONLY transform is a per-axis scale hands its parent
  // unscaled bounds, and every consumer sized off them (the effect layer
  // here, the opacity layer, the texture bake) silently truncates the
  // overflow.
  //
  // The parent takes an identity offset() filter purely to force a bounded
  // saveLayer: that layer clips to recordBounds(), so wrong bounds delete the
  // scaled-out half of the bar instead of merely mis-sizing something.
  Host host(200, 200);
  host.composer.render(
      box().child(box()
                      .absolute()
                      .rect(SkRect::MakeXYWH(20, 20, 40, 40))
                      .effect(material::skia::Effect::filter(
                          SkImageFilters::Offset(0, 0, nullptr)))
                      .child(box()
                                 .absolute()
                                 .rect(SkRect::MakeXYWH(0, 0, 40, 40))
                                 .transformOrigin(0, 0)
                                 .fill(red())
                                 .scaleX(3.0f))));
  host.frame();
  EXPECT_EQ(host.pixel(30, 40), SK_ColorRED) << "the unscaled part is missing";
  EXPECT_EQ(host.pixel(120, 40), SK_ColorRED)
      << "the scaled-out part of the bar was clipped away — recordBounds() "
         "did not see scaleX on the child";
  EXPECT_EQ(host.pixel(150, 40), SK_ColorBLACK) << "…and it over-reached";
}
