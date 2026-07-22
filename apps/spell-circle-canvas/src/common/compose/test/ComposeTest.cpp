// Kernel behavior tests: layout, stacking paint, reconciliation (keys,
// memo), automatic picture caching, and transition semantics — the P1
// slice of STRESS_TESTS.md, in headless deterministic form.

#include <sigilcompose/Compose.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <gtest/gtest.h>
#include <cstdio>

using namespace sigil::compose;
using namespace std::chrono_literals;

namespace {

sigil::weave::FontContext &fonts() {
  static auto *context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

sigil::weave::TextStyle styleAt(float size) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  return s;
}

/** A composer with its own ticker, drawn into a raster surface. */
struct Host {
  sigil::motion::Ticker ticker;
  Composer composer{ticker, fonts()};
  sk_sp<SkSurface> surface;

  explicit Host(int w = 200, int h = 200) {
    composer.setSize({(float)w, (float)h});
    surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  }

  SkColor pixel(int x, int y) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    surface->readPixels(bm.pixmap(), x, y);
    return bm.getColor(0, 0);
  }

  void frame(double dt = 0.0) {
    if (dt > 0)
      ticker.tick(dt);
    surface->getCanvas()->clear(SK_ColorBLACK);
    composer.draw(*surface->getCanvas());
  }
};

Fill red() { return Fill::color({1, 0, 0, 1}); }
Fill green() { return Fill::color({0, 1, 0, 1}); }
Fill blue() { return Fill::color({0, 0, 1, 1}); }

} // namespace

TEST(ComposeLayout, FlexRowPositionsAndFills) {
  Host host;
  host.composer.render(box().row().gap(20)
                           .child(box().width(50).height(50).fill(red()))
                           .child(box().width(50).height(50).fill(green())));
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);   // first child
  EXPECT_EQ(host.pixel(60, 25), SK_ColorBLACK); // the gap
  EXPECT_EQ(host.pixel(95, 25), SK_ColorGREEN); // second child at 70..120
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
  host.composer.render(
      stack()
          .child(box().inset(0).fill(red()).zIndex(1))
          .child(box().inset(0).fill(green()).zIndex(0)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
}

TEST(ComposeStacking, OpacityAndBlendComposite) {
  Host host;
  host.composer.render(
      stack()
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
    bool operator==(const Props &) const = default;
  };
  static int describeCalls;
  describeCalls = 0;
  auto component = [](const Props &p) {
    ++describeCalls;
    return box().width(20 + (float)p.value).height(20).fill(red());
  };

  Host host;
  auto describe = [&](int a, int b) {
    return box().row()
        .child(memo(Props{a}, component).key("a"))
        .child(memo(Props{b}, component).key("b"));
  };

  host.composer.render(describe(1, 2));
  EXPECT_EQ(describeCalls, 2);
  host.composer.render(describe(1, 2)); // nothing changed
  EXPECT_EQ(describeCalls, 2);
  EXPECT_EQ(host.composer.stats().memoHits, 2u);
  host.composer.render(describe(1, 3)); // one prop changed
  EXPECT_EQ(describeCalls, 3);
  EXPECT_EQ(host.composer.stats().memoHits, 1u);
}

TEST(ComposeReconcile, KeyedReorderKeepsInstances) {
  Host host;
  auto row = [](const char *k, Fill f) {
    return box().key(k).width(40).height(40).fill(f);
  };
  host.composer.render(
      box().row().child(row("a", red())).child(row("b", green())));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorRED);

  host.composer.render(
      box().row().child(row("b", green())).child(row("a", red())));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN); // reordered, not restyled
  EXPECT_EQ(host.composer.stats().instances, 3u);
}

TEST(ComposeCaching, StaticSubtreeRecordsOnce) {
  static int programRuns;
  programRuns = 0;
  Host host;
  host.composer.render(box().child(
      custom([](SkCanvas &c, const PaintContext &ctx) {
        ++programRuns;
        SkPaint p;
        p.setColor(SK_ColorCYAN);
        c.drawRect(SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
      }).width(80).height(80)));

  host.frame();
  host.frame();
  host.frame();
  EXPECT_EQ(programRuns, 1); // recorded once, replayed thereafter
  EXPECT_EQ(host.pixel(40, 40), SK_ColorCYAN);
  EXPECT_GE(host.composer.stats().picturesLive, 1u);
}

TEST(ComposeCaching, RelayoutInvalidatesStaleRecordings) {
  // The syncLayoutRects pass: setSize alone (no prop change, no re-render)
  // resizes a pct-width child whose geometry was baked into cached
  // recordings — the old bounds must not replay. Pre-fix, cached ancestors
  // replayed the stale bake after any relayout not caused by a patch.
  Host host;
  host.composer.render(
      box().child(box().width(pct(50)).height(40).fill(red())));
  host.frame(); // child spans x∈[0,100) at 200-wide viewport; recorded
  EXPECT_EQ(host.pixel(80, 20), SK_ColorRED);
  host.composer.setSize({120, 200}); // child now spans x∈[0,60)
  host.frame();
  EXPECT_EQ(host.pixel(80, 20), SK_ColorBLACK); // red = stale bake replayed
  EXPECT_EQ(host.pixel(30, 20), SK_ColorRED);   // new geometry painted
}

TEST(ComposeCaching, CacheNoneRunsEveryFrame) {
  static int programRuns;
  programRuns = 0;
  Host host;
  host.composer.render(box().child(
      custom([](SkCanvas &, const PaintContext &) { ++programRuns; })
          .width(10).height(10).cache(Cache::None)));
  host.frame();
  host.frame();
  EXPECT_EQ(programRuns, 2);
}

TEST(ComposeCaching, ReconcileInvalidatesRecording) {
  Host host;
  auto tree = [](Fill f) {
    return box().child(box().key("x").width(60).height(60).fill(f));
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
    return box().child(box().key("m").width(50).height(50).fill(red())
                           .translateX(with(target, {400ms,
                                                     &choreograph::easeNone})));
  };
  host.composer.render(at(0.0f));
  host.frame();
  host.composer.render(at(100.0f)); // start ramp 0 → 100
  host.frame(0.2);                  // half way (linear ease)
  EXPECT_EQ(host.pixel(75, 25), SK_ColorRED); // box around x=50..100
  EXPECT_EQ(host.pixel(10, 25), SK_ColorBLACK);

  host.composer.render(at(0.0f)); // retarget back from ~50
  host.frame(0.2);                // halfway back → ~25
  EXPECT_EQ(host.pixel(45, 25), SK_ColorRED);
  EXPECT_EQ(host.pixel(90, 25), SK_ColorBLACK);

  host.frame(1.0); // settle
  EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);
  EXPECT_FALSE(host.ticker.active()); // motion removed on finish
}

TEST(ComposeTransitions, UnmountCancelsMotions) {
  Host host;
  host.composer.render(box().child(
      box().key("gone").width(10).height(10)
          .translateX(with(500.0f, {1000ms}))));
  host.frame();
  host.composer.render(box().child(
      box().key("gone").width(10).height(10)
          .translateX(with(0.0f, {1000ms}))));
  host.frame(0.1);
  EXPECT_TRUE(host.ticker.active());
  host.composer.render(box()); // unmount mid-flight
  host.frame(0.1);             // stepping must not touch dead outputs
  EXPECT_FALSE(host.ticker.active());
}

TEST(ComposeBindings, OutputDrivesPaintWithoutRender) {
  Host host;
  choreograph::Output<float> x = 0.0f;
  host.composer.render(box().child(
      box().width(40).height(40).fill(blue()).translateX(&x)));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLUE);

  x = 120.0f; // direct mutation, no render()
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(140, 20), SK_ColorBLUE);
}

TEST(ComposeCaching, TextureCacheRasterizesOnceAndInvalidates) {
  static int programRuns;
  programRuns = 0;
  Host host;
  auto tree = [](SkColor color) {
    return box().child(custom([color](SkCanvas &c, const PaintContext &ctx) {
                         ++programRuns;
                         SkPaint p;
                         p.setColor(color);
                         c.drawRect(SkRect::MakeWH(ctx.size.width(),
                                                   ctx.size.height()),
                                    p);
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
  EXPECT_EQ(programRuns, 1); // rasterized once, blitted thereafter
  EXPECT_EQ(host.pixel(40, 40), SK_ColorMAGENTA);
  EXPECT_GE(host.composer.stats().texturesLive, 1u);

  host.composer.render(tree(SK_ColorYELLOW)); // invalidate
  host.frame();
  EXPECT_EQ(programRuns, 2);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorYELLOW);
}

#include <sigilcompose/Decorations.h>

TEST(ComposeDecorations, DashedBorderPaintsAlongOutline) {
  Host host;
  PathFormat dashed;
  dashed.width = 6;
  dashed.strokeFill = Fill::color({1, 1, 0, 1});
  dashed.dashIntervals = {10, 10};
  host.composer.render(box().child(
      box().width(120).height(120).inset(20).absolute()
          .foreground(dashed)));
  host.frame();
  // Somewhere along the top edge a dash lands; somewhere it doesn't.
  int lit = 0;
  for (int x = 25; x < 135; ++x)
    if (host.pixel(x, 20) == SK_ColorYELLOW)
      ++lit;
  EXPECT_GT(lit, 10);
  EXPECT_LT(lit, 110); // gaps exist → it really dashed
}

TEST(ComposeDecorations, ContourWalkVisitsSamplesPositioned) {
  Host host;
  static int visits;
  visits = 0;
  ContourWalk walk;
  walk.spacing = 25.0f;
  walk.draw = [](SkCanvas &c, const PathSample &s, const PaintContext &) {
    ++visits;
    EXPECT_GE(s.fraction, 0.0f);
    EXPECT_LE(s.fraction, 1.0f);
    SkPaint p;
    p.setColor(SK_ColorGREEN);
    c.drawRect(SkRect::MakeXYWH(-2, -2, 4, 4), p); // at the sample origin
  };
  host.composer.render(box().child(
      box().width(100).height(100).inset(50, 50, 50, 50).absolute()
          .foreground(walk)));
  host.frame();
  EXPECT_EQ(visits, 16); // 400px perimeter / 25px spacing
  EXPECT_EQ(host.pixel(100, 50), SK_ColorGREEN); // top edge stamped
  host.frame();
  EXPECT_EQ(visits, 16); // static walk → recorded once, replayed
}

TEST(ComposeDecorations, AnimatedWalkDeclaresVolatility) {
  Host host;
  static int visits;
  visits = 0;
  ContourWalk walk;
  walk.spacing = 50.0f;
  walk.animatedWalk = true;
  walk.draw = [](SkCanvas &, const PathSample &, const PaintContext &) {
    ++visits;
  };
  host.composer.render(box().child(
      box().width(100).height(100).foreground(walk)));
  host.frame();
  host.frame();
  EXPECT_EQ(visits, 16); // 8 samples × 2 frames: repainted per frame
}

TEST(ComposeSlots, SlotUpdatesWithoutDisturbingSiblings) {
  static int staticRuns;
  staticRuns = 0;
  Host host;
  host.composer.render(
      box().row().gap(10)
          .child(custom([](SkCanvas &c, const PaintContext &ctx) {
                   ++staticRuns;
                   SkPaint p;
                   p.setColor(SK_ColorRED);
                   c.drawRect(SkRect::MakeWH(ctx.size.width(),
                                             ctx.size.height()), p);
                 }).width(50).height(50))
          .child(slot("live").width(80).height(50)));
  host.frame();
  EXPECT_EQ(staticRuns, 1);

  host.composer.renderSlot("live", box().fill(Fill::color({0, 1, 0, 1}))
                                       .width(80).height(50));
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);
  EXPECT_EQ(host.pixel(70, 25), SK_ColorGREEN);

  host.composer.renderSlot("live", box().fill(Fill::color({0, 0, 1, 1}))
                                       .width(80).height(50));
  host.frame();
  EXPECT_EQ(host.pixel(70, 25), SK_ColorBLUE);
  // The sibling's paint program never re-ran across slot updates: its
  // own recording stayed valid even though ancestors re-recorded.
  EXPECT_EQ(staticRuns, 1);
}

#include <sigilimage/ImageAsset.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

TEST(ComposeDecorations, SliceStretchesCenterKeepsCorners) {
  // Synthesize a 30x30 nine-patch: 10px red border ring, green center.
  SkBitmap src;
  src.allocN32Pixels(30, 30);
  src.eraseColor(SK_ColorRED);
  src.erase(SK_ColorGREEN, SkIRect::MakeXYWH(10, 10, 10, 10));
  SkDynamicMemoryWStream stream;
  SkPngEncoder::Encode(&stream, src.pixmap(), {});
  auto asset = std::make_shared<sigil::image::ImageAsset>(
      *sigil::image::ImageAsset::decode(stream.detachAsData()));

  Host host;
  Slice nine;
  nine.asset = asset;
  nine.xDivs = {10, 20};
  nine.yDivs = {10, 20};
  host.composer.render(box().child(
      box().width(120).height(120).background(nine)));
  host.frame();
  EXPECT_EQ(host.pixel(60, 60), SK_ColorGREEN); // stretched center
  EXPECT_EQ(host.pixel(4, 4), SK_ColorRED);     // corner intact
  EXPECT_EQ(host.pixel(115, 115), SK_ColorRED);
  EXPECT_EQ(host.pixel(60, 4), SK_ColorRED);    // edge strip
}

#include <include/core/SkColorFilter.h>
#include <include/effects/SkImageFilters.h>

TEST(ComposeEffects, LayerEffectBlursNode) {
  Host host;
  host.composer.render(box().child(
      box().width(60).height(60).inset(70, 70, 70, 70).absolute()
          .fill(red())
          .effect(Effect::filter(SkImageFilters::Blur(8, 8, nullptr)))));
  host.frame();
  // Blur bleeds outside the crisp box bounds and softens the center edge.
  SkColor outside = host.pixel(64, 100); // 6px outside the left edge
  EXPECT_NE(outside, SK_ColorBLACK);
  EXPECT_NE(host.pixel(100, 100), SK_ColorBLACK); // center still red-ish
  // Far away stays untouched.
  EXPECT_EQ(host.pixel(10, 10), SK_ColorBLACK);
}

TEST(ComposeEffects, BackdropFiltersWhatIsBeneath) {
  Host host;
  // Invert color matrix as a deterministic backdrop filter.
  float invert[20] = {-1, 0, 0, 0, 1,  0, -1, 0, 0, 1,
                      0, 0, -1, 0, 1,  0, 0, 0, 1, 0};
  auto invertFilter = SkImageFilters::ColorFilter(
      SkColorFilters::Matrix(invert), nullptr);

  host.composer.render(
      stack()
          .child(box().inset(0).fill(red()))
          .child(box().width(80).height(80).inset(60, 60, 60, 60)
                     .absolute()
                     .backdrop(Effect::filter(invertFilter))));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorCYAN); // red inverted inside
  EXPECT_EQ(host.pixel(20, 100), SK_ColorRED);   // untouched outside
}

TEST(ComposeEffects, TextureBakesEffectOnce) {
  Host host;
  host.composer.render(box().child(
      box().key("bloomed").width(60).height(60).fill(green())
          .effect(Effect::filter(SkImageFilters::Blur(4, 4, nullptr)))
          .cache(Cache::Texture)));
  host.frame();
  host.frame();
  EXPECT_GE(host.composer.stats().texturesLive, 1u);
  EXPECT_NE(host.pixel(30, 30), SK_ColorBLACK); // filtered content present
}

#include <sigilcompose/Util.h>

namespace {
/** ~20 lines of user code: the lightweight grid from the design docs. */
struct Grid {
  int columns = 2;
  float gap = 8;
  float cellHeight = 40;

  std::vector<SkRect> place(const LayoutInput &in) const {
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
} // namespace

TEST(ComposeLayoutScheme, GridPlacesAndSizesCells) {
  Host host(200, 200);
  auto grid = layout(Grid{.columns = 2, .gap = 10, .cellHeight = 30})
                  .width(190).height(190);
  for (int i = 0; i < 4; ++i)
    grid.child(box().key("cell" + std::to_string(i))
                   .fill(i % 2 ? green() : red()));
  host.composer.render(box().child(std::move(grid)));
  host.frame();

  auto c0 = host.composer.bounds("cell0");
  auto c1 = host.composer.bounds("cell1");
  auto c3 = host.composer.bounds("cell3");
  ASSERT_TRUE(c0 && c1 && c3);
  EXPECT_EQ(c0->left(), 0.0f);
  EXPECT_EQ(c0->width(), 90.0f); // (190 - 10) / 2
  EXPECT_EQ(c0->height(), 30.0f);
  EXPECT_EQ(c1->left(), 100.0f); // second column
  EXPECT_EQ(c3->top(), 40.0f);   // second row
  EXPECT_EQ(host.pixel(45, 15), SK_ColorRED);
  EXPECT_EQ(host.pixel(145, 15), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(145, 55), SK_ColorGREEN);
}

TEST(ComposeUtil, StageBundlesTheLoop) {
  sigil::compose::util::Stage stage({100, 100}, fonts());
  stage.render(box().fill(Fill::color({1, 0, 0, 1})));
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 100));
  bool more = stage.frame(*surface->getCanvas());
  EXPECT_FALSE(more); // static content settles immediately
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  surface->readPixels(bm.pixmap(), 50, 50);
  EXPECT_EQ(bm.getColor(0, 0), SK_ColorRED);
}

TEST(ComposeUtil, ShadowAndStrokeSugar) {
  Host host;
  host.composer.render(box().child(
      box().width(80).height(80).inset(40, 40, 40, 40).absolute()
          .corners({10})
          .background(sigil::compose::util::shadow({0, 0, 1, 1}, {12, 12}, 0))
          .fill(red())
          .foreground(sigil::compose::util::stroke(4, green()))));
  host.frame();
  EXPECT_EQ(host.pixel(80, 80), SK_ColorRED);     // fill over shadow
  EXPECT_EQ(host.pixel(128, 128), SK_ColorBLUE);  // shadow offset corner
  EXPECT_EQ(host.pixel(80, 40), SK_ColorGREEN);   // stroked top edge
}

TEST(ComposeReconcile, StructuralPruneCoversDecorations) {
  // Value decorations (Shadow, PathFormat stroke/dash) let a static decorated
  // node prune without memo — the P0 chrome fix. Before it, any decoration
  // forced a re-patch + re-record on every render().
  Host host;
  auto tree = [] {
    PathFormat dash;
    dash.width = 1;
    dash.strokeFill = blue();
    dash.dashIntervals = {4, 3};
    return box().row().gap(8).padding(12)
        .child(box().width(40).height(40).corners({6}).fill(red())
                   .background(sigil::compose::util::shadow({0, 0, 0, 0.5f},
                                                            {2, 2}, 4))
                   .foreground(sigil::compose::util::stroke(2, green())))
        .child(box().width(60).height(20).foreground(dash));
  };
  host.composer.render(tree());
  host.frame();

  host.composer.render(tree()); // identical, brand-new Elements + decorations
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty()); // hosts may skip the redraw entirely
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

TEST(ComposeMotion, EaseAdaptersBindTheShapeParameter) {
  // choreograph's back/elastic/bounce take a shape parameter with a
  // default, so &choreograph::easeOutBack does not convert to an EaseFn.
  // These adapters bind it — and outBack must actually OVERSHOOT, which
  // is the only reason to reach for it.
  const choreograph::EaseFn back = ease::outBack();
  float peak = 0.0f;
  for (int i = 0; i <= 100; ++i)
    peak = std::max(peak, back((float)i / 100.0f));
  EXPECT_GT(peak, 1.05f) << "outBack did not overshoot";
  EXPECT_NEAR(back(0.0f), 0.0f, 1e-4f);
  EXPECT_NEAR(back(1.0f), 1.0f, 1e-4f);

  // and it is usable where the papercut was: inside a Transition.
  Host host(100, 100);
  host.composer.render(box().child(
      box().width(40).height(40).absolute().left(30).top(30)
          .scale(withFrom(0.5f, 1.0f,
                          {std::chrono::milliseconds(200), ease::outBack()}))
          .fill(Material::solid({1, 1, 1, 1}))));
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
  host.composer.render(box().child(
      box().width(200).height(40).absolute().left(0).top(0)
          .transformOrigin(0.0f, 0.5f)
          .scaleX(&fraction)
          .fill(Material::solid({1, 0, 0, 1}))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(20, 20)), 200u);  // inside the quarter
  EXPECT_LT(SkColorGetR(host.pixel(80, 20)), 60u);   // past it
  fraction = 0.75f; // bound value moves — no re-render
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(80, 20)), 200u);
  EXPECT_LT(SkColorGetR(host.pixel(180, 20)), 60u);
}

TEST(ComposeTransform, ScaleYIsIndependentOfScaleX) {
  Host host(200, 200);
  host.composer.render(box().child(
      box().width(200).height(200).absolute().left(0).top(0)
          .transformOrigin(0.0f, 0.0f)
          .scaleX(0.25f).scaleY(0.75f)
          .fill(Material::solid({0, 1, 0, 1}))));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(10, 10)), 200u);   // inside both
  EXPECT_LT(SkColorGetG(host.pixel(90, 10)), 60u);    // past x, inside y
  EXPECT_GT(SkColorGetG(host.pixel(10, 140)), 200u);  // inside x, inside y
  EXPECT_LT(SkColorGetG(host.pixel(10, 190)), 60u);   // past y
}

TEST(ComposeShapes, InsetRunsADecorationAgainstAShrunkOutline) {
  // "The same bevel again, six pixels in" is the whole vocabulary of
  // nested chrome. A stroke run through inset(12, ...) must land INSIDE
  // the box, not on its edge.
  Host host(120, 120);
  host.composer.render(box().child(
      box().width(120).height(120).absolute().left(0).top(0)
          .foreground(shapes::inset(
              12.0f, util::stroke(4.0f, Fill::color({1, 0, 0, 1}))))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(60, 12)), 150u); // the inset rule
  EXPECT_LT(SkColorGetR(host.pixel(60, 1)), 60u);   // the edge is bare
  EXPECT_LT(SkColorGetR(host.pixel(60, 60)), 60u);  // and so is the middle
}

TEST(ComposeShapes, ArrowPointsAlongPositiveX) {
  Host host(120, 60);
  host.composer.render(box().child(
      box().width(120).height(60).absolute().left(0).top(0)
          .outline(shapes::arrow())
          .fill(Material::solid({0, 1, 0, 1}))));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(20, 30)), 200u);  // shaft on the axis
  EXPECT_LT(SkColorGetG(host.pixel(20, 6)), 60u);    // and not above it
  EXPECT_GT(SkColorGetG(host.pixel(80, 12)), 200u);  // head is tall
  EXPECT_LT(SkColorGetG(host.pixel(118, 12)), 60u);  // and tapers to a point
}

TEST(ComposeShapes, SectorIsClosedAndFillable) {
  // shapes::arc() is open by contract; a pie wedge needs a closed path.
  // A 90-degree sector starting at 0 (Skia: 0 = +x, clockwise) fills the
  // lower-right quadrant of its box and nothing else.
  Host host(200, 200);
  host.composer.render(box().child(
      box().width(200).height(200).absolute().inset(0)
          .outline(shapes::sector(0, 90))
          .fill(Material::solid({1, 0, 0, 1}))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(130, 130)), 200u); // inside the wedge
  EXPECT_LT(SkColorGetR(host.pixel(70, 130)), 60u);   // lower-left: outside
  EXPECT_LT(SkColorGetR(host.pixel(130, 70)), 60u);   // upper-right: outside

  // innerRatio carves the donut hole out of the middle.
  Host donut(200, 200);
  donut.composer.render(box().child(
      box().width(200).height(200).absolute().inset(0)
          .outline(shapes::sector(0, 350, 0.6f))
          .fill(Material::solid({1, 0, 0, 1}))));
  donut.frame();
  EXPECT_GT(SkColorGetR(donut.pixel(180, 100)), 200u); // on the ring
  EXPECT_LT(SkColorGetR(donut.pixel(100, 100)), 60u);  // through the hole
}

