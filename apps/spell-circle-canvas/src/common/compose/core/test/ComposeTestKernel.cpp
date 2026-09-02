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

TEST(ComposeReconcile, MemoSkipsDescribe) {
  struct Props {
    int value;
    bool operator==(const Props&) const = default;
  };
  static int describeCalls;
  describeCalls = 0;
  auto component = [](const Props& p) {
    ++describeCalls;
    return box().width(20 + (float)p.value).height(20).fill(red());
  };

  Host host;
  auto describe = [&](int a, int b) {
    return box()
        .row()
        .child(memo(Props{a}, component).key("a"))
        .child(memo(Props{b}, component).key("b"));
  };

  host.composer.render(describe(1, 2));
  EXPECT_EQ(describeCalls, 2);
  host.composer.render(describe(1, 2));  // nothing changed
  EXPECT_EQ(describeCalls, 2);
  EXPECT_EQ(host.composer.stats().memoHits, 2u);
  host.composer.render(describe(1, 3));  // one prop changed
  EXPECT_EQ(describeCalls, 3);
  EXPECT_EQ(host.composer.stats().memoHits, 1u);
}

TEST(ComposeReconcile, KeyedReorderKeepsInstances) {
  Host host;
  auto row = [](const char* k, Fill f) {
    return box().key(k).width(40).height(40).fill(std::move(f));
  };
  host.composer.render(
      box().row().child(row("a", red())).child(row("b", green())));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorRED);

  host.composer.render(
      box().row().child(row("b", green())).child(row("a", red())));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);  // reordered, not restyled
  EXPECT_EQ(host.composer.stats().instances, 3u);
}

TEST(ComposeCaching, StaticSubtreeRecordsOnce) {
  static int programRuns;
  programRuns = 0;
  Host host;
  host.composer.render(box().child(
      custom([](SkCanvas& c, const PaintContext& ctx) {
        ++programRuns;
        SkPaint p;
        p.setColor(SK_ColorCYAN);
        c.drawRect(SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
      })
          .width(80)
          .height(80)));

  host.frame();
  host.frame();
  host.frame();
  EXPECT_EQ(programRuns, 1);  // recorded once, replayed thereafter
  EXPECT_EQ(host.pixel(40, 40), SK_ColorCYAN);
  EXPECT_GE(host.composer.stats().picturesLive, 1u);
}

TEST(ComposeCaching, RelayoutInvalidatesStaleRecordings) {
  // setSize alone — no prop change, no re-render — resizes a pct-width child
  // whose geometry is already baked into cached recordings. Invalidation
  // driven only by patching never sees this, so relayout has to compare the
  // new rects against the baked ones and drop what moved.
  Host host;
  host.composer.render(
      box().child(box().width(pct(50)).height(40).fill(red())));
  host.frame();  // child spans x∈[0,100) at 200-wide viewport; recorded
  EXPECT_EQ(host.pixel(80, 20), SK_ColorRED);
  host.composer.setSize({120, 200});  // child now spans x∈[0,60)
  host.frame();
  EXPECT_EQ(host.pixel(80, 20), SK_ColorBLACK);  // red = stale bake replayed
  EXPECT_EQ(host.pixel(30, 20), SK_ColorRED);    // new geometry painted
}

TEST(ComposeCaching, CacheNoneRunsEveryFrame) {
  static int programRuns;
  programRuns = 0;
  Host host;
  host.composer.render(
      box().child(custom([](SkCanvas&, const PaintContext&) { ++programRuns; })
                      .width(10)
                      .height(10)
                      .cache(Cache::None)));
  host.frame();
  host.frame();
  EXPECT_EQ(programRuns, 2);
}

TEST(ComposeCaching, ReconcileInvalidatesRecording) {
  Host host;
  auto tree = [](Fill f) {
    return box().child(box().key("x").width(60).height(60).fill(std::move(f)));
  };
  host.composer.render(tree(red()));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
  host.composer.render(tree(green()));
  EXPECT_TRUE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorGREEN);
}

TEST(ComposeTransitions, RampsAndRetargetsFromCurrent) {
  Host host;
  auto at = [&](float target) {
    return box().child(
        box().key("m").width(50).height(50).fill(red()).translateX(animate(
            sigil::motion::to(target), {400ms, &choreograph::easeNone})));
  };
  host.composer.render(at(0.0f));
  host.frame();
  host.composer.render(at(100.0f));            // start ramp 0 → 100
  host.frame(0.2);                             // half way (linear ease)
  EXPECT_EQ(host.pixel(75, 25), SK_ColorRED);  // box around x=50..100
  EXPECT_EQ(host.pixel(10, 25), SK_ColorBLACK);

  host.composer.render(at(0.0f));  // retarget back from ~50
  host.frame(0.2);                 // halfway back → ~25
  EXPECT_EQ(host.pixel(45, 25), SK_ColorRED);
  EXPECT_EQ(host.pixel(90, 25), SK_ColorBLACK);

  host.frame(1.0);  // settle
  EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);
  EXPECT_FALSE(host.ticker.active());  // motion removed on finish
}

TEST(ComposeTransitions, UnmountCancelsMotions) {
  Host host;
  host.composer.render(
      box().child(box().key("gone").width(10).height(10).translateX(
          animate(sigil::motion::to(500.0f), {1000ms}))));
  host.frame();
  host.composer.render(
      box().child(box().key("gone").width(10).height(10).translateX(
          animate(sigil::motion::to(0.0f), {1000ms}))));
  host.frame(0.1);
  EXPECT_TRUE(host.ticker.active());
  host.composer.render(box());  // unmount mid-flight
  host.frame(0.1);              // stepping must not touch dead outputs
  EXPECT_FALSE(host.ticker.active());
}

TEST(ComposeBindings, OutputDrivesPaintWithoutRender) {
  Host host;
  choreograph::Output<float> x = 0.0f;
  host.composer.render(
      box().child(box().width(40).height(40).fill(blue()).translateX(&x)));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLUE);

  x = 120.0f;  // direct mutation, no render()
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(140, 20), SK_ColorBLUE);
}

TEST(ComposeCaching, TextureCacheRasterizesOnceAndInvalidates) {
  static int programRuns;
  programRuns = 0;
  Host host;
  auto tree = [](SkColor color) {
    return box().child(
        custom([color](SkCanvas& c, const PaintContext& ctx) {
          ++programRuns;
          SkPaint p;
          p.setColor(color);
          c.drawRect(SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
        })
            .key("tex")
            .width(80)
            .height(80)
            .cache(Cache::Texture));
  };
  host.composer.render(tree(SK_ColorMAGENTA));
  host.frame();
  host.frame();
  host.frame();
  EXPECT_EQ(programRuns, 1);  // rasterized once, blitted thereafter
  EXPECT_EQ(host.pixel(40, 40), SK_ColorMAGENTA);
  EXPECT_GE(host.composer.stats().texturesLive, 1u);

  host.composer.render(tree(SK_ColorYELLOW));  // invalidate
  host.frame();
  EXPECT_EQ(programRuns, 2);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorYELLOW);
}

TEST(ComposeSlots, SlotUpdatesWithoutDisturbingSiblings) {
  static int staticRuns;
  staticRuns = 0;
  Host host;
  host.composer.render(
      box()
          .row()
          .gap(10)
          .child(custom([](SkCanvas& c, const PaintContext& ctx) {
                   ++staticRuns;
                   SkPaint p;
                   p.setColor(SK_ColorRED);
                   c.drawRect(
                       SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
                 })
                     .width(50)
                     .height(50))
          .child(slot("live").width(80).height(50)));
  host.frame();
  EXPECT_EQ(staticRuns, 1);

  host.composer.renderSlot(
      "live", box().fill(Fill::color({0, 1, 0, 1})).width(80).height(50));
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);
  EXPECT_EQ(host.pixel(70, 25), SK_ColorGREEN);

  host.composer.renderSlot(
      "live", box().fill(Fill::color({0, 0, 1, 1})).width(80).height(50));
  host.frame();
  EXPECT_EQ(host.pixel(70, 25), SK_ColorBLUE);
  // The sibling's paint program never re-ran across slot updates: its
  // own recording stayed valid even though ancestors re-recorded.
  EXPECT_EQ(staticRuns, 1);
}

#include <include/core/SkColorFilter.h>
#include <include/core/SkStream.h>
#include <include/effects/SkImageFilters.h>
#include <sigilimage/asset/ImageAsset.h>

TEST(ComposeEffects, LayerEffectBlursNode) {
  Host host;
  host.composer.render(
      box().child(box()
                      .width(60)
                      .height(60)
                      .inset(70, 70, 70, 70)
                      .absolute()
                      .fill(red())
                      .effect(material::skia::Effect::filter(
                          SkImageFilters::Blur(8, 8, nullptr)))));
  host.frame();
  // Blur bleeds outside the crisp box bounds and softens the center edge.
  SkColor outside = host.pixel(64, 100);  // 6px outside the left edge
  EXPECT_NE(outside, SK_ColorBLACK);
  EXPECT_NE(host.pixel(100, 100), SK_ColorBLACK);  // center still red-ish
  // Far away stays untouched.
  EXPECT_EQ(host.pixel(10, 10), SK_ColorBLACK);
}

TEST(ComposeEffects, BackdropFiltersWhatIsBeneath) {
  Host host;
  // Invert color matrix as a deterministic backdrop filter.
  float invert[20] = {-1, 0, 0,  0, 1, 0, -1, 0, 0, 1,
                      0,  0, -1, 0, 1, 0, 0,  0, 1, 0};
  auto invertFilter =
      SkImageFilters::ColorFilter(SkColorFilters::Matrix(invert), nullptr);

  host.composer.render(
      stack()
          .child(box().inset(0).fill(red()))
          .child(box()
                     .width(80)
                     .height(80)
                     .inset(60, 60, 60, 60)
                     .absolute()
                     .backdrop(material::skia::Effect::filter(invertFilter))));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorCYAN);  // red inverted inside
  EXPECT_EQ(host.pixel(20, 100), SK_ColorRED);    // untouched outside
}

TEST(ComposeEffects, TextureBakesEffectOnce) {
  // ONCE is asserted literally, through `texturesBaked` — the per-draw
  // pixel-bake count: one on the frame that bakes, zero on every frame
  // after. A live texture count and a non-black pixel would look identical
  // on a node that re-bakes every single frame.
  //
  // profiledUnder(), not a plain parent: under a cacheable parent the second
  // frame replays the PARENT's picture and never visits this node at all, so
  // "0 bakes" would be true of a node that re-bakes every time it is asked.
  // Cache::None on the wrapper keeps the subject painted every frame, which
  // is what makes the second assertion a statement about the texture.
  Host host;
  host.composer.render(
      profiledUnder(box()
                        .key("bloomed")
                        .width(60)
                        .height(60)
                        .fill(green())
                        .effect(material::skia::Effect::filter(
                            SkImageFilters::Blur(4, 4, nullptr)))
                        .cache(Cache::Texture)));
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u) << "the bake";
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
      << "…and the second frame blits it rather than re-baking";
  EXPECT_GE(host.composer.stats().texturesLive, 1u);
  EXPECT_NE(host.pixel(30, 30), SK_ColorBLACK);  // filtered content present
}

namespace {

/** ~20 lines of user code: the lightweight grid from the design docs. */
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

#include <sigilmaterial/skia/Paint.h>

TEST(ComposeMotion, EaseAdaptersBindTheShapeParameter) {
  // choreograph's back/elastic/bounce take a shape parameter with a
  // default, so &choreograph::easeOutBack does not convert to an EaseFn.
  // These adapters bind it — and outBack must actually OVERSHOOT, which
  // is the only reason to reach for it.
  const choreograph::EaseFn back = motion::ease::outBack();
  float peak = 0.0f;
  for (int i = 0; i <= 100; ++i) peak = std::max(peak, back((float)i / 100.0f));
  EXPECT_GT(peak, 1.05f) << "outBack did not overshoot";
  EXPECT_NEAR(back(0.0f), 0.0f, 1e-4f);
  EXPECT_NEAR(back(1.0f), 1.0f, 1e-4f);

  // and it is usable where the papercut was: inside a motion::Transition.
  Host host(100, 100);
  host.composer.render(
      box().child(box()
                      .width(40)
                      .height(40)
                      .absolute()
                      .left(30)
                      .top(30)
                      .scale(animate(motion::from(0.5f).to(1.0f),
                                     {std::chrono::milliseconds(200),
                                      motion::ease::outBack()}))
                      .fill(material::skia::Paint::solid({1, 1, 1, 1}))));
  host.frame();
  SUCCEED();
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

TEST(ComposeMaterial, UnitRampFollowsTheBoxItLandsIn) {
  // linear() is in node-local PIXELS, which an author cannot know for a
  // content-sized box. linearUnit() is in the unit square, so the SAME
  // material reads correctly at two different sizes.
  auto card = [](float w, float h) {
    return box().width(w).height(h).absolute().left(0).top(0).fill(
        material::skia::Paint::linearUnit(
            {0, 0}, {0, 1}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}));
  };
  Host small(80, 40);
  small.composer.render(box().child(card(80, 40)));
  small.frame();
  EXPECT_GT(SkColorGetR(small.pixel(40, 2)), 180u);   // top is red…
  EXPECT_GT(SkColorGetB(small.pixel(40, 37)), 180u);  // …bottom is blue

  Host tall(80, 300);
  tall.composer.render(box().child(card(80, 300)));
  tall.frame();
  EXPECT_GT(SkColorGetR(tall.pixel(40, 3)), 180u);
  EXPECT_GT(SkColorGetB(tall.pixel(40, 296)), 180u);
  // and the midpoint is the blend at BOTH sizes, which a pixel-space ramp
  // authored for one of them could not manage
  const SkColor midSmall = small.pixel(40, 20);
  const SkColor midTall = tall.pixel(40, 150);
  EXPECT_NEAR((int)SkColorGetR(midSmall), (int)SkColorGetR(midTall), 24);
  EXPECT_NEAR((int)SkColorGetB(midSmall), (int)SkColorGetB(midTall), 24);
}

TEST(ComposeMaterial, LinearGradientFillPaints) {
  Host host;
  host.composer.render(
      box().child(box()
                      .width(100)
                      .height(20)
                      .inset(0, 0, 100, 180)
                      .absolute()
                      .fill(material::skia::Paint::linear(
                          {0, 0}, {100, 0},
                          {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}))));
  host.frame();
  const SkColor left = host.pixel(2, 10);
  const SkColor right = host.pixel(98, 10);
  EXPECT_GT(SkColorGetR(left), 200u);  // red end
  EXPECT_LT(SkColorGetB(left), 70u);
  EXPECT_GT(SkColorGetB(right), 200u);  // blue end
  EXPECT_LT(SkColorGetR(right), 70u);
}

TEST(ComposeMaterial, ConicalMovesTheHighlightWithoutMovingTheFalloff) {
  // The offset-focus radial. Displacing a plain radial()'s CENTRE slides the
  // whole ramp, so falloff and highlight are one knob and a lit sphere is
  // impossible to spell. conical()
  // (SkShaders::TwoPointConicalGradient) keeps the outer circle put and
  // moves only the focus, which is the sphere-shading primitive.
  const auto sphere = [](SkPoint focus) {
    return box().child(
        box()
            .width(120)
            .height(120)
            .inset(40, 40, 40, 40)
            .absolute()
            .fill(material::skia::Paint::conical(
                focus, 0.0f, {60, 60}, 60.0f,
                {{0.0f, {1, 1, 1, 1}}, {1.0f, {0, 0, 0.2f, 1}}})));
  };
  Host centered, offset;
  centered.composer.render(sphere({60, 60}));
  offset.composer.render(sphere({35, 35}));
  centered.frame();
  offset.frame();
  // The highlight visibly MOVES to the focus: at the displaced focus
  // (node-local (35,35) = canvas (75,75)) the offset arm is near-white
  // and clearly brighter than the centered arm at that same pixel…
  EXPECT_GT(SkColorGetR(offset.pixel(75, 75)), 200u);
  EXPECT_GT((int)SkColorGetR(offset.pixel(75, 75)),
            (int)SkColorGetR(centered.pixel(75, 75)) + 40)
      << "the focus offset did not move the highlight";
  // …while at the old centre the ordering reverses.
  EXPECT_GT((int)SkColorGetR(centered.pixel(100, 100)),
            (int)SkColorGetR(offset.pixel(100, 100)) + 40);
  // And the recipe is comparable: identical conicals prune-equal, a moved
  // focus does not (it would freeze the highlight forever), and the
  // conical never aliases the radial it displaces.
  const std::vector<material::skia::Stop> stops{{0.0f, {1, 1, 1, 1}},
                                                {1.0f, {0, 0, 0.2f, 1}}};
  EXPECT_TRUE(
      material::skia::Paint::conical({35, 35}, 0, {60, 60}, 60, stops) ==
      material::skia::Paint::conical({35, 35}, 0, {60, 60}, 60, stops));
  EXPECT_FALSE(
      material::skia::Paint::conical({35, 35}, 0, {60, 60}, 60, stops) ==
      material::skia::Paint::conical({36, 35}, 0, {60, 60}, 60, stops));
  EXPECT_FALSE(
      material::skia::Paint::conical({60, 60}, 0, {60, 60}, 60, stops) ==
      material::skia::Paint::radial({60, 60}, 60, stops));
}

TEST(ComposeMaterial, SweepWarnsWhenTheWindowLeavesTheCircle) {
  // Skia's sweep CLAMPS outside [startDeg, endDeg] rather than wrapping, so
  // a window wider than the circle — sweep(c, stops, 90, 450), the obvious
  // way to spell a hue wheel starting at red — paints part of the ring in
  // the first stop's flat colour with no error. The factory warns instead.
  //
  // Control first (a legal window must stay silent), then the trap arm. The
  // warning fires once per process, so this must be the only place that
  // triggers it and the order within the test matters.
  const std::vector<material::skia::Stop> stops{{0.0f, {1, 0, 0, 1}},
                                                {1.0f, {0, 0, 1, 1}}};
  ::testing::internal::CaptureStderr();
  (void)material::skia::Paint::sweep({50, 50}, stops, 0.0f, 360.0f);
  (void)material::skia::Paint::sweep({50, 50}, stops, 90.0f, 270.0f);
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "a window inside the circle must not warn";
  ::testing::internal::CaptureStderr();
  (void)material::skia::Paint::sweep({50, 50}, stops, 90.0f, 450.0f);
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("skia::Paint::sweep"), std::string::npos) << log;
  EXPECT_NE(log.find("wrap"), std::string::npos) << log;
}

