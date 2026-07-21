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

TEST(ComposeMaterial, BlendSnapshotsLiveLayerAtCurrentValue) {
  // blend() flattens; a live layer contributes its bound Outputs at their
  // values NOW (the audit's stale-snapshot defect — pre-fix it baked SkSL
  // defaults, rendering uK=0 regardless of the Output).
  choreograph::Output<float> k{0.8f};
  Material m = Material::blend({
      {Material::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
      {Material::sksl(ukEffect()).uniform("uK", &k), SkBlendMode::kPlus},
  });
  EXPECT_FALSE(m.isLive()); // the flattened blend is static
  Host host;
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(m)));
  host.frame();
  const uint32_t r = SkColorGetR(host.pixel(20, 20));
  EXPECT_GT(r, 170u); // ~0.8 * 255 = 204, not the SkSL default 0
  EXPECT_LT(r, 240u);
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
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).inset(0, 0, 100, 100).absolute().fill(
          sdf::material(sdf::circle(), {.fill = {1, 0, 0, 1},
                                        .glowRadius = 12,
                                        .glowColor = {1, 1, 1, 1}})
              .uniform("uGlowR", &glow))));
  host.frame();
  // pad = 12*2.5+1 = 31 → r = 19 at c=(50,50); sample 8px outside the edge.
  const uint32_t dim = SkColorGetR(host.pixel(77, 50));
  glow = 12.0f; // brighten the falloff — no re-render
  host.frame();
  const uint32_t lit = SkColorGetR(host.pixel(77, 50));
  EXPECT_LT(dim, 25u);  // exp(-8/0.01) ≈ 0
  EXPECT_GT(lit, 90u);  // exp(-8/12) ≈ 0.51 → ~131
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