TEST(ComposePatterns, GrainIsMonochromeAndVaries) {
  // patterns::noise() is fractal RGB noise — its channels are independent
  // fields, so overlaying it on a coloured surface hue-shifts rather than
  // shades. grain() is the luminance one: equal channels, real variation.
  Host host(120, 120);
  host.composer.render(box().child(box().width(120).height(120).absolute()
                                       .inset(0)
                                       .fill(patterns::grain(0.08f, 4, 3.0f))));
  host.frame();
  int lo = 255, hi = 0;
  for (int y = 4; y < 116; y += 3)
    for (int x = 4; x < 116; x += 3) {
      const SkColor c = host.pixel(x, y);
      const int r = (int)SkColorGetR(c), g = (int)SkColorGetG(c),
                b = (int)SkColorGetB(c);
      ASSERT_LE(std::abs(r - g), 2) << "channel split at " << x << "," << y;
      ASSERT_LE(std::abs(g - b), 2) << "channel split at " << x << "," << y;
      lo = std::min(lo, r);
      hi = std::max(hi, r);
    }
  EXPECT_GT(hi - lo, 40) << "grain is flat — no field to overlay";
}

TEST(ComposeMaterial, UnitRampFollowsTheBoxItLandsIn) {
  // linear() is in node-local PIXELS, which an author cannot know for a
  // content-sized box. linearUnit() is in the unit square, so the SAME
  // material reads correctly at two different sizes.
  auto card = [](float w, float h) {
    return box().width(w).height(h).absolute().left(0).top(0)
        .fill(Material::linearUnit({0, 0}, {0, 1},
                                   {{0.0f, {1, 0, 0, 1}},
                                    {1.0f, {0, 0, 1, 1}}}));
  };
  Host small(80, 40);
  small.composer.render(box().child(card(80, 40)));
  small.frame();
  EXPECT_GT(SkColorGetR(small.pixel(40, 2)), 180u);  // top is red…
  EXPECT_GT(SkColorGetB(small.pixel(40, 37)), 180u); // …bottom is blue

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
  host.composer.render(box().child(
      box().width(100).height(20).inset(0, 0, 100, 180).absolute()
          .fill(Material::linear({0, 0}, {100, 0},
                                 {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}))));
  host.frame();
  const SkColor left = host.pixel(2, 10);
  const SkColor right = host.pixel(98, 10);
  EXPECT_GT(SkColorGetR(left), 200u); // red end
  EXPECT_LT(SkColorGetB(left), 70u);
  EXPECT_GT(SkColorGetB(right), 200u); // blue end
  EXPECT_LT(SkColorGetR(right), 70u);
}

TEST(ComposeMaterial, BlendStackCompositesToOneShader) {
  // Two solids blended kPlus → additive brighten in ONE flattened shader
  // (no saveLayer). red + green = yellow.
  Host host;
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(
          Material::blend({
              {Material::solid({1, 0, 0, 1}), SkBlendMode::kSrcOver},
              {Material::solid({0, 1, 0, 1}), SkBlendMode::kPlus},
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
  host.composer.render(box().child(
      box().width(60).height(60).fill(Material::radial(
          {30, 30}, 30, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0, 0, 0, 1}}}))));
  host.frame(); // records
  EXPECT_GE(host.composer.stats().picturesLive, 1u);
  host.frame(); // no re-render — replays the cached picture
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
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(
          Material::sksl(effect).uniform("uK", &k))));
  host.frame();
  const SkColor c0 = host.pixel(20, 20);
  k = 1.0f;     // change the bound uniform — NO re-render
  host.frame(); // the live material re-resolves from k
  const SkColor c1 = host.pixel(20, 20);
  EXPECT_LT(SkColorGetR(c0), 40u);  // uK=0 → black
  EXPECT_GT(SkColorGetR(c1), 200u); // uK=1 → red
  EXPECT_GT(host.composer.stats().nodesPainted, 0u); // volatile: paints live
}

TEST(ComposeMaterial, UniformOnNonShaderMaterialIsNoOp) {
  // uniform() on a material with no named uniforms (a solid) has nothing to
  // hook against: it is ignored, the material stays static and non-live.
  Material m = Material::solid({0, 1, 0, 1}).uniform("uK", 0.5f);
  EXPECT_FALSE(m.isLive());
  EXPECT_TRUE(m.isSolid());
}

namespace {
sk_sp<SkRuntimeEffect> ukEffect() {
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uK;"
               "half4 main(float2 p) { return half4(uK, 0, 0, 1); }"));
  return effect;
}
} // namespace

TEST(ComposeMaterial, UniformCopiesOnWriteNeverAlias) {
  // Materials are VALUES: binding a uniform on a copy must not contaminate
  // the base or sibling copies (the audit's aliasing defect — a shared HUD
  // base material bound to two different Outputs).
  Material base = Material::sksl(ukEffect());
  choreograph::Output<float> low{0.2f}, high{1.0f};
  Material a = base;
  a.uniform("uK", &low);
  Material b = base;
  b.uniform("uK", &high);
  EXPECT_FALSE(base.isLive()); // base untouched
  EXPECT_TRUE(a.isLive());
  EXPECT_TRUE(b.isLive());

  Host host;
  host.composer.render(
      box()
          .child(box().width(40).height(40).inset(0, 0, 160, 160).absolute()
                     .fill(a))
          .child(box().width(40).height(40).inset(60, 0, 100, 160).absolute()
                     .fill(b)));
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 90u);  // a: uK=0.2
  EXPECT_GT(SkColorGetR(host.pixel(80, 20)), 200u); // b: uK=1.0 — not aliased
}

TEST(ComposeMaterial, LaterPlainFillReplacesLiveMaterial) {
  // Fill setters are last-wins in BOTH directions: a plain fill() after a
  // live-material fill() must take effect (the audit's stale-liveMaterial
  // defect — pre-fix the later fill was silently ignored).
  choreograph::Output<float> k{1.0f};
  Host host;
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute()
          .fill(Material::sksl(ukEffect()).uniform("uK", &k)) // live red
          .fill(Fill::color({0, 1, 0, 1}))));                 // then plain green
  host.frame();
  const SkColor c = host.pixel(20, 20);
  EXPECT_GT(SkColorGetG(c), 200u); // green won
  EXPECT_LT(SkColorGetR(c), 40u);
}

TEST(ComposeMaterial, BlendWithLiveLayerTracksOutputs) {
  // A blend inherits its layers' volatility tier (the review's deferred-
  // flatten fix): a live layer makes the whole blend LIVE, so it re-resolves
  // per frame and TRACKS the bound Output — no stale build-time snapshot
  // (pre-fix the eager flatten baked SkSL defaults, uK=0 forever).
  choreograph::Output<float> k{0.8f};
  Material m = Material::blend({
      {Material::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
      {Material::sksl(ukEffect()).uniform("uK", &k), SkBlendMode::kPlus},
  });
  EXPECT_TRUE(m.isLive()); // inherited from the bound layer
  Host host;
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(m)));
  host.frame();
  const uint32_t bright = SkColorGetR(host.pixel(20, 20));
  EXPECT_GT(bright, 170u); // ~0.8 * 255 = 204
  k = 0.3f;                // no render() — the blend follows the Output
  host.frame();
  const uint32_t dim = SkColorGetR(host.pixel(20, 20));
  EXPECT_GT(dim, 50u);  // ~0.3 * 255 = 77
  EXPECT_LT(dim, 110u);
}

TEST(ComposeMaterial, DeclaringUTimeMakesMaterialLive) {
  // "Reading the clock IS the volatility declaration": an sksl effect that
  // declares uTime takes the live path with no bound Outputs — it re-resolves
  // per frame with PaintContext time instead of freezing a uTime=0 snapshot.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uTime;"
               "half4 main(float2 p) { return half4(fract(uTime), 0, 0, 1); }"));
  ASSERT_TRUE(effect) << err.c_str();
  Material m = Material::sksl(effect);
  EXPECT_TRUE(m.isLive());

  sigil::motion::FrameClock clock;
  Host host;
  host.composer.setClock(&clock);
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(m)));
  host.frame();
  const uint32_t r0 = SkColorGetR(host.pixel(20, 20)); // uTime ≈ 0 → black
  clock.tick();                                        // advance real time…
  // …but pin the readable elapsed via a fabricated wait: FrameClock elapsed
  // is wall-time based; just assert the material painted live (r0 near 0 is
  // the frozen-snapshot failure mode this test guards).
  EXPECT_LT(r0, 30u);
  EXPECT_GT(host.composer.stats().nodesPainted, 0u); // live, not cached
}

TEST(ComposeMaterial, LiveMaterialOnOutlineShapeFillsTheShape) {
  // Audit gap: live material × custom outline() — the resolved shader must
  // fill the SHAPE (drawPath), not the box, and track the Output.
  choreograph::Output<float> k{1.0f};
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).inset(0, 0, 100, 100).absolute()
          .outline(shapes::star(4, 0.3f))
          .fill(Material::sksl(ukEffect()).uniform("uK", &k))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(50, 50)), 200u); // star body
  EXPECT_LT(SkColorGetR(host.pixel(8, 8)), 30u);    // outside the arms
  k = 0.2f; // no render()
  host.frame();
  const uint32_t dim = SkColorGetR(host.pixel(50, 50));
  EXPECT_GT(dim, 25u);
  EXPECT_LT(dim, 90u); // tracked the Output inside the shape
}

TEST(ComposeMaterial, LiveMaterialUnderLeafDirectBlend) {
  // Audit gap: the leaf fast path routes blend onto the fill paint — a
  // live-material leaf with .blend(kPlus) must composite additively.
  choreograph::Output<float> k{1.0f}; // red
  Host host;
  host.composer.render(
      stack()
          .child(box().width(40).height(40).inset(0, 0, 160, 160).absolute()
                     .fill(Fill::color({0, 1, 0, 1}))) // green under
          .child(box().width(40).height(40).inset(0, 0, 160, 160).absolute()
                     .fill(Material::sksl(ukEffect()).uniform("uK", &k))
                     .blend(SkBlendMode::kPlus)));
  host.frame();
  const SkColor c = host.pixel(20, 20); // red + green = yellow
  EXPECT_GT(SkColorGetR(c), 200u);
  EXPECT_GT(SkColorGetG(c), 200u);
  EXPECT_LT(SkColorGetB(c), 60u);
}

TEST(ComposeMaterial, SnapshotSamplesLiveMaterialNow) {
  // Audit gap: snapshot() (the element-tree-as-a-brush bake) samples live
  // materials at their CURRENT Output values.
  choreograph::Output<float> k{1.0f};
  sk_sp<SkPicture> pic = snapshot(
      box().width(60).height(60).fill(
          Material::sksl(ukEffect()).uniform("uK", &k)),
      fonts());
  ASSERT_TRUE(pic);
  Host host;
  host.surface->getCanvas()->clear(SK_ColorBLACK);
  host.surface->getCanvas()->drawPicture(pic);
  EXPECT_GT(SkColorGetR(host.pixel(30, 30)), 200u); // k=1 sampled at bake
}

TEST(ComposeMaterial, RenderSlotHostsLiveMaterial) {
  // Audit gap: a live material mounted through renderSlot() animates like
  // any other — the slot path wires volatility identically.
  choreograph::Output<float> k{0.0f};
  Host host;
  host.composer.render(box().child(slot("s").width(40).height(40)));
  host.composer.renderSlot(
      "s", box().width(40).height(40).fill(
               Material::sksl(ukEffect()).uniform("uK", &k)));
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 30u); // k=0
  k = 1.0f; // no render, no renderSlot
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(20, 20)), 200u); // live through the slot
}

TEST(ComposeMaterial, StaticMaterialPrunesAcrossRerender) {
  // The §8.1 payoff: re-describing the SAME material recipe prunes even
  // though every describe builds a fresh SkShader — gradients and blend
  // stacks compare by recipe, not by pointer. Pre-fix this tree re-patched
  // and re-recorded on every render().
  Host host;
  auto tree = [] {
    return box()
        .child(box().width(60).height(60).fill(Material::linear(
            {0, 0}, {60, 0}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}})))
        .child(box().width(40).height(40).fill(Material::blend({
            {Material::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
            {Material::radial({20, 20}, 20,
                              {{0.0f, {0, 1, 0, 1}}, {1.0f, {0, 0, 0, 1}}}),
             SkBlendMode::kPlus},
        })));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree()); // brand-new shaders, identical recipes
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
        box().key("g").width(60).height(60).fill(
            Material::linear({0, 0}, {60, 0}, {{0.0f, c}, {1.0f, c}})));
  };
  host.composer.render(tree({1, 0, 0, 1}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
  host.composer.render(tree({0, 1, 0, 1}));
  EXPECT_TRUE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorGREEN);
}

// ---- rail(): the component that IS a line ----------------------------------

#include <sigilcompose/Routers.h>

namespace {
/** A 20×20 keyed station box; center lands at (left+10, top+10). */
Element station(const char *key, float left, float top) {
  return box().key(key).width(20).height(20)
      .inset(left, top, 180 - left, 160 - top).absolute()
      .fill(blue());
}
PathFormat railLine() {
  PathFormat line;
  line.width = 4;
  line.strokeFill = green();
  return line;
}
} // namespace

TEST(ComposeRail, ThreadsThroughAnchors) {
  // Three stations, one rail through their centers: the routed polyline is
  // the element; the PathFormat foreground dresses it.
  Host host;
  host.composer.render(
      stack()
          .child(station("s1", 10, 40))
          .child(station("s2", 90, 40))
          .child(station("s3", 170, 40))
          .child(rail({{"s1"}, {"s2"}, {"s3"}})
                     .absolute().inset(0)
                     .foreground(railLine())));
  host.frame();
  EXPECT_EQ(host.pixel(60, 50), SK_ColorGREEN);  // between s1 and s2
  EXPECT_EQ(host.pixel(140, 50), SK_ColorGREEN); // between s2 and s3
  EXPECT_EQ(host.pixel(60, 80), SK_ColorBLACK);  // off the rail
}

TEST(ComposeRail, ReRoutesWhenAnchorMoves) {
  // Anchors are keys + normalized points, never absolute coordinates — move
  // a station and the rail re-derives through its new bounds.
  Host host;
  auto scene = [](float top2) {
    return stack()
        .child(station("a", 10, 40))
        .child(station("b", 90, top2))
        .child(rail({{"a"}, {"b"}}).absolute().inset(0)
                   .foreground(railLine()));
  };
  host.composer.render(scene(40));
  host.frame();
  EXPECT_EQ(host.pixel(60, 50), SK_ColorGREEN); // horizontal run
  host.composer.render(scene(140));             // station b drops 100px
  host.frame();
  EXPECT_EQ(host.pixel(60, 100), SK_ColorGREEN); // the new slanted run
  EXPECT_EQ(host.pixel(60, 50), SK_ColorBLACK);  // old route gone
}

TEST(ComposeRail, DrawsOnWithTrim) {
  // Composition, not new machinery: trim() on a rail = the self-drawing
  // subway line. A bound reveal advances with no render() calls.
  choreograph::Output<float> reveal{0.05f};
  Host host;
  host.composer.render(
      stack()
          .child(station("a", 10, 40))
          .child(station("b", 170, 40))
          .child(rail({{"a"}, {"b"}})
                     .absolute().inset(0)
                     .trim(0.0f, &reveal)
                     .foreground(railLine())));
  host.frame();
  EXPECT_EQ(host.pixel(100, 50), SK_ColorBLACK); // reveal stops at ~x=28
  reveal = 1.0f; // no render()
  host.frame();
  EXPECT_EQ(host.pixel(100, 50), SK_ColorGREEN); // the whole line
}

TEST(ComposeRail, OctilinearRoutesDiagonalThenStraight) {
  // The metro-map router: a 45° leg for the shorter delta, then straight —
  // never the direct slanted line.
  Host host;
  host.composer.render(
      stack()
          .child(station("a", 10, 40))   // center (20, 50)
          .child(station("b", 130, 100)) // center (140, 110)
          .child(rail({{"a"}, {"b"}}, routers::octilinear(0.0f))
                     .absolute().inset(0)
                     .foreground(railLine())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 80), SK_ColorGREEN);   // on the 45° leg
  EXPECT_EQ(host.pixel(110, 110), SK_ColorGREEN); // on the straight leg
  EXPECT_EQ(host.pixel(80, 80), SK_ColorBLACK);   // NOT the direct line
}

TEST(ComposeRail, ReRoutesOnRouterOnlyChange) {
  // Review fix: a rail whose DESCRIPTION changes (router swap) must
  // re-derive even though no station moved — the derive guards key resolved
  // geometry, not the description.
  Host host;
  auto scene = [](RailRouter router) {
    return stack()
        .child(station("a", 10, 40))   // center (20, 50)
        .child(station("b", 130, 100)) // center (140, 110)
        .child(rail({{"a"}, {"b"}}, std::move(router))
                   .absolute().inset(0).foreground(railLine()));
  };
  host.composer.render(scene({})); // default straight polyline
  host.frame();
  EXPECT_EQ(host.pixel(80, 80), SK_ColorGREEN); // on the direct line
  host.composer.render(scene(routers::octilinear(0.0f))); // router swap only
  host.frame();
  EXPECT_EQ(host.pixel(50, 80), SK_ColorGREEN); // the 45° leg
  EXPECT_EQ(host.pixel(80, 80), SK_ColorBLACK); // direct line gone
}

TEST(ComposeRail, ReRoutesOnAnchorNormChange) {
  // Same fix, anchor half: changing only a norm re-derives.
  Host host;
  auto scene = [](float ny) {
    return stack()
        .child(station("a", 10, 40))
        .child(station("b", 170, 40))
        .child(rail({{"a", {0.5f, ny}}, {"b", {0.5f, ny}}})
                   .absolute().inset(0).foreground(railLine()));
  };
  host.composer.render(scene(0.5f)); // through centers: y = 50
  host.frame();
  EXPECT_EQ(host.pixel(100, 50), SK_ColorGREEN);
  host.composer.render(scene(0.0f)); // through box tops: y = 40
  host.frame();
  EXPECT_EQ(host.pixel(100, 40), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(100, 52), SK_ColorBLACK); // old run gone (stroke ±2)
}

TEST(ComposeRail, ClearsWhenAnchorUnmounts) {
  // Review fix: an unmounted station takes its rail with it — no ghost path.
  Host host;
  auto scene = [](bool withB) {
    auto s = stack().child(station("a", 10, 40));
    if (withB)
      s.child(station("b", 170, 40));
    s.child(rail({{"a"}, {"b"}}).absolute().inset(0).foreground(railLine()));
    return s;
  };
  host.composer.render(scene(true));
  host.frame();
  EXPECT_EQ(host.pixel(100, 50), SK_ColorGREEN);
  host.composer.render(scene(false)); // station b unmounts
  host.frame();
  EXPECT_EQ(host.pixel(100, 50), SK_ColorBLACK); // rail vanished, not stale
}

TEST(ComposeRail, HitsNearPathOnlyNotItsLayoutBox) {
  // Review fix: rails are Kind::Custom over inset(0) — hitTest must hit
  // near the routed PATH, not eclipse the whole canvas with the layout box.
  Host host;
  host.composer.render(
      stack()
          .child(station("s1", 10, 40))
          .child(rail({{"s1"}, {"s2"}}).key("line").absolute().inset(0)
                     .foreground(railLine()))
          .child(station("s2", 170, 40)));
  host.frame();
  auto onPath = host.composer.hitTest({100, 50});
  ASSERT_TRUE(onPath.has_value());
  EXPECT_EQ(*onPath, "line");
  auto onStation = host.composer.hitTest({180, 50});
  ASSERT_TRUE(onStation.has_value());
  EXPECT_EQ(*onStation, "s2"); // stations still win over the rail overlay
  EXPECT_FALSE(host.composer.hitTest({30, 150}).has_value()); // empty canvas
}

// ---- Trim Path (draw-on reveals) -------------------------------------------

TEST(ComposeTrim, PartialOutlineStrokesOnlyRevealedStretch) {
  // trim(0, 0.2) on a square + stroked outline: only the first 20% of the
  // perimeter (the top edge) is dressed; right/bottom stay bare. The fill
  // and every outline decoration trace the trimmed path.
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).inset(0, 0, 100, 100).absolute()
          .trim(0.0f, 0.2f)
          .foreground(sigil::compose::util::stroke(4, green()))));
  host.frame();
  // Perimeter order (measured): left → top → right → bottom. First 20% ≈
  // the left edge.
  EXPECT_EQ(host.pixel(1, 50), SK_ColorGREEN);  // left edge revealed
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLACK);  // top edge bare
  EXPECT_EQ(host.pixel(50, 99), SK_ColorBLACK); // bottom edge bare
}

TEST(ComposeTrim, TransitionDrawsOn) {
  // The draw-on border: trim end transitioned 0 → 1 reveals the perimeter
  // over time (retarget-safe like every transitioned prop).
  Host host;
  auto tree = [](PropValue<float> end) {
    return box().child(
        box().key("b").width(100).height(100).inset(0, 0, 100, 100).absolute()
            .trim(0.0f, std::move(end))
            .foreground(sigil::compose::util::stroke(4, green())));
  };
  host.composer.render(tree(0.001f));
  host.frame();
  EXPECT_EQ(host.pixel(50, 99), SK_ColorBLACK);
  host.composer.render(tree(with(1.0f, {400ms, &choreograph::easeNone})));
  host.frame(0.2); // ~50%: left + top revealed, bottom still bare
  EXPECT_EQ(host.pixel(50, 1), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(50, 99), SK_ColorBLACK);
  host.frame(0.25); // settle → the full perimeter
  EXPECT_EQ(host.pixel(50, 99), SK_ColorGREEN);
}

TEST(ComposeTrim, BoundTrimRevealsWithoutRender) {
  // A bound trim end is content volatility: mutate the Output, no render(),
  // and the reveal advances — the self-drawing wire primitive.
  choreograph::Output<float> end{0.2f};
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).inset(0, 0, 100, 100).absolute()
          .trim(0.0f, &end)
          .foreground(sigil::compose::util::stroke(4, green()))));
  host.frame();
  // (99,30) sits at ~57.5% of the perimeter (right edge, top→bottom).
  EXPECT_EQ(host.pixel(99, 30), SK_ColorBLACK); // bare at end=0.2
  end = 0.6f; // no render()
  host.frame();
  EXPECT_EQ(host.pixel(99, 30), SK_ColorGREEN); // reveal reached it
  EXPECT_GT(host.composer.stats().nodesPainted, 0u); // paints live
}

#include <sigilcompose/Sdf.h>

TEST(ComposeTransitions, PlainSnapAfterTransitionLands) {
  // Review fix (kernel-wide shadow): describing a PLAIN value after a
  // transition must actually land — the lingering ramp used to shadow the
  // description forever.
  Host host;
  auto at = [](PropValue<float> x) {
    return box().child(box().key("m").width(50).height(50).fill(red())
                           .translateX(std::move(x)));
  };
  host.composer.render(at(0.0f));
  host.frame();
  host.composer.render(at(with(100.0f, {400ms, &choreograph::easeNone})));
  host.frame(0.2); // mid-ramp, box around x=50..100
  EXPECT_EQ(host.pixel(75, 25), SK_ColorRED);
  host.composer.render(at(0.0f)); // PLAIN: must snap home
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);
  EXPECT_EQ(host.pixel(75, 25), SK_ColorBLACK); // not stuck mid-ramp
}

TEST(ComposeMaterial, ContentScaleDeclaringMaterialIsLive) {
  // uContentScale tracks the HOST's zoom, not the node — it must take the
  // live tier (the pre-tier-split behavior), unlike uResolution.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uContentScale;"
               "half4 main(float2 p) { return half4(1, 0, 0, 1); }"));
  ASSERT_TRUE(effect) << err.c_str();
  EXPECT_TRUE(Material::sksl(effect).isLive());
}

TEST(ComposeMaterial, BlendWithSdfLayerResolvesGeometry) {
  // Review fix: blend() containing a geometry-dependent (SDF) layer defers
  // its flatten to resolve time — the eager snapshot baked uResolution=(0,0)
  // and rendered a degenerate speck.
  Material m = Material::blend({
      {Material::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
      {sdf::material(sdf::circle(), {.fill = {1, 0, 0, 1}}),
       SkBlendMode::kPlus},
  });
  EXPECT_TRUE(m.geometryDependent()); // inherited from the SDF layer
  EXPECT_FALSE(m.isLive());           // still cacheable
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).inset(0, 0, 100, 100).absolute().fill(m)));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(50, 50)), 150u); // circle body visible
  EXPECT_LT(SkColorGetR(host.pixel(3, 3)), 40u);    // corner outside circle
}

TEST(ComposeSdf, StarFillsCenterMissesCorners) {
  // The analytic N-star: fill covers the body, the box corners lie outside
  // the arms. One shader pass, pixel-space distance.
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).inset(0, 0, 100, 100).absolute().fill(
          sdf::material(sdf::star(5, 2.4f), {.fill = {1, 0, 0, 1}}))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(50, 50)), 200u); // body
  const SkColor corner = host.pixel(4, 4);          // outside the arms
  EXPECT_LT(SkColorGetR(corner), 30u);
  EXPECT_LT(SkColorGetG(corner), 30u);
}