TEST(ComposeMaterial, ANullSkslEffectIsLoudAtBuild) {
  // Host & tooling: "a material that fails to build should be loud." The
  // silent half of that failure is MakeForShader returning null and the
  // caller passing it straight in — the node then paints NOTHING with the
  // compile error long scrolled away. Control first: a valid effect stays
  // silent; the null build warns once, at build, not at draw.
  ::testing::internal::CaptureStderr();
  (void)material::skia::Paint::sksl(ukEffect(), {{"uK", 1.0f}});
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "a valid effect must not warn";
  ::testing::internal::CaptureStderr();
  (void)material::skia::Paint::sksl(sk_sp<SkRuntimeEffect>(nullptr));
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("skia::Paint::sksl"), std::string::npos) << log;
  EXPECT_NE(log.find("nothing"), std::string::npos) << log;
}

#include <cstring>  // memcmp — for the no-conversion control at the bottom
#include <utility>

TEST(ComposeComposer, DeclaredInputSpaceIsALoudDeclarationAndNothingElse) {
  // declareInputSpace lets "I deliberately declared my colour space" and
  // "nobody thought about colour at all" stop being the same tree. It is a
  // QUESTION the library asks, never a conversion stage: compositing happens
  // in encoded sRGB, so the whole response to a mismatched declaration is
  // one precise warning and not a single changed pixel.
  //
  // All of it is one test because the warning fires once per process, which
  // makes the order load-bearing: truthful controls first, the trap arm
  // second, and the no-conversion pixel comparison last, where its own
  // mismatched declarations are already silenced.
  //
  // Control 1: the default and an explicit truthful declaration are
  // silent — they match reality, and a warning here would teach authors
  // to ignore the real one.
  ::testing::internal::CaptureStderr();
  {
    Host host;
    EXPECT_EQ(
        host.composer.declaredInputSpace(),
        Composer::InputSpace::EncodedSRGB);  // the default IS today's truth
    host.composer.declareInputSpace(Composer::InputSpace::EncodedSRGB);
    host.composer.render(box().fill(red()));
    host.frame();
  }
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "a truthful declaration must not warn";
  // The trap arm: a mismatched declaration warns ONCE, naming the
  // consequence — values treated as encoded sRGB, maths wrong at the
  // edges — not merely that something is off.
  ::testing::internal::CaptureStderr();
  Host host;
  host.composer.declareInputSpace(Composer::InputSpace::LinearSRGB);
  EXPECT_EQ(host.composer.declaredInputSpace(),
            Composer::InputSpace::LinearSRGB);
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("declareInputSpace"), std::string::npos) << log;
  EXPECT_NE(log.find("TREATED as encoded sRGB"), std::string::npos) << log;
  EXPECT_NE(log.find("no pixel"), std::string::npos) << log;
  // Once per process: a second mismatch — even a DIFFERENT one — stays
  // silent (renderSlot's unknown-name contract).
  ::testing::internal::CaptureStderr();
  host.composer.declareInputSpace(Composer::InputSpace::DisplayP3);
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "the mismatch warning is once per process";
  // Control 2 — THE control: the declaration participates in NOTHING.
  // Two renders of one gradient tree under opposite declarations must be
  // byte-identical; any conversion machinery that snuck in dies here,
  // because a linear→encoded transfer moves every mid-gradient byte.
  auto plate = [](Composer::InputSpace space) {
    Host h;
    h.composer.declareInputSpace(space);
    h.composer.render(box().child(box().width(160).height(120).fill(
        material::skia::Paint::linear({0, 0}, {160, 120},
                                      {{0.0f, {1, 0, 0, 1}},
                                       {0.5f, {0.25f, 0.5f, 0.25f, 0.8f}},
                                       {1.0f, {0, 0, 1, 1}}}))));
    h.frame();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
    h.surface->readPixels(bm.pixmap(), 0, 0);
    return bm;
  };
  const SkBitmap a = plate(Composer::InputSpace::EncodedSRGB);
  const SkBitmap b = plate(Composer::InputSpace::LinearSRGB);
  ASSERT_EQ(a.computeByteSize(), b.computeByteSize());
  EXPECT_EQ(0, std::memcmp(a.getPixels(), b.getPixels(), a.computeByteSize()))
      << "the declaration must not touch a pixel: it performs no "
         "conversion";
}

TEST(ComposeMaterial, BlendStackCompositesToOneShader) {
  // Two solids blended kPlus → additive brighten in ONE flattened shader
  // (no saveLayer). red + green = yellow.
  Host host;
  host.composer.render(box().child(
      box()
          .width(40)
          .height(40)
          .inset(0, 0, 160, 160)
          .absolute()
          .fill(material::skia::Paint::blend({
              {material::skia::Paint::solid({1, 0, 0, 1}),
               SkBlendMode::kSrcOver},
              {material::skia::Paint::solid({0, 1, 0, 1}), SkBlendMode::kPlus},
          }))));
  host.frame();
  const SkColor c = host.pixel(20, 20);
  EXPECT_GT(SkColorGetR(c), 200u);
  EXPECT_GT(SkColorGetG(c), 200u);
  EXPECT_LT(SkColorGetB(c), 70u);
}

TEST(ComposeMaterial, StaticMaterialCollapsesToFillAndCaches) {
  // A gradient Material is static → collapses to Fill::shader → the parent
  // picture-caches like any static subtree: records once, replays on later
  // draws. (Reconcile-side pruning across re-render is pinned separately by
  // StaticMaterialPrunesAcrossRerender.)
  Host host;
  host.composer.render(
      box().child(box().width(60).height(60).fill(material::skia::Paint::radial(
          {30, 30}, 30, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0, 0, 0, 1}}}))));
  host.frame();  // records
  EXPECT_GE(host.composer.stats().picturesLive, 1u);
  host.frame();  // no re-render — replays the cached picture
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>

TEST(ComposeMaterial, LiveUniformAnimatesAndDeclaresVolatility) {
  // A ch::Output-bound uniform makes an sksl() Material LIVE: it re-resolves
  // every frame from the Output (no re-render), and its node paints live
  // (never freezes into a cache). This is what gives uniform(name, &output)
  // something to hook against.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uK;"
               "half4 main(float2 p) { return half4(uK, 0, 0, 1); }"));
  ASSERT_TRUE(effect) << err.c_str();
  choreograph::Output<float> k{0.0f};
  Host host;
  host.composer.render(box().child(
      box()
          .width(40)
          .height(40)
          .inset(0, 0, 160, 160)
          .absolute()
          .fill(material::skia::Paint::sksl(effect).uniform("uK", &k))));
  host.frame();
  const SkColor c0 = host.pixel(20, 20);
  k = 1.0f;      // change the bound uniform — NO re-render
  host.frame();  // the live material re-resolves from k
  const SkColor c1 = host.pixel(20, 20);
  EXPECT_LT(SkColorGetR(c0), 40u);                    // uK=0 → black
  EXPECT_GT(SkColorGetR(c1), 200u);                   // uK=1 → red
  EXPECT_GT(host.composer.stats().nodesPainted, 0u);  // volatile: paints live
}

TEST(ComposeMaterial, UniformOnNonShaderMaterialIsNoOp) {
  // uniform() on a material with no named uniforms (a solid) has nothing to
  // hook against: it is ignored, the material stays static and non-live.
  material::skia::Paint m =
      material::skia::Paint::solid({0, 1, 0, 1}).uniform("uK", 0.5f);
  EXPECT_FALSE(m.isAnimated());
  EXPECT_TRUE(m.isSolid());
}

TEST(ComposeMaterial, UniformCopiesOnWriteNeverAlias) {
  // Materials are VALUES: binding a uniform on a copy must not contaminate
  // the base or its sibling copies. The shape that catches this is a shared
  // base material bound to two different Outputs — with aliasing, both
  // copies read whichever binding was applied last.
  material::skia::Paint base = material::skia::Paint::sksl(ukEffect());
  choreograph::Output<float> low{0.2f}, high{1.0f};
  material::skia::Paint a = base;
  a.uniform("uK", &low);
  material::skia::Paint b = base;
  b.uniform("uK", &high);
  EXPECT_FALSE(base.isAnimated());  // base untouched
  EXPECT_TRUE(a.isAnimated());
  EXPECT_TRUE(b.isAnimated());

  Host host;
  host.composer.render(box()
                           .child(box()
                                      .width(40)
                                      .height(40)
                                      .inset(0, 0, 160, 160)
                                      .absolute()
                                      .fill(a))
                           .child(box()
                                      .width(40)
                                      .height(40)
                                      .inset(60, 0, 100, 160)
                                      .absolute()
                                      .fill(b)));
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 90u);   // a: uK=0.2
  EXPECT_GT(SkColorGetR(host.pixel(80, 20)), 200u);  // b: uK=1.0 — not aliased
}

TEST(ComposeMaterial, LaterPlainFillReplacesLiveMaterial) {
  // Fill setters are last-wins in BOTH directions. The easy half to get
  // wrong is a plain fill() following a live-material fill(): if the live
  // material is held in a separate slot that paint consults first, the later
  // plain fill is silently ignored.
  choreograph::Output<float> k{1.0f};
  Host host;
  host.composer.render(
      box().child(box()
                      .width(40)
                      .height(40)
                      .inset(0, 0, 160, 160)
                      .absolute()
                      .fill(material::skia::Paint::sksl(ukEffect())
                                .uniform("uK", &k))        // live red
                      .fill(Fill::color({0, 1, 0, 1}))));  // then plain green
  host.frame();
  const SkColor c = host.pixel(20, 20);
  EXPECT_GT(SkColorGetG(c), 200u);  // green won
  EXPECT_LT(SkColorGetR(c), 40u);
}

TEST(ComposeMaterial, BlendWithLiveLayerTracksOutputs) {
  // A blend inherits its layers' volatility tier: a live layer makes the
  // whole blend LIVE, so it re-resolves per frame and TRACKS the bound
  // Output. Flattening the stack eagerly at build time instead would bake
  // the shader's default uniform values in permanently.
  choreograph::Output<float> k{0.8f};
  material::skia::Paint m = material::skia::Paint::blend({
      {material::skia::Paint::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
      {material::skia::Paint::sksl(ukEffect()).uniform("uK", &k),
       SkBlendMode::kPlus},
  });
  EXPECT_TRUE(m.isAnimated());  // inherited from the bound layer
  Host host;
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(m)));
  host.frame();
  const uint32_t bright = SkColorGetR(host.pixel(20, 20));
  EXPECT_GT(bright, 170u);  // ~0.8 * 255 = 204
  k = 0.3f;                 // no render() — the blend follows the Output
  host.frame();
  const uint32_t dim = SkColorGetR(host.pixel(20, 20));
  EXPECT_GT(dim, 50u);  // ~0.3 * 255 = 77
  EXPECT_LT(dim, 110u);
}