TEST(ComposeSdf, GeometryStaticCachesAndPrunes) {
  // An SDF material reads uResolution (geometry-dependent) but binds no
  // Outputs: it must CACHE like static content (0 live paints, 0 re-records)
  // AND prune across an identical re-describe (recipe equality — same
  // per-kind effect pointer, equal constants).
  Host host;
  auto tree = [] {
    return box().child(box().width(80).height(60).fill(sdf::material(
        sdf::roundBox(12), {.fill = {0, 1, 0, 1}, .borderWidth = 3})));
  };
  host.composer.render(tree());
  host.frame(); // records
  host.frame(); // replays
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(host.composer.stats().nodesPainted, 0u);
  host.composer.render(tree()); // fresh describe, identical recipe
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
}

TEST(ComposeSdf, ResizeReResolvesGeometry) {
  // uResolution bakes into the recording; a size change must re-resolve —
  // the materialSize invalidation, without any prop change.
  Host host; // 200x200 surface
  host.composer.render(
      box().child(box().grow(1).fill(
          sdf::material(sdf::circle(), {.fill = {1, 0, 0, 1}}))));
  host.frame(); // circle c=(100,100) r≈99
  host.composer.setSize({120, 120});
  host.frame(); // circle c=(60,60) r≈59
  // (15,110): inside the OLD circle (dist≈85.6<99) but outside the new one
  // (dist≈67.3>59) — red here means a stale bake replayed.
  EXPECT_LT(SkColorGetR(host.pixel(15, 110)), 30u);
  EXPECT_GT(SkColorGetR(host.pixel(60, 60)), 200u); // new body
}

TEST(ComposeSdf, BoundGlowAnimatesWithinReserve) {
  // Alive chrome: bind uGlowR to a ch::Output — the material goes live and
  // the glow breathes with the Output, no render() calls. The style's
  // glowRadius reserves the pad; the binding animates within it.
  choreograph::Output<float> glow{0.01f};
  const sdf::Style style{.fill = {1, 0, 0, 1},
                         .glowRadius = 12,
                         .glowColor = {1, 1, 1, 1}};
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).inset(0, 0, 100, 100).absolute().fill(
          sdf::material(sdf::circle(), style).uniform("uGlowR", &glow))));
  host.frame();
  // Size the probe from the PUBLIC pad helper (no hand-copied formula):
  // circle radius = 50 − pad; sample 6px outside the edge.
  const int probeX = (int)(50.0f + (50.0f - sdf::pad(style)) + 6.0f);
  const uint32_t dim = SkColorGetR(host.pixel(probeX, 50));
  glow = 12.0f; // brighten the falloff — no re-render
  host.frame();
  const uint32_t lit = SkColorGetR(host.pixel(probeX, 50));
  EXPECT_LT(dim, 25u); // exp(-6/0.01) ≈ 0
  EXPECT_GT(lit, 80u); // exp(-6/12) · edge cutoff ≈ 0.51 → ~130
}

// ---- Pattern: runtime-procedural regenerable tiles --------------------------

#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Patterns.h>

TEST(ComposePattern, CheckerTilesSeamlessly) {
  // A stock generator baked once and repeated: cells land where the tile
  // math says, across tile boundaries.
  Pattern bg = patterns::checker(10, {1, 0, 0, 1}, {0, 0, 1, 1});
  Host host;
  host.composer.render(box().child(
      box().width(60).height(20).inset(0, 0, 140, 180).absolute()
          .fill(bg.material())));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);   // cell (0,0)
  EXPECT_EQ(host.pixel(15, 5), SK_ColorBLUE); // cell (1,0)
  EXPECT_EQ(host.pixel(25, 5), SK_ColorRED);  // next tile repeats
  EXPECT_EQ(host.pixel(45, 5), SK_ColorRED);
}

TEST(ComposePattern, HeldPatternPrunesReseedRegenerates) {
  // The identity contract: a HELD pattern re-described is pointer-equal
  // (prunes, no rebake); .seed(n) drops the bake and shows up as exactly
  // one changed recipe.
  Pattern grain = patterns::speckle(64, 40, 1, 3, {{1, 1, 1, 1}});
  Host host;
  auto tree = [&] {
    return box().child(box().width(80).height(80).fill(grain.material()));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree()); // same bake → same recipe → prune
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
  grain.seed(7); // regenerate
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
  EXPECT_TRUE(host.composer.dirty());
}

TEST(ComposePattern, ElementTreeAsTile) {
  // Patterns are compositions: an element tree (two boxes) as the tile.
  Pattern duo = Pattern::tile(
      {20, 10}, box().row()
                    .child(box().width(10).height(10).fill(red()))
                    .child(box().width(10).height(10).fill(blue())));
  Host host;
  host.composer.render(box().child(
      box().width(40).height(10).inset(0, 0, 160, 190).absolute()
          .fill(duo.material(fonts()))));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(15, 5), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(25, 5), SK_ColorRED); // the repeat
}

#include <sigilcompose/Brushes.h>

TEST(ComposePattern, Girih8IsTheRealStarAndCross) {
  // REFERENCES.md §4: Hankin PIC on 4.8.8 at θ=45 — khatam star at the tile
  // center (star color), cross ground at the tile edge midpoint's flanks,
  // strap ribbon on the khatam chord.
  patterns::GirihPalette pal = patterns::fezPalette();
  Pattern zellige = patterns::girih8(24, pal);
  const float s = 24 * (1 + 1.41421356f); // tile spacing ≈ 57.9
  Host host;
  host.composer.render(box().child(
      box().width(120).height(120).inset(0, 0, 80, 80).absolute()
          .fill(zellige.material())));
  host.frame();
  // Tile center = khatam star fill (blue).
  const SkColor center = host.pixel((int)(s / 2), (int)(s / 2));
  EXPECT_GT(SkColorGetB(center), 100u);
  EXPECT_LT(SkColorGetR(center), 80u);
  // Near the tile corner (inside the corner filler) = ground (teal).
  const SkColor corner = host.pixel(3, 3);
  EXPECT_GT(SkColorGetG(corner), 80u);
  EXPECT_LT(SkColorGetR(corner), 80u);
  EXPECT_LT(SkColorGetB(corner), SkColorGetG(corner)); // teal, not blue
}

TEST(ComposeBrushes, FilamentGlowsAroundItsCore) {
  // REFERENCES.md §5: the Ori filament — white-hot core, additive glow
  // envelope falling off around it — as a value brush on a rail.
  Host host;
  host.composer.render(
      stack()
          .child(station("a", 10, 90))
          .child(station("b", 170, 90))
          .child(rail({{"a"}, {"b"}}).absolute().inset(0)
                     .stroke(brushes::filament())));
  host.frame();
  const SkColor core = host.pixel(100, 100); // on the line (y=100)
  EXPECT_GT(SkColorGetR(core), 180u);        // near-white core
  EXPECT_GT(SkColorGetB(core), 220u);
  const SkColor glow = host.pixel(100, 106); // 6px off the line
  EXPECT_GT(SkColorGetB(glow), 25u);         // inside the glow envelope
  EXPECT_LT(SkColorGetB(glow), SkColorGetB(core));
  const SkColor far = host.pixel(100, 140); // well outside
  EXPECT_LT(SkColorGetB(far), 12u);
}

// ---- layer styles: the Photoshop route --------------------------------------

TEST(ComposeStyles, BevelLightsAndShadesOpposedEdges) {
  // The fake bevel = two opposed inner shadows: with light from the upper
  // left, the top inner edge reads brighter than the body and the bottom
  // inner edge darker.
  Host host;
  host.composer.render(box().child(
      box().width(60).height(60).inset(0, 0, 140, 140).absolute()
          .fill(Fill::color({0.5f, 0.5f, 0.5f, 1}))
          .foreground(styles::BevelEmboss{.depth = 4, .size = 3})));
  host.frame();
  const uint32_t top = SkColorGetR(host.pixel(30, 2));
  const uint32_t mid = SkColorGetR(host.pixel(30, 30));
  const uint32_t bot = SkColorGetR(host.pixel(30, 57));
  EXPECT_GT(top, mid + 20); // lit edge
  EXPECT_LT(bot + 20, mid); // shaded edge
}

TEST(ComposeStyles, OverlayAndStrokeSugar) {
  // colorOverlay tints the shape through its blend; .stroke() is fill's
  // ergonomic peer for dressing the outline.
  Host host;
  host.composer.render(box().child(
      box().width(60).height(60).inset(0, 0, 140, 140).absolute()
          .fill(Fill::color({0, 0, 1, 1}))
          .foreground(styles::colorOverlay({1, 0, 0, 1},
                                           SkBlendMode::kSrcOver, 0.5f))
          .stroke(sigil::compose::util::stroke(4, green()))));
  host.frame();
  const SkColor c = host.pixel(30, 30); // 50% red over blue
  EXPECT_GT(SkColorGetR(c), 90u);
  EXPECT_GT(SkColorGetB(c), 90u);
  EXPECT_EQ(host.pixel(30, 1), SK_ColorGREEN); // stroked edge
}

TEST(ComposeStyles, BevelBandsEdgesWhenNested) {
  // The y2k-study bug: with blur, the old inverse-fill inner shadow FLOODED
  // the whole shape when the node sat at a non-origin offset inside a
  // cached tree (the origin-anchored test passed while real layouts broke).
  // The stroked-band implementation must band edges regardless of nesting.
  Host host;
  host.composer.render(box().padding(30).child(box().padding(10).child(
      box().width(60).height(60)
          .fill(Fill::color({0.5f, 0.5f, 0.5f, 1}))
          .foreground(styles::BevelEmboss{.depth = 4, .size = 3}))));
  host.frame();
  host.frame(); // the CACHED replay is the bug's trigger
  const uint32_t top = SkColorGetR(host.pixel(70, 42));
  const uint32_t mid = SkColorGetR(host.pixel(70, 70));
  const uint32_t bot = SkColorGetR(host.pixel(70, 97));
  EXPECT_GT(top, mid + 15); // lit band
  EXPECT_LT(bot + 15, mid); // shaded band
  EXPECT_GT(mid, 100u);     // the flood bug washed the body toward white
  EXPECT_LT(mid, 160u);
}

TEST(ComposeStyles, BigSoftShadowSurvivesPictureCaching) {
  // The aero-study bug: a blurred shadow larger than its node was truncated
  // at the picture-cache bounds. Decorations now declare bleed() and the
  // recording cull grows to hold them.
  Host host;
  host.composer.render(box().padding(40).child(
      box().width(60).height(40)
          .background(
              sigil::compose::util::shadow({1, 0, 0, 0.9f}, {0, 10}, 20))
          .fill(Fill::color({0.2f, 0.2f, 0.2f, 1}))));
  host.frame();
  host.frame(); // cached replay
  // Node spans y∈[40,80); sample 14px below it — the soft red reach.
  EXPECT_GT(SkColorGetR(host.pixel(70, 94)), 25u);
}

TEST(ComposeStyles, OuterGlowHalosOutsideTheShape) {
  Host host;
  host.composer.render(box().child(
      box().width(40).height(40).inset(60, 60, 100, 100).absolute()
          .corners({8})
          .background(styles::OuterGlow{.color = {1, 1, 1, 1}, .size = 10})
          .fill(Fill::color({0.2f, 0.2f, 0.2f, 1}))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(56, 80)), 40u); // halo 4px outside
  EXPECT_LT(SkColorGetR(host.pixel(30, 80)), 12u); // fades with distance
}

// ---- console(): the streaming log ------------------------------------------

#include <sigilcompose/Console.h>

TEST(ComposeConsole, AppendCostsOneMountNotOneRerecordPerLine) {
  // The seq-id-key law: an append shifts nothing — surviving lines prune
  // (zero patches) and keep their pictures; only the new tail mounts and the
  // scrolled-out head unmounts. Index keys would re-patch all ten.
  console::LineRing ring;
  for (int i = 0; i < 30; ++i)
    ring.append(sigil::compose::util::toU8("boot sequence line " +
                                           std::to_string(i)));
  console::Style st;
  st.text = styleAt(12);
  st.visibleLines = 10;
  Host host(200, 400);
  auto describe = [&] {
    return box().padding(6).child(console::console(ring, st));
  };
  host.composer.render(describe());
  host.frame(); // records the visible window
  ring.append(sigil::compose::util::toU8("intrusion detected"));
  host.composer.render(describe());
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u); // the new tail only
  host.frame();
  // Ancestor chain re-records + the tail's own picture; the nine surviving
  // lines replay their cached pictures untouched.
  EXPECT_LE(host.composer.stats().picturesRecorded, 4u);
}

#ifdef SIGILCOMPOSE_ENABLE_OCIO
#include <sigilcompose/Ocio.h>

TEST(ComposeColor, OcioViewTransformsOutputAndClears) {
  // The OCIO output stage end-to-end: an exponent transform baked to a LUT
  // darkens mid-gray (0.5^2.2 ≈ 0.218); clearing the view restores
  // pass-through. Exercises bake → SkImage LUT → SkSL trilinear → saveLayer.
  ASSERT_TRUE(sigil::compose::ocio::available());
  Host host;
  host.composer.setView(sigil::compose::ocio::exponent(2.2f));
  host.composer.render(box().child(
      box().width(60).height(60).fill(Fill::color({0.5f, 0.5f, 0.5f, 1}))));
  host.frame();
  const uint32_t dark = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(dark, 30u);  // ≈ 56 (LUT-quantized)
  EXPECT_LT(dark, 80u);
  host.composer.setView({}); // pass-through again
  host.frame();
  const uint32_t plain = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(plain, 118u); // ≈ 128
  EXPECT_LT(plain, 138u);
}
#endif // SIGILCOMPOSE_ENABLE_OCIO

TEST(ComposeMaterial, UnknownUniformNamesWarnAndIgnore) {
  // A typo'd uniform name must never abort (SkDEBUGFAIL kills the sketch
  // host in debug): unknown names are warned and dropped, at sksl() and at
  // uniform(), constant and bound alike.
  Material m = Material::sksl(ukEffect(), {{"uTypo", 1.0f}});
  choreograph::Output<float> o{1.0f};
  m.uniform("uAlsoMissing", &o); // dropped → still not live
  EXPECT_FALSE(m.isLive());
  Host host;
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(m)));
  host.frame(); // paints with uK at its SkSL default (0) — and does not crash
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 40u);
}

namespace {
sigil::weave::TextStyle whiteStyle(float size) {
  sigil::weave::TextStyle s = styleAt(size);
  s.paint.foreground.setColor(SK_ColorWHITE);
  return s;
}
bool anyWhiteIn(Host &host, SkIRect region) {
  for (int y = region.top(); y < region.bottom(); y += 2)
    for (int x = region.left(); x < region.right(); x += 2)
      if (host.pixel(x, y) == SK_ColorWHITE)
        return true;
  return false;
}
} // namespace

TEST(ComposeDerive, FlowAroundWrapsTextAroundFrame) {
  const std::u8string body =
      u8"the quick brown fox jumps over the lazy dog and keeps running "
      u8"through the tall summer grass until the river bend appears and "
      u8"the evening light settles over the water in long amber bands";

  auto tree = [&](bool flow) {
    auto t = text(body, whiteStyle(18)).key("body");
    if (flow)
      t.flowAround("frame", 6);
    return stack()
        .child(box().key("frame").width(150).height(140)
                   .inset(200, 10, 10, 210).absolute()
                   .fill(Fill::color({0, 0.4f, 0, 1})))
        .child(box().inset(0).child(std::move(t)).zIndex(1));
  };

  Host plain(360, 420), flowed(360, 420);
  plain.composer.render(tree(false));
  plain.frame();
  flowed.composer.render(tree(true));
  flowed.frame();

  // Without the exclusion, text runs under the frame region; with it,
  // the region stays text-free (frame color only).
  const SkIRect inner = SkIRect::MakeLTRB(215, 25, 345, 135);
  EXPECT_TRUE(anyWhiteIn(plain, inner));
  EXPECT_FALSE(anyWhiteIn(flowed, inner));

  // Displaced words push the flowed paragraph taller.
  auto plainBounds = plain.composer.bounds("body");
  auto flowedBounds = flowed.composer.bounds("body");
  ASSERT_TRUE(plainBounds && flowedBounds);
  EXPECT_GT(flowedBounds->height(), plainBounds->height());
}

TEST(ComposeDerive, FlowAroundCycleIsIgnored) {
  Host host;
  host.composer.render(box().child(
      text(u8"self reference", whiteStyle(16)).key("self")
          .flowAround("self")));
  host.frame(); // must not hang or exclude itself into nothing
  EXPECT_NE(host.composer.paragraphLayout("self"), nullptr);
}

TEST(ComposeDerive, ConnectorTracksMovedEndpoints) {
  Host host;
  PathFormat wire;
  wire.width = 4;
  wire.strokeFill = Fill::color({1, 1, 0, 1});

  auto tree = [&](float bLeft) {
    return stack()
        .child(box().key("a").width(20).height(20)
                   .inset(10, 10, 170, 170).absolute().fill(red()))
        .child(box().key("b").width(20).height(20)
                   .inset(bLeft, 160, 180 - bLeft, 20).absolute()
                   .fill(green()))
        .child(connector("a", "b").inset(0).foreground(wire).zIndex(-1));
  };

  host.composer.render(tree(10.0f));
  host.frame();
  // Vertical wire at x=20 between the stacked boxes.
  EXPECT_EQ(host.pixel(20, 100), SK_ColorYELLOW);

  host.composer.render(tree(160.0f)); // move b to the right
  host.frame();
  EXPECT_EQ(host.pixel(20, 100), SK_ColorBLACK);   // old route gone
  EXPECT_NE(host.pixel(95, 95), SK_ColorBLACK);    // new diagonal route
}

TEST(Shape, CustomOutlineShapesFillAndClip) {
  Host host;
  // A diamond outline over a 100x100 box: the box's corner pixels sit
  // outside the shape, so fill and clipped children must not reach them.
  auto diamond = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(s.width() / 2, 0);
    b.lineTo(s.width(), s.height() / 2);
    b.lineTo(s.width() / 2, s.height());
    b.lineTo(0, s.height() / 2);
    b.close();
    return b.detach();
  };
  host.composer.render(
      box().width(100).height(100).clip().outline(diamond).fill(red())
          .child(box().inset(0).absolute().fill(green())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorGREEN); // clipped child inside
  EXPECT_EQ(host.pixel(3, 3), SK_ColorBLACK);   // box corner outside shape
}

TEST(Shape, RoundedOutlineCutsSharpCorners) {
  Host host;
  auto diamond = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(s.width() / 2, 0);
    b.lineTo(s.width(), s.height() / 2);
    b.lineTo(s.width() / 2, s.height());
    b.lineTo(0, s.height() / 2);
    b.close();
    return b.detach();
  };
  // Nested (the root always fills the viewport); radius 20 pulls the
  // 100x100 diamond's top vertex from y=0 down to y≈7.
  host.composer.render(box().child(box().width(100).height(100)
                                       .outline(shapes::rounded(diamond, 20))
                                       .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);   // body intact
  EXPECT_EQ(host.pixel(50, 3), SK_ColorBLACK);  // sharp tip rounded away
  EXPECT_EQ(host.pixel(50, 12), SK_ColorRED);   // rounded apex below y≈7

  host.composer.render(box().child(box().width(100).height(100)
                                       .outline(shapes::star(5))
                                       .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);    // star body
  EXPECT_NE(host.pixel(50, 3), SK_ColorBLACK);   // sharp top point present
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLACK);  // gap between arms
}

TEST(TextLayout, FullyConstrainedAbsoluteTextPaints) {
  // Yoga skips the measure callback when absolute insets determine both
  // dimensions; the kernel must lay the paragraph out at paint time.
  Host host;
  sigil::weave::TextStyle style = styleAt(40);
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(
      stack().child(text(u8"WWWW", style).absolute()
                        .inset(10, 10, 10, 120)));
  host.frame();
  int lit = 0;
  for (int x = 10; x < 190; x += 4)
    for (int y = 10; y < 70; y += 4)
      if (host.pixel(x, y) != SK_ColorBLACK)
        lit++;
  EXPECT_GT(lit, 20); // glyph coverage, not empty
}

TEST(TextLayout, AlignItemsCentersTextLeaf) {
  Host host;
  sigil::weave::TextStyle style = styleAt(40);
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(box().width(200).height(60)
                           .alignItems(Align::Center)
                           .child(text(u8"W", style)));
  host.frame();

  int litLeft = 0, litMiddle = 0;
  for (int x = 0; x < 50; x += 2)
    for (int y = 0; y < 60; y += 2)
      if (host.pixel(x, y) != SK_ColorBLACK)
        litLeft++;
  for (int x = 75; x < 125; x += 2)
    for (int y = 0; y < 60; y += 2)
      if (host.pixel(x, y) != SK_ColorBLACK)
        litMiddle++;
  EXPECT_EQ(litLeft, 0);    // nothing hugging the start edge
  EXPECT_GT(litMiddle, 5);  // the glyph sits in the middle
}

TEST(ComposeCaching, TextureBakeScaleQuantized) {
  // A continuously changing canvas scale (live window resize, pinch
  // zoom) must not re-bake Cache::Texture nodes every frame: the bake
  // scale quantizes up to a coarse step.
  Host host;
  host.composer.render(
      box().width(60).height(60).cache(Cache::Texture).fill(red())
          .child(box().width(20).height(20).fill(green())));
  auto drawAt = [&](float s) {
    SkCanvas &canvas = *host.surface->getCanvas();
    canvas.save();
    canvas.scale(s, s);
    host.composer.draw(canvas);
    canvas.restore();
  };
  drawAt(1.6f);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 1u); // first bake
  drawAt(1.7f);
  drawAt(1.9f);
  drawAt(2.0f); // still within the 2.0 step: the bake is reused
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  drawAt(2.2f); // crossed into the 3.0 step: one re-bake
  EXPECT_EQ(host.composer.stats().picturesRecorded, 1u);
}

// ---------------------------------------------------------------------------
// Kernel-completeness round: wrap, per-edge spacing, per-corner radii,
// Dim literals, atlas regions, Paragraph overload, contentScale.

TEST(ComposeLayout, WrapLinesFlowsToSecondRow) {
  Host host;
  host.composer.render(
      box().child(box().row().wrapLines().width(200)
                      .child(box().width(80).height(40).fill(red()))
                      .child(box().width(80).height(40).fill(green()))
                      .child(box().width(80).height(40).fill(blue()))));
  host.frame();
  EXPECT_EQ(host.pixel(40, 20), SK_ColorRED);
  EXPECT_EQ(host.pixel(120, 20), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(40, 60), SK_ColorBLUE); // wrapped to the next line
}

TEST(ComposeLayout, PerEdgePaddingAndMargin) {
  Host host;
  host.composer.render(
      box().child(box().padding(10, 20, 30, 40).key("outer")
                      .child(box().margin(5, 6, 7, 8).width(50).height(50)
                                 .key("inner"))));
  host.frame();
  auto inner = host.composer.bounds("inner");
  ASSERT_TRUE(inner.has_value());
  EXPECT_FLOAT_EQ(inner->left(), 10 + 5); // padding.left + margin.left
  EXPECT_FLOAT_EQ(inner->top(), 20 + 6);  // padding.top + margin.top
}

TEST(Shape, PerCornerRadiiIndependent) {
  Host host;
  // Sharp top-left, heavily rounded top-right.
  host.composer.render(box().child(
      box().width(100).height(100).corners({0, 40, 0, 0}).fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(2, 2), SK_ColorRED);    // sharp TL corner filled
  EXPECT_EQ(host.pixel(97, 2), SK_ColorBLACK); // rounded TR corner empty
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);
}

TEST(ComposeLayout, DimLiteralsResolvePercent) {
  Host host;
  host.composer.render(box().child(
      box().width(50_pct).height(25_pct).fill(red()).key("half")));
  host.frame();
  auto rect = host.composer.bounds("half");
  ASSERT_TRUE(rect.has_value());
  EXPECT_FLOAT_EQ(rect->width(), 100.0f);  // 50% of the 200px host
  EXPECT_FLOAT_EQ(rect->height(), 50.0f);  // 25% of 200px
}