TEST(ComposeMaterial, NestedBlendAsShaderFoldsItsLiveLayersPerCall) {
  // A blend carries no live-uniform block of its own — it INHERITS liveness
  // from its layers — so any code path that reaches for that block directly
  // has nothing to dereference. The shape that reaches it is a blend nested
  // inside another blend's layer list, because blend() folds every layer to
  // a shader as it is constructed: building `outer` below is the moment it
  // happens, before anything is painted.
  choreograph::Output<float> k{0.8f};
  material::skia::Paint inner = material::skia::Paint::blend({
      {material::skia::Paint::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
      {material::skia::Paint::sksl(ukEffect()).uniform("uK", &k),
       SkBlendMode::kPlus},
  });
  ASSERT_TRUE(inner.isAnimated());  // inherited from the bound layer
  material::skia::Paint outer = material::skia::Paint::blend({
      {inner, SkBlendMode::kSrcOver},  // <- the null deref was HERE
      {material::skia::Paint::solid({0, 0, 0, 1}), SkBlendMode::kPlus},
  });
  ASSERT_TRUE(outer.isAnimated());  // liveness survives one more nesting

  // And the answer must be folded PER CALL. Merely guarding the null would
  // fall through to m_shader — blend()'s eager snapshot, built once at
  // construction — which is the stale-snapshot defect asShader()'s live
  // branch exists to prevent; it would answer 0.8 forever.
  auto sampleR = [](const material::skia::Paint& m) -> uint32_t {
    sk_sp<SkShader> s = m.asShader();
    EXPECT_TRUE(s);
    sk_sp<SkSurface> surf =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
    surf->getCanvas()->clear(SK_ColorBLACK);
    SkPaint p;
    p.setShader(std::move(s));
    surf->getCanvas()->drawPaint(p);
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    surf->readPixels(bm.pixmap(), 2, 2);
    return SkColorGetR(bm.getColor(0, 0));
  };
  EXPECT_NEAR((int)sampleR(outer), 204, 12);  // 0.8 · 255, through two blends
  k = 0.3f;
  EXPECT_NEAR((int)sampleR(outer), 77, 12);  // 0.3 · 255 — the fold is fresh
  // The same claim for the inner blend, which the outer one reaches through.
  EXPECT_NEAR((int)sampleR(inner), 77, 12);
}

TEST(ComposeMaterial, DeclaringUTimeMakesMaterialLive) {
  // "Reading the clock IS the volatility declaration": an sksl effect that
  // declares uTime takes the live path with no bound Outputs — it re-resolves
  // per frame with PaintContext time instead of freezing a uTime=0 snapshot.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform float uTime;"
      "half4 main(float2 p) { return half4(fract(uTime), 0, 0, 1); }"));
  ASSERT_TRUE(effect) << err.c_str();
  material::skia::Paint m = material::skia::Paint::sksl(effect);
  EXPECT_TRUE(m.isAnimated());

  sigil::motion::FrameClock clock;
  Host host;
  host.composer.setClock(&clock);
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(m)));
  host.frame();
  const uint32_t r0 = SkColorGetR(host.pixel(20, 20));  // uTime ≈ 0 → black
  clock.tick();                                         // advance real time…
  // …but pin the readable elapsed via a fabricated wait: FrameClock elapsed
  // is wall-time based; just assert the material painted live (r0 near 0 is
  // the frozen-snapshot failure mode this test guards).
  EXPECT_LT(r0, 30u);
  EXPECT_GT(host.composer.stats().nodesPainted, 0u);  // live, not cached
}

TEST(ComposeMaterial, LiveMaterialUnderLeafDirectBlend) {
  // Audit gap: the leaf fast path routes blend onto the fill paint — a
  // live-material leaf with .blend(kPlus) must composite additively.
  choreograph::Output<float> k{1.0f};  // red
  Host host;
  host.composer.render(
      stack()
          .child(box()
                     .width(40)
                     .height(40)
                     .inset(0, 0, 160, 160)
                     .absolute()
                     .fill(Fill::color({0, 1, 0, 1})))  // green under
          .child(
              box()
                  .width(40)
                  .height(40)
                  .inset(0, 0, 160, 160)
                  .absolute()
                  .fill(
                      material::skia::Paint::sksl(ukEffect()).uniform("uK", &k))
                  .blend(SkBlendMode::kPlus)));
  host.frame();
  const SkColor c = host.pixel(20, 20);  // red + green = yellow
  EXPECT_GT(SkColorGetR(c), 200u);
  EXPECT_GT(SkColorGetG(c), 200u);
  EXPECT_LT(SkColorGetB(c), 60u);
}

TEST(ComposeMaterial, SnapshotSamplesLiveMaterialNow) {
  // Audit gap: snapshot() (the element-tree-as-a-brush bake) samples live
  // materials at their CURRENT Output values.
  choreograph::Output<float> k{1.0f};
  sk_sp<SkPicture> pic =
      snapshot(box().width(60).height(60).fill(
                   material::skia::Paint::sksl(ukEffect()).uniform("uK", &k)),
               fonts());
  ASSERT_TRUE(pic);
  Host host;
  host.surface->getCanvas()->clear(SK_ColorBLACK);
  host.surface->getCanvas()->drawPicture(pic);
  EXPECT_GT(SkColorGetR(host.pixel(30, 30)), 200u);  // k=1 sampled at bake
}

TEST(ComposeMaterial, RenderSlotHostsLiveMaterial) {
  // Audit gap: a live material mounted through renderSlot() animates like
  // any other — the slot path wires volatility identically.
  choreograph::Output<float> k{0.0f};
  Host host;
  host.composer.render(box().child(slot("s").width(40).height(40)));
  host.composer.renderSlot(
      "s", box().width(40).height(40).fill(
               material::skia::Paint::sksl(ukEffect()).uniform("uK", &k)));
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 30u);  // k=0
  k = 1.0f;                                         // no render, no renderSlot
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(20, 20)), 200u);  // live through the slot
}

TEST(ComposeMaterial, StaticMaterialPrunesAcrossRerender) {
  // Re-describing the SAME material recipe prunes even though every describe
  // builds a fresh SkShader: gradients and blend stacks compare by RECIPE,
  // not by shader pointer. Compare by pointer and a tree like this one
  // re-patches and re-records on every render() with nothing to show for it.
  Host host;
  auto tree = [] {
    return box()
        .child(box().width(60).height(60).fill(material::skia::Paint::linear(
            {0, 0}, {60, 0}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}})))
        .child(box().width(40).height(40).fill(material::skia::Paint::blend({
            {material::skia::Paint::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
            {material::skia::Paint::radial(
                 {20, 20}, 20, {{0.0f, {0, 1, 0, 1}}, {1.0f, {0, 0, 0, 1}}}),
             SkBlendMode::kPlus},
        })));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree());  // brand-new shaders, identical recipes
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeMaterial, ChangedRecipeStillInvalidates) {
  // Over-prune guard: a changed ramp color is a different recipe — the node
  // patches and repaints.
  Host host;
  auto tree = [](SkColor4f c) {
    return box().child(
        box().key("g").width(60).height(60).fill(material::skia::Paint::linear(
            {0, 0}, {60, 0}, {{0.0f, c}, {1.0f, c}})));
  };
  host.composer.render(tree({1, 0, 0, 1}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
  host.composer.render(tree({0, 1, 0, 1}));
  EXPECT_TRUE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorGREEN);
}

namespace {

/** A 1-row image whose pixels are the given colors (N32, no color space —
 *  the Host surface has none either, so nothing converts and a byte written
 *  here is the byte the shader reads). */
sk_sp<SkImage> rowImage(const std::vector<SkColor>& pixels) {
  SkBitmap bm;
  bm.allocN32Pixels((int)pixels.size(), 1);
  for (size_t i = 0; i < pixels.size(); ++i)
    *bm.getAddr32((int)i, 0) = SkPreMultiplyColor(pixels[i]);
  bm.setImmutable();
  return bm.asImage();
}

/** THE PALETTED SHADER. `uIndex`'s red channel is a palette INDEX, not a
 *  colour; `uPalette` is the 4-entry LUT it selects from. */
sk_sp<SkRuntimeEffect> paletteEffect() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader uIndex;"
                 "uniform shader uPalette;"
                 "uniform float uShade;"
                 "half4 main(float2 xy) {"
                 "  float i = floor(uIndex.eval(xy).r * 255.0 + 0.5);"
                 "  i = min(i + uShade, 3.0);"  // X-COM's ramp arithmetic
                 "  return uPalette.eval(float2(i + 0.5, 0.5));"
                 "}"));
    if (!effect) ADD_FAILURE() << err.c_str();
    return effect;
  }();
  return fx;
}

/** The IMAGES are process-wide, the way a decoded asset is: Material::image
 *  compares by image POINTER (the documented recipe rule), so a helper that
 *  minted a fresh SkImage per call would make every material unequal to
 *  every other and the prune question below unaskable. */
const sk_sp<SkImage>& indexImage() {
  static sk_sp<SkImage> img =
      rowImage({SkColorSetARGB(255, 0, 0, 0), SkColorSetARGB(255, 1, 0, 0),
                SkColorSetARGB(255, 2, 0, 0), SkColorSetARGB(255, 3, 0, 0)});
  return img;
}

const sk_sp<SkImage>& rampPalette() {
  static sk_sp<SkImage> img =
      rowImage({SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE, SK_ColorWHITE});
  return img;
}

const sk_sp<SkImage>& reversedPalette() {
  static sk_sp<SkImage> img =
      rowImage({SK_ColorWHITE, SK_ColorBLUE, SK_ColorGREEN, SK_ColorRED});
  return img;
}

const sk_sp<SkImage>& flatWhitePalette() {
  static sk_sp<SkImage> img =
      rowImage({SK_ColorWHITE, SK_ColorWHITE, SK_ColorWHITE, SK_ColorWHITE});
  return img;
}

/** The index texture: four 1-px cells carrying indices 0..3, blown up to
 *  20 px each so a node pixel lands unambiguously inside one cell. NEAREST
 *  everywhere — an index sampled at kLinear is a blend of two unrelated
 *  palette entries, which is the trap this whole texture kind carries. */
material::skia::Paint indexSource() {
  return material::skia::Paint::image(
      indexImage(), SkTileMode::kClamp, SkTileMode::kClamp,
      SkMatrix::Scale(20, 20), SkSamplingOptions(SkFilterMode::kNearest));
}

material::skia::Paint paletteSource(const sk_sp<SkImage>& lut) {
  return material::skia::Paint::image(
      lut, SkTileMode::kClamp, SkTileMode::kClamp, SkMatrix::I(),
      SkSamplingOptions(SkFilterMode::kNearest));
}

}  // namespace

TEST(ComposeMaterial, AChildSlotSamplesAnIndexTextureThroughAPalette) {
  // THE DRIVING CASE, end to end: two images, one shader, one draw. Neither
  // source is the node's own painted content (that is Effect's `content`
  // child) — they are sources the material brings with it.
  Host host(80, 20);
  host.composer.render(stack().child(box().absolute().inset(0).fill(
      material::skia::Paint::sksl(paletteEffect(), {{"uShade", 0.0f}})
          .child("uIndex", indexSource())
          .child("uPalette", paletteSource(rampPalette())))));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorRED) << "index 0";
  EXPECT_EQ(host.pixel(30, 10), SK_ColorGREEN) << "index 1";
  EXPECT_EQ(host.pixel(50, 10), SK_ColorBLUE) << "index 2";
  EXPECT_EQ(host.pixel(70, 10), SK_ColorWHITE) << "index 3";

  // The LUT is the point: re-authoring the palette re-colours the picture
  // without touching the index texture — the paletted-shading trick itself.
  Host swapped(80, 20);
  swapped.composer.render(stack().child(box().absolute().inset(0).fill(
      material::skia::Paint::sksl(paletteEffect(), {{"uShade", 0.0f}})
          .child("uIndex", indexSource())
          .child("uPalette", paletteSource(reversedPalette())))));
  swapped.frame();
  EXPECT_EQ(swapped.pixel(10, 10), SK_ColorWHITE) << "same indices, new LUT";
  EXPECT_EQ(swapped.pixel(70, 10), SK_ColorRED);

  // And the shade step is index ARITHMETIC, clamped at the ramp's end —
  // every cell moves one entry down the palette and the last one sticks.
  Host shaded(80, 20);
  shaded.composer.render(stack().child(box().absolute().inset(0).fill(
      material::skia::Paint::sksl(paletteEffect(), {{"uShade", 1.0f}})
          .child("uIndex", indexSource())
          .child("uPalette", paletteSource(rampPalette())))));
  shaded.frame();
  EXPECT_EQ(shaded.pixel(10, 10), SK_ColorGREEN) << "0 + 1";
  EXPECT_EQ(shaded.pixel(50, 10), SK_ColorWHITE) << "2 + 1";
  EXPECT_EQ(shaded.pixel(70, 10), SK_ColorWHITE) << "3 + 1, clamped";
}

TEST(ComposeMaterial, TheChildRidesThePruneSignature) {
  // THE CACHE CONDITION. Anything read at paint time must participate in
  // reconciler equality. A child that does not leaves a pruned node sampling
  // the OLD palette forever, with no diagnostic and a picture that looks
  // deliberate.
  const material::skia::Paint a =
      material::skia::Paint::sksl(paletteEffect())
          .child("uPalette", paletteSource(rampPalette()));
  const material::skia::Paint b =
      material::skia::Paint::sksl(paletteEffect())
          .child("uPalette", paletteSource(rampPalette()));
  const material::skia::Paint c =
      material::skia::Paint::sksl(paletteEffect())
          .child("uPalette", paletteSource(flatWhitePalette()));
  const material::skia::Paint bare =
      material::skia::Paint::sksl(paletteEffect());
  EXPECT_TRUE(a == b) << "same effect, same child recipe → prunes";
  EXPECT_FALSE(a == c) << "a different palette is a different material";
  EXPECT_FALSE(a == bare) << "a filled slot is not an empty one";

  // …and the reconciler agrees: identical describe prunes, a swapped
  // palette patches and repaints.
  Host host(80, 20);
  auto tree = [](const sk_sp<SkImage>& lut) {
    return stack().child(box().key("lut").absolute().inset(0).fill(
        material::skia::Paint::sksl(paletteEffect(), {{"uShade", 0.0f}})
            .child("uIndex", indexSource())
            .child("uPalette", paletteSource(lut))));
  };
  host.composer.render(tree(rampPalette()));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorRED);
  host.composer.render(
      tree(rampPalette()));  // identical recipe, fresh Materials…
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  host.composer.render(tree(flatWhitePalette()));
  EXPECT_TRUE(host.composer.dirty()) << "over-prune guard";
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorWHITE);
}

TEST(ComposeMaterial, ALiveChildMakesTheParentLive) {
  // TIER INHERITANCE, upward. The parent effect declares no uniform of its
  // own and no clock: everything volatile about it belongs to the child.
  // If the tier did not propagate, the parent would collapse to a Fill and
  // freeze the child at whatever the Output read on the frame it recorded.
  static const sk_sp<SkRuntimeEffect> passthrough = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader uSrc;"
                 "half4 main(float2 p) { return uSrc.eval(p); }"));
    return fx;
  }();
  ASSERT_TRUE(passthrough);
  choreograph::Output<float> k{0.0f};
  const material::skia::Paint live =
      material::skia::Paint::sksl(passthrough)
          .child("uSrc",
                 material::skia::Paint::sksl(ukEffect()).uniform("uK", &k));
  EXPECT_TRUE(live.isAnimated()) << "the child's volatility is the parent's";
  EXPECT_FALSE(material::skia::Paint::sksl(passthrough)
                   .child("uSrc", material::skia::Paint::solid({0, 1, 0, 1}))
                   .isAnimated())
      << "…and a static child leaves the parent static";

  Host host;
  host.composer.render(
      stack().child(box().absolute().inset(0).width(40).height(40).fill(live)));
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 40u);  // uK = 0 → black
  k = 1.0f;                                         // no render()
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(20, 20)), 200u) << "uK = 1 → red";
}

TEST(ComposeMaterial, AGeometryChildPropagatesTheGeometryTier) {
  // TIER INHERITANCE, the cheaper half: a child reading uResolution needs
  // the PaintContext at RECORD time but not every frame, and it must read
  // the parent NODE's box (there is one box here, not two).
  static const sk_sp<SkRuntimeEffect> passthrough = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader uSrc;"
                 "half4 main(float2 p) { return uSrc.eval(p); }"));
    return fx;
  }();
  static const sk_sp<SkRuntimeEffect> unitRamp = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform float2 uResolution;"
                 "half4 main(float2 p) {"
                 "  return half4(half(p.x / max(uResolution.x, 1.0)), 0, 0, 1);"
                 "}"));
    return fx;
  }();
  ASSERT_TRUE(passthrough && unitRamp);
  const material::skia::Paint m =
      material::skia::Paint::sksl(passthrough)
          .child("uSrc", material::skia::Paint::sksl(unitRamp));
  EXPECT_TRUE(m.geometryDependent()) << "the child's tier is the parent's";
  EXPECT_FALSE(m.isAnimated()) << "geometry is not live";

  Host host(100, 20);
  host.composer.render(stack().child(box().absolute().inset(0).fill(m)));
  host.frame();
  // The ramp spans the node's own width: dark at the left edge, bright at
  // the right. A child resolved with a null context would read uResolution
  // as 0 and clamp to full red everywhere.
  EXPECT_LT(SkColorGetR(host.pixel(2, 10)), 40u);
  EXPECT_GT(SkColorGetR(host.pixel(97, 10)), 200u);
}

TEST(ComposeMaterial, AnUndeclaredChildNameIsIgnored) {
  // uniform()'s guardrail, verbatim: assigning a child the effect does not
  // declare SkDEBUGFAILs, which would kill the hot-reload host over one
  // typo. Warn, ignore, keep painting.
  Host host(80, 20);
  material::skia::Paint m =
      material::skia::Paint::sksl(paletteEffect(), {{"uShade", 0.0f}})
          .child("uIndex", indexSource())
          .child("uPalette", paletteSource(rampPalette()))
          .child("uNoSuchSlot", material::skia::Paint::solid({1, 1, 1, 1}));
  EXPECT_FALSE(m.isAnimated());
  host.composer.render(stack().child(box().absolute().inset(0).fill(m)));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorRED) << "the declared slots still ran";

  // And on a material with no slots at all it is a no-op, like uniform().
  material::skia::Paint solid =
      material::skia::Paint::solid({0, 1, 0, 1}).child("uSrc", indexSource());
  EXPECT_TRUE(solid.isSolid());
  EXPECT_FALSE(solid.isAnimated());
}

// ---- the order the declared reads imply -------------------------------------

namespace {

/** A decoration that strokes a path BORROWED from a keyed node — the one
 *  kind of mark whose answer is another node's finished geometry. */
struct BorrowedStroke {
  std::string key;
  std::vector<std::string> borrows() const { return {key}; }
  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(6.0f);
    paint.setAntiAlias(false);
    canvas.drawPath(ctx.borrowedPath(key), paint);
  }
};

}  // namespace

TEST(ComposeDerive, ABorrowOfAConnectorWrittenAfterItLandsOnTheFirstFrame) {
  // The borrower reads the wire's OUTLINE and is written before the wire,
  // so resolving the two in the order they were written hands it a route
  // that has not been laid yet — it dresses the connector's empty box for
  // a whole frame and only catches up on the next one. Both declare what
  // they read, so both are resolved in one pass in the order those
  // declarations imply, and the borrowed route is right the first time.
  Host host;
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          .child(box()
                     .absolute()
                     .inset(0, 0, 0, 0)
                     .foreground(Decoration(BorrowedStroke{"wire"})))
          .child(box().key("a").left(20).top(90).width(20).height(20))
          .child(box().key("b").left(160).top(90).width(20).height(20))
          .child(connector("a", "b").key("wire").absolute().inset(0, 0, 0, 0)));
  host.frame();  // THE FIRST frame — a pass behind is visible only here
  // The route runs centre to centre along y=100, and the borrowed stroke
  // is on it. An unrouted borrow dresses the connector's own box instead,
  // whose edges are nowhere near the middle of the canvas.
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(40, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 40), SK_ColorBLACK);
}

// ---- rail(): the component that IS a line ----------------------------------

namespace {

/** A 20×20 keyed station box; center lands at (left+10, top+10). */
Element station(const char* key, float left, float top) {
  return box()
      .key(key)
      .width(20)
      .height(20)
      .inset(left, top, 180 - left, 160 - top)
      .absolute()
      .fill(blue());
}

}  // namespace

TEST(ComposeTransitions, PlainSnapAfterTransitionLands) {
  // Describing a PLAIN value after a transition must land immediately. A
  // running ramp resolves ahead of the described value, so unless the snap
  // disconnects it the ramp shadows the description for as long as it lives
  // — and a settled ramp holds its target forever.
  Host host;
  auto at = [](sigil::motion::Animatable<float> x) {
    return box().child(
        box().key("m").width(50).height(50).fill(red()).translateX(
            std::move(x)));
  };
  host.composer.render(at(0.0f));
  host.frame();
  host.composer.render(
      at(animate(sigil::motion::to(100.0f), {400ms, &choreograph::easeNone})));
  host.frame(0.2);  // mid-ramp, box around x=50..100
  EXPECT_EQ(host.pixel(75, 25), SK_ColorRED);
  host.composer.render(at(0.0f));  // PLAIN: must snap home
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);
  EXPECT_EQ(host.pixel(75, 25), SK_ColorBLACK);  // not stuck mid-ramp
}

TEST(ComposeMaterial, ContentScaleDeclaringMaterialIsLive) {
  // uContentScale tracks the HOST's zoom, not the node — it must take the
  // live tier (the pre-tier-split behavior), unlike uResolution.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uContentScale;"
               "half4 main(float2 p) { return half4(1, 0, 0, 1); }"));
  ASSERT_TRUE(effect) << err.c_str();
  EXPECT_TRUE(material::skia::Paint::sksl(effect).isAnimated());
}

// ---- Pattern: runtime-procedural regenerable tiles --------------------------

TEST(ComposeMaterial, DeclaredBleedGrowsTheRecordingCull) {
  // A decoration declares how far it escapes the node with bleed(), and a
  // Material needs the same word: a fill on a shape() that overflows the box
  // — which is legal — is otherwise truncated at the cached bounds, leaving
  // the caller to pad the node by hand.
  //
  // Both carriers are checked, the static recipe and the live slot.
  // Cache::Texture makes the truncation hard rather than merely likely: the
  // bake surface is exactly recordBounds, so anything outside it cannot
  // survive by accident.
  auto overflowShape = [](SkSize s) {
    // A disc centered on the box, poking 20px beyond every edge.
    SkPathBuilder b;
    b.addOval(SkRect::MakeLTRB(-20, -20, s.width() + 20, s.height() + 20));
    return b.detach();
  };
  {
    Host host;  // recipe carrier: a static solid material
    host.composer.render(box().padding(40).child(
        box()
            .width(60)
            .height(40)
            .cache(Cache::Texture)
            .shape(overflowShape)
            .fill(material::skia::Paint::solid({1, 0, 0, 1}).bleed(24))));
    host.frame();
    host.frame();  // the cached replay is where a small cull would bite
    // Node spans y∈[40,80); 14px below is inside the disc's overflow.
    EXPECT_EQ(host.pixel(70, 94), SK_ColorRED);
  }
  {
    Host host;  // live carrier: a geometry-tier material (uResolution ramp)
    host.composer.render(box().padding(40).child(
        box()
            .width(60)
            .height(40)
            .cache(Cache::Texture)
            .shape(overflowShape)
            .fill(material::skia::Paint::linearUnit(
                      {0, 0}, {1, 1}, {{0, {1, 0, 0, 1}}, {1, {1, 0, 0, 1}}})
                      .bleed(24))));
    host.frame();
    host.frame();
    EXPECT_EQ(host.pixel(70, 94), SK_ColorRED);
  }
  // The reserve is recipe: it participates in equality, so a changed
  // bleed re-records instead of replaying a stale, smaller cull.
  material::skia::Paint a = material::skia::Paint::solid({1, 0, 0, 1});
  material::skia::Paint b = material::skia::Paint::solid({1, 0, 0, 1});
  b.bleed(24);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a == material::skia::Paint::solid({1, 0, 0, 1}));
  EXPECT_FLOAT_EQ(b.bleed(), 24.0f);
}

// ---------------------------------------------------------------------------
// Effect live uniforms: the same uniform(name, &output) contract Material
// offers, on the effect seam.

TEST(ComposeEffects, ALiveUniformAnimatesWithoutRedescribe) {
  // With constant uniforms only, animating a ripple phase or a bloom
  // threshold costs a full re-describe every frame. A bound uniform resolves
  // per paint and declares the node volatile —
  // exactly the live-material contract.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform float uK;"
               "half4 main(float2 p) { return content.eval(p) * uK; }"));
  ASSERT_TRUE(effect) << err.c_str();
  choreograph::Output<float> k{1.0f};
  Host host;
  host.composer.render(box().child(
      box()
          .width(60)
          .height(60)
          .inset(0, 0, 140, 140)
          .absolute()
          .fill(green())
          .effect(material::skia::Effect::shader(effect).uniform("uK", &k))));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(30, 30)), 200u);  // uK=1 → full green
  k = 0.25f;     // move the bound uniform — NO re-describe
  host.frame();  // the live effect re-resolves from k
  const SkColor dimmed = host.pixel(30, 30);
  EXPECT_LT(SkColorGetG(dimmed), 120u);
  EXPECT_GT(SkColorGetG(dimmed), 20u);               // dimmed, not gone
  EXPECT_GT(host.composer.stats().nodesPainted, 0u)  // volatile: paints live
      << "a bound effect uniform must declare volatility";
}

TEST(ComposeEffects, AStaticShaderEffectPrunesByRecipe) {
  // The other half of the seam: a STATIC shader effect compares by RECIPE —
  // the runtime-effect pointer plus its constant uniforms — so a caller that
  // holds one SkRuntimeEffect and re-describes around it prunes. Comparing
  // the built filter pointer instead would re-patch every frame, since a
  // fresh filter is built each time.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform float uK;"
               "half4 main(float2 p) { return content.eval(p) * uK; }"));
  ASSERT_TRUE(effect) << err.c_str();
  Host host;
  auto tree = [&](float uK) {
    return box().child(box().width(60).height(60).fill(green()).effect(
        material::skia::Effect::shader(effect, {{"uK", uK}})));
  };
  host.composer.render(tree(0.5f));
  host.frame();
  host.composer.render(
      tree(0.5f));  // fresh material::skia::Effect, same recipe
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical shader-effect recipe re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  // …and the equality is honest: a different constant IS a change.
  host.composer.render(tree(0.9f));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
}

TEST(ComposeEffects, LiveChainsRecomposeAndStaticChainsStayCheap) {
  // then() precomposes static sides once (unchanged behaviour); a chain
  // with a live side re-composes per paint and stays honest about it.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform float uK;"
               "half4 main(float2 p) { return content.eval(p) * uK; }"));
  ASSERT_TRUE(effect) << err.c_str();
  choreograph::Output<float> k{1.0f};
  const material::skia::Effect liveChain =
      material::skia::Effect::shader(effect).uniform("uK", &k).then(
          material::skia::Effect::shader(effect, {{"uK", 0.5f}}));
  EXPECT_TRUE(liveChain.isAnimated());
  ASSERT_TRUE(liveChain.resolvedImageFilter() != nullptr);
  const material::skia::Effect staticChain =
      material::skia::Effect::shader(effect, {{"uK", 0.5f}})
          .then(material::skia::Effect::shader(effect, {{"uK", 0.5f}}));
  EXPECT_FALSE(staticChain.isAnimated());
  EXPECT_TRUE(staticChain.imageFilter() != nullptr);  // precomposed once

  // The chain applies BOTH stages: 1.0 * 0.5 through the live chain dims
  // a green fill to about half.
  Host host;
  host.composer.render(box().child(box()
                                       .width(60)
                                       .height(60)
                                       .inset(0, 0, 140, 140)
                                       .absolute()
                                       .fill(green())
                                       .effect(liveChain)));
  host.frame();
  const unsigned g = SkColorGetG(host.pixel(30, 30));
  EXPECT_GT(g, 90u);
  EXPECT_LT(g, 170u);
}

// ---------------------------------------------------------------------------
// Effect::directionalBlur — one spelling for an anisotropic blur at any
// angle, built entirely from filters Skia already has.

TEST(ComposeEffects, ADirectionalBlurAtAnAxisAngleIsBlurBitwise) {
  // At an axis-aligned angle directionalBlur must BE the plain
  // SkImageFilters::Blur call it replaces — same factory, same arguments —
  // so a caller who already wrote the Blur by hand gets identical pixels.
  // Compared pixel-for-pixel over the whole plate.
  auto plate = [](Host& host, material::skia::Effect e) {
    host.composer.render(box().child(box()
                                         .width(60)
                                         .height(60)
                                         .inset(70, 70, 70, 70)
                                         .absolute()
                                         .fill(green())
                                         .effect(std::move(e))));
    host.frame();
  };
  Host ported, hand, swapped;
  plate(ported, material::skia::Effect::directionalBlur(26, 90, 14));
  plate(hand,
        material::skia::Effect::filter(SkImageFilters::Blur(14, 26, nullptr)));
  EXPECT_TRUE(identicalPixels(ported, hand, 200, 200))
      << "directionalBlur(26, 90, 14) must BE Blur(14, 26)";
  // The control that keeps the pin honest: swapped sigmas are a
  // different picture, and this comparison can see it.
  plate(swapped,
        material::skia::Effect::filter(SkImageFilters::Blur(26, 14, nullptr)));
  EXPECT_FALSE(identicalPixels(ported, swapped, 200, 200));
}

TEST(ComposeEffects, ADirectionalBlurAtAnArbitraryAngleSmearsAlongIt) {
  // Any other angle is a rotate → Blur → unrotate sandwich — three filter
  // nodes Skia already provides, no new SkSL. A 45° streak on a centred
  // square throws ink down-right along the smear axis and none the same
  // distance across it, which is what the two probes below read.
  Host host;
  host.composer.render(box().child(
      box()
          .width(40)
          .height(40)
          .inset(80, 80, 80, 80)
          .absolute()
          .fill(green())
          .effect(material::skia::Effect::directionalBlur(18, 45))));
  host.frame();
  const unsigned along = SkColorGetG(host.pixel(125, 125));
  const unsigned acrossAxis = SkColorGetG(host.pixel(75, 125));
  EXPECT_GT(along, 40u);       // the streak reaches down-right
  EXPECT_LT(acrossAxis, 10u);  // nothing rides across the axis
  EXPECT_GT(along, acrossAxis * 4 + 8);
}