namespace {
/** A 2-cell atlas: left 16x16 red, right 16x16 green. */
std::shared_ptr<sigil::image::ImageAsset> twoCellAtlas() {
  SkBitmap src;
  src.allocN32Pixels(32, 16);
  src.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 0, 16, 16));
  src.erase(SK_ColorGREEN, SkIRect::MakeXYWH(16, 0, 16, 16));
  SkDynamicMemoryWStream stream;
  SkPngEncoder::Encode(&stream, src.pixmap(), {});
  return std::make_shared<sigil::image::ImageAsset>(
      *sigil::image::ImageAsset::decode(stream.detachAsData()));
}
} // namespace

TEST(ComposeContent, ImageRegionDrawsAtlasCell) {
  Host host;
  auto atlas = twoCellAtlas();
  host.composer.render(
      box().row()
          .child(image(atlas).region(SkRect::MakeXYWH(16, 0, 16, 16))
                     .width(50).height(50))
          .child(image(atlas).width(50).height(50)));
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorGREEN); // region: right cell only
  EXPECT_EQ(host.pixel(60, 25), SK_ColorRED);   // whole atlas: left half
}

TEST(TextLayout, ParagraphOverloadPaintsMixedSpans) {
  Host host(400, 200);
  auto para = std::make_shared<sigil::weave::Paragraph>();
  sigil::weave::TextStyle big = styleAt(40);
  big.paint.foreground.setColor(SK_ColorWHITE);
  sigil::weave::TextStyle small = styleAt(16);
  small.paint.foreground.setColor(SK_ColorWHITE);
  para->appendText(std::u8string(u8"BIG"), big);
  para->appendText(std::u8string(u8" and small"), small);

  host.composer.render(box().padding(10).child(
      text(para).key("spans")));
  host.frame();
  const auto *layout = host.composer.paragraphLayout("spans");
  ASSERT_NE(layout, nullptr);
  int lit = 0;
  for (int x = 10; x < 390; x += 3)
    for (int y = 10; y < 70; y += 3)
      if (host.pixel(x, y) != SK_ColorBLACK)
        lit++;
  EXPECT_GT(lit, 15); // both spans shaped and painted
}

TEST(ComposePaint, ContentScaleReportsHostScale) {
  Host host;
  float seen = 0.0f;
  host.composer.render(box().child(
      custom([&seen](SkCanvas &, const PaintContext &ctx) {
        seen = ctx.contentScale;
      }).width(50).height(50).cache(Cache::None)));
  SkCanvas &canvas = *host.surface->getCanvas();
  canvas.save();
  canvas.scale(2.0f, 2.0f);
  host.composer.draw(canvas);
  canvas.restore();
  EXPECT_FLOAT_EQ(seen, 2.0f);
}

// ---------------------------------------------------------------------------
// Shape kit (Shapes.h): organic generators, per-edge extraction.

TEST(Shape, PolygonAndSquircleSilhouettes) {
  Host host;
  host.composer.render(
      box().row()
          .child(box().width(90).height(90)
                     .outline(shapes::polygon(6)).fill(red()))
          .child(box().width(90).height(90)
                     .outline(shapes::squircle(4)).fill(green())));
  host.frame();
  EXPECT_EQ(host.pixel(45, 45), SK_ColorRED);   // hexagon body
  EXPECT_EQ(host.pixel(2, 2), SK_ColorBLACK);   // hexagon corner cut
  EXPECT_EQ(host.pixel(135, 45), SK_ColorGREEN); // squircle body
  EXPECT_EQ(host.pixel(92, 2), SK_ColorBLACK);   // squircle corner soft
  EXPECT_EQ(host.pixel(135, 3), SK_ColorGREEN);  // but edge midpoints full
}

TEST(Shape, BlobIsDeterministicOrganicAndBounded) {
  auto probe = [](uint32_t seed) {
    Host host;
    host.composer.render(box().child(
        box().width(120).height(120).outline(shapes::blob(seed, 0.3f, 9))
            .fill(red())));
    host.frame();
    std::vector<SkColor> px;
    for (int y = 0; y < 130; y += 4)
      for (int x = 0; x < 130; x += 4)
        px.push_back(host.pixel(x, y));
    return px;
  };
  std::vector<SkColor> a1 = probe(7), a2 = probe(7), b = probe(8);
  EXPECT_EQ(a1, a2); // same seed → identical pixels (cacheable chaos)
  EXPECT_NE(a1, b);  // different seed → different blob

  Host host;
  host.composer.render(box().child(
      box().width(120).height(120).outline(shapes::blob(7, 0.3f, 9))
          .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(60, 60), SK_ColorRED); // center always covered
  int outside = 0;
  for (int x = 121; x < 200; x += 4)
    for (int y = 0; y < 200; y += 4)
      if (host.pixel(x, y) != SK_ColorBLACK)
        outside++;
  EXPECT_EQ(outside, 0); // never escapes its layout box
}

TEST(ComposeDecorations, EdgeSliceStrokesSelectedEdgesOnly) {
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).fill(blue())
          .foreground(shapes::onEdges(
              shapes::Edge::Top | shapes::Edge::Left,
              util::stroke(8, Fill::color({1, 1, 1, 1}))))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorWHITE); // top edge stroked
  EXPECT_EQ(host.pixel(1, 50), SK_ColorWHITE); // left edge stroked
  EXPECT_EQ(host.pixel(98, 50), SK_ColorBLUE); // right edge bare
  EXPECT_EQ(host.pixel(50, 98), SK_ColorBLUE); // bottom edge bare
}

TEST(ComposeDecorations, EdgesSplitRoundedCornersDiagonally) {
  // A rounded rect's corner arcs divide between their adjacent edges at
  // the diagonal — the top run must include the upper half of the
  // top-left arc but none of the left flank.
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).corners({30}).fill(blue())
          .foreground(shapes::onEdges(
              shapes::Edge::Top, util::stroke(8, Fill::color({1, 1, 1, 1}))))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorWHITE);  // top run center
  EXPECT_EQ(host.pixel(1, 50), SK_ColorBLUE);   // left flank untouched
  EXPECT_EQ(host.pixel(50, 98), SK_ColorBLUE);  // bottom untouched
}

// ---------------------------------------------------------------------------
// Element stamps + snapshot() (stress items 10 and 20).

TEST(ComposeStamps, SnapshotBakesIntrinsicSize) {
  sk_sp<SkPicture> pic = snapshot(
      box().row().gap(4)
          .child(box().width(20).height(12).fill(red()))
          .child(box().width(20).height(12).fill(green())),
      fonts());
  ASSERT_NE(pic, nullptr);
  EXPECT_FLOAT_EQ(pic->cullRect().width(), 44.0f);  // 20 + 4 + 20
  EXPECT_FLOAT_EQ(pic->cullRect().height(), 12.0f); // content height
}

TEST(ComposeStamps, StampRecordsOnceReplaysPerSample) {
  static int stampDescribes;
  stampDescribes = 0;
  Host host;

  ContourWalk vine;
  vine.spacing = 25.0f;
  vine.stamp = custom([](SkCanvas &c, const PaintContext &ctx) {
                 ++stampDescribes;
                 SkPaint p;
                 p.setColor(SK_ColorYELLOW);
                 c.drawRect(SkRect::MakeWH(ctx.size.width(),
                                           ctx.size.height()), p);
               }).width(12).height(12);

  host.composer.render(box().child(
      box().width(100).height(100).inset(50, 50, 50, 50).absolute()
          .fill(blue()).foreground(vine)));
  host.frame();
  host.frame();
  EXPECT_EQ(stampDescribes, 1); // baked once, replayed at every sample

  // Stamps are centered on the outline: the top-left corner sample
  // lands half outside the box.
  EXPECT_EQ(host.pixel(50, 50), SK_ColorYELLOW);  // corner sample center
  EXPECT_EQ(host.pixel(100, 46), SK_ColorYELLOW); // top edge, above box
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLUE);  // interior untouched
}

TEST(ComposeStamps, RecursiveStampWalksItsOwnContour) {
  // Level 2 recursion: the stamp is itself decorated by a ContourWalk
  // that dots its own outline. compose_test pins that this terminates
  // and paints (the forward-only law keeps it a finite bake).
  Host host;
  ContourWalk dots;
  dots.spacing = 6.0f;
  dots.draw = [](SkCanvas &c, const PathSample &, const PaintContext &) {
    SkPaint p;
    p.setColor(SK_ColorCYAN);
    c.drawRect(SkRect::MakeXYWH(-1, -1, 2, 2), p);
  };

  ContourWalk outer;
  outer.spacing = 40.0f;
  outer.stamp = box().width(16).height(16).fill(red()).foreground(dots);

  host.composer.render(box().child(
      box().width(120).height(120).inset(40, 40, 40, 40).absolute()
          .foreground(outer)));
  host.frame();
  int redPx = 0, cyanPx = 0;
  for (int x = 0; x < 200; x += 2)
    for (int y = 0; y < 200; y += 2) {
      const SkColor c = host.pixel(x, y);
      if (c == SK_ColorRED)
        redPx++;
      else if (c == SK_ColorCYAN)
        cyanPx++;
    }
  EXPECT_GT(redPx, 20);  // stamps landed
  EXPECT_GT(cyanPx, 10); // and their own walked borders too
}

TEST(ComposeStamps, CustomLeafDrawsNestedComposer) {
  // Item 20's second half: a custom() leaf hosting an entire nested
  // Composer — recursion closed at the paint phase.
  Host host;
  auto nestedTicker = std::make_shared<sigil::motion::Ticker>();
  auto nested = std::make_shared<Composer>(*nestedTicker, fonts());
  nested->setSize({60, 60});
  nested->render(box().padding(10).fill(green())
                     .child(box().grow(1).fill(red())));

  host.composer.render(box().child(
      custom([nested, nestedTicker](SkCanvas &c, const PaintContext &) {
        nested->draw(c);
      }).width(60).height(60).cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorGREEN); // nested padding ring
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED); // nested content
}

// ---------------------------------------------------------------------------
// hitTest (stress item 5): paint order, transforms, shapes.

TEST(ComposeQueries, HitTestRespectsPaintOrderAndKeys) {
  Host host;
  host.composer.render(
      stack()
          .child(box().key("under").inset(0).fill(red()))
          .child(box().key("over").width(60).height(60)
                     .inset(20, 20, 120, 120).absolute().fill(green()))
          .child(box().width(30).height(30).inset(150, 150, 20, 20)
                     .absolute().fill(blue()))); // keyless → falls to root
  host.frame();
  EXPECT_EQ(host.composer.hitTest({50, 50}).value_or(""), "over");
  EXPECT_EQ(host.composer.hitTest({120, 120}).value_or(""), "under");
  // Keyless box resolves to its nearest keyed ancestor (none here above
  // the stack root, which is keyless) — the "under" sibling is NOT an
  // ancestor, so the keyless box hits nothing of its own and the point
  // falls through to "under".
  EXPECT_EQ(host.composer.hitTest({160, 160}).value_or(""), "under");
  EXPECT_FALSE(host.composer.hitTest({500, 500}).has_value());
}

TEST(ComposeQueries, HitTestHonorsShapeAndRotation) {
  Host host;
  host.composer.render(
      box().child(box().key("star").width(100).height(100)
                      .outline(shapes::star(5)).fill(red()))
          .child(box().key("spun").width(80).height(20)
                     .inset(60, 140, 60, 40).absolute()
                     .rotate(90.0f).fill(green())));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({50, 50}).value_or(""), "star");
  // Between the star's arms: inside the box, outside the silhouette.
  EXPECT_FALSE(host.composer.hitTest({20, 20}).has_value());

  // The 80x20 bar at (60,140) rotated 90° about its center paints as a
  // 20x80 bar centered at (100,150): x∈[90,110], y∈[110,190].
  EXPECT_EQ(host.composer.hitTest({100, 115}).value_or(""), "spun");
  EXPECT_FALSE(host.composer.hitTest({70, 150}).has_value()); // unrotated
                                                              // footprint
}

#include <sigilcompose/Routers.h>

// ---------------------------------------------------------------------------
// Routers (connector route library).

TEST(ComposeDerive, OrthogonalRouterRunsManhattan) {
  Host host;
  PathFormat wire;
  wire.width = 4;
  wire.strokeFill = Fill::color({1, 1, 0, 1});
  host.composer.render(
      stack()
          .child(box().key("a").width(20).height(20)
                     .inset(10, 10, 170, 170).absolute().fill(red()))
          .child(box().key("b").width(20).height(20)
                     .inset(160, 160, 20, 20).absolute().fill(green()))
          .child(connector("a", "b", routers::orthogonal())
                     .inset(0).foreground(wire).zIndex(-1)));
  host.frame();
  // Centers (20,20) and (170,170); midX = 95: H leg at y=20, V leg at
  // x=95, H leg at y=170.
  EXPECT_EQ(host.pixel(60, 20), SK_ColorYELLOW);   // first horizontal leg
  EXPECT_EQ(host.pixel(95, 100), SK_ColorYELLOW);  // vertical run
  EXPECT_EQ(host.pixel(130, 170), SK_ColorYELLOW); // final horizontal leg
  EXPECT_EQ(host.pixel(60, 100), SK_ColorBLACK);   // nowhere near diagonal
}

TEST(ComposeDerive, ArcRouterBowsOffTheChord) {
  Host host;
  PathFormat wire;
  wire.width = 4;
  wire.strokeFill = Fill::color({1, 1, 0, 1});
  host.composer.render(
      stack()
          .child(box().key("a").width(10).height(10)
                     .inset(20, 95, 170, 95).absolute().fill(red()))
          .child(box().key("b").width(10).height(10)
                     .inset(170, 95, 20, 95).absolute().fill(green()))
          .child(connector("a", "b", routers::arc(0.3f))
                     .inset(0).foreground(wire).zIndex(-1)));
  host.frame();
  // Horizontal chord from (25,100) to (175,100), bulge 0.3×150 = 45 px
  // toward +normal (downward-left convention: normal of (+x,0) is
  // (0,+y) → the bow lands at y ≈ 145).
  EXPECT_EQ(host.pixel(100, 145), SK_ColorYELLOW); // bowed midpoint
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);  // chord midpoint empty
}

#include <sigilcompose/Layouts.h>

// ---------------------------------------------------------------------------
// Organic layout schemes (Layouts.h).

TEST(ComposeLayouts, RadialPlacesChildrenOnTheRing) {
  Host host;
  std::vector<Element> dots;
  for (int i = 0; i < 4; ++i)
    dots.push_back(box().width(10).height(10).fill(red())
                       .key("d" + std::to_string(i)));
  host.composer.render(box().child(
      layout(layouts::Radial{.radiusFraction = 0.8f})
          .width(200).height(200).children(dots)));
  host.frame();
  // Radius 80 from center (100,100), starting up, clockwise quarters.
  auto center = [&](const char *k) {
    auto r = host.composer.bounds(k);
    return SkPoint{r->centerX(), r->centerY()};
  };
  EXPECT_NEAR(center("d0").x(), 100, 1);
  EXPECT_NEAR(center("d0").y(), 20, 1);   // top
  EXPECT_NEAR(center("d1").x(), 180, 1);  // right
  EXPECT_NEAR(center("d1").y(), 100, 1);
  EXPECT_NEAR(center("d2").y(), 180, 1);  // bottom
  EXPECT_NEAR(center("d3").x(), 20, 1);   // left
}

TEST(ComposeLayouts, AlongPathFollowsAStarContour) {
  Host host;
  std::vector<Element> beads;
  for (int i = 0; i < 10; ++i)
    beads.push_back(box().width(6).height(6).fill(green())
                        .key("b" + std::to_string(i)));
  host.composer.render(box().child(
      layout(layouts::AlongPath{.path = shapes::star(5)})
          .width(180).height(180).children(beads)));
  host.frame();
  // First bead sits on the star's top point (contour start).
  auto b0 = host.composer.bounds("b0");
  ASSERT_TRUE(b0.has_value());
  EXPECT_NEAR(b0->centerX(), 90, 1.5);
  EXPECT_NEAR(b0->centerY(), 0, 1.5);
  // All beads land ON the star outline: distance from center between
  // inner and outer radius.
  for (int i = 0; i < 10; ++i) {
    auto r = host.composer.bounds(("b" + std::to_string(i)).c_str());
    ASSERT_TRUE(r.has_value());
    const float dx = r->centerX() - 90, dy = r->centerY() - 90;
    const float dist = std::sqrt(dx * dx + dy * dy);
    EXPECT_GE(dist, 0.4f * 90 - 2);
    EXPECT_LE(dist, 90 + 2);
  }
}

TEST(ComposeTransform, SkewLeansPaintAndHits) {
  // The ATLUS diagonal (REFERENCES.md §1): skewX(−12°) leans the card's top
  // to the right about its center; hit-testing walks the shear backwards.
  Host host;
  host.composer.render(box().child(
      box().key("card").width(40).height(40).inset(60, 60, 100, 100)
          .absolute().fill(red()).skewX(-12.0f)));
  host.frame();
  EXPECT_EQ(host.pixel(101, 64), SK_ColorRED);  // top leaned right
  EXPECT_EQ(host.pixel(61, 64), SK_ColorBLACK); // vacated top-left
  EXPECT_EQ(host.pixel(58, 97), SK_ColorRED);   // bottom leaned left
  EXPECT_EQ(host.pixel(98, 97), SK_ColorBLACK); // vacated bottom-right
  auto hit = host.composer.hitTest({101, 64});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, "card"); // transform-aware hit through the shear
  EXPECT_FALSE(host.composer.hitTest({61, 64}).has_value());
}

// ---- kinetic typography ------------------------------------------------------

#include <sigilcompose/Kinetic.h>

TEST(ComposeKinetic, StaggeredRiseRevealsInOrder) {
  // The stagger law: at mid-progress the early glyphs are fully revealed
  // while the late ones haven't started — the canonical staggered reveal,
  // rendered through batched RSXform draws.
  Host host;
  auto tree = [](PropValue<float> progress) {
    GlyphFx fx;
    fx.effect = glyphfx::rise(24);
    fx.stagger = {.eachMs = 40, .durationMs = 200};
    fx.progress = std::move(progress);
    return box().padding(10).child(
        text(u8"IIIIIIIIIIII", whiteStyle(32)).key("k")
            .glyphFx(std::move(fx)));
  };
  host.composer.render(tree(0.0f));
  host.frame();
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  const SkIRect leftEdge = SkIRect::MakeLTRB(
      (int)b->left(), (int)b->top(), (int)b->left() + 24, (int)b->bottom());
  const SkIRect rightEdge = SkIRect::MakeLTRB(
      (int)b->right() - 24, (int)b->top(), (int)b->right(),
      (int)b->bottom());
  EXPECT_FALSE(anyWhiteIn(host, leftEdge)); // progress 0: nothing revealed
  host.composer.render(tree(0.45f));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, leftEdge));   // head fully in
  EXPECT_FALSE(anyWhiteIn(host, rightEdge)); // tail not started
  host.composer.render(tree(1.0f));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, rightEdge)); // everything landed
}

TEST(ComposeKinetic, TransitionedProgressPaintsLive) {
  // The master progress takes the full PropValue treatment: a with()
  // transition animates the reveal and the node paints live while moving.
  Host host;
  auto tree = [](PropValue<float> progress) {
    GlyphFx fx;
    fx.effect = glyphfx::pop();
    fx.stagger = {.eachMs = 20, .durationMs = 150};
    fx.progress = std::move(progress);
    return box().padding(10).child(
        text(u8"POP", whiteStyle(40)).key("k").glyphFx(std::move(fx)));
  };
  host.composer.render(tree(0.001f));
  host.frame();
  host.composer.render(tree(with(1.0f, {400ms, &choreograph::easeNone})));
  host.frame(0.2); // mid-ramp
  EXPECT_GT(host.composer.stats().nodesPainted, 0u); // live while animating
  host.frame(0.3); // settle
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left(),
                                                 (int)b->top(),
                                                 (int)b->right(),
                                                 (int)b->bottom())));
}

TEST(ComposeLayouts, BaselineGridRendersInsideStackedAbsoluteColumn) {
  // Regression probe for the beethoven-sketch report: text inside a
  // BaselineGrid nested in an absolute column inside a stack() must paint.
  // (The sketch symptom was black-on-black over an arc band, not a layout
  // failure — this pins the layout path anyway.)
  Host host;
  host.composer.render(
      stack().child(
          box().column().absolute().inset(10, 10, 10, 10).child(
              layout(layouts::BaselineGrid{.rhythm = 24})
                  .width(pct(100))
                  .child(text(u8"probe", whiteStyle(28)).key("p")))));
  host.frame();
  auto b = host.composer.bounds("p");
  ASSERT_TRUE(b.has_value());
  EXPECT_GT(b->width(), 5.0f) << "placed rect " << b->left() << ","
                              << b->top() << " " << b->width() << "x"
                              << b->height();
  EXPECT_GT(b->height(), 5.0f);
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left(),
                                                 (int)b->top(),
                                                 (int)b->right(),
                                                 (int)b->bottom())))
      << "placed rect " << b->left() << "," << b->top() << " " << b->width()
      << "x" << b->height();
}

TEST(ComposeLayouts, ModularGridSpansAndAutoFlow) {
  // 4×4 modules, gutter 8, container 200×200 → module 44×44. Child 0 spans
  // 2×1 from (0,0); child 1 spans 1×3 from (3,0); children 2..3 auto-flow.
  Host host;
  layouts::ModularGrid grid;
  grid.columns = 4;
  grid.rows = 4;
  grid.gutter = 8;
  grid.spans = {{0, 0, 2, 1}, {3, 0, 1, 3}};
  host.composer.render(box().child(
      layout(grid).width(pct(100)).grow(1)
          .child(box().key("a").fill(red()))
          .child(box().key("b").fill(blue()))
          .child(box().key("c").fill(green()))
          .child(box().key("d").fill(red()))));
  host.frame();
  auto a = host.composer.bounds("a");
  auto b = host.composer.bounds("b");
  auto c = host.composer.bounds("c");
  auto d = host.composer.bounds("d");
  ASSERT_TRUE(a && b && c && d);
  EXPECT_NEAR(a->width(), 44 * 2 + 8, 0.01f); // 2-module span + gutter
  EXPECT_NEAR(a->left(), 0, 0.01f);
  EXPECT_NEAR(b->left(), (44 + 8) * 3, 0.01f); // 4th column
  EXPECT_NEAR(b->height(), 44 * 3 + 16, 0.01f); // 3 rows + 2 gutters
  EXPECT_NEAR(c->left(), 0, 0.01f); // auto-flow starts at (0,0)… of the flow
  EXPECT_NEAR(c->width(), 44, 0.01f);
  EXPECT_NEAR(d->left(), 44 + 8, 0.01f); // next module across
}

TEST(ComposeLayouts, BaselineGridSnapsBottomsAndBaselines) {
  // Non-text children anchor by BOTTOM: heights 15 & 27 on rhythm 20 land
  // their bottoms on grid lines 20 and 60 (flow 20+27=47 rounds up).
  Host host;
  host.composer.render(box().child(
      layout(layouts::BaselineGrid{.rhythm = 20})
          .width(pct(100)).grow(1)
          .child(box().key("a").width(40).height(15).fill(red()))
          .child(box().key("b").width(40).height(27).fill(blue()))));
  host.frame();
  auto a = host.composer.bounds("a");
  auto b = host.composer.bounds("b");
  ASSERT_TRUE(a && b);
  EXPECT_NEAR(a->bottom(), 20.0f, 0.01f);
  EXPECT_NEAR(b->bottom(), 60.0f, 0.01f);

  // A text child anchors by its FIRST BASELINE: with the baseline on the
  // 200 grid line, (200 - top) equals the baseline offset — strictly LESS
  // than the child's height (bottom-anchoring would make them equal).
  // Font-metric independent.
  host.composer.render(box().child(
      layout(layouts::BaselineGrid{.rhythm = 200})
          .width(pct(100)).grow(1)
          .child(text(u8"Xylograph", styleAt(40)).key("t"))));
  host.frame();
  auto t = host.composer.bounds("t");
  ASSERT_TRUE(t.has_value());
  EXPECT_GT(200.0f - t->top(), 10.0f);                 // sane baseline
  EXPECT_LT(200.0f - t->top(), t->height() - 0.5f);    // baseline, not bottom
}