TEST(ComposeEffects, AStaticDirectionalBlurPrunesByRecipe) {
  // filter() compares by pointer, so the hand-built sites re-patched on
  // every describe. The recipe compares structurally: a re-described
  // equal directionalBlur prunes, and the equality is honest about a
  // changed angle.
  Host host;
  auto tree = [&](float angle) {
    return box().child(box().width(60).height(60).fill(green()).effect(
        material::skia::Effect::directionalBlur(12, angle, 4)));
  };
  host.composer.render(tree(30));
  host.frame();
  host.composer.render(tree(30));  // fresh material::skia::Effect, same recipe
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical directionalBlur recipe re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  host.composer.render(tree(75));  // a different angle IS a change
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
}

TEST(ComposeEffects, ABoundDirectionalBlurAngleAnimatesWithoutRedescribe) {
  // Live parameters ride the same uniform channel a Material uses: the
  // recipe's named parameters accept a bound Output, and the rotate/blur/
  // unrotate sandwich is rebuilt per paint. So an animated smear angle needs
  // no new mechanism and no re-describe — which is the difference between
  // animating it and faking it with a stack of pre-baked gradients.
  choreograph::Output<float> angle{0.0f};
  Host host;
  host.composer.render(box().child(
      box()
          .width(40)
          .height(40)
          .inset(80, 80, 80, 80)
          .absolute()
          .fill(green())
          .effect(material::skia::Effect::directionalBlur(18, 0).uniform(
              "angle", &angle))));
  host.frame();
  // angle 0: the streak runs horizontally — ink right of the box, a
  // sharp edge below it.
  EXPECT_GT(SkColorGetG(host.pixel(125, 100)), 60u);
  EXPECT_LT(SkColorGetG(host.pixel(100, 125)), 20u);
  angle = 90.0f;  // move the bound parameter — NO re-describe
  host.frame();   // the live effect re-resolves and the streak turns
  EXPECT_GT(SkColorGetG(host.pixel(100, 125)), 60u);
  EXPECT_LT(SkColorGetG(host.pixel(125, 100)), 20u);
  EXPECT_GT(host.composer.stats().nodesPainted, 0u)
      << "a bound directionalBlur parameter must declare volatility";
}

TEST(ComposeEffects, AnUnknownDirectionalBlurUniformIsIgnoredNotLive) {
  // The same guardrail Material applies: a name that is not
  // "sigma"/"angle"/"across" warns and is IGNORED. Two things must follow —
  // it does not bind, and it does not silently declare the node volatile,
  // which would repaint every frame forever over a typo.
  choreograph::Output<float> v{1.0f};
  const material::skia::Effect typo =
      material::skia::Effect::directionalBlur(10, 0).uniform("sgima", &v);
  EXPECT_FALSE(typo.isAnimated());
  // The control: a real parameter name does bind.
  const material::skia::Effect bound =
      material::skia::Effect::directionalBlur(10, 0).uniform("sigma", &v);
  EXPECT_TRUE(bound.isAnimated());
}

namespace {

/** A hard-edged test target: 8px vertical stripes in the node's OWN local
 *  space. Blur is measured as the loss of stripe contrast, and stripes
 *  make that loss readable at a pixel pair instead of over an edge
 *  profile. Static (no uniforms), so it never perturbs volatility. */
material::skia::Paint stripeFill() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, error] = SkRuntimeEffect::MakeForShader(
        SkString("half4 main(float2 p) {"
                 "  float band = mod(floor(p.x / 8.0), 2.0);"
                 "  return band < 1.0 ? half4(1) : half4(0, 0, 0, 1);"
                 "}"));
    return effect;
  }();
  return material::skia::Paint::sksl(fx);
}

/** THE PARAMETER: 0 at the node's left edge, 1 at its right, authored in
 *  the UNIT SQUARE — which is the point of using a Material as the
 *  carrier, because the box here is decided by the layout. */
material::skia::Paint focalRamp() {
  return material::skia::Paint::linearUnit(
      {0, 0}, {1, 0}, {{0.0f, {0, 0, 0, 1}}, {1.0f, {1, 1, 1, 1}}});
}

/** Local stripe contrast at canvas x (a stripe centre) — 0 is fully
 *  washed out, 255 fully sharp. The pair straddles one stripe boundary. */
int contrastAt(Host& host, int x, int y) {
  const SkColor a = host.pixel(x, y);
  const SkColor b = host.pixel(x + 8, y);
  return std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b));
}

/** The fixture every arm below shares: a 120x120 striped node at canvas
 *  (40, 40) — deliberately NOT at the origin, because a parameter
 *  Material must resolve in the NODE's space, and a map that read layer
 *  or canvas coordinates would shift its falloff by a third of the box. */
void stripePlate(Host& host, material::skia::Effect e) {
  host.composer.render(box().child(box()
                                       .width(120)
                                       .height(120)
                                       .inset(40, 40, 40, 40)
                                       .absolute()
                                       .fill(stripeFill())
                                       .effect(std::move(e))));
  host.frame();
}

}  // namespace

TEST(ComposeEffects, AParameterMapVariesTheBlurAcrossTheNode) {
  // Sharp at one edge, soft at the other, from ONE effect on ONE node — a
  // picture no constant sigma can produce, and the reason the channel
  // exists at all (a depth-of-field falloff, a lens edge).
  Host varying;
  stripePlate(varying, material::skia::Effect::blur(focalRamp(), 16));
  const int y = 100;                             // the node's vertical middle
  const int sharp = contrastAt(varying, 51, y);  // local x 11 → sigma ~1.5
  const int mid = contrastAt(varying, 67, y);    // local x 27 → sigma ~3.6
  const int soft = contrastAt(varying, 139, y);  // local x 99 → sigma ~13
  EXPECT_GT(sharp, 150) << "the map's 0 end must stay legibly sharp";
  EXPECT_LT(soft, 40) << "the map's 1 end must be washed out";
  EXPECT_GT(sharp, mid) << "sharp " << sharp << " mid " << mid;
  EXPECT_GT(mid, soft) << "the falloff must be monotonic across the node"
                       << " (sharp " << sharp << " mid " << mid << " soft "
                       << soft << ")";

  // THE CONTROLS, both directions, because "a blur happened" is not the
  // claim — "the blur VARIES" is. A constant blur at the same sigma
  // washes the sharp end too; a constant blur at zero leaves the soft end
  // sharp. Neither can be the picture above.
  Host constantMax, unblurred;
  stripePlate(constantMax, material::skia::Effect::filter(
                               SkImageFilters::Blur(16, 16, nullptr)));
  EXPECT_LT(contrastAt(constantMax, 51, y), 40)
      << "a constant max-sigma blur cannot leave the left end sharp";
  stripePlate(unblurred, material::skia::Effect::filter(
                             SkImageFilters::Offset(0, 0, nullptr)));
  EXPECT_GT(contrastAt(unblurred, 139, y), 150)
      << "…and no blur at all cannot make the right end soft";
}

TEST(ComposeEffects, AStaticParamBlurPrunesByRecipeAndByItsMap) {
  // Carrying the sigma map as a Material rather than a callable is what
  // makes the effect comparable at all. The map has to be IN the equality
  // too: a parameter read live but excluded from the comparison leaves a
  // pruned node sampling last frame's map forever.
  Host host;
  auto tree = [&](float maxSigma, material::skia::Paint map) {
    return box().child(box().width(60).height(60).fill(green()).effect(
        material::skia::Effect::blur(std::move(map), maxSigma)));
  };
  host.composer.render(tree(10, focalRamp()));
  host.frame();
  host.composer.render(
      tree(10, focalRamp()));  // fresh material::skia::Effect, same recipe
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical blur recipe re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  host.composer.render(tree(14, focalRamp()));  // a different range IS a change
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
  host.frame();
  // …and so is a different MAP at the same range.
  const material::skia::Paint flipped = material::skia::Paint::linearUnit(
      {0, 0}, {1, 0}, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0, 0, 0, 1}}});
  host.composer.render(tree(14, flipped));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "the sigma map must ride the prune signature";
}

TEST(ComposeEffects, ALiveSigmaMapMakesTheWholeEffectLive) {
  // Liveness has to be INHERITED: a live parameter must lift the whole
  // effect to live, or a bake samples the map once and the effect freezes
  // at that sample while everything around it keeps moving. The recursion
  // is Material::isAnimated()'s own.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uK;"
               "half4 main(float2 p) { return half4(half(uK), 0, 0, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  choreograph::Output<float> k{0.0f};
  const material::skia::Paint liveMap =
      material::skia::Paint::sksl(fx).uniform("uK", &k);
  EXPECT_TRUE(material::skia::Effect::blur(liveMap, 16).isAnimated());
  EXPECT_FALSE(material::skia::Effect::blur(focalRamp(), 16).isAnimated())
      << "a static map must NOT declare volatility (the control)";

  // The pixels follow the live map with no re-describe: uK 0 is sharp
  // everywhere, uK 1 is blurred everywhere.
  Host host;
  stripePlate(host, material::skia::Effect::blur(liveMap, 16));
  EXPECT_GT(contrastAt(host, 99, 100), 150);
  k = 1.0f;      // move the map — NO re-describe
  host.frame();  // the live effect re-resolves the parameter
  EXPECT_LT(contrastAt(host, 99, 100), 40);
  EXPECT_GT(host.composer.stats().nodesPainted, 0u)
      << "a live sigma map must declare volatility";
}

TEST(ComposeEffects, ABoundMaxSigmaAnimatesOnTheExistingChannel) {
  // The range rides the SAME uniform channel as every other live parameter,
  // so there is exactly one way to animate a blur rather than one per knob.
  choreograph::Output<float> range{0.0f};
  Host host;
  stripePlate(
      host,
      material::skia::Effect::blur(focalRamp(), 0).uniform("maxSigma", &range));
  EXPECT_GT(contrastAt(host, 139, 100), 150);  // range 0: no blur anywhere
  range = 16.0f;
  host.frame();
  EXPECT_LT(contrastAt(host, 139, 100), 40);  // the map's 1 end now washes
  EXPECT_GT(contrastAt(host, 51, 100), 150);  // …and its 0 end still does not
}

TEST(ComposeEffects, AnEffectChildFillsASecondDeclaredShaderSlot) {
  // Effect::shader fills exactly ONE child — "content", the node's own
  // layer — so a second declared `uniform shader` has nothing to bind it.
  // child() fills it with a Material, resolved against THIS node's box, so
  // unit-space authoring works here exactly as it does on a fill.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform shader param;"
               "half4 main(float2 p) {"
               "  return content.eval(p) * param.eval(p).r;"
               "}"));
  ASSERT_TRUE(fx) << err.c_str();
  Host host;
  host.composer.render(
      box().child(box()
                      .width(120)
                      .height(120)
                      .inset(40, 40, 40, 40)
                      .absolute()
                      .fill(green())
                      .effect(material::skia::Effect::shader(fx).child(
                          "param", focalRamp()))));
  host.frame();
  // The ramp modulates the green layer left (0) to right (1) — and the
  // ramp is in the NODE's unit square, so the dark end is at the node's
  // left edge, not the canvas's.
  EXPECT_LT(SkColorGetG(host.pixel(45, 100)), 40u);
  EXPECT_GT(SkColorGetG(host.pixel(155, 100)), 200u);
  EXPECT_NEAR((int)SkColorGetG(host.pixel(100, 100)), 128, 40);

  // …AND A STATIC CHILD REACHES THE SNAPSHOT, which the arm above cannot
  // show: a unit ramp is geometry-tier, so the paint path re-resolves it and
  // a mistake at store time is invisible. A solid never needs a context, so
  // it appears in the filter only if child() actually rebuilt the snapshot.
  Host flat;
  flat.composer.render(box().child(
      box()
          .width(120)
          .height(120)
          .inset(40, 40, 40, 40)
          .absolute()
          .fill(green())
          .effect(material::skia::Effect::shader(fx).child(
              "param", material::skia::Paint::solid({0.5f, 0.5f, 0.5f, 1})))));
  flat.frame();
  EXPECT_NEAR((int)SkColorGetG(flat.pixel(60, 100)), 128, 24);
  EXPECT_NEAR((int)SkColorGetG(flat.pixel(140, 100)), 128, 24);
}

TEST(ComposeEffects, AnUndeclaredEffectChildIsIgnoredNotBound) {
  // Material::child's guardrail, verbatim: an undeclared name warns and is
  // IGNORED — and, the sharp half, an ignored child must not declare
  // volatility either (a node painting live for a child that does nothing
  // is the silent failure this pins).
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uK;"
               "half4 main(float2 p) { return half4(half(uK), 0, 0, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  choreograph::Output<float> k{1.0f};
  const material::skia::Paint liveMap =
      material::skia::Paint::sksl(fx).uniform("uK", &k);

  // (a) filter() has no child to fill, exactly as it has no uniform.
  const sk_sp<SkImageFilter> raw = SkImageFilters::Blur(4, 4, nullptr);
  material::skia::Effect plain = material::skia::Effect::filter(raw);
  plain.child("param", liveMap);
  EXPECT_EQ(plain.imageFilter(), raw) << "filter()'s filter was replaced";
  EXPECT_FALSE(plain.isAnimated());

  // (b) a shader() effect that declares no such child.
  auto [oneChild, err2] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "half4 main(float2 p) { return content.eval(p); }"));
  ASSERT_TRUE(oneChild) << err2.c_str();
  material::skia::Effect narrow = material::skia::Effect::shader(oneChild);
  narrow.child("param", liveMap);
  EXPECT_FALSE(narrow.isAnimated());
  // …and "content" is the library's, never the author's to overwrite.
  material::skia::Effect content = material::skia::Effect::shader(oneChild);
  content.child("content", liveMap);
  EXPECT_FALSE(content.isAnimated());

  // (c) a blur()'s one child is "sigma"; a typo must not bind.
  material::skia::Effect typo = material::skia::Effect::blur(focalRamp(), 8);
  typo.child("sgima", liveMap);
  EXPECT_FALSE(typo.isAnimated());
  // THE CONTROL: the declared name does bind, and does go live.
  auto [twoChild, err3] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform shader param;"
               "half4 main(float2 p) { return content.eval(p) * "
               "param.eval(p).r; }"));
  ASSERT_TRUE(twoChild) << err3.c_str();
  material::skia::Effect bound = material::skia::Effect::shader(twoChild);
  bound.child("param", liveMap);
  EXPECT_TRUE(bound.isAnimated());
  // …and blur()'s real name re-aims the map, which is what makes the
  // child vector one mechanism rather than two.
  material::skia::Effect reaimed = material::skia::Effect::blur(focalRamp(), 8);
  reaimed.child("sigma", liveMap);
  EXPECT_TRUE(reaimed.isAnimated());
}