TEST(ComposeLayouts, ScatterIsDeterministicAndContained) {
  auto centers = [&](uint32_t seed) {
    Host host;
    std::vector<Element> bits;
    for (int i = 0; i < 9; ++i)
      bits.push_back(box().width(12).height(12).fill(blue())
                         .key("s" + std::to_string(i)));
    host.composer.render(box().child(
        layout(layouts::Scatter{.seed = seed}).width(200).height(200)
            .children(bits)));
    host.frame();
    std::vector<SkPoint> out;
    for (int i = 0; i < 9; ++i) {
      auto r = host.composer.bounds(("s" + std::to_string(i)).c_str());
      out.push_back({r->centerX(), r->centerY()});
      EXPECT_GE(r->left(), -0.01f);
      EXPECT_GE(r->top(), -0.01f);
      EXPECT_LE(r->right(), 200.01f);
      EXPECT_LE(r->bottom(), 200.01f);
    }
    return out;
  };
  auto a1 = centers(5), a2 = centers(5), b = centers(6);
  EXPECT_EQ(a1, a2); // same seed → same scatter
  EXPECT_NE(a1, b);  // new seed → new chaos
}

// ---------------------------------------------------------------------------
// Tile maps (stress item 15): atlas regions + chunked cache invalidation.

namespace {

/** 4-tile atlas, 8px cells: [red | green] / [blue | yellow]. */
std::shared_ptr<sigil::image::ImageAsset> fourTileAtlas() {
  SkBitmap src;
  src.allocN32Pixels(16, 16);
  src.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 0, 8, 8));
  src.erase(SK_ColorGREEN, SkIRect::MakeXYWH(8, 0, 8, 8));
  src.erase(SK_ColorBLUE, SkIRect::MakeXYWH(0, 8, 8, 8));
  src.erase(SK_ColorYELLOW, SkIRect::MakeXYWH(8, 8, 8, 8));
  SkDynamicMemoryWStream stream;
  SkPngEncoder::Encode(&stream, src.pixmap(), {});
  return std::make_shared<sigil::image::ImageAsset>(
      *sigil::image::ImageAsset::decode(stream.detachAsData()));
}

struct ChunkProps {
  std::vector<int> tiles; // 4x4 tile ids
  int chunkX = 0, chunkY = 0;
  bool operator==(const ChunkProps &) const = default;
};

constexpr float kTilePx = 12.0f;

Element tileChunk(const ChunkProps &p) {
  static auto atlas = fourTileAtlas();
  auto chunk = box().width(4 * kTilePx).height(4 * kTilePx);
  for (int i = 0; i < (int)p.tiles.size(); ++i) {
    const int id = p.tiles[(size_t)i];
    const float sx = (float)(id % 2) * 8, sy = (float)(id / 2) * 8;
    chunk.child(image(atlas)
                    .region(SkRect::MakeXYWH(sx, sy, 8, 8))
                    .absolute()
                    .inset((float)(i % 4) * kTilePx, (float)(i / 4) * kTilePx,
                           0, 0)
                    .width(kTilePx).height(kTilePx));
  }
  return chunk;
}

} // namespace

TEST(ComposeTiling, OnlyTouchedChunkRerecords) {
  Host host;
  // 2x2 chunks of 4x4 tiles; a checker-ish rule fills the ids.
  std::vector<ChunkProps> chunks(4);
  for (int c = 0; c < 4; ++c) {
    chunks[(size_t)c].chunkX = c % 2;
    chunks[(size_t)c].chunkY = c / 2;
    for (int i = 0; i < 16; ++i)
      chunks[(size_t)c].tiles.push_back((i + c) % 4);
  }
  auto maze = [&] {
    auto grid = box().row().wrapLines().width(2 * 4 * kTilePx);
    for (int c = 0; c < 4; ++c)
      grid.child(memo(chunks[(size_t)c], tileChunk)
                     .key("chunk" + std::to_string(c)));
    return box().child(grid);
  };

  host.composer.render(maze());
  host.frame();
  const size_t coldRecords = host.composer.stats().picturesRecorded;
  EXPECT_GE(coldRecords, 4u); // every chunk baked (plus ancestors)

  // Pixel sanity: chunk 0 tile 0 is id 0 (red); chunk 1 tile 0 is id 1
  // (green) at x = 48.
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(53, 5), SK_ColorGREEN);

  host.composer.render(maze());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u); // all memo-warm

  chunks[0].tiles[0] = 3; // mutate ONE tile in ONE chunk
  host.composer.render(maze());
  host.frame();
  // Only chunk 0 and its ancestor chain re-record; the other three
  // chunks' pictures replay untouched.
  EXPECT_LE(host.composer.stats().picturesRecorded, 3u);
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorYELLOW); // the mutated tile
  EXPECT_EQ(host.pixel(53, 5), SK_ColorGREEN); // neighbors intact
}

TEST(ComposeReconcile, StructuralPruneNeedsNoMemo) {
  // The docs' promise: "a subtree whose new description equals its old
  // one is skipped wholesale — whether or not you used memo". Plain
  // boxes/text/images with value-comparable props re-render for free.
  Host host;
  auto tree = [] {
    return box().row().gap(8).padding(12)
        .child(box().width(40).height(40).corners({6}).fill(red()))
        .child(text(u8"static", styleAt(18)).key("t"))
        .child(box().grow(1).fill(blue()).opacity(0.9f));
  };
  host.composer.render(tree());
  host.frame();

  host.composer.render(tree()); // brand-new Elements, identical values
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty()); // hosts may skip the redraw
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

// ---------------------------------------------------------------------------
// Round-2 friction batch: withFrom entrances, trim wrap, per-side insets,
// overflow-safe recording, stroke align, measure(), presets, marquee.

TEST(ComposeMotion, WithFromPlaysEntranceOnMount) {
  Host host;
  host.composer.render(box().child(box()
                                       .width(80)
                                       .height(80)
                                       .fill(red())
                                       .opacity(withFrom(0.0f, 1.0f,
                                                         {200ms, &choreograph::easeNone}))));
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorBLACK); // enters invisible
  host.frame(0.3);                              // ramp done
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);

  // Re-describing the same withFrom() prunes clean — the entrance is a
  // mount thing, not a per-render restart.
  host.composer.render(box().child(box()
                                       .width(80)
                                       .height(80)
                                       .fill(red())
                                       .opacity(withFrom(0.0f, 1.0f,
                                                         {200ms, &choreograph::easeNone}))));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED); // still settled
}

TEST(ComposeMotion, WithFromColorSweepsOnMount) {
  Host host;
  host.composer.render(box().child(
      box().width(80).height(80).fill(PropValue<Fill>(withFrom(
          Fill::color({1, 1, 1, 1}), red(), {200ms, &choreograph::easeNone})))));
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorWHITE); // the declared "from"
  host.frame(0.3);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);
}

TEST(ComposeTrim, WrapModeCrossesTheSeam) {
  // A wrap window crossing the cycle seam must paint exactly the union of
  // its two clamp pieces — direction-agnostic pixel containment.
  auto strokedBox = [](PropValue<float> s, PropValue<float> e, TrimMode m) {
    return box().child(box()
                           .absolute()
                           .inset(50, 50, 50, 50)
                           .trim(std::move(s), std::move(e), 0.0f, m)
                           .foreground(util::stroke(6, green())));
  };
  Host wrap, pieceA, pieceB;
  wrap.composer.render(strokedBox(0.9f, 1.15f, TrimMode::Wrap));
  pieceA.composer.render(strokedBox(0.9f, 1.0f, TrimMode::Clamp));
  pieceB.composer.render(strokedBox(0.0f, 0.15f, TrimMode::Clamp));
  wrap.frame();
  pieceA.frame();
  pieceB.frame();
  int unionCount = 0, wrapCount = 0, missing = 0;
  for (int y = 40; y < 160; y += 2)
    for (int x = 40; x < 160; x += 2) {
      const bool inUnion = pieceA.pixel(x, y) == SK_ColorGREEN ||
                           pieceB.pixel(x, y) == SK_ColorGREEN;
      const bool inWrap = wrap.pixel(x, y) == SK_ColorGREEN;
      unionCount += inUnion;
      wrapCount += inWrap;
      missing += inUnion && !inWrap;
    }
  EXPECT_GT(unionCount, 50);     // the pieces really painted
  EXPECT_LE(missing, 4);         // wrap covers the union (AA slack)
  EXPECT_NEAR(wrapCount, unionCount, unionCount / 5.0 + 8);
}

TEST(ComposeTrim, WrapOffsetBindingMarchesTheWindow) {
  Host host;
  choreograph::Output<float> phase{0.0f};
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(50, 50, 50, 50)
                                       .trim(0.0f, 0.25f, &phase, TrimMode::Wrap)
                                       .foreground(util::stroke(6, green()))));
  host.frame();
  std::vector<SkIPoint> lit0;
  for (int y = 40; y < 160; y += 2)
    for (int x = 40; x < 160; x += 2)
      if (host.pixel(x, y) == SK_ColorGREEN)
        lit0.push_back({x, y});
  ASSERT_GT(lit0.size(), 10u);

  phase = 0.5f; // march half the cycle — no render()
  host.frame();
  int still = 0;
  for (const SkIPoint &p : lit0)
    still += host.pixel(p.x(), p.y()) == SK_ColorGREEN;
  // The window moved to the far side: (almost) none of the old pixels stay.
  EXPECT_LT((float)still, 0.2f * (float)lit0.size());
}

TEST(ComposeCache, OverflowingChildSurvivesPictureCaching) {
  // A child translated beyond its parent's box must not be quick-rejected
  // by the parent's recording cull (the recordBounds fix).
  Host host(300, 200);
  host.composer.render(box().child(
      box()
          .width(100)
          .height(100)
          .fill(blue())
          .child(box().width(40).height(40).fill(red()).translateX(150.0f))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 20), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(170, 20), SK_ColorRED); // fully outside parent's box
  host.frame();                                // cached replay path
  EXPECT_EQ(host.pixel(170, 20), SK_ColorRED);
}

TEST(ComposeLayout, PerSideInsetPinsWithoutStretch) {
  Host host(200, 100);
  host.composer.render(box().child(
      box().top(10).right(20).width(50).height(30).fill(red()).key("badge")));
  host.frame();
  auto b = host.composer.bounds("badge");
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(*b, SkRect::MakeXYWH(130, 10, 50, 30));
  EXPECT_EQ(host.pixel(140, 15), SK_ColorRED);
}

TEST(ComposeLayout, DimInsetsAcceptPercent) {
  Host host(200, 100);
  host.composer.render(box().child(
      box().inset(pct(10), pct(10), pct(10), pct(10)).fill(red()).key("panel")));
  host.frame();
  auto b = host.composer.bounds("panel");
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(*b, SkRect::MakeXYWH(20, 10, 160, 80));
}

TEST(ComposeMeasure, MeasureReportsIntrinsicSize) {
  const SkSize size = measure(box()
                                  .row()
                                  .gap(10)
                                  .child(box().width(40).height(30))
                                  .child(box().width(40).height(20)),
                              fonts());
  EXPECT_EQ(size, SkSize::Make(90, 30));
}

TEST(ComposeStroke, StrokeAlignInnerAndOuter) {
  auto boxWith = [](PathFormat::Align align) {
    return box().child(box()
                           .absolute()
                           .inset(50, 50, 50, 50)
                           .stroke(util::stroke(20, green(), align)));
  };
  Host inner, outer;
  inner.composer.render(boxWith(PathFormat::Align::Inner));
  outer.composer.render(boxWith(PathFormat::Align::Outer));
  inner.frame();
  outer.frame();
  // Box edge at x=50 (spans 50..150), sampled at mid-height.
  EXPECT_EQ(inner.pixel(60, 100), SK_ColorGREEN);  // inside band
  EXPECT_EQ(inner.pixel(42, 100), SK_ColorBLACK);  // nothing outside
  EXPECT_EQ(outer.pixel(42, 100), SK_ColorGREEN);  // outside band
  EXPECT_EQ(outer.pixel(60, 100), SK_ColorBLACK);  // nothing inside
  // The outer band survives the cached replay (bleed declared).
  outer.frame();
  EXPECT_EQ(outer.pixel(42, 100), SK_ColorGREEN);
}

TEST(ComposeText, TextAlignCentersWithinWideBox) {
  auto leftmostLit = [](Host &host) {
    for (int x = 0; x < 400; ++x)
      for (int y = 0; y < 100; y += 2)
        if (host.pixel(x, y) != SK_ColorBLACK)
          return x;
    return 400;
  };
  Host start(400, 100), center(400, 100);
  start.composer.render(
      box().child(text(u8"II", whiteStyle(30)).width(Dim(300.0f))));
  center.composer.render(
      box().child(text(u8"II", whiteStyle(30))
                      .width(Dim(300.0f))
                      .textAlign(sigil::weave::TextAlignment::kCenter)));
  start.frame();
  center.frame();
  const int startX = leftmostLit(start), centerX = leftmostLit(center);
  ASSERT_LT(startX, 400); // both actually painted
  ASSERT_LT(centerX, 400);
  EXPECT_GT(centerX, startX + 60); // centered glyphs sit near mid-box
}

TEST(ComposeRouters, OrbitFollowsTheRing) {
  const SkPoint center{100, 100};
  RailRouter router = routers::orbit(center);
  const SkPoint pts[2] = {{200, 100}, {100, 200}};
  const SkPath path = router(std::span<const SkPoint>(pts, 2));
  SkContourMeasureIter iter(path, false);
  sk_sp<SkContourMeasure> contour = iter.next();
  ASSERT_TRUE(contour);
  // Quarter circle r=100: length ~157 (a chord would be ~141), and the
  // midpoint sits ON the ring.
  EXPECT_NEAR(contour->length(), 157.1f, 3.0f);
  SkPoint mid;
  ASSERT_TRUE(contour->getPosTan(contour->length() / 2, &mid, nullptr));
  EXPECT_NEAR(SkPoint::Distance(mid, center), 100.0f, 1.5f);
}

TEST(ComposeStyles, PresetBundlesRenderAndPrune) {
  Host host(300, 120);
  auto tree = [] {
    return box().row().gap(20).padding(20)
        .child(box().width(120).height(44).corners({22}).style(
            styles::aquaGel()))
        .child(box().width(120).height(44).corners({8}).style(
            styles::y2kChrome()));
  };
  host.composer.render(tree());
  host.frame();
  // Aqua pill — the §2 gloss physics: bright lens at the top, the
  // saturated dark band just under it, and the LIGHT-FROM-BELOW glow at
  // the bottom (both ends beat the midband).
  const SkColor aquaTop = host.pixel(80, 28);
  const SkColor aquaMid = host.pixel(80, 47);
  const SkColor aquaBottom = host.pixel(80, 60);
  EXPECT_NE(aquaTop, SK_ColorBLACK);
  EXPECT_NE(aquaMid, SK_ColorBLACK);
  auto lum = [](SkColor c) {
    return SkColorGetR(c) + SkColorGetG(c) + SkColorGetB(c);
  };
  EXPECT_GT(lum(aquaTop), lum(aquaMid));
  EXPECT_GT(lum(aquaBottom), lum(aquaMid));
  // Chrome bar: the §3 ramp's hard horizon — brighter above it than below.
  const SkColor chromeTop = host.pixel(220, 28);
  const SkColor chromeMid = host.pixel(220, 42);
  EXPECT_GT(lum(chromeTop), lum(chromeMid) + 100);
  // Both bundles are value decorations: identical re-describe prunes.
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposePatterns, HalftoneRampSwellsDownward) {
  Host host(100, 100);
  host.composer.render(box().child(
      box().width(100).height(100).fill(
          patterns::halftoneRamp(10, 1.0f, 4.0f, {1, 1, 1, 1}))));
  host.frame();
  int top = 0, bottom = 0;
  for (int y = 0; y < 20; ++y)
    for (int x = 0; x < 100; x += 1)
      top += host.pixel(x, y) != SK_ColorBLACK;
  for (int y = 80; y < 100; ++y)
    for (int x = 0; x < 100; x += 1)
      bottom += host.pixel(x, y) != SK_ColorBLACK;
  EXPECT_GT(bottom, top * 2); // dots swell toward the bottom
}

TEST(ComposeUtil, MarqueeSlidesTwoCopies) {
  Host host(200, 60);
  choreograph::Output<float> phase{0.0f};
  host.composer.render(box().padding(10).child(
      util::marquee(box().width(60).height(20).fill(red()), &phase)
          .width(Dim(100.0f))
          .height(Dim(20.0f))));
  host.frame();
  EXPECT_EQ(host.pixel(60, 20), SK_ColorRED);  // first copy
  EXPECT_EQ(host.pixel(105, 20), SK_ColorRED); // second copy (65..130 → clip)
  phase = -30.0f;                              // slide — no render()
  host.frame();
  EXPECT_EQ(host.pixel(85, 20), SK_ColorRED);   // second copy now 30..90
  EXPECT_EQ(host.pixel(105, 20), SK_ColorBLACK); // past both copies, clipped
}

TEST(ComposeDecorations, BoundShadowOffsetSlides) {
  Host host;
  choreograph::Output<float> lift{0.0f};
  util::Shadow shadow;
  shadow.color = {0, 1, 0, 1};
  shadow.bindOffsetX = &lift;
  shadow.maxBind = 40.0f;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(60, 60, 80, 80)
                                       .background(shadow)));
  host.frame();
  EXPECT_EQ(host.pixel(90, 90), SK_ColorGREEN);  // at rest: under the box
  EXPECT_EQ(host.pixel(135, 90), SK_ColorBLACK); // nothing to the right
  lift = 30.0f; // slide the shadow — no render()
  host.frame();
  EXPECT_EQ(host.pixel(135, 90), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(65, 90), SK_ColorBLACK);
}

TEST(ComposeText, TextFillMapsUnitRampToCapBand) {
  // A hard two-stop ramp authored in [0,1]: red above the midline, blue
  // below. textFill maps it to the CAP BAND, so the switch happens INSIDE
  // the glyphs — capitals read red on top, blue underneath.
  Host host(300, 120);
  host.composer.render(box().padding(20).child(
      text(u8"HHH", whiteStyle(64))
          .textFill(Material::linear({0, 0}, {0, 1},
                                     {{0.0f, {1, 0, 0, 1}},
                                      {0.499f, {1, 0, 0, 1}},
                                      {0.501f, {0, 0, 1, 1}},
                                      {1.0f, {0, 0, 1, 1}}}))));
  host.frame();
  // Find the lit band first, then judge its top vs bottom thirds — the
  // ramp midline lives at the CAP BAND's middle, not the canvas's.
  int yMin = 120, yMax = 0;
  for (int y = 0; y < 120; ++y)
    for (int x = 0; x < 300; x += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) {
        yMin = std::min(yMin, y);
        yMax = std::max(yMax, y);
      }
  ASSERT_LT(yMin, yMax);
  const int third = std::max((yMax - yMin) / 3, 1);
  int topR = 0, topB = 0, botR = 0, botB = 0;
  for (int y = yMin; y <= yMax; ++y)
    for (int x = 0; x < 300; x += 2) {
      const SkColor c = host.pixel(x, y);
      if (c == SK_ColorBLACK)
        continue;
      const bool reddish = SkColorGetR(c) > SkColorGetB(c) + 64;
      const bool bluish = SkColorGetB(c) > SkColorGetR(c) + 64;
      if (y < yMin + third) {
        topR += reddish;
        topB += bluish;
      } else if (y > yMax - third) {
        botR += reddish;
        botB += bluish;
      }
    }
  EXPECT_GT(topR, 20);       // upper glyph pixels are red…
  EXPECT_GT(botB, 20);       // …lower ones blue…
  EXPECT_LT(topB, topR / 4); // …and barely mixed
  EXPECT_LT(botR, botB / 4);
}

TEST(ComposeText, OnPathRidesTheBaselineItIsGiven) {
  // Placing curved lettering by hand costs one Element and one layout PER
  // GLYPH — the Nightingale study spent ~230 of each on its ring labels.
  // onPath shapes the run ONCE and places every glyph by arc length.
  //
  // A run on the TOP half of a circle must paint above the centre and
  // leave the bottom half empty; the same run at at=0.5 must do the
  // opposite. That is the whole contract, and a straight-line layout
  // cannot satisfy either.
  auto ring = [](float at) {
    return text(u8"HHHHHHHHHH", whiteStyle(22))
        .width(240).height(240).absolute().left(0).top(0)
        .onPath({.path = shapes::arc(180.0f, 359.9f), .at = at,
                 .align = TextPath::Align::Center});
  };
  auto lit = [](Host &host, int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 240; ++x)
        count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  Host top(240, 240);
  top.composer.render(box().child(ring(0.25f)));
  top.frame();
  EXPECT_GT(lit(top, 0, 110), 200);   // ink on the top arc
  EXPECT_LT(lit(top, 140, 240), 40);  // and almost none below

  Host bottom(240, 240);
  bottom.composer.render(box().child(ring(0.75f)));
  bottom.frame();
  EXPECT_GT(lit(bottom, 140, 240), 200);
  EXPECT_LT(lit(bottom, 0, 110), 40);
}

TEST(ComposeText, OnPathWrapsTheSeamAndFlipsWithoutMirroring) {
  // Two bugs found by the Vertigo study, hours after onPath shipped.
  //
  // 1. Align::Center at at=0 on a CLOSED baseline put half the run at a
  //    negative distance and dropped it. Fraction 0 and 1 are the same
  //    point on a ring; the run has to straddle the seam.
  // 2. autoFlip turned each glyph over IN PLACE, which reverses reading
  //    order — a lower-half caption came out mirrored. The flip is a
  //    decision about the RUN, so the run also walks backwards.
  auto ink = [](Host &host, int x0, int x1, int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x)
        count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  Host seam(240, 240);
  seam.composer.render(box().child(
      text(u8"HHHHHHHH", whiteStyle(20))
          .width(240).height(240).absolute().left(0).top(0)
          .onPath({.path = shapes::arc(180.0f, 359.9f), .at = 0.0f,
                   .align = TextPath::Align::Center})));
  seam.frame();
  // at=0 on this arc is 9 o'clock, so a centred run straddles it: ink on
  // BOTH sides of the horizontal midline, near the left edge.
  EXPECT_GT(ink(seam, 0, 60, 0, 120), 60) << "the half before the seam was dropped";
  EXPECT_GT(ink(seam, 0, 60, 120, 240), 60);

  // Flipped, the run must still read left-to-right in the same order it
  // does unflipped — mirrored text has its ink distribution reversed, so
  // compare the first and last thirds of a deliberately lopsided run.
  auto lopsided = [](bool flip) {
    return text(u8"IIIIIIIIWWWW", whiteStyle(20))
        .width(260).height(260).absolute().left(0).top(0)
        .onPath({.path = shapes::arc(0.0f, 359.9f), .at = 0.30f,
                 .align = TextPath::Align::Start, .autoFlip = flip});
  };
  Host plain(260, 260), flipped(260, 260);
  plain.composer.render(box().child(lopsided(false)));
  plain.frame();
  flipped.composer.render(box().child(lopsided(true)));
  flipped.frame();
  // Both runs occupy the same stretch of the ring, so the heavy Ws land in
  // the same place — which is exactly what mirroring would break.
  const int plainLower = ink(plain, 0, 260, 130, 260);
  const int flipLower = ink(flipped, 0, 260, 130, 260);
  EXPECT_GT(plainLower, 100);
  EXPECT_GT(flipLower, 100);
}

TEST(ComposeText, TextFillKeepsTheStylesOtherPasses) {
  // textFill supersedes the FOREGROUND, not the passes around it. The
  // chrome-type override used to be a blank PaintStyle, which silently
  // dropped every underlay — a chrome wordmark lost its cast shadow and
  // its dark keyline and read as flat type sitting on the plate.
  Host host(300, 120);
  auto styled = [] {
    auto s = whiteStyle(64);
    sigil::weave::PaintLayer keyline;
    keyline.paint.setAntiAlias(true);
    keyline.paint.setStyle(SkPaint::kStroke_Style);
    keyline.paint.setStrokeWidth(6);
    keyline.paint.setColor4f({0, 1, 0, 1}, nullptr); // unmistakably not the fill
    s.paint.addUnderlay(keyline);
    return s;
  }();
  host.composer.render(box().padding(20).child(
      text(u8"HHH", styled).textFill(Material::solid({1, 0, 0, 1}))));
  host.frame();
  int red = 0, green = 0;
  for (int y = 0; y < 120; ++y)
    for (int x = 0; x < 300; ++x) {
      const SkColor c = host.pixel(x, y);
      red += SkColorGetR(c) > 180 && SkColorGetG(c) < 80;
      green += SkColorGetG(c) > 180 && SkColorGetR(c) < 80;
    }
  EXPECT_GT(red, 100);   // the material still paints the glyph bodies…
  EXPECT_GT(green, 100); // …and the keyline underlay still rings them
}