TEST(ComposeEffects, ADroppedUniformBindingIsLoudNotSilent) {
  // The drop the recipe-name guardrails do not reach: a filter() has no
  // uniform to receive a binding at all. It must warn like the blur paths
  // do — an author animating a filter() uniform otherwise gets neither
  // motion nor diagnostic. Control first: a valid binding on a shader()
  // stays silent. (The other drop this once covered, a null Output, can no
  // longer be spelled: the parameter is an animatable, and the empty case
  // of one is a plain number.)
  choreograph::Output<float> k{0.5f};
  ::testing::internal::CaptureStderr();
  (void)material::skia::Effect::shader(ukEffect()).uniform("uK", &k);
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "a valid binding must not warn";
  // uniform() on a filter(): warned and ignored, and still not live.
  ::testing::internal::CaptureStderr();
  material::skia::Effect plain =
      material::skia::Effect::filter(SkImageFilters::Blur(4, 4, nullptr));
  plain.uniform("uK", &k);
  const std::string filterLog = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(filterLog.find("skia::Effect::uniform"), std::string::npos)
      << filterLog;
  EXPECT_NE(filterLog.find("uK"), std::string::npos) << filterLog;
  EXPECT_FALSE(plain.isAnimated());
}

TEST(ComposeEffects, AnUndeclaredShaderUniformIsWarnedAndIgnored) {
  // The shader() path is the one that takes arbitrary names, and the builder
  // answers a name the effect does not declare — or one declared at another
  // type — with a debug abort and no write. This Skia has no SK_DEBUG, so
  // without a check the value is dropped, the effect paints with a zeroed
  // uniform, and nothing says so. Material's discipline is the standard:
  // validate at store time, warn, ignore.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform shader content;"
      "uniform float uK;"
      "uniform float2 uV;"
      "half4 main(float2 p) { return content.eval(p) * (uK + uV.x); }"));
  ASSERT_TRUE(effect) << err.c_str();
  choreograph::Output<float> k{0.5f};

  // Control: the declared float binds, silently, on both doors.
  ::testing::internal::CaptureStderr();
  const material::skia::Effect good =
      material::skia::Effect::shader(effect, {{"uK", 0.5f}});
  material::skia::Effect goodBound = material::skia::Effect::shader(effect);
  goodBound.uniform("uK", &k);
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "a declared float uniform must bind without a word";
  EXPECT_TRUE(goodBound.isAnimated());
  EXPECT_TRUE(good.imageFilter() != nullptr);

  // (a) a typo'd constant on shader(): warned, and the filter it builds is
  // the one it would have built with no binding at all.
  ::testing::internal::CaptureStderr();
  const material::skia::Effect typoConst =
      material::skia::Effect::shader(effect, {{"noSuchConst", 1.0f}});
  const std::string constLog = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(constLog.find("skia::Effect::shader"), std::string::npos)
      << constLog;
  EXPECT_NE(constLog.find("noSuchConst"), std::string::npos) << constLog;
  EXPECT_EQ(typoConst, material::skia::Effect::shader(effect))
      << "a rejected constant must leave no trace in the recipe";

  // (b) a typo'd binding on uniform(): warned, ignored, and — the part that
  // costs a repaint every frame if it is got wrong — NOT declared live.
  ::testing::internal::CaptureStderr();
  material::skia::Effect typoBound = material::skia::Effect::shader(effect);
  typoBound.uniform("noSuchBinding", &k);
  const std::string boundLog = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(boundLog.find("skia::Effect::uniform"), std::string::npos)
      << boundLog;
  EXPECT_NE(boundLog.find("noSuchBinding"), std::string::npos) << boundLog;
  EXPECT_FALSE(typoBound.isAnimated())
      << "an ignored binding must not mark the node live forever";
  EXPECT_EQ(typoBound, material::skia::Effect::shader(effect));

  // (c) a name the effect DOES declare, at another type: a float2 is not a
  // float, and assigning it is the same abort.
  ::testing::internal::CaptureStderr();
  material::skia::Effect wrongType =
      material::skia::Effect::shader(effect, {{"uV", 1.0f}});
  wrongType.uniform("uV", &k);
  const std::string typeLog = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(typeLog.find("uV"), std::string::npos) << typeLog;
  EXPECT_FALSE(wrongType.isAnimated());
  EXPECT_EQ(wrongType, material::skia::Effect::shader(effect));

  // Once per name, not once per call: a description is rebuilt every frame
  // in a live-coding host and a per-call warning would bury the console.
  ::testing::internal::CaptureStderr();
  (void)material::skia::Effect::shader(effect, {{"noSuchConst", 1.0f}});
  material::skia::Effect again = material::skia::Effect::shader(effect);
  again.uniform("noSuchBinding", &k);
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "the same rejected name must not warn twice";
}

// ---------------------------------------------------------------------------
// Material::amount(): a blend layer's strength.

TEST(ComposeMaterial, ABlendLayerCompositesAtItsAmount) {
  // "Soft-light this noise at 30%" had no expression — the only route was
  // baking the amplitude into a forked copy of the generator's SkSL.
  // amount() is Photoshop layer opacity: composite the layer in full,
  // then mix the RESULT back toward the accumulation — which on a
  // srcOver white-over-red at 0.5 lands on pink, and at 0 leaves red.
  auto plate = [](float amt) {
    Host host;
    host.composer.render(box().child(
        box()
            .width(60)
            .height(60)
            .inset(0, 0, 140, 140)
            .absolute()
            .fill(material::skia::Paint::blend(
                {{material::skia::Paint::solid({1, 0, 0, 1}),
                  SkBlendMode::kSrcOver},
                 {material::skia::Paint::solid({1, 1, 1, 1}).amount(amt),
                  SkBlendMode::kSrcOver}}))));
    host.frame();
    return host.pixel(30, 30);
  };
  const SkColor full = plate(1.0f), half = plate(0.5f), none = plate(0.0f);
  EXPECT_GT(SkColorGetG(full), 240u);       // white wins outright
  EXPECT_NEAR(SkColorGetG(half), 128, 12);  // half toward white…
  EXPECT_GT(SkColorGetR(half), 240u);       // …with red intact
  EXPECT_LT(SkColorGetG(none), 12u);        // 0 leaves the base
  EXPECT_GT(SkColorGetR(none), 240u);

  // The amount is recipe: equal amounts prune, different amounts patch.
  const material::skia::Paint a =
      material::skia::Paint::solid({1, 1, 1, 1}).amount(0.3f);
  const material::skia::Paint b =
      material::skia::Paint::solid({1, 1, 1, 1}).amount(0.3f);
  const material::skia::Paint c =
      material::skia::Paint::solid({1, 1, 1, 1}).amount(0.7f);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
}

// ---------------------------------------------------------------------------
// A texture-cached node's blend rides its blit, not a saveLayer.

TEST(ComposeCaching, ATextureBlendCompositesOnTheBlitNotALayer) {
  // Cache::Texture plus a non-srcOver blend must not allocate a
  // device-clip-sized saveLayer just to composite ONE blit — the blend
  // belongs on the blit's own paint.
  //
  // That is safe because compositing an image through a layer and drawing
  // it with the same paint directly are one operation minus an
  // intermediate, so the deferred plate must match a hand-built layer
  // composite to within the 8-bit rounding residual. And kPlus over the
  // red base must actually ACCUMULATE, or the blend was dropped rather
  // than moved.
  auto plate = [](bool texture) {
    Host host;
    host.composer.render(box().fill(red()).child(
        box()
            .width(80)
            .height(80)
            .inset(20, 20, 100, 100)
            .absolute()
            .fill(Fill::color({0.2f, 0.4f, 0.2f, 1}))
            .blend(SkBlendMode::kPlus)
            .cache(texture ? Cache::Texture : Cache::Picture)));
    for (int i = 0; i < 3; ++i)
      host.frame();  // settle: bake once, then replay/blit
    std::vector<SkColor> px;
    for (int y = 10; y < 110; y += 2)
      for (int x = 10; x < 110; x += 2) px.push_back(host.pixel(x, y));
    return px;
  };
  const auto deferred = plate(true), layered = plate(false);
  ASSERT_EQ(deferred.size(), layered.size());
  int peak = 0;
  for (size_t i = 0; i < deferred.size(); ++i)
    for (unsigned shift : {0u, 8u, 16u, 24u})
      peak = std::max(peak, std::abs((int)((deferred[i] >> shift) & 0xFFu) -
                                     (int)((layered[i] >> shift) & 0xFFu)));
  EXPECT_LE(peak, 2) << "deferred blit and layer composite disagree "
                        "beyond the 8-bit residual";
  // …and the blend is really live: plus over red saturates the red
  // channel where the child overlaps.
  Host host;
  host.composer.render(
      box().fill(red()).child(box()
                                  .width(80)
                                  .height(80)
                                  .inset(20, 20, 100, 100)
                                  .absolute()
                                  .fill(Fill::color({0.2f, 0.4f, 0.2f, 1}))
                                  .blend(SkBlendMode::kPlus)
                                  .cache(Cache::Texture)));
  for (int i = 0; i < 3; ++i) host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(50, 50)), 250u);  // 1.0 + 0.2 clamps
  EXPECT_GT(SkColorGetG(host.pixel(50, 50)), 90u);   // the child's green
}

// ---------------------------------------------------------------------------
// Material::buffer: content that changes without re-describing.

TEST(ComposeMaterial, ABufferPrunesBetweenCommitsAndPatchesOnCommit) {
  // The Instances pruning rule, on pixels: identical re-describes prune
  // while the revision holds; one commit() patches exactly once. Before
  // this seam, anything with STATE — a simulation, a video frame, a
  // scrollback — fell to custom() + Cache::None and forfeited every
  // cache and decoration slot on the node.
  auto src = std::make_shared<sigil::material::skia::PixelBuffer>(40, 40);
  src->canvas().clear(SkColorSetARGB(255, 255, 0, 0));  // red frame
  src->commit();
  Host host;
  auto tree = [&] {
    return box().child(box()
                           .width(100)
                           .height(100)
                           .inset(0, 0, 100, 100)
                           .absolute()
                           .fill(material::skia::Paint::buffer(src)));
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);

  host.composer.render(tree());  // same revision: prune
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an uncommitted buffer re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  src->canvas().clear(SkColorSetARGB(255, 0, 0, 255));  // new frame…
  src->commit();                                        // …published
  host.composer.render(tree());
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "a commit must patch its node";
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);

  // …and the new revision is itself stable.
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposeContent, AKeyedCustomPrunesAndTheKeyIsHonest) {
  // An unkeyed custom() carries an incomparable callable, so its node
  // re-records on every render(). custom(key, program) lets the author
  // declare identity instead — one key names one program — while the
  // unkeyed form stays the conservative escape hatch.
  static int runs;
  runs = 0;
  auto tree = [](const char* key, float shade) {
    auto program = [shade](SkCanvas& c, const PaintContext& ctx) {
      ++runs;
      SkPaint p;
      p.setColor4f({shade, 0, 0, 1});
      c.drawRect(SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
    };
    return box().child(key ? custom(key, program).width(60).height(60)
                           : custom(program).width(60).height(60));
  };
  Host host;
  host.composer.render(tree("panel-a", 1.0f));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
  host.composer.render(tree("panel-a", 1.0f));  // same key: prune
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  // A different key IS a change.
  host.composer.render(tree("panel-b", 0.5f));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(30, 30)), 200u);
  // Unkeyed: conservative forever.
  Host raw;
  raw.composer.render(tree(nullptr, 1.0f));
  raw.frame();
  raw.composer.render(tree(nullptr, 1.0f));
  EXPECT_GE(raw.composer.stats().patchedNodes, 1u);
}

namespace {

struct EnvPalette {
  SkColor4f surface{1, 0, 0, 1};
  SkColor4f accent{0, 1, 0, 1};
  bool operator==(const EnvPalette&) const = default;
};

/** A component four levels below whoever bound the value, handed nothing
 *  and reading the environment — the `feed::`/decoration case. */
Element envThemedChip() {
  return box().width(20).height(20).fill(
      Fill::color(core::env::inheritedOr(EnvPalette{}).surface));
}

/** Its sibling, which reads nothing and must never repatch for a theme. */
Element envPlainChip() { return box().width(20).height(20).fill(blue()); }

Element envLevel3() {
  return box().child(envThemedChip()).child(envPlainChip());
}

Element envLevel2() { return box().child(envLevel3()); }

Element envLevel1() { return box().child(envLevel2()); }

/** Describe under a binding, and hand back a tree the binding no longer
 *  touches — the whole design in three lines. */
Element envDescribeWith(EnvPalette p) {
  core::env::Provide<EnvPalette> theme(p);
  return box().child(envLevel1());
}

}  // namespace

TEST(ComposeEnv, InheritedValueReachesAComponentNobodyHandedIt) {
  Host host;
  Element tree = envDescribeWith(EnvPalette{{0, 0, 1, 1}, {1, 1, 0, 1}});
  EXPECT_FALSE(
      core::env::bound<EnvPalette>());  // the scope ended; the VALUE is
  host.composer.render(tree);           // already baked into the tree
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLUE);   // the themed chip
  EXPECT_EQ(host.pixel(5, 25), SK_ColorBLUE);  // its plain sibling

  // Unbound: the component's own default, exactly like a React context's.
  Host bare;
  bare.composer.render(box().child(envLevel1()));
  bare.frame();
  EXPECT_EQ(bare.pixel(5, 5), SK_ColorRED);
  EXPECT_FALSE(core::env::bound<EnvPalette>());  // and the scope unwound
}

TEST(ComposeEnv, UnchangedEnvironmentStillPrunes) {
  // THE PRUNING PIN. A theme-reading node whose theme did not change must
  // prune like any other structurally-equal description — no patch, no
  // re-record, host free to skip the frame.
  Host host;
  const EnvPalette dark{{0, 0, 1, 1}, {1, 1, 0, 1}};
  auto renderWith = [&](EnvPalette p) {
    host.composer.render(envDescribeWith(p));
  };
  renderWith(dark);
  host.frame();
  ASSERT_EQ(host.pixel(5, 5), SK_ColorBLUE);  // it IS the inherited colour —
                                              // without this the pin below
                                              // would pass on a tree that
                                              // never read the environment

  renderWith(dark);  // a DISTINCT palette object, equal by operator==
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeEnv, ThemeChangeRepatchesOnlyTheNodesThatMoved) {
  // The other half: a change costs exactly the readers whose props moved,
  // not the provider's subtree. Four container levels and one plain
  // sibling sit between the binding and the reader; none of them repatch.
  Host host;
  auto renderWith = [&](EnvPalette p) {
    host.composer.render(envDescribeWith(p));
  };
  renderWith(EnvPalette{{0, 0, 1, 1}, {1, 1, 0, 1}});
  host.frame();

  renderWith(EnvPalette{{0, 1, 0, 1}, {1, 1, 0, 1}});  // surface moved only
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(5, 25), SK_ColorBLUE);
}