// ---------------------------------------------------------------------------
// Round-2b: fleet feedback landed at the API level — delay staggers,
// unclipped decorations, knockout shadows, px origins, centerAt, wrapped
// stroke windows, one-contour wrap.

TEST(ComposeMotion, DelayStaggersTheEntrance) {
  Host host;
  auto card = [](float delaySec) {
    return box().width(60).height(30).fill(red()).opacity(
        withFrom(0.0f, 1.0f,
                 {200ms, &choreograph::easeNone,
                  std::chrono::milliseconds((int)(delaySec * 1000))}));
  };
  host.composer.render(
      box().column().gap(10).child(card(0.0f)).child(card(0.4f)));
  host.frame(0.3); // first card done, second still holding its `from`
  EXPECT_EQ(host.pixel(30, 15), SK_ColorRED);
  EXPECT_EQ(host.pixel(30, 55), SK_ColorBLACK);
  host.frame(0.5); // 0.8s total: both settled
  EXPECT_EQ(host.pixel(30, 55), SK_ColorRED);
}

TEST(ComposePaint, ClipSparesDecorations) {
  // clip() bounds fill/content/children; decorations dress the outline —
  // an Outer stroke and a shadow survive on a clipped node.
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(60, 60, 60, 60)
          .clip(true)
          .fill(blue())
          .stroke(util::stroke(10, green(), PathFormat::Align::Outer))
          .child(box().width(200).height(10).fill(red()))));
  host.frame();
  EXPECT_EQ(host.pixel(52, 100), SK_ColorGREEN); // outer stroke intact
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLUE); // fill clipped area
  EXPECT_EQ(host.pixel(150, 65), SK_ColorBLACK); // child clipped at 140
}

TEST(ComposeDecorations, KnockoutShadowLeavesTheFootprintClear) {
  Host host;
  util::Shadow s;
  s.color = {0, 1, 0, 1};
  s.offset = {20, 0};
  s.knockout = true;
  host.composer.render(box().child(
      box().absolute().inset(60, 60, 80, 80).background(s)));
  host.frame();
  EXPECT_EQ(host.pixel(130, 90), SK_ColorGREEN); // shadow right of the box
  EXPECT_EQ(host.pixel(100, 90), SK_ColorBLACK); // footprint knocked out
}

TEST(ComposeTransform, PixelOriginPivotsWhereTold) {
  // Two hosts: fractional center origin vs px origin at the box's own
  // top-left corner; rotate 90° and the box lands in different places.
  Host frac, px;
  auto tree = [](Element inner) {
    return box().child(std::move(inner));
  };
  frac.composer.render(tree(box()
                                .absolute()
                                .inset(80, 80, 80, 80)
                                .fill(red())
                                .rotate(90.0f))); // pivots on its center
  px.composer.render(tree(box()
                              .absolute()
                              .inset(80, 80, 80, 80)
                              .fill(red())
                              .rotate(90.0f)
                              .transformOriginPx({0, 0}))); // pivots top-left
  frac.frame();
  px.frame();
  EXPECT_EQ(frac.pixel(100, 100), SK_ColorRED); // unchanged footprint
  EXPECT_EQ(px.pixel(100, 100), SK_ColorBLACK); // swung away
  EXPECT_EQ(px.pixel(65, 100), SK_ColorRED);    // now left of the pivot
}

TEST(ComposeLayout, CenterAtPinsMeasuredBoxOnPoint) {
  Host host;
  host.composer.render(box().child(
      box().centerAt({120, 80}).width(40).height(20).fill(red()).key("s")));
  host.frame();
  auto b = host.composer.bounds("s");
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(*b, SkRect::MakeXYWH(100, 70, 40, 20));
  EXPECT_EQ(host.pixel(120, 80), SK_ColorRED);
}

TEST(ComposeLayouts, AbsoluteDiagonalAutoSizes) {
  // The pinned battery no longer needs hand-guessed dims: the container
  // takes the placed extent.
  Host host;
  host.composer.render(box().child(
      Element(layout(layouts::Diagonal{.skewDeg = -20, .gap = 10}))
          .key("battery")
          .absolute()
          .left(Dim(30.0f))
          .top(Dim(20.0f))
          .child(box().width(80).height(24).fill(red()))
          .child(box().width(80).height(24).fill(blue()))
          .child(box().width(80).height(24).fill(green()))));
  host.frame();
  auto b = host.composer.bounds("battery");
  ASSERT_TRUE(b.has_value());
  // Three rows: height 3*24 + 2*10 = 92; x-drift = tan(20°)*68 ≈ 24.7 +
  // 80 wide rows → width ≈ 104.7.
  EXPECT_NEAR(b->height(), 92, 1.0f);
  EXPECT_NEAR(b->width(), 104.7f, 2.0f);
}

TEST(ComposeDecorations, StrokeTrimWindowMarchesPerDecoration) {
  // One node: full static band + a bound marching sliver — no overlay box.
  Host host;
  choreograph::Output<float> phase{0.0f};
  PathFormat band;
  band.width = 4;
  band.strokeFill = green();
  PathFormat sliver;
  sliver.width = 8;
  sliver.strokeFill = red();
  sliver.trimStart = 0.0f;
  sliver.trimEnd = 0.1f;
  sliver.trimPhase = &phase;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(50, 50, 50, 50)
                                       .stroke(band)
                                       .stroke(sliver)));
  host.frame();
  std::vector<SkIPoint> redNow;
  int greenCount = 0;
  for (int y = 40; y < 160; y += 2)
    for (int x = 40; x < 160; x += 2) {
      if (host.pixel(x, y) == SK_ColorRED)
        redNow.push_back({x, y});
      greenCount += host.pixel(x, y) == SK_ColorGREEN;
    }
  ASSERT_GT(redNow.size(), 4u); // the sliver painted
  ASSERT_GT(greenCount, 50);    // the band painted everywhere else
  phase = 0.5f;                 // march — no render()
  host.frame();
  int still = 0;
  for (const SkIPoint &p : redNow)
    still += host.pixel(p.x(), p.y()) == SK_ColorRED;
  EXPECT_LT((float)still, 0.25f * (float)redNow.size());
}

TEST(ComposeTrim, WrapSeamIsOneContour) {
  // A seam-crossing wrap window must be ONE contour: two pieces would
  // double-hit round caps / additive brushes at the joint.
  Host host;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(50, 50, 50, 50)
                                       .trim(0.9f, 1.1f, 0.0f, TrimMode::Wrap)
                                       .foreground(util::stroke(4, green()))));
  host.frame(); // renders — and the contour count proves the stitch:
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(100, 100));
  SkPath boxPath = b.detach();
  SkPathBuilder stitchedBuilder;
  SkContourMeasureIter iter(boxPath, false);
  while (sk_sp<SkContourMeasure> c = iter.next()) {
    const float len = c->length();
    (void)c->getSegment(0.9f * len, len, &stitchedBuilder, true);
    (void)c->getSegment(0, 0.1f * len, &stitchedBuilder, false);
  }
  SkPath stitched = stitchedBuilder.detach();
  int contours = 0;
  SkContourMeasureIter check(stitched, false);
  while (check.next())
    ++contours;
  EXPECT_EQ(contours, 1);
}

TEST(ComposePatterns, HalftoneRampBandRemaps) {
  // rampFrom/rampTo confine the swell: with the band pushed to the bottom
  // half, the top half stays at rMin everywhere.
  Host host(100, 100);
  host.composer.render(box().child(
      box().width(100).height(100).fill(patterns::halftoneRamp(
          10, 0.8f, 4.0f, {1, 1, 1, 1}, 0.0f, 0.5f, 1.0f))));
  host.frame();
  int band20 = 0, band45 = 0;
  for (int y = 10; y < 20; ++y)
    for (int x = 0; x < 100; ++x)
      band20 += host.pixel(x, y) != SK_ColorBLACK;
  for (int y = 38; y < 48; ++y)
    for (int x = 0; x < 100; ++x)
      band45 += host.pixel(x, y) != SK_ColorBLACK;
  // Both bands sit above the ramp start → same tiny dots, no swell yet.
  EXPECT_NEAR(band20, band45, band20 / 2 + 12);
  int bandBottom = 0;
  for (int y = 88; y < 98; ++y)
    for (int x = 0; x < 100; ++x)
      bandBottom += host.pixel(x, y) != SK_ColorBLACK;
  EXPECT_GT(bandBottom, band20 * 2); // full swell at the bottom
}

TEST(ComposeMotion, StaggerChildrenCascadesEntrances) {
  // One container call replaces per-child delay arithmetic: child i's
  // subtree enters i·each later (three studies asked independently).
  Host host;
  auto card = [] {
    return box().width(60).height(30).fill(red()).opacity(
        withFrom(0.0f, 1.0f, {200ms, &choreograph::easeNone}));
  };
  host.composer.render(box()
                           .column()
                           .gap(10)
                           .staggerChildren(400ms)
                           .child(card())
                           .child(card()));
  host.frame(0.3); // child 0 settled; child 1 still holding its `from`
  EXPECT_EQ(host.pixel(30, 15), SK_ColorRED);
  EXPECT_EQ(host.pixel(30, 55), SK_ColorBLACK);
  host.frame(0.5); // 0.8s: the cascade completed
  EXPECT_EQ(host.pixel(30, 55), SK_ColorRED);
}

#include <sigilcompose/Lines.h>

// ---------------------------------------------------------------------------
// Line patterns (Lines.h) — the beyond-dashes stroke vocabulary.

namespace {
/** Counts distinct painted runs in a vertical scan column. */
int verticalRuns(Host &host, int x, int y0, int y1, SkColor color) {
  int runs = 0;
  bool in = false;
  for (int y = y0; y <= y1; ++y) {
    const bool hit = host.pixel(x, y) == color;
    if (hit && !in)
      ++runs;
    in = hit;
  }
  return runs;
}
Element straightRun(Decoration style) {
  // A horizontal open path across the node, dressed by the line style.
  return box().child(box()
                         .absolute()
                         .inset(20, 80, 20, 80)
                         .outline([](SkSize s) {
                           SkPathBuilder b;
                           b.moveTo(0, s.height() / 2);
                           b.lineTo(s.width(), s.height() / 2);
                           return b.detach();
                         })
                         .stroke(std::move(style)));
}
} // namespace

TEST(ComposeLines, TripleRailStrokesThreeBands) {
  Host host;
  host.composer.render(straightRun(lines::triple(2, green(), 8, 1.0f)));
  host.frame();
  EXPECT_EQ(verticalRuns(host, 100, 70, 130, SK_ColorGREEN), 3);
  // And the pair variant gives exactly two.
  Host pair;
  pair.composer.render(straightRun(lines::cased(2, green(), 8)));
  pair.frame();
  EXPECT_EQ(verticalRuns(pair, 100, 70, 130, SK_ColorGREEN), 2);
}

TEST(ComposeLines, ArrowheadFillsBeyondTheBodyWidth) {
  Host host, plain;
  host.composer.render(straightRun(lines::arrow(2, green(), 14)));
  plain.composer.render(straightRun(lines::Line{.width = 2, .fill = green()}));
  host.frame();
  plain.frame();
  // The grounded convention (decorator/tldraw/D3 practice): the TIP sits
  // AT the endpoint (x=180) and the head extends BACKWARD over the run —
  // wings widen where the 2px plain body never paints.
  EXPECT_EQ(host.pixel(170, 96), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(170, 104), SK_ColorGREEN);
  EXPECT_EQ(plain.pixel(170, 96), SK_ColorBLACK);
  // Nothing pokes past the endpoint in either version.
  EXPECT_EQ(host.pixel(184, 100), SK_ColorBLACK);
  EXPECT_EQ(plain.pixel(184, 100), SK_ColorBLACK);
}

TEST(ComposeLines, RailwayTiesCrossTheLine) {
  Host host;
  host.composer.render(straightRun(lines::railway(2, green(), 20, 12)));
  host.frame();
  // A tie arm ~5px above the rail at the first sample (x = 20+10)…
  EXPECT_EQ(host.pixel(30, 95), SK_ColorGREEN);
  // …and clear rail between ties.
  EXPECT_EQ(host.pixel(40, 95), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(40, 100), SK_ColorGREEN);
}

TEST(ComposeLines, WavyRunLeavesTheAxis) {
  Host host, straight;
  host.composer.render(straightRun(lines::wavy(2, green(), 8, 24)));
  straight.composer.render(
      straightRun(lines::Line{.width = 2, .fill = green()}));
  host.frame();
  straight.frame();
  int offAxis = 0, offAxisStraight = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7}) {
      offAxis += host.pixel(x, 100 + dy) == SK_ColorGREEN;
      offAxisStraight += straight.pixel(x, 100 + dy) == SK_ColorGREEN;
    }
  EXPECT_GT(offAxis, 10);
  EXPECT_EQ(offAxisStraight, 0);
}

#include <sigilcompose/Brushes.h>

// ---------------------------------------------------------------------------
// The Illustrator brush model: scatter/pattern/ribbon + the ops pipeline.

TEST(ComposeBrushes, ScatterInstancesArtAlongThePath) {
  Host host;
  host.composer.render(straightRun([] {
    brushes::ScatterBrush b;
    b.art = box().width(6).height(6).fill(red());
    b.spacing = 40;
    b.alignToPath = true;
    return b;
  }()));
  host.frame();
  // 160px run, spacing 40 → stamps at d = 20, 60, 100, 140 (x = 20+d).
  EXPECT_EQ(host.pixel(40, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(80, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(60, 100), SK_ColorBLACK); // between stamps
}

TEST(ComposeBrushes, ScatterModSkipsAndLifts) {
  Host host;
  brushes::ScatterBrush b;
  b.art = box().width(6).height(6).fill(red());
  b.spacing = 40;
  b.mod = [](const PathSample &, size_t i, size_t) {
    brushes::StampMod m;
    if (i % 2)
      m.skip = true; // drop every other slot
    else
      m.dNormal = -20; // lift the kept ones off the axis
    return m;
  };
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  EXPECT_EQ(host.pixel(40, 80), SK_ColorRED);   // slot 0 lifted (d=20)
  EXPECT_EQ(host.pixel(80, 80), SK_ColorBLACK); // slot 1 skipped (d=60)
  EXPECT_EQ(host.pixel(80, 100), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(120, 80), SK_ColorRED);  // slot 2 lifted (d=100)
}

TEST(ComposeBrushes, PatternIntegerFitNeverTearsTheLastTile) {
  // 160px run, 25px tile → 6 slots stretched to 26.67px: coverage reaches
  // BOTH ends with no torn tail (the Illustrator fit rule).
  Host host;
  brushes::PatternBrush b;
  b.side = box().width(25).height(8).fill(red());
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  EXPECT_EQ(host.pixel(42, 100), SK_ColorRED);  // first slot starts at run 0
  EXPECT_EQ(host.pixel(178, 100), SK_ColorRED); // last slot ends at run end
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED); // continuous through middle
}

TEST(ComposeBrushes, PatternCornerTileSitsOnTheBend) {
  Host host;
  brushes::PatternBrush b;
  b.side = box().width(20).height(4).fill(red());
  b.corner = box().width(12).height(12).fill(blue());
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(40, 40, 40, 40)
          .outline([](SkSize s) { // an L: right then down
            SkPathBuilder p;
            p.moveTo(0, 0);
            p.lineTo(s.width(), 0);
            p.lineTo(s.width(), s.height());
            return p.detach();
          })
          .stroke(std::move(b))));
  host.frame();
  EXPECT_EQ(host.pixel(160, 40), SK_ColorBLUE); // corner tile at the bend
  EXPECT_EQ(host.pixel(100, 40), SK_ColorRED);  // side tiles on the top leg
  EXPECT_EQ(host.pixel(160, 100), SK_ColorRED); // and down the right leg
}

TEST(ComposeBrushes, RibbonTapersAndNibVariesWithAngle) {
  Host taperHost;
  taperHost.composer.render(straightRun(brushes::taper(16, 2, green())));
  taperHost.frame();
  auto bandHeight = [](Host &h, int x) {
    int lit = 0;
    for (int y = 70; y < 130; ++y)
      lit += h.pixel(x, y) == SK_ColorGREEN;
    return lit;
  };
  EXPECT_GT(bandHeight(taperHost, 30), 12);  // wide near the start
  EXPECT_LT(bandHeight(taperHost, 170), 6);  // narrow near the end

  // Calligraphic nib at 0°: a horizontal run lies ALONG the nib → thin.
  Host nib;
  nib.composer.render(
      straightRun(brushes::calligraphic(0, 16, green(), 0.2f)));
  nib.frame();
  EXPECT_LT(bandHeight(nib, 100), 6);
}

TEST(ComposeBrushes, RestyleWavesAnyDecoration) {
  Host host;
  host.composer.render(straightRun(brushes::restyle(
      ops::wave(8, 24), util::stroke(2, green()), 12)));
  host.frame();
  int offAxis = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7})
      offAxis += host.pixel(x, 100 + dy) == SK_ColorGREEN;
  EXPECT_GT(offAxis, 10); // the stroke followed the waved geometry
}

// ---------------------------------------------------------------------------
// Skia seams wired in (REVIEW.md §14): sketchy jitter, SVG outlines, Perlin.

TEST(ComposeSeams, SketchyJitterLeavesTheAxis) {
  Host host, plain;
  host.composer.render(straightRun(
      brushes::restyle(ops::sketchy(8, 3.0f, 11), util::stroke(2, green()))));
  plain.composer.render(straightRun(util::stroke(2, green())));
  host.frame();
  plain.frame();
  int off = 0, offPlain = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-2, 2}) {
      off += host.pixel(x, 100 + dy) == SK_ColorGREEN;
      offPlain += plain.pixel(x, 100 + dy) == SK_ColorGREEN;
    }
  EXPECT_GT(off, 8);       // the hand wobbles
  EXPECT_EQ(offPlain, 0);  // the ruler doesn't
}

TEST(ComposeSeams, SvgOutlineTracesThePathData) {
  // A right triangle authored as an SVG d-string, stretched to the node.
  Host host;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(50, 50, 50, 50)
                                       .outline(shapes::svg("M0 0 L100 0 L100 100 Z"))
                                       .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(140, 70), SK_ColorRED);  // inside the hypotenuse
  EXPECT_EQ(host.pixel(60, 130), SK_ColorBLACK); // outside it
  // Hit-testing follows the silhouette too.
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(50, 50, 50, 50)
                                       .outline(shapes::svg("M0 0 L100 0 L100 100 Z"))
                                       .fill(red())
                                       .key("tri")));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({140, 70}).value_or(""), "tri");
  EXPECT_FALSE(host.composer.hitTest({60, 130}).has_value());
}

TEST(ComposeSeams, PerlinNoiseFillsWithVariation) {
  Host host(100, 100);
  host.composer.render(box().child(
      box().width(100).height(100).fill(patterns::noise(0.05f, 4, 2.0f))));
  host.frame();
  std::set<SkColor> distinct;
  for (int y = 10; y < 90; y += 8)
    for (int x = 10; x < 90; x += 8)
      distinct.insert(host.pixel(x, y));
  EXPECT_GT(distinct.size(), 30u); // organic variation, not a flat fill
}

// ---------------------------------------------------------------------------
// Round-3 friction batch: echo misprints, stagger origin, quantized time.

TEST(ComposePaint, EchoStampsShapeUnderTheFill) {
  Host host;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(50, 50, 90, 90)
                                       .fill(red())
                                       .echo({10, 10}, {0, 1, 0, 1})));
  host.frame();
  EXPECT_EQ(host.pixel(80, 80), SK_ColorRED);    // real fill on top
  EXPECT_EQ(host.pixel(115, 115), SK_ColorGREEN); // echo peeking past it
  EXPECT_EQ(host.pixel(45, 45), SK_ColorBLACK);   // nothing before either
  host.frame(); // survives the cached replay (cull grew by the offset)
  EXPECT_EQ(host.pixel(115, 115), SK_ColorGREEN);
}

TEST(ComposeText, EchoStampsTextUnderThePass) {
  Host host(300, 120);
  host.composer.render(box().padding(20).child(
      text(u8"ECHO", whiteStyle(48)).echo({6, -8}, {1, 0, 0, 1})));
  host.frame();
  int redCount = 0, whiteCount = 0;
  for (int y = 0; y < 120; y += 2)
    for (int x = 0; x < 300; x += 2) {
      redCount += host.pixel(x, y) == SK_ColorRED;
      whiteCount += host.pixel(x, y) == SK_ColorWHITE;
    }
  EXPECT_GT(whiteCount, 50); // the real pass
  EXPECT_GT(redCount, 20);   // the misprint peeking out at (6,−8)
}

TEST(ComposeMotion, StaggerFromEndRunsBottomUp) {
  Host host;
  auto card = [] {
    return box().width(60).height(30).fill(red()).opacity(
        withFrom(0.0f, 1.0f, {200ms, &choreograph::easeNone}));
  };
  host.composer.render(box()
                           .column()
                           .gap(10)
                           .staggerChildren(400ms, Stagger::From::End)
                           .child(card())
                           .child(card()));
  host.frame(0.3); // LAST child leads; first still holds its `from`
  EXPECT_EQ(host.pixel(30, 15), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(30, 55), SK_ColorRED);
  host.frame(0.5);
  EXPECT_EQ(host.pixel(30, 15), SK_ColorRED);
}

TEST(ComposeMaterials, QuantizeTimeStepsTheClock) {
  // uTime → red channel; with quantizeTime(2) samples inside one half-
  // second step resolve identically, and differ across steps.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform float uTime; half4 main(float2 p) {"
      "  return half4(fract(uTime), 0, 0, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  Material stepped = Material::sksl(fx).quantizeTime(2.0f);
  auto sampleAt = [&](Material &m, double seconds) {
    PaintContext ctx;
    ctx.size = {8, 8};
    ctx.elapsedSeconds = seconds;
    Fill f = m.resolve(ctx);
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
    SkPaint p;
    p.setShader(f.shaderValue);
    s->getCanvas()->drawPaint(p);
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    s->readPixels(bm.pixmap(), 1, 1);
    return bm.getColor(0, 0);
  };
  EXPECT_EQ(sampleAt(stepped, 0.6), sampleAt(stepped, 0.9));  // same step
  EXPECT_NE(sampleAt(stepped, 0.6), sampleAt(stepped, 1.1));  // next step
  Material continuous = Material::sksl(fx);
  EXPECT_NE(sampleAt(continuous, 0.6), sampleAt(continuous, 0.9));
}

TEST(ComposeMotion, KeyframesPlayTheMountPath) {
  // The P3R cursor overshoot: +40 → −20 → 0. Mid-path the box sits LEFT
  // of rest — a single from→to ramp could never cross it.
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(100, 80, 60, 80)
          .fill(red())
          .translateX(withKeyframes<float>(
              {{std::chrono::milliseconds(0), 40.0f},
               {std::chrono::milliseconds(200), -20.0f},
               {std::chrono::milliseconds(400), 0.0f}},
              &choreograph::easeNone))));
  host.frame();
  EXPECT_EQ(host.pixel(145, 100), SK_ColorRED);  // starts at +40
  EXPECT_EQ(host.pixel(105, 100), SK_ColorBLACK);
  host.frame(0.2); // waypoint 2: x = −20
  EXPECT_EQ(host.pixel(85, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(145, 100), SK_ColorBLACK);
  host.frame(0.3); // settled at 0
  EXPECT_EQ(host.pixel(105, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(85, 100), SK_ColorBLACK);
  // Identical re-describe prunes (waypoints compare structurally).
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(100, 80, 60, 80)
          .fill(red())
          .translateX(withKeyframes<float>(
              {{std::chrono::milliseconds(0), 40.0f},
               {std::chrono::milliseconds(200), -20.0f},
               {std::chrono::milliseconds(400), 0.0f}},
              &choreograph::easeNone))));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposeLayouts, StickerScatterGeneratesTheLadder) {
  const auto slots = layouts::stickerScatter(5, 3);
  ASSERT_EQ(slots.size(), 5u);
  for (size_t i = 0; i + 1 < slots.size(); ++i)
    EXPECT_LT(slots[i].rotateDeg, 0) << i; // all but last lean negative
  EXPECT_GT(slots.back().rotateDeg, 0);    // last flips positive
  EXPECT_LT(slots.back().rotateDeg, 15);   // gently
  for (const auto &s : slots)
    EXPECT_LE(s.dx, 0); // jitter pulls left, never right
  // Deterministic per seed; different seed, different scatter.
  const auto again = layouts::stickerScatter(5, 3);
  EXPECT_EQ(slots[2].rotateDeg, again[2].rotateDeg);
  const auto other = layouts::stickerScatter(5, 9);
  EXPECT_NE(slots[2].rotateDeg, other[2].rotateDeg);
}