TEST(ComposeEnv, MemoIsAPureFunctionOfPropsAndEnvironment) {
  // memo is the ONE deferred describe in the library, so it is the one
  // place an inherited value could go stale: its `fn` runs inside the
  // reconciler, long after the scope that bound the palette ended.
  struct Props {
    int id = 0;
    bool operator==(const Props&) const = default;
  };
  static int describeCalls;
  describeCalls = 0;
  auto component = [](const Props&) {
    ++describeCalls;
    return box().width(20).height(20).fill(
        Fill::color(core::env::inheritedOr(EnvPalette{}).surface));
  };

  Host host;
  // DESCRIBED inside the scope, RECONCILED after it ends — which is the
  // whole difficulty: `component` runs during render(), by which time the
  // Provide below has been destroyed. Building the tree and handing it to
  // the composer are two statements, deliberately.
  auto describeWith = [&component](EnvPalette p) {
    core::env::Provide<EnvPalette> theme(p);
    return box().child(memo(Props{1}, component).key("m"));
  };
  auto renderWith = [&](EnvPalette p) {
    Element tree = describeWith(p);
    ASSERT_FALSE(
        core::env::bound<EnvPalette>());  // the binding is gone by here
    host.composer.render(tree);
  };

  renderWith(EnvPalette{{0, 0, 1, 1}, {}});
  host.frame();
  EXPECT_EQ(describeCalls, 1);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLUE);  // the captured stack reached fn

  renderWith(EnvPalette{{0, 0, 1, 1}, {}});  // same props, EQUAL environment
  EXPECT_EQ(describeCalls, 1);
  EXPECT_EQ(host.composer.stats().memoHits, 1u);

  renderWith(EnvPalette{{0, 1, 0, 1}, {}});  // same props, environment moved
  EXPECT_EQ(describeCalls, 2);
  EXPECT_EQ(host.composer.stats().memoHits, 0u);
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorGREEN);  // not the stale blue
}

TEST(ComposeEnv, InnerProvideShadowsAndUnwinds) {
  struct EnvOther {
    int v = 0;
    bool operator==(const EnvOther&) const = default;
  };
  core::env::Provide<EnvPalette> outer(EnvPalette{{1, 0, 0, 1}, {}});
  ASSERT_TRUE(core::env::bound<EnvPalette>());
  EXPECT_TRUE(core::env::inherited<EnvPalette>()->surface ==
              SkColor4f({1, 0, 0, 1}));
  {
    core::env::Provide<EnvPalette> inner(EnvPalette{{0, 0, 1, 1}, {}});
    core::env::Provide<EnvOther> other(EnvOther{7});
    EXPECT_TRUE(core::env::inherited<EnvPalette>()->surface ==
                SkColor4f({0, 0, 1, 1}));
    EXPECT_EQ(core::env::inherited<EnvOther>()->v,
              7);  // keyed by TYPE, no crosstalk
  }
  EXPECT_TRUE(core::env::inherited<EnvPalette>()->surface ==
              SkColor4f({1, 0, 0, 1}));
  EXPECT_FALSE(core::env::bound<EnvOther>());
}

TEST(ComposeEnv, OutOfOrderDestructionCannotUnbindASibling) {
  // LIFO nesting is the contract, and violating it must be detected in
  // every build: a destructor that popped the top unconditionally would
  // remove a SIBLING's binding when scopes die out of order, corrupting an
  // environment the sibling still believes it provides. Heap providers
  // force the wrong order deliberately.
  auto outer = std::make_unique<core::env::Provide<EnvPalette>>(
      EnvPalette{{1, 0, 0, 1}, {}});
  auto inner = std::make_unique<core::env::Provide<EnvPalette>>(
      EnvPalette{{0, 0, 1, 1}, {}});
  ::testing::internal::CaptureStderr();
  outer.reset();  // destroyed FIRST, from under the inner scope
  EXPECT_NE(::testing::internal::GetCapturedStderr().find("env::Provide"),
            std::string::npos)
      << "the misuse must be loud";
  // The surviving scope's binding still resolves — the misused destructor
  // removed its own entry, not the top of the stack.
  const EnvPalette* survivor = core::env::inherited<EnvPalette>();
  ASSERT_NE(survivor, nullptr);
  EXPECT_TRUE(survivor->surface == SkColor4f({0, 0, 1, 1}));
  // The inner scope's own destruction is now below its recorded depth, so
  // it too takes the identity path; the stack still fully unwinds.
  ::testing::internal::CaptureStderr();
  inner.reset();
  (void)::testing::internal::GetCapturedStderr();
  EXPECT_FALSE(core::env::bound<EnvPalette>());
}

TEST(ComposeEnv, ALibraryComponentReadsTheEnvironmentByItsOwnPropsType) {
  // The entry's actual complaint: a library component had to be handed its
  // colours by whoever composed it. The env key is feed::TextOptions — the
  // component's OWN props type — so no library-wide Theme exists or needs
  // to.
  feed::TextRing ring;
  ring.append({u8"ready."});

  feed::TextOptions themed;
  themed.styles.base(whiteStyle(12));
  themed.window.gap = 7.0f;

  Element tree = [&] {
    core::env::Provide<feed::TextOptions> style(themed);
    return box().padding(4).child(box().child(feed::feed(ring)));
  }();
  ASSERT_FALSE(core::env::bound<feed::TextOptions>());

  Host host;
  host.composer.render(tree);
  host.frame();
  const SkRect got = require(host.composer.bounds(feed::rowKey(1)));
  EXPECT_GT(got.width(), 0.0f);

  // The unbound spelling still compiles to the component's own default —
  // and a DIFFERENT default, which is what proves the binding was read.
  Host bare;
  bare.composer.render(box().padding(4).child(
      box().child(feed::feed(ring, feed::TextOptions{}))));
  bare.frame();
  EXPECT_NE(require(bare.composer.bounds(feed::rowKey(1))).width(),
            got.width());
}

// ---------------------------------------------------------------------------
// wiggle() and the reconciler — the prune behaviour of BoundFloat's noise
// stage.

TEST(ComposeReconcile, WiggledBindingsPruneOnlyWhenEveryParameterMatches) {
  // THE TRAP, pinned. `boundMapEqual()` in Reconcile.cpp compares BoundFloat
  // FIELD BY FIELD, and a field left out of that list fails INVISIBLY: two
  // different wiggles compare equal, the node prunes, and the instance keeps
  // applying the OLD map forever while every other test still passes. So
  // each of the five wiggle fields gets its own re-describe here.
  //
  // If this test fails, do not relax it — a field is missing from
  // boundMapEqual().
  static choreograph::Output<float> phase;
  phase = 0.35f;
  struct Rig {
    float amount = 12.0f;
    float frequency = 7.0f;
    uint32_t seed = 1;
    int octaves = 1;
    float falloff = 0.5f;
  };
  auto tree = [](Rig r) {
    return box().child(
        box().key("shaken").width(40).height(40).fill(red()).translateX(
            motion::bind(&phase)
                .target(-70.0f, 170.0f)
                .wiggle(r.amount, r.frequency, r.seed, r.octaves, r.falloff)));
  };

  Host host;
  host.composer.render(tree({}));
  host.frame();

  // The prune half: an identical re-describe costs nothing.
  host.composer.render(tree({}));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical wiggle must still prune";
  EXPECT_FALSE(host.composer.dirty());

  // The over-prune half, one field at a time. Each of these is a DIFFERENT
  // wiggle and must reach the instance.
  const Rig moved[] = {
      {.amount = 20.0f}, {.frequency = 11.0f}, {.seed = 2},
      {.octaves = 3},    {.falloff = 0.9f},
  };
  const char* named[] = {"amount", "frequency", "seed", "octaves", "falloff"};
  for (size_t i = 0; i < std::size(moved); ++i) {
    host.composer.render(tree({}));  // back to the baseline rig
    host.frame();
    host.composer.render(tree(moved[i]));
    EXPECT_GT(host.composer.stats().patchedNodes, 0u)
        << named[i]
        << " changed and the node PRUNED — boundMapEqual() is "
           "missing that field";
    EXPECT_TRUE(host.composer.dirty()) << named[i];
  }
}

TEST(ComposeReconcile, TwoSeedsShakeIndependentlyOnScreen) {
  // The whole path in PIXELS: two seeds, two marks, and the displacement
  // the paint actually produced. (Not a prune pin — keyed siblings never
  // prune into one another; the prune pin is the test above. This one
  // proves the seed survives Element → reconciler → paint, which is what
  // makes a two-axis shake possible instead of a diagonal slide.)
  static choreograph::Output<float> t;
  t = 0.0f;
  Host host(200, 200);
  auto tree = [] {
    // stack(): both marks lay out at the origin, so each row below is
    // unambiguously one of them.
    return stack()
        .child(box()
                   .key("x")
                   .width(8)
                   .height(8)
                   .fill(red())
                   .translateX(
                       sigil::motion::wiggle(&t, 40.0f, 3.0f, 1).offset(100.0f))
                   .translateY(30.0f))
        .child(box()
                   .key("y")
                   .width(8)
                   .height(8)
                   .fill(green())
                   .translateX(
                       sigil::motion::wiggle(&t, 40.0f, 3.0f, 2).offset(100.0f))
                   .translateY(90.0f));
  };
  host.composer.render(tree());

  // bounds() is the LAYOUT rect and a translate is paint-only, so the
  // observation has to be pixels: where the mark actually landed.
  const auto centerOf = [&host](int row, SkColor want) {
    int lo = -1, hi = -1;
    for (int x = 0; x < 200; ++x)
      if (host.pixel(x, row) == want) {
        if (lo < 0) lo = x;
        hi = x;
      }
    return lo < 0 ? -1.0f : 0.5f * (float)(lo + hi);
  };

  bool everApart = false, xMoved = false, yMoved = false;
  float firstX = -1, firstY = -1;
  for (int frame = 0; frame < 90; ++frame) {
    t = (float)frame / 30.0f;
    host.frame();
    const float x = centerOf(34, SK_ColorRED);
    const float y = centerOf(94, SK_ColorGREEN);
    ASSERT_GE(x, 0.0f) << "frame " << frame;
    ASSERT_GE(y, 0.0f) << "frame " << frame;
    if (firstX < 0) {
      firstX = x;
      firstY = y;
    }
    if (std::fabs(x - firstX) > 4.0f) xMoved = true;
    if (std::fabs(y - firstY) > 4.0f) yMoved = true;
    if (std::fabs(x - y) > 6.0f) everApart = true;
  }
  EXPECT_TRUE(xMoved) << "the x shake never moved";
  EXPECT_TRUE(yMoved) << "the y shake never moved";
  EXPECT_TRUE(everApart)
      << "two seeds produced the same displacement every frame — either the "
         "seed is not reaching the noise, or the second node pruned into the "
         "first";
}

namespace {

/** The centroid of every pixel of @p color, or (-1,-1) when none. Motion is
 *  a VISUAL feature: these pins scan the frame, they do not read floats out
 *  of the resolver. */
SkPoint inkCentroid(Host& host, SkColor color, int w, int h) {
  double sx = 0, sy = 0;
  int n = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (host.pixel(x, y) == color) {
        sx += x;
        sy += y;
        ++n;
      }
  if (n == 0) return {-1, -1};
  return {(float)(sx / n), (float)(sy / n)};
}

/** The bounding box of everything even faintly @p color-ish — enough to
 *  ask "is this bar lying flat or standing up", which is what an
 *  orientation pin actually wants to know. */
SkIRect inkBounds(Host& host, int w, int h) {
  SkIRect box = SkIRect::MakeEmpty();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (SkColorGetR(host.pixel(x, y)) > 100) {
        const SkIRect one = SkIRect::MakeXYWH(x, y, 1, 1);
        if (box.isEmpty())
          box = one;
        else
          box.join(one);
      }
  return box;
}

/** A 160x160 frame inset at (20,20) of a 200x200 canvas, carrying one small
 *  square that travels. The inscribed circle is then centre (100,100) r=80
 *  in CANVAS coordinates, so every quadrant point is on-screen. */
Element travelFrame(Element rider) {
  return box().child(box()
                         .key("frame")
                         .absolute()
                         .rect(SkRect::MakeXYWH(20, 20, 160, 160))
                         .child(std::move(rider)));
}

Element rider(MotionPath along, float size = 8) {
  return box()
      .key("dot")
      .absolute()
      .rect(SkRect::MakeXYWH(0, 0, size, size))
      .fill(red())
      .travel(std::move(along));
}

}  // namespace

TEST(ComposeTravel, TIsAFractionOfTotalArcLengthAcrossEveryContour) {
  // An L with legs of 100 and 20: half the LENGTH is 60 px along the long
  // leg. Anything parameter-flavoured (per verb, per contour) lands at the
  // bend instead.
  Host host(200, 200);
  choreograph::Output<float> t{0.5f};
  const auto bent = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(0, 0);
    b.lineTo(100, 0);
    b.lineTo(100, 20);
    return b.detach();
  };
  host.composer.render(travelFrame(rider({.path = bent, .t = &t})));
  host.frame();
  SkPoint ink = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(ink.x(), 80.0f, 1.5f) << "t=0.5 is not 60 px along a 120 px L";
  EXPECT_NEAR(ink.y(), 20.0f, 1.5f);

  // Two contours of 20 and 100: half the total (60) is 40 into the SECOND,
  // which no per-contour split can produce.
  const auto twoRuns = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(0, 0);
    b.lineTo(20, 0);
    b.moveTo(0, 100);
    b.lineTo(100, 100);
    return b.detach();
  };
  host.composer.render(travelFrame(rider({.path = twoRuns, .t = &t})));
  host.frame();
  ink = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(ink.x(), 60.0f, 1.5f)
      << "the contours are not concatenated by LENGTH";
  EXPECT_NEAR(ink.y(), 120.0f, 1.5f);
}

TEST(ComposeTravel, APathWithNoMeasurableLengthLeavesTheLanesStanding) {
  Host host(200, 200);
  host.composer.render(
      travelFrame(rider({.path = [](SkSize) { return SkPath(); }, .t = 0.5f})
                      .translateX(40)));
  host.frame();
  const SkPoint ink = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(ink.x(), 63.5f, 1.5f)
      << "an empty path engaged anyway and swallowed the translate lane";
  EXPECT_NEAR(ink.y(), 23.5f, 1.5f);
}

TEST(ComposeTravel, PerAxisScaleParticipatesInReconcilerEquality) {
  // Per-axis scale has to reach propsEqual like every other paint field.
  // Left out, two descriptions differing only in scaleX compare EQUAL: the
  // patch prunes, the node is never marked paint-dirty, and the old picture
  // replays at the old scale — a wrong picture with no failure anywhere.
  Host host(200, 200);
  const auto bar = [](float sx) {
    return box().child(box()
                           .key("bar")
                           .absolute()
                           .rect(SkRect::MakeXYWH(0, 0, 40, 40))
                           .transformOrigin(0, 0)
                           .fill(red())
                           .scaleX(sx));
  };
  host.composer.render(bar(1.0f));
  host.frame();
  EXPECT_EQ(host.pixel(60, 20), SK_ColorBLACK);

  host.composer.render(bar(1.0f));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an unchanged scaleX patched — the counter is measuring something "
         "else";

  host.composer.render(bar(2.0f));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
      << "a CHANGED scaleX pruned into the old description";
  EXPECT_EQ(host.pixel(60, 20), SK_ColorRED)
      << "…and the stale picture replayed";

  host.composer.render(
      bar(2.0f).child(box()
                          .key("y")
                          .absolute()
                          .rect(SkRect::MakeXYWH(0, 60, 40, 40))
                          .transformOrigin(0, 0)
                          .fill(green())
                          .scaleY(1.0f)));
  host.frame();
  host.composer.render(
      bar(2.0f).child(box()
                          .key("y")
                          .absolute()
                          .rect(SkRect::MakeXYWH(0, 60, 40, 40))
                          .transformOrigin(0, 0)
                          .fill(green())
                          .scaleY(2.0f)));
  host.frame();
  EXPECT_EQ(host.pixel(20, 130), SK_ColorGREEN)
      << "a CHANGED scaleY pruned into the old description";
}

namespace {

SkPoint brightestPixel(Host& host) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(bm.pixmap(), 0, 0);
  int bestX = 0, bestY = 0, best = -1;
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 200; ++x) {
      const SkColor c = bm.getColor(x, y);
      const int lum =
          (int)SkColorGetR(c) + (int)SkColorGetG(c) + (int)SkColorGetB(c);
      if (lum > best) {
        best = lum;
        bestX = x;
        bestY = y;
      }
    }
  return {(float)bestX, (float)bestY};
}

/** One light over the whole canvas — a radial highlight authored at CANVAS
 *  (70,70). Flagged, it is authored once; unflagged, it is hand-converted
 *  into the node's local px exactly as chaucer_astrolabe's brass() does. */
material::skia::Paint canvasLight(bool flagged,
                                  SkPoint nodeOriginForHandConversion) {
  const SkPoint c = flagged ? SkPoint{70, 70}
                            : SkPoint{70 - nodeOriginForHandConversion.x(),
                                      70 - nodeOriginForHandConversion.y()};
  material::skia::Paint m = material::skia::Paint::radial(
      c, 70, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0.1f, 0.05f, 0, 1}}});
  if (flagged) m.worldSpace();
  return m;
}

/** The chaucer shape in miniature: a panel at canvas (40,40,120,120)
 *  inside a group that rotates about its own centre — the rete. */
Element rotatedInstrument(float rotationDeg, bool flagged) {
  auto group = box()
                   .rect(SkRect::MakeXYWH(20, 20, 160, 160))
                   .key("group")
                   .rotate(rotationDeg)
                   .transformOrigin(0.5f, 0.5f);
  // The corner child makes the panel RECORD — a childless leaf paints
  // live and re-resolves on every reach, which would hide a stale-W
  // recording from every pin built on this fixture.
  group.child(
      box()
          .absolute()
          .left(20)
          .top(20)
          .width(120)
          .height(120)
          .key("panel")
          .fill(canvasLight(flagged, {40, 40}))
          .child(box().absolute().left(0).top(0).width(4).height(4).fill(
              Fill::color({0, 0.3f, 0, 1}))));
  return box().child(std::move(group));
}

}  // namespace

// A rotated node samples the world field THROUGH its rotation, so the
// highlight sits still in canvas space while the object turns. Without the
// flag the highlight rotates with the object, which is the control below.
//
// This also exercises the reconcile-time staleWorldSpaceBelow walk: the
// rotation is a DESCRIBED transform on an ancestor, and the panel below it
// prunes, so its recording has to be staled explicitly — nothing about the
// panel's own description changed.
TEST(ComposeWorldSpace, ARotatedNodeSamplesTheWorldFieldThroughItsRotation) {
  Host host;
  host.composer.render(rotatedInstrument(0, true));
  host.frame();
  const SkPoint at0 = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(at0, {70, 70}), 3.0f)
      << "flagged light not at its authored canvas position";
  // SAME host, re-described rotation — the ancestor patches, the panel
  // prunes, and the panel's recording must not replay the old anchoring.
  host.composer.render(rotatedInstrument(40, true));
  host.frame();
  const SkPoint at40 = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(at0, at40), 3.0f)
      << "the highlight turned with the object — world anchoring failed";
  // Control: the same instrument WITHOUT the flag, whose light turns with
  // the object. Without this arm the assertion above would also pass on a
  // fixture where the rotation never reached the light at all.
  Host c0, c40;
  c0.composer.render(rotatedInstrument(0, false));
  c40.composer.render(rotatedInstrument(40, false));
  c0.frame();
  c40.frame();
  EXPECT_GT(SkPoint::Distance(brightestPixel(c0), brightestPixel(c40)), 10.0f)
      << "the unflagged control no longer reproduces the defect";
}

// A field continuous ACROSS separately-laid-out nodes: two flex siblings
// share one worldSpace ramp and the edge between them is pixel-continuous.
// Unflagged, each node restarts the ramp in its own space and the edge
// jumps, which is the control.
TEST(ComposeWorldSpace, TwoSiblingsShareOneContinuousField) {
  const auto scene = [](bool flagged) {
    material::skia::Paint ramp = material::skia::Paint::linear(
        {0, 0}, {200, 0}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}});
    if (flagged) ramp.worldSpace();
    return box()
        .row()
        .child(box().width(100).height(200).fill(ramp))
        .child(box().width(100).height(200).fill(ramp));
  };
  Host flagged, control;
  flagged.composer.render(scene(true));
  control.composer.render(scene(false));
  flagged.frame();
  control.frame();
  const auto redAt = [](Host& h, int x) {
    return (int)SkColorGetR(h.pixel(x, 100));
  };
  // Flagged: the edge is continuous (a 4-px step across it moves the ramp
  // by ~2% of its span at most)…
  EXPECT_LT(std::abs(redAt(flagged, 98) - redAt(flagged, 102)), 20)
      << "the flagged siblings do not share one field";
  // …and it IS the canvas-wide ramp: red end left, blue end right.
  EXPECT_GT(redAt(flagged, 4), 220);
  EXPECT_LT(redAt(flagged, 196), 40);
  // Control: each node restarts, so the second sibling snaps back to red.
  EXPECT_GT(std::abs(redAt(control, 98) - redAt(control, 102)), 80)
      << "the unflagged control is continuous — the pin lost its contrast";
}

// Alignment through the layout offset: a node at (40,40) samples the field
// where it SITS. The control is a resolve with an identity root matrix —
// the documented degradation when a material is resolved outside a composer
// — which anchors node-locally and shifts the falloff by exactly the
// offset.
TEST(ComposeWorldSpace, TheLayoutOffsetAlignsTheFieldAndIdentityDegrades) {
  Host host;
  host.composer.render(
      box().child(box().absolute().left(40).top(40).width(120).height(120).fill(
          canvasLight(true, {0, 0}))));
  host.frame();
  EXPECT_LT(SkPoint::Distance(brightestPixel(host), {70, 70}), 3.0f)
      << "the composer resolve did not anchor through the layout offset";
  // Identity-toRoot resolve: same material, standalone PaintContext (no
  // composer, toRoot = I). The falloff lands node-LOCALLY — painted at the
  // node's offset it sits at canvas (110,110), a (40,40) shift.
  PaintContext bare;
  bare.size = {120, 120};
  const Fill f = resolveFill(canvasLight(true, {0, 0}), bare);
  ASSERT_EQ(f.kind, Fill::Kind::Shader);
  Host raw;
  SkPaint p;
  p.setShader(f.shaderValue);
  raw.surface->getCanvas()->clear(SK_ColorBLACK);
  raw.surface->getCanvas()->save();
  raw.surface->getCanvas()->translate(40, 40);
  raw.surface->getCanvas()->drawRect(SkRect::MakeWH(120, 120), p);
  raw.surface->getCanvas()->restore();
  EXPECT_LT(SkPoint::Distance(brightestPixel(raw), {110, 110}), 3.0f)
      << "identity toRoot must degrade to node-local, deterministically";
}

// LAYOUT moves the node while the field stays put in canvas space. The
// node's own props PRUNE here — only a sibling spacer changes — so the
// invalidation can only come from the layout-rect sync noticing the node's
// world matrix moved. No other phase sees this.
TEST(ComposeWorldSpace, ALayoutMoveLeavesTheFieldAnchored) {
  const auto scene = [](float spacer) {
    material::skia::Paint light =
        material::skia::Paint::radial(
            {100, 100}, 70, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0.1f, 0.05f, 0, 1}}})
            .worldSpace();
    // The corner child makes the panel RECORD (a childless leaf paints
    // live and would re-resolve on every reach, hiding the stale-W hole
    // this pin exists to close).
    return box()
        .row()
        .child(box().width(spacer).height(10))
        .child(box().width(120).height(200).key("panel").fill(light).child(
            box().absolute().left(0).top(0).width(4).height(4).fill(green())));
  };
  Host host;
  host.composer.render(scene(20));
  host.frame();
  const SkPoint before = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(before, {100, 100}), 3.0f);
  host.composer.render(scene(60));  // the panel slides right, pruning
  host.frame();
  const SkPoint after = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(before, after), 3.0f)
      << "the field moved with the layout — the recording kept its old W";
}

// An ANCESTOR's layout move must re-anchor a descendant whose own
// parent-relative rect never changed. The descendant has no local evidence
// that it moved at all, so "an ancestor above you moved" has to be threaded
// down the walk.
TEST(ComposeWorldSpace, AnAncestorsMoveReanchorsTheDescendant) {
  const auto scene = [](float spacerH) {
    material::skia::Paint light =
        material::skia::Paint::radial(
            {100, 100}, 70, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0.1f, 0.05f, 0, 1}}})
            .worldSpace();
    // column: spacer, then a group whose panel child is absolutely inset —
    // the group MOVES, the panel's rect relative to the group does not.
    return box()
        .child(box().width(10).height(spacerH))
        .child(box().width(200).height(140).key("group").child(
            box().absolute().inset(10).key("panel").fill(light)));
  };
  Host host;
  host.composer.render(scene(20));
  host.frame();
  const SkPoint before = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(before, {100, 100}), 3.0f);
  host.composer.render(scene(50));  // the whole group slides down
  host.frame();
  EXPECT_LT(SkPoint::Distance(before, brightestPixel(host)), 3.0f)
      << "the ancestor moved and the descendant kept its old anchoring";
}

// A BOUND transform drives the ancestor and the field stays anchored on
// every frame. Three stages are exercised in order: the volatility lift
// while the motion runs, the release once it settles, and the per-draw scan
// on the frame it resumes. Without the lift the parent's recording replays
// the old anchoring under the live rotation, which is a wrong picture rather
// than a stale one.
TEST(ComposeWorldSpace, ABoundTransformKeepsTheFieldAnchoredPerFrame) {
  ch::Output<float> rot{0};
  Host host;
  const auto describe = [&] {
    auto group = box()
                     .rect(SkRect::MakeXYWH(20, 20, 160, 160))
                     .key("group")
                     .rotate(&rot)
                     .transformOrigin(0.5f, 0.5f);
    group.child(box()
                    .absolute()
                    .left(20)
                    .top(20)
                    .width(120)
                    .height(120)
                    .key("panel")
                    .fill(canvasLight(true, {40, 40})));
    host.composer.render(box().child(std::move(group)));
  };
  describe();
  host.frame();
  const SkPoint anchored = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(anchored, {70, 70}), 3.0f);
  // Drive the rotation externally — no re-describe, no ticker motion.
  for (float angle : {10.0f, 25.0f, 40.0f}) {
    rot = angle;
    host.frame(1.0 / 60.0);
    EXPECT_LT(SkPoint::Distance(anchored, brightestPixel(host)), 3.0f)
        << "at bound rotation " << angle << " the field turned with the object";
  }
  // Hold still long enough for the released-scalar path to take over…
  for (int i = 0; i < 12; ++i) host.frame(1.0 / 60.0);
  EXPECT_LT(SkPoint::Distance(anchored, brightestPixel(host)), 3.0f);
  // …then RESUME: the released scan gains the node's W, so the very next
  // frame re-anchors — nothing stale replays.
  rot = 65.0f;
  host.frame(1.0 / 60.0);
  EXPECT_LT(SkPoint::Distance(anchored, brightestPixel(host)), 3.0f)
      << "the frame the released rotation resumed served stale anchoring";
}

// The worldSpace flag is part of the RECIPE: an identical re-describe with
// it set prunes, and flipping it patches. Leave it out of Material equality
// and a node that stops being world-space keeps the old anchoring forever.
TEST(ComposeWorldSpace, TheFlagRidesThePruneSignature) {
  const std::vector<material::skia::Stop> stops{{0.0f, {1, 0, 0, 1}},
                                                {1.0f, {0, 0, 1, 1}}};
  EXPECT_TRUE(
      material::skia::Paint::linear({0, 0}, {200, 0}, stops).worldSpace() ==
      material::skia::Paint::linear({0, 0}, {200, 0}, stops).worldSpace());
  EXPECT_FALSE(
      material::skia::Paint::linear({0, 0}, {200, 0}, stops).worldSpace() ==
      material::skia::Paint::linear({0, 0}, {200, 0}, stops));
  const auto scene = [&](bool flagged) {
    material::skia::Paint m =
        material::skia::Paint::linear({0, 0}, {200, 0}, stops);
    if (flagged) m.worldSpace();
    return box().child(box().width(100).height(100).key("panel").fill(m));
  };
  Host host;
  host.composer.render(scene(true));
  host.frame();
  host.composer.render(scene(true));  // identical → prunes
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical world-space fill failed to prune";
  host.composer.render(scene(false));  // the flip must patch
  EXPECT_GT(host.composer.stats().patchedNodes, 0u)
      << "a flag flip pruned — the node would keep the old anchoring";
}

// The node's world matrix joins the resolve digest. A LIVE world-space
// material whose bound inputs hold still while the NODE moves must still
// rebuild its shader — a digest cannot notice an input it was never fed, so
// omitting the matrix serves the pre-move shader on the frame after a move.
//
// The move here is a setSize() RELAYOUT against a grow() spacer, with no
// re-describe, so the same live recipe and its memo survive it. Re-describing
// the material would mint a fresh memo and hide the hole entirely.
TEST(ComposeWorldSpace, TheResolveDigestSeesTheNodeMove) {
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uDrive;"
               "half4 main(float2 p) {"
               "  return p.x < 130.0 ? half4(1, 0, 0, 1) + half4(uDrive)*0.0"
               "                     : half4(0, 0, 1, 1);"
               "}"));
  ASSERT_TRUE(fx) << err.c_str();
  ch::Output<float> drive{0};  // bound and HELD — the digest's other input
  material::skia::Paint m = material::skia::Paint::sksl(fx);
  m.uniform("uDrive", &drive);
  m.worldSpace();
  Host host;
  host.composer.render(
      box()
          .row()
          .child(box().grow(1).height(10))
          .child(box().width(120).height(200).key("panel").fill(m)));
  host.frame();
  // Canvas 200 wide: the spacer grows to 80, the panel spans [80, 200] —
  // the red→blue boundary sits at CANVAS x=130 (world coordinates).
  EXPECT_GT(SkColorGetR(host.pixel(124, 100)), 200u);
  EXPECT_GT(SkColorGetB(host.pixel(136, 100)), 200u);
  // Shrink the canvas: the spacer shrinks to 40, the panel slides to
  // [40, 160]. NOTHING was re-described and the binds never moved — W is
  // the only input that changed.
  host.composer.setSize({160, 200});
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(124, 100)), 200u)
      << "the boundary rode the node — the digest served a stale shader";
  EXPECT_GT(SkColorGetB(host.pixel(136, 100)), 200u)
      << "the boundary rode the node — the digest served a stale shader";
}