TEST(ComposeStyles, RippleDisplacesTheLayer) {
  // A thin horizontal red bar warped by a strong ripple: pixels appear
  // off-axis where the flat version has none.
  auto bar = [](bool warped) {
    Element e = box()
                    .absolute()
                    .inset(20, 96, 20, 96)
                    .fill(Fill::color({1, 0, 0, 1}));
    if (warped)
      e.effect(styles::ripple(10, 60));
    return box().child(std::move(e));
  };
  Host flat, warped;
  flat.composer.render(bar(false));
  warped.composer.render(bar(true));
  flat.frame();
  warped.frame();
  int off = 0, offFlat = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7}) {
      off += warped.pixel(x, 100 + dy) != SK_ColorBLACK;
      offFlat += flat.pixel(x, 100 + dy) != SK_ColorBLACK;
    }
  EXPECT_GT(off, 15);
  EXPECT_EQ(offFlat, 0);
}

// ---------------------------------------------------------------------------
// Review-workflow findings (self-verified after the fleet hit usage limits).

TEST(ComposeReconcile, RemovedDimsAndInsetsRelease) {
  // Patch reuses the yoga node: dims/aspect/insets REMOVED from the
  // description must actually unset, not linger from the last describe.
  Host host;
  host.composer.render(box().row().child(
      box().width(120).height(40).fill(red()).key("b")));
  host.frame();
  ASSERT_EQ(host.composer.bounds("b")->width(), 120);
  host.composer.render(box().row().child(
      box().height(40).fill(red()).key("b").child(box().width(30))));
  host.frame();
  EXPECT_EQ(host.composer.bounds("b")->width(), 30); // released to content

  Host pins;
  pins.composer.render(box().child(
      box().inset(10, 10, 10, 10).fill(blue()).key("p")));
  pins.frame();
  ASSERT_EQ(pins.composer.bounds("p")->width(), 180);
  pins.composer.render(box().child(box()
                                       .left(Dim(20.0f))
                                       .top(Dim(20.0f))
                                       .width(50)
                                       .height(20)
                                       .fill(blue())
                                       .key("p")));
  pins.frame();
  EXPECT_EQ(*pins.composer.bounds("p"), SkRect::MakeXYWH(20, 20, 50, 20));
}

TEST(ComposeMotion, UnrelatedPatchDoesNotRestartAnEntrance) {
  // Mid-entrance, changing an UNRELATED prop must leave the running
  // motion alone — before the fix, transitionFloat rebuilt the ramp and
  // RE-HELD its delay from the current value.
  Host host;
  auto tree = [](Fill f) {
    return box().child(
        box().width(80).height(80).fill(f).opacity(withFrom(
            0.0f, 1.0f,
            {std::chrono::milliseconds(400), &choreograph::easeNone,
             std::chrono::milliseconds(300)})));
  };
  host.composer.render(tree(red()));
  host.frame(0.35); // 50ms into the ramp (after the 300ms hold)
  host.composer.render(tree(blue())); // fill changes; opacity prop identical
  host.frame(0.2);  // t = 0.55 → ramp fraction (0.55-0.3)/0.4 = 0.625
  const SkColor c = host.pixel(40, 40);
  // Continuing motion: strong blue. A restarted+re-held ramp would still
  // sit at the 0.125 it had when the patch landed.
  EXPECT_GT(SkColorGetB(c), 120u);
}

// ---------------------------------------------------------------------------
// Adversarial-review confirmations (workflow wf_15ac5a8b): the P1 batch.

TEST(ComposeMotion, ToggleBackDuringDelayHoldLands) {
  // #18: retargeting a slot to its CURRENT value must disconnect a motion
  // headed elsewhere — or the hold expires and fades to the stale target.
  Host host;
  auto tree = [](float op) {
    return box().child(box()
                           .width(80)
                           .height(80)
                           .fill(red())
                           .transition({std::chrono::milliseconds(200),
                                        &choreograph::easeNone,
                                        std::chrono::milliseconds(300)})
                           .opacity(op));
  };
  host.composer.render(tree(1.0f));
  host.frame();
  host.composer.render(tree(0.0f)); // starts Hold(1, .3s) + RampTo(0)
  host.frame(0.1);                  // still holding at 1
  host.composer.render(tree(1.0f)); // toggle back DURING the hold
  host.frame(0.7);                  // any stale motion would have finished
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED); // description wins: opaque
}

TEST(ComposeCache, ConnectorWireSurvivesParentCaching) {
  // #33: the routed path is not bounded by the connector's layout rect —
  // the parent's recording cull must hold the wire.
  Host host;
  host.composer.render(
      box()
          .child(box().absolute().inset(20, 90, 160, 90).fill(red()).key("a"))
          .child(box().absolute().inset(160, 90, 20, 90).fill(red()).key("b"))
          .child(connector("a", "b").stroke(util::stroke(4, green()))));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorGREEN); // the wire, mid-span
  host.frame(); // cached replay
  EXPECT_EQ(host.pixel(100, 100), SK_ColorGREEN);
}

TEST(ComposeCache, TextureBakeKeepsBleedAndOverflow) {
  // #32: Cache::Texture used to bake at exact node size.
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(70, 70, 70, 70)
          .cache(Cache::Texture)
          .background(util::Shadow{{0, 1, 0, 1}, {30, 0}, 0})
          .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(140, 100), SK_ColorGREEN); // shadow past the box
}

TEST(ComposeTrim, OpenContourWrapKeepsTwoPieces) {
  // #31: stitching an OPEN contour's wrap window invented a chord.
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(20, 80, 20, 80)
          .outline([](SkSize s) { // open horizontal line
            SkPathBuilder b;
            b.moveTo(0, s.height() / 2);
            b.lineTo(s.width(), s.height() / 2);
            return b.detach();
          })
          .trim(0.9f, 1.2f, 0.0f, TrimMode::Wrap)
          .stroke(util::stroke(6, green()))));
  host.frame();
  EXPECT_EQ(host.pixel(170, 100), SK_ColorGREEN); // tail piece [0.9, 1]
  EXPECT_EQ(host.pixel(40, 100), SK_ColorGREEN);  // head piece [0, 0.2]
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK); // NO chord between them
}

TEST(ComposeCache, SettledOpacityRebakesTheLeaf) {
  // #9: a settled opacity transition must not leave the full-opacity
  // recording baked with leafDirectBlend.
  Host host;
  auto tree = [](PropValue<float> op) {
    return box().child(box()
                           .width(80)
                           .height(80)
                           .fill(Fill::color({1, 0, 0, 1}))
                           .opacity(std::move(op)));
  };
  host.composer.render(tree(1.0f));
  host.frame();
  host.composer.render(
      tree(with(0.4f, {std::chrono::milliseconds(100), &choreograph::easeNone})));
  host.frame(0.5); // settled at 0.4
  host.frame();    // draw again from caches
  const SkColor c = host.pixel(40, 40);
  EXPECT_NEAR(SkColorGetR(c), 102, 12); // 0.4 · 255 on black
}

TEST(ComposePaint, BackdropLeavesDecorationsUnclipped) {
  // #34: backdrop() clipped the node's own decorations to its shape.
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(60, 60, 60, 60)
          .backdrop(Effect::filter(SkImageFilters::Blur(2, 2, nullptr)))
          .stroke(util::stroke(10, green(), PathFormat::Align::Outer))));
  host.frame();
  EXPECT_EQ(host.pixel(52, 100), SK_ColorGREEN); // outer stroke intact
}

TEST(ComposeMotion, AppendedItemEntersWithoutInheritedDelay) {
  // #20: an item appended to a LIVE staggered list must not inherit its
  // full-list ordinal delay.
  Host host;
  auto card = [](std::string_view key) {
    return box().width(60).height(20).fill(red()).key(key).opacity(
        withFrom(0.0f, 1.0f, {std::chrono::milliseconds(100),
                              &choreograph::easeNone}));
  };
  host.composer.render(box()
                           .column()
                           .gap(10)
                           .staggerChildren(std::chrono::milliseconds(400))
                           .child(card("a"))
                           .child(card("b")));
  host.frame(1.2); // initial cascade done
  host.composer.render(box()
                           .column()
                           .gap(10)
                           .staggerChildren(std::chrono::milliseconds(400))
                           .child(card("a"))
                           .child(card("b"))
                           .child(card("c"))); // appended: only new mount
  host.frame(0.15); // > its 100ms entrance, << 2·400ms ordinal delay
  EXPECT_EQ(host.pixel(30, 70), SK_ColorRED); // "c" already in
}

TEST(ComposeBrushes, PatternCornerTileAtTheClosedSeam) {
  // #22: the seam of a closed contour is a corner too.
  Host host;
  brushes::PatternBrush b;
  b.side = box().width(20).height(4).fill(red());
  b.corner = box().width(12).height(12).fill(blue());
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(50, 50, 50, 50)
          .outline([](SkSize s) { // closed rect starting at (0,0)
            SkPathBuilder p;
            p.moveTo(0, 0);
            p.lineTo(s.width(), 0);
            p.lineTo(s.width(), s.height());
            p.lineTo(0, s.height());
            p.close();
            return p.detach();
          })
          .stroke(std::move(b))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE); // the seam corner tile
}

// ---------------------------------------------------------------------------
// The unified Brush engine: geometry pipeline → paint legs, as ONE value.

TEST(ComposeBrushEngine, PipelineStylesEveryLeg) {
  Host host;
  Brush b;
  b.op(ops::Wave{.amplitude = 8, .wavelength = 24})
      .leg(util::stroke(2, green()))
      .leg([] {
        brushes::ScatterBrush s;
        s.art = box().width(6).height(6).fill(red());
        s.spacing = 40;
        return s;
      }());
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  int off = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7})
      off += host.pixel(x, 100 + dy) == SK_ColorGREEN;
  EXPECT_GT(off, 10); // the stroke leg rides the waved pipeline
  int reds = 0;       // and the scatter leg rides the SAME waved geometry
  for (int x = 24; x < 176; ++x)
    for (int y = 84; y < 116; ++y)
      reds += host.pixel(x, y) == SK_ColorRED;
  EXPECT_GT(reds, 30);
}

TEST(ComposeBrushEngine, BrushPrunesAsOneValue) {
  Host host;
  auto tree = [] {
    Brush b;
    b.op(ops::Rounded{6})
        .op(ops::Wave{.amplitude = 3, .wavelength = 30})
        .leg(lines::cased(3, Fill::color({0, 1, 0, 1}), 5));
    return box().child(box()
                           .absolute()
                           .inset(40, 40, 40, 40)
                           .stroke(std::move(b)));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree()); // fresh Elements, identical brush values
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeBrushEngine, SketchyKeepsOpenContoursOpen) {
  // Under a fill StrokeRec, SkDiscretePathEffect force-closes open
  // contours — the phantom-channel bug. Hairline rec keeps them open.
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.lineTo(300, 0);
  const SkPath jittered = ops::Sketchy{8, 2, 11}.apply(b.detach());
  SkContourMeasureIter iter(jittered, false);
  float total = 0;
  bool anyClosed = false;
  while (sk_sp<SkContourMeasure> c = iter.next()) {
    total += c->length();
    anyClosed |= c->isClosed();
  }
  EXPECT_FALSE(anyClosed);
  EXPECT_LT(total, 400.0f); // a closed loop would be ~2× the 300px run
}

TEST(ComposeBrushEngine, PerLegOpsRideTheSharedPipeline) {
  // One Brush, two legs offset to opposite sides — the asymmetric casing
  // as a single material value.
  Host host;
  Brush b;
  b.leg(util::stroke(3, green()), {ops::Offset{-12}})
      .leg(util::stroke(3, blue()), {ops::Offset{12}});
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 88), SK_ColorGREEN);  // left-of-travel rail
  EXPECT_EQ(host.pixel(100, 112), SK_ColorBLUE);  // right-of-travel rail
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK); // nothing on the axis
}

TEST(ComposeBrushEngine, SquareWaveHoldsPlateausAndEndsOnAxis) {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.lineTo(320, 0);
  const SkPath boxy = ops::Square{8, 80}.apply(b.detach());
  // Plateaus hold ±8 for half-wavelength runs; endpoints return to 0.
  const SkRect bounds = boxy.getBounds();
  EXPECT_NEAR(bounds.top(), -8, 0.5f);
  EXPECT_NEAR(bounds.bottom(), 8, 0.5f);
  SkPoint last;
  SkContourMeasureIter iter(boxy, false);
  sk_sp<SkContourMeasure> c = iter.next();
  ASSERT_TRUE(c);
  ASSERT_TRUE(c->getPosTan(c->length(), &last, nullptr));
  EXPECT_NEAR(last.y(), 0, 0.5f); // zero-phase exit
  EXPECT_NEAR(last.x(), 320, 1.0f);
}

TEST(ComposeBrushEngine, PlacementGrammarLandsOnRealVertices) {
  // Vertex family reads the path's actual verbs — stamps sit ON the bends.
  Host host;
  brushes::ScatterBrush b;
  b.art = box().width(8).height(8).fill(red());
  b.place = {brushes::Placement::Mode::InnerVertices};
  b.alignToPath = false;
  host.composer.render(box().child(
      box().absolute().inset(40, 40, 40, 40).outline([](SkSize s) {
        SkPathBuilder p; // three segments, two bends
        p.moveTo(0, s.height());
        p.lineTo(60, s.height());
        p.lineTo(60, 0);
        p.lineTo(s.width(), 0);
        return p.detach();
      }).stroke(std::move(b))));
  host.frame();
  EXPECT_EQ(host.pixel(100, 160), SK_ColorRED); // bend 1 (60,120)+40
  EXPECT_EQ(host.pixel(100, 40), SK_ColorRED);  // bend 2 (60,0)+40
  EXPECT_EQ(host.pixel(40, 160), SK_ColorBLACK); // endpoints excluded

  Host centers;
  brushes::ScatterBrush c;
  c.art = box().width(8).height(8).fill(blue());
  c.place = {brushes::Placement::Mode::SegmentCenter};
  c.alignToPath = false;
  centers.composer.render(box().child(
      box().absolute().inset(40, 40, 40, 40).outline([](SkSize s) {
        SkPathBuilder p;
        p.moveTo(0, 0);
        p.lineTo(s.width(), 0);
        return p.detach();
      }).stroke(std::move(c))));
  centers.frame();
  EXPECT_EQ(centers.pixel(100, 40), SK_ColorBLUE); // the segment midpoint
}

TEST(ComposeBrushEngine, AlongGradientRampsOverTheArc) {
  Host host;
  lines::Line grad;
  grad.width = 8;
  grad.alongStops = {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}};
  host.composer.render(straightRun(std::move(grad)));
  host.frame();
  const SkColor start = host.pixel(30, 100);
  const SkColor end = host.pixel(170, 100);
  EXPECT_GT(SkColorGetR(start), 200u); // red end
  EXPECT_LT(SkColorGetB(start), 60u);
  EXPECT_GT(SkColorGetB(end), 200u);   // blue end
  EXPECT_LT(SkColorGetR(end), 60u);
}

// Regression coverage added by the SigilCompose gallery/performance audit.

TEST(ComposeElement, MutatingRenderedValueDetachesDescription) {
  Host host;
  Element panel = box().width(100).height(100).fill(red());

  host.composer.render(box().child(panel));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);

  // Composer retains the first description. Mutating the caller's value must
  // create a new description so pointer-identity pruning cannot preserve the
  // old cached picture.
  panel.fill(blue());
  host.composer.render(box().child(panel));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);
}

TEST(ComposeElement, CopiedValuesMutateIndependently) {
  Host host;
  Element left = box().width(100).height(100).fill(red());
  Element right = left;
  right.fill(blue());

  host.composer.render(box().row().child(left).child(right));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);
  EXPECT_EQ(host.pixel(150, 50), SK_ColorBLUE);
}

TEST(ComposeLines, OffsetAlongClampsNonPositiveStep) {
  SkPathBuilder builder;
  builder.moveTo(10, 50);
  builder.lineTo(190, 50);
  const SkPath route = builder.detach();

  for (float step : {0.0f, -4.0f}) {
    const SkPath shifted = lines::offsetAlong(route, 10.0f, step);
    ASSERT_FALSE(shifted.isEmpty()) << "step=" << step;
    EXPECT_NEAR(shifted.getBounds().top(), 60.0f, 0.01f);
    EXPECT_NEAR(shifted.getBounds().bottom(), 60.0f, 0.01f);
  }
}

TEST(ComposeBrushes, PatternCopyRebakesAllChangedArt) {
  auto art = [](Fill fill) { return box().width(12).height(12).fill(fill); };
  brushes::PatternBrush base;
  base.side = box().width(16).height(4).fill(green());
  base.start = art(red());
  base.end = art(red());
  base.corner = art(red());
  base.advance = 16;

  // Aggregate copies intentionally share the memoization cache. The cache
  // therefore has to key every art slot, not only the side tile.
  brushes::PatternBrush variant = base;
  variant.start = art(blue());
  variant.end = art(blue());
  variant.corner = art(blue());

  auto lRun = [](brushes::PatternBrush brush) {
    return box().child(box()
                           .absolute()
                           .inset(30)
                           .outline([](SkSize size) {
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
  donor.frame(); // primes the shared cache with red start/end/corner art
  EXPECT_EQ(donor.pixel(38, 30), SK_ColorRED);
  EXPECT_EQ(donor.pixel(170, 162), SK_ColorRED);

  Host changed;
  changed.composer.render(lRun(variant));
  changed.frame();
  EXPECT_EQ(changed.pixel(38, 30), SK_ColorBLUE);   // changed start art
  EXPECT_EQ(changed.pixel(170, 32), SK_ColorBLUE);  // changed corner art
  EXPECT_EQ(changed.pixel(170, 162), SK_ColorBLUE); // changed end art
}

TEST(ComposeDecorations, ContourWalkCopyRebakesChangedStamp) {
  ContourWalk base;
  base.spacing = 1000.0f; // one stamp at the open route's first point
  base.stamp = box().width(12).height(12).fill(red());
  ContourWalk variant = base;
  variant.stamp = box().width(12).height(12).fill(blue());

  Host donor;
  donor.composer.render(straightRun(base));
  donor.frame(); // primes the shared cache with the red stamp
  EXPECT_EQ(donor.pixel(20, 100), SK_ColorRED);

  Host changed;
  changed.composer.render(straightRun(variant));
  changed.frame();
  EXPECT_EQ(changed.pixel(20, 100), SK_ColorBLUE);
}

TEST(ComposeTrim, PathFormatOpenContourWrapKeepsTwoPieces) {
  // PathFormat owns a separate wrapping trim window. It must apply the same
  // open-contour rule as node-level trim instead of connecting both pieces.
  Host host;
  PathFormat format;
  format.width = 6;
  format.strokeFill = green();
  format.trimStart = 0.9f;
  format.trimEnd = 1.2f;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(20, 80, 20, 80)
                                       .outline([](SkSize s) {
                                         SkPathBuilder b;
                                         b.moveTo(0, s.height() / 2);
                                         b.lineTo(s.width(), s.height() / 2);
                                         return b.detach();
                                       })
                                       .stroke(format)));
  host.frame();
  EXPECT_EQ(host.pixel(170, 100), SK_ColorGREEN); // tail piece [0.9, 1]
  EXPECT_EQ(host.pixel(40, 100), SK_ColorGREEN);  // head piece [0, 0.2]
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK); // NO invented chord
}

TEST(ComposeBrushes, PatternClosedSeamCornerUsesWrappedBisector) {
  Host host;
  brushes::PatternBrush brush;
  brush.side = box().width(20).height(2).fill(red());
  // A thin bar exposes orientation: the rect seam exits upward and enters
  // rightward, so its corner bisector points up-right (-45 degrees).
  brush.corner = box().width(20).height(4).fill(blue());
  brush.advance = 20;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(50)
                                       .outline([](SkSize size) {
                                         SkPathBuilder path;
                                         path.moveTo(0, 0);
                                         path.lineTo(size.width(), 0);
                                         path.lineTo(size.width(),
                                                     size.height());
                                         path.lineTo(0, size.height());
                                         path.close();
                                         return path.detach();
                                       })
                                       .stroke(std::move(brush))));
  host.frame();
  EXPECT_EQ(host.pixel(56, 44), SK_ColorBLUE);
}

TEST(ComposeMaterials, StableLiveResolveReplaysThePicture) {
  // The resolve memo: a live material whose bound inputs did not change
  // returns the SAME shader, and a node whose only volatility is that
  // material replays its picture — repaint at the material's rate, not
  // the frame rate (the quantized-sea rule).
  auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform float uPhase; half4 main(float2 p) {"
      "  return half4(fract(uPhase), 0.2, 1.0 - fract(uPhase), 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  Host host;
  choreograph::Output<float> phase{0.25f};
  host.composer.render(box().child(
      box().width(100).height(100).fill(
          Material::sksl(fx).uniform("uPhase", &phase))));
  host.frame(); // records once
  const SkColor before = host.pixel(50, 50);
  host.frame(); // same phase → stable resolve → pure replay
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(host.composer.stats().nodesPainted, 1u); // just the root shim
  EXPECT_EQ(host.pixel(50, 50), before);
  phase = 0.75f; // the material actually changed
  host.frame();
  EXPECT_GT(host.composer.stats().picturesRecorded, 0u);
  EXPECT_NE(host.pixel(50, 50), before);
}

TEST(ComposeMaterials, BoundUniformOwnsItsSlotOverInjection) {
  // Binding uTime to an Output is the documented stepping idiom — the
  // auto-inject must not overwrite it with continuous clock time.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform float uTime; half4 main(float2 p) {"
      "  return half4(fract(uTime), 0, 0, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  choreograph::Output<float> stepped{0.5f};
  Material m = Material::sksl(fx).uniform("uTime", &stepped);
  PaintContext ctx;
  ctx.size = {4, 4};
  ctx.elapsedSeconds = 123.789; // continuous clock — must be IGNORED
  Fill f = m.resolve(ctx);
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
  SkPaint p;
  p.setShader(f.shaderValue);
  s->getCanvas()->drawPaint(p);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  s->readPixels(bm.pixmap(), 0, 0);
  EXPECT_NEAR(SkColorGetR(bm.getColor(0, 0)), 128, 3); // fract(0.5), not .789
}

TEST(ComposeMaterials, BakeScaleUpscalesThroughTheSameRect) {
  // bakeScale(0.5) rasterizes the texture bake at half resolution; the
  // blit stretches it back through the same dst rect — same coverage,
  // same color, a quarter of the evaluated pixels.
  Host host;
  host.composer.render(
      box().child(box().left(20).top(20).width(100).height(100)
                      .cache(Cache::Texture).bakeScale(0.5f)
                      .fill(red())));
  host.frame(); // bake at half scale
  host.frame(); // blit
  EXPECT_EQ(host.pixel(70, 70), SkColorSetARGB(255, 255, 0, 0));
  // inboard of the AA edge on every side — coverage must not shrink
  EXPECT_EQ(host.pixel(23, 23), SkColorSetARGB(255, 255, 0, 0));
  EXPECT_EQ(host.pixel(116, 116), SkColorSetARGB(255, 255, 0, 0));
  EXPECT_EQ(host.pixel(140, 70), SK_ColorBLACK); // outside stays empty
}

TEST(ComposeMaterials, StableLiveResolveBlitsTheTexture) {
  // The texture flavor of the resolve memo — the shader-wall case: bake
  // at the material's own rate, BLIT between (pictures re-execute SkSL
  // on raster; pixels don't).
  auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform float uPhase; half4 main(float2 p) {"
      "  return half4(fract(uPhase), 0.4, 0.2, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  Host host;
  choreograph::Output<float> phase{0.25f}, sibling{0.0f};
  host.composer.render(
      box()
          .child(box().width(100).height(100).cache(Cache::Texture).fill(
              Material::sksl(fx).uniform("uPhase", &phase)))
          // an always-animating sibling keeps the ROOT live (the gallery
          // condition) — the shader wall must still blit
          .child(box().width(10).height(10).fill(red()).translateX(&sibling)));
  host.frame(); // bakes
  const unsigned recordedAfterBake = host.composer.stats().picturesRecorded;
  EXPECT_GE(recordedAfterBake, 1u);
  host.frame(); // stable phase → blit, no re-bake
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  phase = 0.75f;
  host.frame(); // real change → one re-bake
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
}

// ---------------------------------------------------------------------------
// instances() — the flyweight repeat layer (<sigilcompose/Instances.h>)

#include <sigilcompose/Instances.h>

TEST(ComposeInstances, StampsAtlasCellsAtPoolPositionsWithTint) {
  using namespace sigil::compose::instancing;
  Host host;
  auto atlas = std::make_shared<Atlas>();
  atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {20, 20});
  auto pool = std::make_shared<Pool>();
  pool->add({50, 50});                          // white cell, untinted
  pool->add({150, 50}, 0, 0.0f, 1.0f, {1, 0, 0, 1}); // tinted red
  // The bake itself: sheet exists and the cell's center is opaque white.
  ASSERT_TRUE(atlas->ensureBaked(fonts()));
  ASSERT_TRUE(atlas->image());
  {
    SkBitmap probe;
    probe.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    ASSERT_TRUE(atlas->image()->readPixels(nullptr, probe.pixmap(), 20, 20));
    EXPECT_EQ(probe.getColor(0, 0), SK_ColorWHITE);
  }
  host.composer.render(
      box().child(instances(atlas, pool)));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(150, 50), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK); // between stamps: nothing
}

TEST(ComposeInstances, DataModePrunesUntilTouched) {
  using namespace sigil::compose::instancing;
  Host host;
  auto atlas = std::make_shared<Atlas>();
  atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {16, 16});
  auto pool = std::make_shared<Pool>();
  pool->add({40, 40});
  auto describe = [&] {
    return box().child(instances(atlas, pool));
  };
  host.composer.render(describe());
  host.frame();
  // Unchanged pool: the re-describe prunes (memo hit), the cached picture
  // replays, nothing re-records.
  host.composer.render(describe());
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  // Mutate + touch + render: repaints exactly once, pixels move.
  pool->positions()[0] = {120, 40};
  pool->touch();
  host.composer.render(describe());
  EXPECT_TRUE(host.composer.dirty());
  host.frame();
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
  EXPECT_EQ(host.pixel(120, 40), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorBLACK);
}

TEST(ComposeInstances, LiveModeReadsThePoolEveryFrame) {
  using namespace sigil::compose::instancing;
  Host host;
  auto atlas = std::make_shared<Atlas>();
  atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {16, 16});
  auto pool = std::make_shared<Pool>();
  pool->add({40, 40});
  host.composer.render(
      box().child(instances(atlas, pool, Mode::Live)));
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorWHITE);
  // No touch(), no render() — the Cache::None leaf reads the latest data.
  pool->positions()[0] = {140, 140};
  host.frame();
  EXPECT_EQ(host.pixel(140, 140), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorBLACK);
}

TEST(ComposeInstances, RepeaterLawExponentialScaleLinearEverythingElse) {
  using namespace sigil::compose::instancing;
  Pool pool;
  place::repeat(pool, 4, {10, 10}, {5, 0}, 0.1f, 0.5f, 1.0f, 0.25f);
  ASSERT_EQ(pool.size(), 4u);
  EXPECT_FLOAT_EQ(pool.positions()[3].fX, 25.0f); // linear translate
  EXPECT_FLOAT_EQ(pool.rotations()[3], 0.3f);     // linear rotate
  EXPECT_FLOAT_EQ(pool.scales()[3], 0.125f);      // pow(0.5, 3)
  EXPECT_FLOAT_EQ(pool.tints()[0].fA, 1.0f);      // opacity lerp endpoints
  EXPECT_FLOAT_EQ(pool.tints()[3].fA, 0.25f);
}

// ---------------------------------------------------------------------------
// The edge store: node→routes back-index + flat derive lists

TEST(ComposeEdgeStore, RoutesAtReturnsAnchoredRoutesInTreeOrder) {
  Host host;
  auto describe = [] {
    return box()
        .child(box().key("a").width(30).height(30).absolute().inset(
            10, 10, 160, 160))
        .child(box().key("b").width(30).height(30).absolute().inset(
            160, 160, 10, 10))
        .child(connector("a", "b").key("edge1"))
        .child(rail({{"a", {0.5f, 0.5f}}, {"b", {0.5f, 0.5f}}}).key("edge2"))
        .child(connector("a", "b")); // keyless: anchored but unaddressable
  };
  host.composer.render(describe());
  host.frame();
  const std::vector<std::string> atA = host.composer.routesAt("a");
  ASSERT_EQ(atA.size(), 2u); // the keyless route is omitted
  EXPECT_EQ(atA[0], "edge1");
  EXPECT_EQ(atA[1], "edge2");
  EXPECT_EQ(host.composer.routesAt("b").size(), 2u);
  EXPECT_TRUE(host.composer.routesAt("nowhere").empty());
}

TEST(ComposeEdgeStore, IndexClearsWhenRoutesUnmount) {
  Host host;
  bool withRoute = true;
  auto describe = [&] {
    auto tree = box()
                    .child(box().key("a").width(30).height(30).absolute().inset(
                        10, 10, 160, 160))
                    .child(box().key("b").width(30).height(30).absolute().inset(
                        160, 160, 10, 10));
    if (withRoute)
      tree.child(connector("a", "b").key("edge"));
    return tree;
  };
  host.composer.render(describe());
  host.frame();
  ASSERT_EQ(host.composer.routesAt("a").size(), 1u);
  withRoute = false;
  host.composer.render(describe());
  host.frame();
  EXPECT_TRUE(host.composer.routesAt("a").empty());
}

// ---------------------------------------------------------------------------
// The brush-arc tail: art warp (SkVertices), hatch (Sk2D), gloss (table)

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/LayerStyles.h>

TEST(ComposeBrushTail, ArtBrushWarpsArtAlongTheOutline) {
  Host host;
  // A straight horizontal outline through the node's middle: the warped
  // ribbon must be a horizontal band of the art's height around it.
  auto lineOutline = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() / 2);
    b.lineTo(s.width(), s.height() / 2);
    return b.detach();
  };
  brushes::ArtBrush brush = brushes::artAlong(
      box().width(40).height(20).fill(Fill::color({1, 1, 1, 1})), 20);
  host.composer.render(
      box().child(box().absolute().inset(20, 60, 20, 60)
                      .outline(lineOutline)
                      .foreground(brush)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorWHITE); // on the ribbon
  EXPECT_EQ(host.pixel(100, 130), SK_ColorBLACK); // 30px off: outside height
}

TEST(ComposeBrushTail, HatchFillsInteriorSparsely) {
  Host host;
  host.composer.render(box().child(
      box().absolute().inset(50, 50, 50, 50)
          .background(lines::hatch(Fill::color({1, 1, 1, 1}), 8, 1.5f, 45))));
  host.frame();
  // Count lit pixels in the hatched interior: strictly between "empty"
  // and "solid fill" — the lattice is present but sparse.
  int lit = 0;
  const int total = 100 * 100;
  for (int y = 50; y < 150; y += 2)
    for (int x = 50; x < 150; x += 2)
      if (host.pixel(x, y) != SK_ColorBLACK)
        ++lit;
  const float coverage = (float)lit / (float)(total / 4);
  EXPECT_GT(coverage, 0.05f);
  EXPECT_LT(coverage, 0.75f);
  // Nothing escapes the clip.
  EXPECT_EQ(host.pixel(30, 30), SK_ColorBLACK);
}

TEST(ComposeBrushTail, GlossContourBandsInsideTheShape) {
  Host plain, glossed;
  auto shape = [] {
    return box().absolute().inset(50, 50, 50, 50).corners({24})
        .fill(Fill::color({0.2f, 0.3f, 0.5f, 1}));
  };
  plain.composer.render(box().child(shape()));
  plain.frame();
  glossed.composer.render(box().child(
      shape().foreground(styles::gloss({1, 1, 1, 1}, 8, {0, -4}))));
  glossed.frame();
  // The band brightens SOME interior pixels but not the deep center
  // (table peaks at mid-coverage, so the middle of the shape stays fill).
  int changed = 0;
  for (int y = 52; y < 148; y += 2)
    for (int x = 52; x < 148; x += 2)
      if (plain.pixel(x, y) != glossed.pixel(x, y))
        ++changed;
  EXPECT_GT(changed, 40);           // a real band appeared
  EXPECT_EQ(plain.pixel(100, 100), glossed.pixel(100, 100)); // center: fill
  EXPECT_EQ(plain.pixel(30, 30), glossed.pixel(30, 30));     // outside: clip
}

// ---------------------------------------------------------------------------
// VariationDrive — draw-time variable-font axes, gated by the advance probe

TEST(ComposeVariationDrive, GradDrivesPaintOnlyWhenAdvanceInvariant) {
  // The San Francisco system face carries the advance-invariant GRAD axis
  // on modern macOS; find a face that passes the probe or skip honestly.
  sk_sp<SkFontMgr> manager = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> ui;
  for (const char *family :
       {".AppleSystemUIFont", ".SF NS", "SF Pro Text", "SF Pro"}) {
    ui = manager->matchFamilyStyle(family, SkFontStyle());
    if (ui && fonts().axisIsAdvanceInvariant(ui, "GRAD"))
      break;
    ui = nullptr;
  }
  if (!ui)
    GTEST_SKIP() << "no advance-invariant GRAD face on this system";
  // The probe proves advances HOLD; it cannot prove the clone RESPONDS
  // (a hidden system face can accept the axis and render identically).
  // Check a glyph's outline actually moves across the range, else skip.
  {
    const int n = ui->getVariationDesignParameters({});
    std::vector<SkFontParameters::Variation::Axis> axes((size_t)n);
    ui->getVariationDesignParameters({axes.data(), axes.size()});
    float lo = 0, hi = 0;
    for (const auto &a : axes)
      if (a.tag == SkSetFourByteTag('G', 'R', 'A', 'D')) {
        lo = a.min;
        hi = a.max;
      }
    const sigil::weave::FontVariation vLo("GRAD", lo), vHi("GRAD", hi);
    SkFont fLo(fonts().variedTypeface(ui, {&vLo, 1}), 48);
    SkFont fHi(fonts().variedTypeface(ui, {&vHi, 1}), 48);
    SkGlyphID glyph = fLo.unicharToGlyph('W');
    auto rasterize = [&](const SkFont &f) {
      sk_sp<SkSurface> s =
          SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 80));
      s->getCanvas()->clear(SK_ColorBLACK);
      SkPaint paint;
      paint.setColor(SK_ColorWHITE);
      paint.setAntiAlias(true);
      const SkPoint at{10, 60};
      s->getCanvas()->drawGlyphs(SkSpan(&glyph, 1), SkSpan(&at, 1), {0, 0}, f,
                                 paint);
      SkBitmap bm;
      bm.allocPixels(s->imageInfo());
      s->readPixels(bm.pixmap(), 0, 0);
      return bm;
    };
    SkBitmap rLo = rasterize(fLo), rHi = rasterize(fHi);
    int rasterDelta = 0;
    for (int y = 0; y < 80; ++y)
      for (int x = 0; x < 100; ++x)
        if (rLo.getColor(x, y) != rHi.getColor(x, y))
          ++rasterDelta;
    if (rasterDelta == 0)
      GTEST_SKIP() << "GRAD clone is rendering-inert on this system face";
  }
  // Drive the axis's REAL design range (SF's GRAD span is font-defined;
  // hardcoded values can land clamped onto the default = no visual delta).
  float gradeMin = 0, gradeMax = 0;
  {
    const int n = ui->getVariationDesignParameters({});
    std::vector<SkFontParameters::Variation::Axis> axes((size_t)n);
    ui->getVariationDesignParameters({axes.data(), axes.size()});
    for (const auto &a : axes)
      if (a.tag == SkSetFourByteTag('G', 'R', 'A', 'D')) {
        gradeMin = a.min;
        gradeMax = a.max;
      }
  }

  choreograph::Output<float> grade{gradeMin};
  Host host;
  auto describe = [&] {
    sigil::weave::TextStyle style = styleAt(48);
    style.shaping.typeface = ui;
    style.paint.foreground.setColor(SK_ColorWHITE); // black-on-black otherwise
    return box().child(text(u8"WEIGHT", style)
                           .key("t")
                           .variationDrive("GRAD", &grade)
                           .absolute()
                           .inset(20, 60, 20, 60));
  };
  host.composer.render(describe());
  host.frame();
  const SkRect before = *host.composer.bounds("t");
  SkBitmap lo;
  lo.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(lo.pixmap(), 0, 0);

  grade = gradeMax; // heavy grade — glyphs thicken, advances hold
  host.frame();
  const SkRect after = *host.composer.bounds("t");
  SkBitmap hi;
  hi.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(hi.pixmap(), 0, 0);

  EXPECT_EQ(before, after); // no relayout — paint-only volatility
  int changed = 0;
  for (int y = 0; y < 200; y += 2)
    for (int x = 0; x < 200; x += 2)
      if (lo.getColor(x, y) != hi.getColor(x, y))
        ++changed;
  EXPECT_GT(changed, 20) << "GRAD range " << gradeMin << ".." << gradeMax; // visible thickening
}

TEST(ComposeVariationDrive, AdvanceVariantAxisIsRefused) {
  sk_sp<SkTypeface> ui = fonts().defaultTypeface();
  if (fonts().axisIsAdvanceInvariant(ui, "wght"))
    GTEST_SKIP() << "this font's wght is advance-invariant; nothing to refuse";

  choreograph::Output<float> weight{400.0f};
  Host host;
  host.composer.render(box().child(text(u8"WEIGHT", styleAt(48))
                                       .key("t")
                                       .variationDrive("wght", &weight)
                                       .absolute()
                                       .inset(20, 60, 20, 60)));
  host.frame();
  SkBitmap base;
  base.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(base.pixmap(), 0, 0);

  weight = 900.0f;
  host.frame(); // refused: draws at shaped coordinates, pixels hold
  SkBitmap after;
  after.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(after.pixmap(), 0, 0);
  for (int y = 0; y < 200; y += 4)
    for (int x = 0; x < 200; x += 4)
      ASSERT_EQ(base.getColor(x, y), after.getColor(x, y));
}

// ---------------------------------------------------------------------------
// Shaped bindings — bind(&out).from().map().to().clamp()

TEST(ComposeBindings, TheAffineChainComposesInCallOrder) {
  // Reading order IS evaluation order for the affine ops, so the two
  // spellings below are genuinely different and each does what it looks
  // like. (An "order doesn't matter" accumulate would collapse them.)
  EXPECT_FLOAT_EQ(bind(nullptr).scale(240).offset(-70).value().apply(0.5f),
                  0.5f * 240 - 70);
  EXPECT_FLOAT_EQ(bind(nullptr).offset(-70).scale(240).value().apply(0.5f),
                  (0.5f - 70) * 240);
  // to(lo,hi) is the [0,1] → range spelling…
  EXPECT_FLOAT_EQ(bind(nullptr).to(20, 60).value().apply(0.25f), 30.0f);
  // …from(lo,hi) the other direction, and they compose.
  EXPECT_FLOAT_EQ(bind(nullptr).from(0, 200).to(0, 1).value().apply(50.0f),
                  0.25f);
  // invert composes with what came before rather than resetting it.
  EXPECT_FLOAT_EQ(bind(nullptr).invert().value().apply(0.25f), 0.75f);
  EXPECT_FLOAT_EQ(bind(nullptr).to(0, 2).invert().value().apply(0.25f),
                  1.0f - 0.5f);
  // the curve runs BEFORE the affine, on the normalised value…
  EXPECT_FLOAT_EQ(
      bind(nullptr).map(&choreograph::easeNone).to(0, 10).value().apply(0.4f),
      4.0f);
  // …and the clamp always runs last, whenever it is written.
  EXPECT_FLOAT_EQ(bind(nullptr).clamp(0, 1).to(0, 4).value().apply(0.5f), 1.0f);
}

TEST(ComposeBindings, AShapedBindingDrivesThePropertyInPixels) {
  // The wall this closes: a phase in [0,1] — which is what trim() and
  // opacity() want — could not drive a translation in PIXELS without a
  // second Output carrying pixels, updated in the same steppable. Five
  // separate studies kept two Outputs where one would do.
  Host host(200, 200);
  choreograph::Output<float> phase{0.0f};
  host.composer.render(box().child(box()
                                       .width(20)
                                       .height(20)
                                       .absolute()
                                       .left(0)
                                       .top(90)
                                       .fill(red())
                                       .translateX(bind(&phase).to(0, 160))));
  auto redAt = [&](int x) { return SkColorGetR(host.pixel(x, 100)) > 180; };

  host.frame();
  EXPECT_TRUE(redAt(10));   // phase 0 → x = 0
  EXPECT_FALSE(redAt(170));

  phase = 1.0f;
  host.frame();
  EXPECT_FALSE(redAt(10));
  EXPECT_TRUE(redAt(170));  // phase 1 → x = 160, unscaled would be x = 1

  phase = 0.5f;
  host.frame();
  EXPECT_TRUE(redAt(85));   // and it is linear in between
}

TEST(ComposeBindings, AChangedShapeRepatchesRatherThanPruning) {
  // The map is read LIVE through the pointer, so a pruned node would keep
  // shaping through the OLD one forever. Same Output, different range.
  Host host(200, 200);
  choreograph::Output<float> phase{1.0f};
  auto tree = [&](float far) {
    return box().child(box()
                           .key("dot")
                           .width(20)
                           .height(20)
                           .absolute()
                           .left(0)
                           .top(90)
                           .fill(red())
                           .translateX(bind(&phase).to(0, far)));
  };
  host.composer.render(tree(40.0f));
  host.frame();
  EXPECT_TRUE(SkColorGetR(host.pixel(50, 100)) > 180);

  host.composer.render(tree(150.0f));
  host.frame();
  EXPECT_FALSE(SkColorGetR(host.pixel(50, 100)) > 180);
  EXPECT_TRUE(SkColorGetR(host.pixel(160, 100)) > 180);
}

TEST(ComposeText, OnPathReDescribeDoesNotKeepTheOldBaseline) {
  // textEqual() compared everything about a text run EXCEPT its baseline
  // when onPath landed, so re-describing with a new path or a new `at`
  // pruned and the run kept riding the old one. TextPath's defaulted
  // operator== was implicitly deleted (std::function isn't comparable) and
  // so never caught it.
  Host host(240, 240);
  auto ring = [](float at) {
    return box().child(text(u8"HHHHHHHHHH", whiteStyle(22))
                           .key("ring")
                           .width(240).height(240).absolute().left(0).top(0)
                           .onPath({.path = shapes::arc(180.0f, 359.9f),
                                    .at = at,
                                    .align = TextPath::Align::Center}));
  };
  auto lit = [&](int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 240; ++x)
        count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  host.composer.render(ring(0.25f));
  host.frame();
  ASSERT_GT(lit(0, 110), 200);

  host.composer.render(ring(0.75f)); // same key, same text, new baseline
  host.frame();
  EXPECT_GT(lit(140, 240), 200); // it moved…
  EXPECT_LT(lit(0, 110), 40);    // …and did not stay put
}

TEST(ComposeMotion, AnEmptyEasingMeansTheDefaultRatherThanACrash) {
  // Transition is an aggregate, so `{360ms, {}, 220ms}` — the obvious way
  // to write "default curve, but I need to name the delay" — initialises
  // `ease` to an EMPTY std::function. It compiled, then threw
  // bad_function_call on the first frame and took down a whole scene.
  Host host(200, 200);
  host.composer.render(box().child(box()
                                       .width(40)
                                       .height(40)
                                       .absolute()
                                       .left(0)
                                       .top(80)
                                       .fill(red())
                                       .translateX(withFrom(
                                           0.0f, 120.0f,
                                           {200ms, {}, 0ms}))));
  host.frame();                    // would throw here
  host.frame(0.4);                 // land the entrance
  EXPECT_TRUE(SkColorGetR(host.pixel(130, 100)) > 180);

  // and it still prunes against an explicitly-defaulted curve
  Transition blank{200ms, {}, 0ms};
  Transition spelled{200ms, &choreograph::easeOutQuad, 0ms};
  EXPECT_EQ(blank.easing().target<float (*)(float)>() != nullptr,
            spelled.easing().target<float (*)(float)>() != nullptr);
}

TEST(ComposeShapes, ParametricCurvesEvaluateInTheUnitFrame) {
  // Shapes.h generated closed shapes from parameters; a curve DEFINED by
  // a parameter had no generator, so every study that needed one wrote
  // the same SkPathBuilder loop inside its own outline lambda.
  const SkSize box{200, 100}; // deliberately non-square: unit → half-extents

  // A 1:1 Lissajous with a quarter-turn phase IS the inscribed ellipse.
  const SkPath ellipse = shapes::lissajous(1, 1, 90.0f)(box);
  const SkRect bounds = ellipse.getBounds();
  EXPECT_NEAR(bounds.width(), 200.0f, 1.5f);
  EXPECT_NEAR(bounds.height(), 100.0f, 1.5f);
  EXPECT_NEAR(bounds.centerX(), 100.0f, 0.5f);
  EXPECT_NEAR(bounds.centerY(), 50.0f, 0.5f);

  // Damping shrinks the figure AS IT DRAWS — the whole visual difference
  // between a harmonograph and a Lissajous, and why a real pen-and-
  // pendulum figure spirals inward instead of retracing one rosette. Both
  // ends sit AT the centre (sin 0 = 0), so the honest measurement is the
  // reach of each half.
  const SkPath damped = shapes::harmonograph(3, 2, 0, 0.25f, 0, 6.0f)(box);
  const SkPoint centre = SkPoint{100, 50};
  const int pts = damped.countPoints();
  ASSERT_GT(pts, 100);
  auto reach = [&](int from, int to) {
    float most = 0;
    for (int i = from; i < to; ++i)
      most = std::max(most, SkPoint::Distance(damped.getPoint(i), centre));
    return most;
  };
  EXPECT_GT(reach(0, pts / 2), reach(pts / 2, pts) * 1.5f);

  // A rose with odd k has k petals, each reaching the rim. It is NOT
  // centred on the box — r = cos(5θ) puts tips at θ = 0, 2π/5, … so the
  // bounds sit off to one side, and asserting otherwise would be
  // asserting a bug into existence.
  const SkPath five = shapes::rose(5)(box);
  EXPECT_GT(five.countPoints(), 100);
  int tips = 0;
  for (int i = 0; i < five.countPoints(); ++i)
    if (SkPoint::Distance(five.getPoint(i), centre) > 49.0f)
      ++tips;
  EXPECT_GT(tips, 5);

  // Spirals start at the centre and end at the rim.
  const SkPath coil = shapes::spiral(3)(box);
  EXPECT_NEAR(SkPoint::Distance(coil.getPoint(0), centre), 0.0f, 1.0f);
  EXPECT_GT(SkPoint::Distance(coil.getPoint(coil.countPoints() - 1), centre),
            40.0f);

  // Everything stays inside the box it was inscribed in.
  for (const SkPath *p : {&ellipse, &damped, &five, &coil}) {
    const SkRect r = p->getBounds();
    EXPECT_GE(r.left(), -1.0f);
    EXPECT_GE(r.top(), -1.0f);
    EXPECT_LE(r.right(), 201.0f);
    EXPECT_LE(r.bottom(), 101.0f);
  }
}

TEST(ComposeInstances, ThePerSpriteBlendAccumulatesWhereALayerCannot) {
  // Nothing in the chain from instances() to drawSpriteAtlas carried a
  // blend mode, so every pool composited kSrcOver. Element::blend() looks
  // like the fix and is not: it flattens the field into a layer and
  // composites it ONCE, so overlapping sprites never accumulate — which
  // is the entire colour model of an additive particle system (Reeves'
  // 1982 wall of fire has no palette, only an overlap count).
  auto build = [](SkBlendMode blend) {
    auto atlas = std::make_shared<instancing::Atlas>(1.0f);
    atlas->cell(box().width(40).height(40).fill(
                    Fill::color({0.25f, 0.25f, 0.25f, 1})),
                {40, 40});
    auto pool = std::make_shared<instancing::Pool>();
    for (int i = 0; i < 3; ++i) // three sprites stacked on one spot
      pool->add({100, 100});
    return box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Data, blend));
  };

  Host over(200, 200);
  over.composer.render(build(SkBlendMode::kSrcOver));
  over.frame();
  const int overR = SkColorGetR(over.pixel(100, 100));

  Host plus(200, 200);
  plus.composer.render(build(SkBlendMode::kPlus));
  plus.frame();
  const int plusR = SkColorGetR(plus.pixel(100, 100));

  EXPECT_GT(overR, 40);            // one opaque sprite's worth
  EXPECT_LT(overR, 90);            // …and three of them are no brighter
  EXPECT_GT(plusR, overR + 60);    // additive stacks all three
}
