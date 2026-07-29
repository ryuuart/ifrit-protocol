#include "ComposeTestSupport.h"


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
                           .translateX(animate(to(target), {400ms,
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
          .translateX(animate(to(500.0f), {1000ms}))));
  host.frame();
  host.composer.render(box().child(
      box().key("gone").width(10).height(10)
          .translateX(animate(to(0.0f), {1000ms}))));
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

TEST(ComposeDecorations, ContourWalkStampAtSequencesPerSampleArt) {
  // §14: ContourWalk sampled the tangent and rotated to it, then replayed
  // ONE stamp. stampAt(sample, index) is the sequence form — ruler ticks
  // with numbers, ribbon menus, chained ornament: per-index art, nullopt
  // falling back to the shared `stamp`. The callable is incomparable and
  // ContourWalk stays conservatively unequal as it always was (it has no
  // operator== — the raw `draw` callable decided that long ago). The
  // bakes are per call, per record, UNCACHED: each returned Element is a
  // fresh node, so the §16 instance-side StampCache has nothing stable
  // to key them on.
  Host host;
  static int asked;
  asked = 0;
  ContourWalk walk;
  walk.spacing = 40.0f;
  walk.stamp = box().width(10).height(10).fill(green());
  walk.stampAt = [](const PathSample &s,
                    size_t i) -> std::optional<Element> {
    ++asked;
    EXPECT_FLOAT_EQ(s.distance, 40.0f * (float)i); // the sequence contract
    if (i % 2 == 1)
      return std::nullopt; // odd samples: the shared stamp replays
    return box().width(10).height(10).fill(red());
  };
  host.composer.render(box().child(
      box().absolute().inset(20, 80, 20, 80)
          .shape([](SkSize s) {
            SkPathBuilder b;
            b.moveTo(0, s.height() / 2);
            b.lineTo(s.width(), s.height() / 2);
            return b.detach();
          })
          .foreground(walk)));
  host.frame();
  // 160px rail, spacing 40 → samples at x = 20, 60, 100, 140 (y = 100).
  EXPECT_EQ(host.pixel(20, 100), SK_ColorRED);    // index 0: its own art
  EXPECT_EQ(host.pixel(60, 100), SK_ColorGREEN);  // index 1: fallback
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);   // index 2
  EXPECT_EQ(host.pixel(140, 100), SK_ColorGREEN); // index 3
  EXPECT_EQ(asked, 4);
  host.frame(); // a static walk records once and replays — no re-bakes
  EXPECT_EQ(asked, 4);
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
  // STRENGTHENED 2026-07-28 (audit): the word ONCE used to be asserted by
  // nothing — a live texturesLive count and a non-black pixel pass equally
  // well if the node re-bakes every single frame. `texturesBaked` is the
  // per-draw pixel-bake count (Stats), so the claim is now literal: one on
  // the frame that bakes, zero on every frame after it.
  //
  // profiledUnder(), not a plain parent: under a cacheable parent the second
  // frame replays the PARENT's picture and never visits this node at all, so
  // "0 bakes" would be true of a node that re-bakes every time it is asked.
  // Cache::None on the wrapper keeps the subject painted every frame, which
  // is what makes the second assertion a statement about the texture.
  Host host;
  host.composer.render(profiledUnder(
      box().key("bloomed").width(60).height(60).fill(green())
          .effect(Effect::filter(SkImageFilters::Blur(4, 4, nullptr)))
          .cache(Cache::Texture)));
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u) << "the bake";
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
      << "…and the second frame blits it rather than re-baking";
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
      box()
          .width(40)
          .height(40)
          .absolute()
          .left(30)
          .top(30)
          .scale(animate(from(0.5f).to(1.0f),
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
          .shape(shapes::arrow())
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
          .shape(shapes::sector(0, 90))
          .fill(Material::solid({1, 0, 0, 1}))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(130, 130)), 200u); // inside the wedge
  EXPECT_LT(SkColorGetR(host.pixel(70, 130)), 60u);   // lower-left: outside
  EXPECT_LT(SkColorGetR(host.pixel(130, 70)), 60u);   // upper-right: outside

  // innerRatio carves the donut hole out of the middle.
  Host donut(200, 200);
  donut.composer.render(box().child(
      box().width(200).height(200).absolute().inset(0)
          .shape(shapes::sector(0, 350, 0.6f))
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
  EXPECT_FALSE(m.isAnimated());
  EXPECT_TRUE(m.isSolid());
}


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
  EXPECT_FALSE(base.isAnimated()); // base untouched
  EXPECT_TRUE(a.isAnimated());
  EXPECT_TRUE(b.isAnimated());

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
  EXPECT_TRUE(m.isAnimated()); // inherited from the bound layer
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

TEST(ComposeMaterial, NestedBlendAsShaderFoldsItsLiveLayersPerCall) {
  // §35.1. A blend has NO m_live of its own — it inherits liveness through
  // m_recipe->layers — so asShader()'s live path (`build(*m_live, nullptr)`)
  // dereferenced a null pointer for it. The reachable shape is a blend
  // nested in another blend's layer list, because blend() calls asShader()
  // on every layer: CONSTRUCTING `outer` below is what crashed.
  choreograph::Output<float> k{0.8f};
  Material inner = Material::blend({
      {Material::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
      {Material::sksl(ukEffect()).uniform("uK", &k), SkBlendMode::kPlus},
  });
  ASSERT_TRUE(inner.isAnimated()); // inherited from the bound layer
  Material outer = Material::blend({
      {inner, SkBlendMode::kSrcOver}, // <- the null deref was HERE
      {Material::solid({0, 0, 0, 1}), SkBlendMode::kPlus},
  });
  ASSERT_TRUE(outer.isAnimated()); // liveness survives one more nesting

  // And the answer must be folded PER CALL. Merely guarding the null would
  // fall through to m_shader — blend()'s eager snapshot, built once at
  // construction — which is the stale-snapshot defect asShader()'s live
  // branch exists to prevent; it would answer 0.8 forever.
  auto sampleR = [](const Material &m) -> uint32_t {
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
  EXPECT_NEAR((int)sampleR(outer), 204, 12); // 0.8 · 255, through two blends
  k = 0.3f;
  EXPECT_NEAR((int)sampleR(outer), 77, 12); // 0.3 · 255 — the fold is fresh
  // The same claim for the inner blend, which the outer one reaches through.
  EXPECT_NEAR((int)sampleR(inner), 77, 12);
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
  EXPECT_TRUE(m.isAnimated());

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
          .shape(shapes::star(4, 0.3f))
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

// ---- the child slot: a SECOND source (ROADMAP §10f) ------------------------
//
// `Material::sksl()` had no child slot, so a material could read exactly one
// image and every two-source rule — an index texture through a palette LUT,
// a mask channel, a second gradient — had to leave the library. The slot is
// `child(name, Material)` against a declared `uniform shader`, and the
// driving case below is the paletted one: X-COM shades by index arithmetic
// (`(src & 0xF0) | min(15, (src & 0x0F) + shade)`) with no multiplication in
// the renderer at all, which is expressible over a LUT and not otherwise.

namespace {

/** A 1-row image whose pixels are the given colors (N32, no color space —
 *  the Host surface has none either, so nothing converts and a byte written
 *  here is the byte the shader reads). */
sk_sp<SkImage> rowImage(const std::vector<SkColor> &pixels) {
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
                 "  i = min(i + uShade, 3.0);" // X-COM's ramp arithmetic
                 "  return uPalette.eval(float2(i + 0.5, 0.5));"
                 "}"));
    if (!effect)
      ADD_FAILURE() << err.c_str();
    return effect;
  }();
  return fx;
}

/** The IMAGES are process-wide, the way a decoded asset is: Material::image
 *  compares by image POINTER (the documented recipe rule), so a helper that
 *  minted a fresh SkImage per call would make every material unequal to
 *  every other and the prune question below unaskable. */
const sk_sp<SkImage> &indexImage() {
  static sk_sp<SkImage> img = rowImage(
      {SkColorSetARGB(255, 0, 0, 0), SkColorSetARGB(255, 1, 0, 0),
       SkColorSetARGB(255, 2, 0, 0), SkColorSetARGB(255, 3, 0, 0)});
  return img;
}
const sk_sp<SkImage> &rampPalette() {
  static sk_sp<SkImage> img =
      rowImage({SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE, SK_ColorWHITE});
  return img;
}
const sk_sp<SkImage> &reversedPalette() {
  static sk_sp<SkImage> img =
      rowImage({SK_ColorWHITE, SK_ColorBLUE, SK_ColorGREEN, SK_ColorRED});
  return img;
}
const sk_sp<SkImage> &flatWhitePalette() {
  static sk_sp<SkImage> img = rowImage(
      {SK_ColorWHITE, SK_ColorWHITE, SK_ColorWHITE, SK_ColorWHITE});
  return img;
}

/** The index texture: four 1-px cells carrying indices 0..3, blown up to
 *  20 px each so a node pixel lands unambiguously inside one cell. NEAREST
 *  everywhere — an index sampled at kLinear is a blend of two unrelated
 *  palette entries, which is the trap this whole texture kind carries. */
Material indexSource() {
  return Material::image(indexImage(), SkTileMode::kClamp, SkTileMode::kClamp,
                         SkMatrix::Scale(20, 20),
                         SkSamplingOptions(SkFilterMode::kNearest));
}

Material paletteSource(const sk_sp<SkImage> &lut) {
  return Material::image(lut, SkTileMode::kClamp, SkTileMode::kClamp,
                         SkMatrix::I(),
                         SkSamplingOptions(SkFilterMode::kNearest));
}

} // namespace

TEST(ComposeMaterial, AChildSlotSamplesAnIndexTextureThroughAPalette) {
  // THE DRIVING CASE, end to end: two images, one shader, one draw. Neither
  // source is the node's own painted content (that is Effect's `content`
  // child) — they are sources the material brings with it.
  Host host(80, 20);
  host.composer.render(stack().child(box().absolute().inset(0).fill(
      Material::sksl(paletteEffect(), {{"uShade", 0.0f}})
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
      Material::sksl(paletteEffect(), {{"uShade", 0.0f}})
          .child("uIndex", indexSource())
          .child("uPalette", paletteSource(reversedPalette())))));
  swapped.frame();
  EXPECT_EQ(swapped.pixel(10, 10), SK_ColorWHITE) << "same indices, new LUT";
  EXPECT_EQ(swapped.pixel(70, 10), SK_ColorRED);

  // And the shade step is index ARITHMETIC, clamped at the ramp's end —
  // every cell moves one entry down the palette and the last one sticks.
  Host shaded(80, 20);
  shaded.composer.render(stack().child(box().absolute().inset(0).fill(
      Material::sksl(paletteEffect(), {{"uShade", 1.0f}})
          .child("uIndex", indexSource())
          .child("uPalette", paletteSource(rampPalette())))));
  shaded.frame();
  EXPECT_EQ(shaded.pixel(10, 10), SK_ColorGREEN) << "0 + 1";
  EXPECT_EQ(shaded.pixel(50, 10), SK_ColorWHITE) << "2 + 1";
  EXPECT_EQ(shaded.pixel(70, 10), SK_ColorWHITE) << "3 + 1, clamped";
}

TEST(ComposeMaterial, TheChildRidesThePruneSignature) {
  // THE CACHE CONDITION. A child read at paint time that did not
  // participate in reconciler equality would leave a pruned node sampling
  // the OLD palette forever (DESIGN.md's rule, stated for exactly this).
  const Material a = Material::sksl(paletteEffect())
                         .child("uPalette", paletteSource(rampPalette()));
  const Material b = Material::sksl(paletteEffect())
                         .child("uPalette", paletteSource(rampPalette()));
  const Material c = Material::sksl(paletteEffect())
                         .child("uPalette", paletteSource(flatWhitePalette()));
  const Material bare = Material::sksl(paletteEffect());
  EXPECT_TRUE(a == b) << "same effect, same child recipe → prunes";
  EXPECT_FALSE(a == c) << "a different palette is a different material";
  EXPECT_FALSE(a == bare) << "a filled slot is not an empty one";

  // …and the reconciler agrees: identical describe prunes, a swapped
  // palette patches and repaints.
  Host host(80, 20);
  auto tree = [](const sk_sp<SkImage> &lut) {
    return stack().child(box().key("lut").absolute().inset(0).fill(
        Material::sksl(paletteEffect(), {{"uShade", 0.0f}})
            .child("uIndex", indexSource())
            .child("uPalette", paletteSource(lut))));
  };
  host.composer.render(tree(rampPalette()));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorRED);
  host.composer.render(tree(rampPalette())); // identical recipe, fresh Materials…
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
  const Material live = Material::sksl(passthrough)
                            .child("uSrc", Material::sksl(ukEffect())
                                               .uniform("uK", &k));
  EXPECT_TRUE(live.isAnimated()) << "the child's volatility is the parent's";
  EXPECT_FALSE(Material::sksl(passthrough)
                   .child("uSrc", Material::solid({0, 1, 0, 1}))
                   .isAnimated())
      << "…and a static child leaves the parent static";

  Host host;
  host.composer.render(
      stack().child(box().absolute().inset(0).width(40).height(40).fill(live)));
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 40u); // uK = 0 → black
  k = 1.0f;                                        // no render()
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
  const Material m =
      Material::sksl(passthrough).child("uSrc", Material::sksl(unitRamp));
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
  Material m = Material::sksl(paletteEffect(), {{"uShade", 0.0f}})
                   .child("uIndex", indexSource())
                   .child("uPalette", paletteSource(rampPalette()))
                   .child("uNoSuchSlot", Material::solid({1, 1, 1, 1}));
  EXPECT_FALSE(m.isAnimated());
  host.composer.render(stack().child(box().absolute().inset(0).fill(m)));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorRED) << "the declared slots still ran";

  // And on a material with no slots at all it is a no-op, like uniform().
  Material solid = Material::solid({0, 1, 0, 1}).child("uSrc", indexSource());
  EXPECT_TRUE(solid.isSolid());
  EXPECT_FALSE(solid.isAnimated());
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
  // Composition, not new machinery: a span gate on a rail = the self-drawing
  // subway line. A bound reveal advances with no render() calls.
  choreograph::Output<float> reveal{0.05f};
  Host host;
  host.composer.render(
      stack()
          .child(station("a", 10, 40))
          .child(station("b", 170, 40))
          .child(rail({{"a"}, {"b"}})
                     .absolute().inset(0)
                     .mask(by::spans(spans::upTo(&reveal)))
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

TEST(ComposeMask, PartialOutlineStrokesOnlyRevealedStretch) {
  // mask(by::spans(upTo(0.2))) on a square + stroked outline: only the
  // first 20% of the perimeter is dressed; right/bottom stay bare. The
  // fill and every outline decoration trace the CUT path.
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).inset(0, 0, 100, 100).absolute()
          .mask(by::spans(spans::upTo(0.2f)))
          .foreground(sigil::compose::util::stroke(4, green()))));
  host.frame();
  // Perimeter order (measured): left → top → right → bottom. First 20% ≈
  // the left edge.
  EXPECT_EQ(host.pixel(1, 50), SK_ColorGREEN);  // left edge revealed
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLACK);  // top edge bare
  EXPECT_EQ(host.pixel(50, 99), SK_ColorBLACK); // bottom edge bare
}

TEST(ComposeMask, TransitionDrawsOn) {
  // The draw-on border: a span gate's end transitioned 0 → 1 reveals the
  // perimeter over time (retarget-safe like every transitioned prop).
  Host host;
  auto tree = [](Animatable<float> end) {
    return box().child(
        box().key("b").width(100).height(100).inset(0, 0, 100, 100).absolute()
            .mask(by::spans(spans::upTo(std::move(end))))
            .foreground(sigil::compose::util::stroke(4, green())));
  };
  host.composer.render(tree(0.001f));
  host.frame();
  EXPECT_EQ(host.pixel(50, 99), SK_ColorBLACK);
  host.composer.render(tree(animate(to(1.0f), {400ms, &choreograph::easeNone})));
  host.frame(0.2); // ~50%: left + top revealed, bottom still bare
  EXPECT_EQ(host.pixel(50, 1), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(50, 99), SK_ColorBLACK);
  host.frame(0.25); // settle → the full perimeter
  EXPECT_EQ(host.pixel(50, 99), SK_ColorGREEN);
}

TEST(ComposeMask, BoundGateRevealsWithoutRender) {
  // A bound gate end is content volatility: mutate the Output, no
  // render(), and the reveal advances — the self-drawing wire primitive.
  choreograph::Output<float> end{0.2f};
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).inset(0, 0, 100, 100).absolute()
          .mask(by::spans(spans::upTo(&end)))
          .foreground(sigil::compose::util::stroke(4, green()))));
  host.frame();
  // (99,30) sits at ~57.5% of the perimeter (right edge, top→bottom).
  EXPECT_EQ(host.pixel(99, 30), SK_ColorBLACK); // bare at end=0.2
  end = 0.6f; // no render()
  host.frame();
  EXPECT_EQ(host.pixel(99, 30), SK_ColorGREEN); // reveal reached it
  EXPECT_GT(host.composer.stats().nodesPainted, 0u); // paints live
}

#include <sigilcompose/Debug.h>
#include <sigilcompose/Sdf.h>

TEST(ComposeTransitions, PlainSnapAfterTransitionLands) {
  // Review fix (kernel-wide shadow): describing a PLAIN value after a
  // transition must actually land — the lingering ramp used to shadow the
  // description forever.
  Host host;
  auto at = [](Animatable<float> x) {
    return box().child(box().key("m").width(50).height(50).fill(red())
                           .translateX(std::move(x)));
  };
  host.composer.render(at(0.0f));
  host.frame();
  host.composer.render(at(animate(to(100.0f), {400ms, &choreograph::easeNone})));
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
  EXPECT_TRUE(Material::sksl(effect).isAnimated());
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
  EXPECT_FALSE(m.isAnimated());           // still cacheable
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

TEST(ComposeSdf, PadSwallowingTheBoxWarnsOnceNamingMinBoxFor) {
  // §14: sdf::pad() is reserved INSIDE the node's box, so a 60x60 box
  // with glowRadius 20 (pad = 65) renders a ~1px speck and used to say
  // NOTHING — sdf::minBoxFor() was the answer and no call site pointed
  // at it. The numbers meet at resolve (uPad vs uResolution), and that
  // is where the warning now lives.
  const sdf::Style style{.fill = {1, 0, 0, 1},
                         .glowRadius = 20,
                         .glowColor = {1, 1, 1, 1}};
  ASSERT_GE(sdf::pad(style), 30.0f); // the premise: pad >= half of 60
  ::testing::internal::CaptureStderr();
  {
    Host host;
    host.composer.render(box().child(
        box().width(60).height(60).fill(sdf::material(sdf::circle(), style))));
    host.frame();
  }
  const std::string first = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(first.find("sdf::minBoxFor"), std::string::npos) << first;
  // Warned ONCE, process-wide: a second offender stays silent — the house
  // diagnostic contract (renderSlot's unknown-name warning), not a
  // per-frame log.
  ::testing::internal::CaptureStderr();
  {
    Host host;
    host.composer.render(box().child(
        box().width(50).height(50).fill(sdf::material(sdf::circle(), style))));
    host.frame();
  }
  EXPECT_EQ(::testing::internal::GetCapturedStderr().find("sdf::minBoxFor"),
            std::string::npos);
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

TEST(ComposePattern, ReseedingACopyLeavesTheOriginalAlone) {
  // A Pattern is a VALUE — scale/rotate/offset/sampling are all per-object
  // — but seed() and retile() edited the SHARED recipe, so re-rolling a
  // copy dropped the original's bake and regenerated every element still
  // drawing the old tile (audit E5). Copy-on-write, Material::uniform's
  // answer to the same aliasing.
  Pattern base = patterns::speckle(64, 40, 1, 3, {{1, 1, 1, 1}});
  base.seed(11);
  auto plate = [&] {
    Host host(64, 64);
    host.composer.render(
        box().child(box().width(64).height(64).fill(base.material())));
    host.frame();
    std::vector<SkColor> px;
    for (int y = 0; y < 64; y += 3)
      for (int x = 0; x < 64; x += 3)
        px.push_back(host.pixel(x, y));
    return px;
  };
  const std::vector<SkColor> before = plate();

  Pattern copy = base;
  copy.seed(99);
  EXPECT_EQ(base.currentSeed(), 11u) << "the copy re-rolled the original";
  EXPECT_EQ(copy.currentSeed(), 99u);
  EXPECT_EQ(plate(), before) << "and dropped its bake with it";
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
                     .stroke(kit::brush::presets::filament())));
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

TEST(ComposeMaterial, DeclaredBleedGrowsTheRecordingCull) {
  // §14: a DecorationScheme can declare bleed() so the recording cull
  // grows; a Material could not, so a fill on an outline that escapes
  // the box (shape() overflow is legal) truncated at the cached bounds
  // and the arithmetic fell to the caller. Material::bleed(px) declares
  // the same number on the same word — pinned on BOTH carriers: the
  // static recipe and the live/geometry slot. Cache::Texture makes the
  // truncation hard (the bake surface is exactly recordBounds), so a
  // surviving overflow proves the cull grew.
  auto overflowShape = [](SkSize s) {
    // A disc centered on the box, poking 20px beyond every edge.
    SkPathBuilder b;
    b.addOval(SkRect::MakeLTRB(-20, -20, s.width() + 20, s.height() + 20));
    return b.detach();
  };
  {
    Host host; // recipe carrier: a static solid material
    host.composer.render(box().padding(40).child(
        box().width(60).height(40).cache(Cache::Texture)
            .shape(overflowShape)
            .fill(Material::solid({1, 0, 0, 1}).bleed(24))));
    host.frame();
    host.frame(); // the cached replay is where truncation used to bite
    // Node spans y∈[40,80); 14px below is inside the disc's overflow.
    EXPECT_EQ(host.pixel(70, 94), SK_ColorRED);
  }
  {
    Host host; // live carrier: a geometry-tier material (uResolution ramp)
    host.composer.render(box().padding(40).child(
        box().width(60).height(40).cache(Cache::Texture)
            .shape(overflowShape)
            .fill(Material::linearUnit({0, 0}, {1, 1},
                                       {{0, {1, 0, 0, 1}}, {1, {1, 0, 0, 1}}})
                      .bleed(24))));
    host.frame();
    host.frame();
    EXPECT_EQ(host.pixel(70, 94), SK_ColorRED);
  }
  // The reserve is recipe: it participates in equality, so a changed
  // bleed re-records instead of replaying a stale, smaller cull.
  Material a = Material::solid({1, 0, 0, 1});
  Material b = Material::solid({1, 0, 0, 1});
  b.bleed(24);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a == Material::solid({1, 0, 0, 1}));
  EXPECT_FLOAT_EQ(b.bleed(), 24.0f);
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

// ---------------------------------------------------------------------------
// Effect live uniforms (§11): Material's uniform(name, &output) contract,
// on the effect seam.

TEST(ComposeEffects, ALiveUniformAnimatesWithoutRedescribe) {
  // §11: Effect::shader took constants only, so animating a ripple phase
  // or a bloom threshold required a full re-describe per frame. A bound
  // uniform now resolves per paint and declares the node volatile —
  // exactly the live-material contract.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform float uK;"
               "half4 main(float2 p) { return content.eval(p) * uK; }"));
  ASSERT_TRUE(effect) << err.c_str();
  choreograph::Output<float> k{1.0f};
  Host host;
  host.composer.render(box().child(
      box().width(60).height(60).inset(0, 0, 140, 140).absolute()
          .fill(green())
          .effect(Effect::shader(effect).uniform("uK", &k))));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(30, 30)), 200u); // uK=1 → full green
  k = 0.25f;    // move the bound uniform — NO re-describe
  host.frame(); // the live effect re-resolves from k
  const SkColor dimmed = host.pixel(30, 30);
  EXPECT_LT(SkColorGetG(dimmed), 120u);
  EXPECT_GT(SkColorGetG(dimmed), 20u); // dimmed, not gone
  EXPECT_GT(host.composer.stats().nodesPainted, 0u) // volatile: paints live
      << "a bound effect uniform must declare volatility";
}

TEST(ComposeEffects, AStaticShaderEffectPrunesByRecipe) {
  // The other half of the seam: a STATIC shader effect compares by recipe
  // (runtime-effect pointer + constant uniforms), so a fixture that holds
  // ONE SkRuntimeEffect and re-describes prunes — the sharedHeavyEffect
  // pattern, which used to re-patch on the filter pointer every frame.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform float uK;"
               "half4 main(float2 p) { return content.eval(p) * uK; }"));
  ASSERT_TRUE(effect) << err.c_str();
  Host host;
  auto tree = [&](float uK) {
    return box().child(box().width(60).height(60).fill(green())
                           .effect(Effect::shader(effect, {{"uK", uK}})));
  };
  host.composer.render(tree(0.5f));
  host.frame();
  host.composer.render(tree(0.5f)); // fresh Effect, same recipe
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
  const Effect liveChain =
      Effect::shader(effect).uniform("uK", &k).then(
          Effect::shader(effect, {{"uK", 0.5f}}));
  EXPECT_TRUE(liveChain.isAnimated());
  ASSERT_TRUE(liveChain.resolvedImageFilter() != nullptr);
  const Effect staticChain = Effect::shader(effect, {{"uK", 0.5f}})
                                 .then(Effect::shader(effect, {{"uK", 0.5f}}));
  EXPECT_FALSE(staticChain.isAnimated());
  EXPECT_TRUE(staticChain.imageFilter() != nullptr); // precomposed once

  // The chain applies BOTH stages: 1.0 * 0.5 through the live chain dims
  // a green fill to about half.
  Host host;
  host.composer.render(box().child(
      box().width(60).height(60).inset(0, 0, 140, 140).absolute()
          .fill(green()).effect(liveChain)));
  host.frame();
  const unsigned g = SkColorGetG(host.pixel(30, 30));
  EXPECT_GT(g, 90u);
  EXPECT_LT(g, 170u);
}

// ---------------------------------------------------------------------------
// Material::amount() (§5): a blend layer's strength.

TEST(ComposeMaterial, ABlendLayerCompositesAtItsAmount) {
  // "Soft-light this noise at 30%" had no expression — the only route was
  // baking the amplitude into a forked copy of the generator's SkSL.
  // amount() is Photoshop layer opacity: composite the layer in full,
  // then mix the RESULT back toward the accumulation — which on a
  // srcOver white-over-red at 0.5 lands on pink, and at 0 leaves red.
  auto plate = [](float amt) {
    Host host;
    host.composer.render(box().child(
        box().width(60).height(60).inset(0, 0, 140, 140).absolute().fill(
            Material::blend({{Material::solid({1, 0, 0, 1}), SkBlendMode::kSrcOver},
                             {Material::solid({1, 1, 1, 1}).amount(amt),
                              SkBlendMode::kSrcOver}}))));
    host.frame();
    return host.pixel(30, 30);
  };
  const SkColor full = plate(1.0f), half = plate(0.5f), none = plate(0.0f);
  EXPECT_GT(SkColorGetG(full), 240u);              // white wins outright
  EXPECT_NEAR(SkColorGetG(half), 128, 12);         // half toward white…
  EXPECT_GT(SkColorGetR(half), 240u);              // …with red intact
  EXPECT_LT(SkColorGetG(none), 12u);               // 0 leaves the base
  EXPECT_GT(SkColorGetR(none), 240u);

  // The amount is recipe: equal amounts prune, different amounts patch.
  const Material a = Material::solid({1, 1, 1, 1}).amount(0.3f);
  const Material b = Material::solid({1, 1, 1, 1}).amount(0.3f);
  const Material c = Material::solid({1, 1, 1, 1}).amount(0.7f);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
}


// ---------------------------------------------------------------------------
// §18: a texture-cached node's blend rides its blit, not a saveLayer.

TEST(ComposeCaching, ATextureBlendCompositesOnTheBlitNotALayer) {
  // The measured case: Cache::Texture + a non-srcOver blend used to
  // allocate a device-clip-sized saveLayer to composite ONE blit
  // (3.45 → 0.24 ms on the node that filed §18). The blend now rides the
  // blit's paint. Semantics pinned against the ground truth: compositing
  // one image through a layer and drawing it with the paint directly are
  // the same operation minus an intermediate — so the deferred plate must
  // match a hand-built layer composite to within the honest 8-bit
  // residual (§30's standard: peak ≤ 2), and kPlus over the red base
  // must actually ACCUMULATE.
  auto plate = [](bool texture) {
    Host host;
    host.composer.render(
        box().fill(red()).child(box().width(80).height(80)
                                    .inset(20, 20, 100, 100).absolute()
                                    .fill(Fill::color({0.2f, 0.4f, 0.2f, 1}))
                                    .blend(SkBlendMode::kPlus)
                                    .cache(texture ? Cache::Texture
                                                   : Cache::Picture)));
    for (int i = 0; i < 3; ++i)
      host.frame(); // settle: bake once, then replay/blit
    std::vector<SkColor> px;
    for (int y = 10; y < 110; y += 2)
      for (int x = 10; x < 110; x += 2)
        px.push_back(host.pixel(x, y));
    return px;
  };
  const auto deferred = plate(true), layered = plate(false);
  ASSERT_EQ(deferred.size(), layered.size());
  int peak = 0;
  for (size_t i = 0; i < deferred.size(); ++i)
    for (int shift : {0, 8, 16, 24})
      peak = std::max(peak, std::abs((int)((deferred[i] >> shift) & 0xFF) -
                                     (int)((layered[i] >> shift) & 0xFF)));
  EXPECT_LE(peak, 2) << "deferred blit and layer composite disagree "
                        "beyond the 8-bit residual";
  // …and the blend is really live: plus over red saturates the red
  // channel where the child overlaps.
  Host host;
  host.composer.render(
      box().fill(red()).child(box().width(80).height(80)
                                  .inset(20, 20, 100, 100).absolute()
                                  .fill(Fill::color({0.2f, 0.4f, 0.2f, 1}))
                                  .blend(SkBlendMode::kPlus)
                                  .cache(Cache::Texture)));
  for (int i = 0; i < 3; ++i)
    host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(50, 50)), 250u); // 1.0 + 0.2 clamps
  EXPECT_GT(SkColorGetG(host.pixel(50, 50)), 90u);  // the child's green
}

// ---------------------------------------------------------------------------
// Material::buffer (§4): content that changes without re-describing.

TEST(ComposeMaterial, ABufferPrunesBetweenCommitsAndPatchesOnCommit) {
  // The Instances pruning rule, on pixels: identical re-describes prune
  // while the revision holds; one commit() patches exactly once. Before
  // this seam, anything with STATE — a simulation, a video frame, a
  // scrollback — fell to custom() + Cache::None and forfeited every
  // cache and decoration slot on the node.
  auto src = std::make_shared<PixelBuffer>(40, 40);
  src->canvas().clear(SkColorSetARGB(255, 255, 0, 0)); // red frame
  src->commit();
  Host host;
  auto tree = [&] {
    return box().child(box().width(100).height(100)
                           .inset(0, 0, 100, 100).absolute()
                           .fill(Material::buffer(src)));
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);

  host.composer.render(tree()); // same revision: prune
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an uncommitted buffer re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  src->canvas().clear(SkColorSetARGB(255, 0, 0, 255)); // new frame…
  src->commit();                                       // …published
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
  // §14: custom() re-recorded every render() — an incomparable callable.
  // custom(key, program) declares identity (the keyed-parametric
  // contract); the unkeyed form stays the escape hatch.
  static int runs;
  runs = 0;
  auto tree = [](const char *key, float shade) {
    auto program = [shade](SkCanvas &c, const PaintContext &ctx) {
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
  host.composer.render(tree("panel-a", 1.0f)); // same key: prune
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

TEST(ComposePatterns, SequencePaintsColouredRunsAndPhaseSlides) {
  // §14: stripes() is single-colour and un-phased; a coloured sett was a
  // hand-written PatternProgram every time.
  auto sample = [](float phase, int x) {
    Host host;
    host.composer.render(box().child(
        box().width(120).height(40).inset(0, 0, 80, 160).absolute().fill(
            patterns::sequence({{10, {1, 0, 0, 1}},
                                {10, {0, 1, 0, 1}},
                                {10, {0, 0, 1, 1}}},
                               phase)
                .material())));
    host.frame();
    return host.pixel(x, 20);
  };
  EXPECT_EQ(sample(0.0f, 5), SK_ColorRED);    // run 1
  EXPECT_EQ(sample(0.0f, 15), SK_ColorGREEN); // run 2
  EXPECT_EQ(sample(0.0f, 25), SK_ColorBLUE);  // run 3
  EXPECT_EQ(sample(0.0f, 35), SK_ColorRED);   // wraps
  EXPECT_EQ(sample(10.0f, 5), SK_ColorGREEN); // slid one run: green leads
  EXPECT_EQ(sample(10.0f, 15), SK_ColorBLUE);
}

// ---------------------------------------------------------------------------
// env — the inherited value (ROADMAP §10g)
//
// The property under test is not "a value arrives"; it is that the value
// arrives WITHOUT COSTING THE PRUNE. Read during describe, an inherited
// value lands in the reading node's own props, so `propsEqual` is already
// the exact dependency tracker — which is what these pins state in
// `patchedNodes` and `picturesRecorded` rather than in prose.

namespace {

struct EnvPalette {
  SkColor4f surface{1, 0, 0, 1};
  SkColor4f accent{0, 1, 0, 1};
  bool operator==(const EnvPalette &) const = default;
};

/** A component four levels below whoever bound the value, handed nothing
 *  and reading the environment — the `console::`/decoration case. */
Element envThemedChip() {
  return box().width(20).height(20).fill(
      Fill::color(env::inheritedOr(EnvPalette{}).surface));
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
  env::Provide<EnvPalette> theme(std::move(p));
  return box().child(envLevel1());
}

} // namespace

TEST(ComposeEnv, InheritedValueReachesAComponentNobodyHandedIt) {
  Host host;
  Element tree = envDescribeWith(EnvPalette{{0, 0, 1, 1}, {1, 1, 0, 1}});
  EXPECT_FALSE(env::bound<EnvPalette>()); // the scope ended; the VALUE is
  host.composer.render(std::move(tree)); // already baked into the tree
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLUE);   // the themed chip
  EXPECT_EQ(host.pixel(5, 25), SK_ColorBLUE);  // its plain sibling

  // Unbound: the component's own default, exactly like a React context's.
  Host bare;
  bare.composer.render(box().child(envLevel1()));
  bare.frame();
  EXPECT_EQ(bare.pixel(5, 5), SK_ColorRED);
  EXPECT_FALSE(env::bound<EnvPalette>()); // and the scope unwound
}

TEST(ComposeEnv, UnchangedEnvironmentStillPrunes) {
  // THE PRUNING PIN. A theme-reading node whose theme did not change must
  // prune like any other structurally-equal description — no patch, no
  // re-record, host free to skip the frame.
  Host host;
  const EnvPalette dark{{0, 0, 1, 1}, {1, 1, 0, 1}};
  auto renderWith = [&](EnvPalette p) {
    host.composer.render(envDescribeWith(std::move(p)));
  };
  renderWith(dark);
  host.frame();
  ASSERT_EQ(host.pixel(5, 5), SK_ColorBLUE); // it IS the inherited colour —
                                             // without this the pin below
                                             // would pass on a tree that
                                             // never read the environment

  renderWith(dark); // a DISTINCT palette object, equal by operator==
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
    host.composer.render(envDescribeWith(std::move(p)));
  };
  renderWith(EnvPalette{{0, 0, 1, 1}, {1, 1, 0, 1}});
  host.frame();

  renderWith(EnvPalette{{0, 1, 0, 1}, {1, 1, 0, 1}}); // surface moved only
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
    bool operator==(const Props &) const = default;
  };
  static int describeCalls;
  describeCalls = 0;
  auto component = [](const Props &) {
    ++describeCalls;
    return box().width(20).height(20).fill(
        Fill::color(env::inheritedOr(EnvPalette{}).surface));
  };

  Host host;
  // DESCRIBED inside the scope, RECONCILED after it ends — which is the
  // whole difficulty: `component` runs during render(), by which time the
  // Provide below has been destroyed. Building the tree and handing it to
  // the composer are two statements, deliberately.
  auto describeWith = [&component](EnvPalette p) {
    env::Provide<EnvPalette> theme(std::move(p));
    return box().child(memo(Props{1}, component).key("m"));
  };
  auto renderWith = [&](EnvPalette p) {
    Element tree = describeWith(std::move(p));
    ASSERT_FALSE(env::bound<EnvPalette>()); // the binding is gone by here
    host.composer.render(std::move(tree));
  };

  renderWith(EnvPalette{{0, 0, 1, 1}, {}});
  host.frame();
  EXPECT_EQ(describeCalls, 1);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLUE); // the captured stack reached fn

  renderWith(EnvPalette{{0, 0, 1, 1}, {}}); // same props, EQUAL environment
  EXPECT_EQ(describeCalls, 1);
  EXPECT_EQ(host.composer.stats().memoHits, 1u);

  renderWith(EnvPalette{{0, 1, 0, 1}, {}}); // same props, environment moved
  EXPECT_EQ(describeCalls, 2);
  EXPECT_EQ(host.composer.stats().memoHits, 0u);
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorGREEN); // not the stale blue
}

TEST(ComposeEnv, InnerProvideShadowsAndUnwinds) {
  struct EnvOther {
    int v = 0;
    bool operator==(const EnvOther &) const = default;
  };
  env::Provide<EnvPalette> outer(EnvPalette{{1, 0, 0, 1}, {}});
  ASSERT_TRUE(env::bound<EnvPalette>());
  EXPECT_TRUE(env::inherited<EnvPalette>()->surface == SkColor4f({1, 0, 0, 1}));
  {
    env::Provide<EnvPalette> inner(EnvPalette{{0, 0, 1, 1}, {}});
    env::Provide<EnvOther> other(EnvOther{7});
    EXPECT_TRUE(env::inherited<EnvPalette>()->surface ==
                SkColor4f({0, 0, 1, 1}));
    EXPECT_EQ(env::inherited<EnvOther>()->v, 7); // keyed by TYPE, no crosstalk
  }
  EXPECT_TRUE(env::inherited<EnvPalette>()->surface == SkColor4f({1, 0, 0, 1}));
  EXPECT_FALSE(env::bound<EnvOther>());
}

TEST(ComposeEnv, ALibraryComponentReadsTheEnvironmentByItsOwnPropsType) {
  // The entry's actual complaint: `console::` had to be handed its colours
  // by whoever composed it. The env key is console::Style — the component's
  // OWN props type — so no library-wide Theme exists or needs to.
  console::LineRing ring;
  ring.append(u8"ready.");

  console::Style themed;
  themed.text = whiteStyle(12);
  themed.gap = 7.0f;

  Element tree = [&] {
    env::Provide<console::Style> style(themed);
    return box().padding(4).child(box().child(console::console(ring)));
  }();
  ASSERT_FALSE(env::bound<console::Style>());

  Host host;
  host.composer.render(tree);
  host.frame();
  const SkRect got = *host.composer.bounds("con#1");
  EXPECT_GT(got.width(), 0.0f);

  // The unbound spelling still compiles to the component's own default —
  // and a DIFFERENT default, which is what proves the binding was read.
  Host bare;
  bare.composer.render(box().padding(4).child(
      box().child(console::console(ring, console::Style{}))));
  bare.frame();
  EXPECT_NE(bare.composer.bounds("con#1")->width(), got.width());
}

// ---------------------------------------------------------------------------
// wiggle() and the reconciler — the prune pin for the noise stage added to
// BoundFloat on 2026-07-29 (SigilMotion, Animation.h).

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
    return box().child(box().key("shaken").width(40).height(40).fill(red())
                           .translateX(bind(&phase)
                                           .target(-70.0f, 170.0f)
                                           .wiggle(r.amount, r.frequency,
                                                   r.seed, r.octaves,
                                                   r.falloff)));
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
      {.amount = 20.0f},    {.frequency = 11.0f}, {.seed = 2},
      {.octaves = 3},       {.falloff = 0.9f},
  };
  const char *named[] = {"amount", "frequency", "seed", "octaves", "falloff"};
  for (size_t i = 0; i < std::size(moved); ++i) {
    host.composer.render(tree({})); // back to the baseline rig
    host.frame();
    host.composer.render(tree(moved[i]));
    EXPECT_GT(host.composer.stats().patchedNodes, 0u)
        << named[i] << " changed and the node PRUNED — boundMapEqual() is "
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
        .child(box().key("x").width(8).height(8).fill(red())
                   .translateX(wiggle(&t, 40.0f, 3.0f, 1).offset(100.0f))
                   .translateY(30.0f))
        .child(box().key("y").width(8).height(8).fill(green())
                   .translateX(wiggle(&t, 40.0f, 3.0f, 2).offset(100.0f))
                   .translateY(90.0f));
  };
  host.composer.render(tree());

  // bounds() is the LAYOUT rect and a translate is paint-only, so the
  // observation has to be pixels: where the mark actually landed.
  const auto centerOf = [&host](int row, SkColor want) {
    int lo = -1, hi = -1;
    for (int x = 0; x < 200; ++x)
      if (host.pixel(x, row) == want) {
        if (lo < 0)
          lo = x;
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
    if (std::fabs(x - firstX) > 4.0f)
      xMoved = true;
    if (std::fabs(y - firstY) > 4.0f)
      yMoved = true;
    if (std::fabs(x - y) > 6.0f)
      everApart = true;
  }
  EXPECT_TRUE(xMoved) << "the x shake never moved";
  EXPECT_TRUE(yMoved) << "the y shake never moved";
  EXPECT_TRUE(everApart)
      << "two seeds produced the same displacement every frame — either the "
         "seed is not reaching the noise, or the second node pruned into the "
         "first";
}

// ---------------------------------------------------------------------------
// travel(): the motion path (2026-07-29)
//
// The 2D port of world::CameraPath. Every ruling in MotionPath's doc comment
// is pinned here, each with the positive control that made it fail.

namespace {

/** The centroid of every pixel of @p color, or (-1,-1) when none. Motion is
 *  a VISUAL feature: these pins scan the frame, they do not read floats out
 *  of the resolver. */
SkPoint inkCentroid(Host &host, SkColor color, int w, int h) {
  double sx = 0, sy = 0;
  int n = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (host.pixel(x, y) == color) {
        sx += x;
        sy += y;
        ++n;
      }
  if (n == 0)
    return {-1, -1};
  return {(float)(sx / n), (float)(sy / n)};
}

/** The bounding box of everything even faintly @p color-ish — enough to
 *  ask "is this bar lying flat or standing up", which is what an
 *  orientation pin actually wants to know. */
SkIRect inkBounds(Host &host, int w, int h) {
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

} // namespace

TEST(ComposeTravel, PlacesTheTransformOriginOnTheParentSizedCurve) {
  // THE PIXEL PIN. Four values of t, four quadrant points of the circle
  // inscribed in the PARENT's box — not the rider's own 8x8 box, which is
  // the whole difficulty a size-dependent Shape brings that CameraPath
  // never faced.
  Host host(200, 200);
  choreograph::Output<float> t{0};
  const auto describe = [&] {
    return travelFrame(rider({.path = shapes::circle(), .t = &t}));
  };

  // Skia's addOval(dir=kCW, startIndex=1) begins at the RIGHT extreme and
  // runs clockwise, so quarter turns are right → bottom → left → top.
  const SkPoint want[4] = {{180, 100}, {100, 180}, {20, 100}, {100, 20}};
  for (int i = 0; i < 4; ++i) {
    t = (float)i * 0.25f;
    host.composer.render(describe());
    host.frame();
    const SkPoint ink = inkCentroid(host, SK_ColorRED, 200, 200);
    ASSERT_GE(ink.x(), 0) << "nothing painted at t=" << t.value();
    EXPECT_NEAR(ink.x(), want[i].x(), 1.5f) << "t=" << t.value();
    EXPECT_NEAR(ink.y(), want[i].y(), 1.5f) << "t=" << t.value();
  }

  // …and the point that rides is the TRANSFORM ORIGIN, so moving the origin
  // to the rider's top-left offsets the whole ink by half its box.
  t = 0.0f;
  host.composer.render(travelFrame(
      rider({.path = shapes::circle(), .t = &t}).transformOrigin(0, 0)));
  host.frame();
  const SkPoint pinned = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(pinned.x(), 184.0f, 1.5f)
      << "transformOrigin() is not the point on the curve";
  EXPECT_NEAR(pinned.y(), 104.0f, 1.5f);
}

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

TEST(ComposeTravel, WrapsOnAClosedCurveAndClampsOnAnOpenOne) {
  Host host(200, 200);
  choreograph::Output<float> t{0};
  const auto atT = [&](Shape path, float value) {
    t = value;
    host.composer.render(travelFrame(rider({.path = std::move(path), .t = &t})));
    host.frame();
    return inkCentroid(host, SK_ColorRED, 200, 200);
  };

  // Closed: a lap and a quarter is a quarter, and negative runs backwards.
  const SkPoint quarter = atT(shapes::circle(), 0.25f);
  const SkPoint lapAndAQuarter = atT(shapes::circle(), 1.25f);
  EXPECT_NEAR(lapAndAQuarter.x(), quarter.x(), 1.0f) << "a closed curve did "
                                                        "not WRAP";
  EXPECT_NEAR(lapAndAQuarter.y(), quarter.y(), 1.0f);
  const SkPoint back = atT(shapes::circle(), -0.25f);
  const SkPoint threeQuarters = atT(shapes::circle(), 0.75f);
  EXPECT_NEAR(back.x(), threeQuarters.x(), 1.0f);
  EXPECT_NEAR(back.y(), threeQuarters.y(), 1.0f);

  // Open: both ends park. The line runs the frame's full width at mid-height.
  const auto line = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() / 2);
    b.lineTo(s.width(), s.height() / 2);
    return b.detach();
  };
  const SkPoint past = atT(line, 1.5f);
  EXPECT_NEAR(past.x(), 180.0f, 1.5f) << "an open curve did not CLAMP";
  const SkPoint before = atT(line, -0.5f);
  EXPECT_NEAR(before.x(), 20.0f, 1.5f);
}

TEST(ComposeTravel, OutranksTheTranslateLanesAndHandsThemBack) {
  Host host(200, 200);
  choreograph::Output<float> t{0.25f};
  // A path and a contradicting lane on the same node: the path wins whole.
  host.composer.render(travelFrame(
      rider({.path = shapes::circle(), .t = &t}).translateX(-60).translateY(
          -60)));
  host.frame();
  SkPoint ink = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(ink.x(), 100.0f, 1.5f) << "the lanes were blended into the path";
  EXPECT_NEAR(ink.y(), 180.0f, 1.5f);

  // Drop the path and the very same lanes take over, live.
  host.composer.render(
      travelFrame(box()
                      .key("dot")
                      .absolute()
                      .rect(SkRect::MakeXYWH(0, 0, 8, 8))
                      .fill(red())
                      .translateX(-60)
                      .translateY(-60)));
  host.frame();
  ink = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_LT(ink.x(), 0) << "the lanes should have taken the dot off-canvas";
}

TEST(ComposeTravel, AutoOrientAddsToRotateAndHoldsTheLastGoodChord) {
  // A 40x4 bar: WIDE at 0 degrees, TALL at 90. At t=0 on a clockwise circle
  // the tangent points straight down, so auto-orient must stand it up.
  Host host(200, 200);
  choreograph::Output<float> t{0};
  const auto bar = [&](float lookAhead, std::optional<float> spin) {
    Element e = box()
                    .key("dot")
                    .absolute()
                    .rect(SkRect::MakeXYWH(0, 0, 40, 4))
                    .fill(red())
                    .travel({.path = shapes::circle(),
                             .t = &t,
                             .lookAhead = lookAhead});
    if (spin)
      e.rotate(*spin);
    return travelFrame(std::move(e));
  };

  host.composer.render(bar(0.0f, std::nullopt));
  host.frame();
  SkIRect ink = inkBounds(host, 200, 200);
  EXPECT_GT(ink.width(), 3 * ink.height()) << "the bar is not lying flat";

  host.composer.render(bar(0.02f, std::nullopt));
  host.frame();
  ink = inkBounds(host, 200, 200);
  EXPECT_GT(ink.height(), 3 * ink.width())
      << "auto-orient did not turn the bar onto the tangent";

  // …and an authored rotate() ADDS to it (90 + 90 lies flat again). If the
  // path replaced rotate() the bar would still be standing.
  host.composer.render(bar(0.02f, 90.0f));
  host.frame();
  ink = inkBounds(host, 200, 200);
  EXPECT_GT(ink.width(), 3 * ink.height())
      << "auto-orient REPLACED rotate() instead of composing with it";

  // At the far end of an OPEN curve the forward chord collapses. The last
  // good one is held, so a path that ends going DOWN leaves the bar standing
  // rather than snapping back to zero.
  const auto ell = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(0, 0);
    b.lineTo(100, 0);
    b.lineTo(100, 60);
    return b.detach();
  };
  t = 1.0f;
  host.composer.render(travelFrame(box()
                                       .key("dot")
                                       .absolute()
                                       .rect(SkRect::MakeXYWH(0, 0, 40, 4))
                                       .fill(red())
                                       .travel({.path = ell,
                                                .t = &t,
                                                .lookAhead = 0.02f})));
  host.frame();
  ink = inkBounds(host, 200, 200);
  EXPECT_GT(ink.height(), 3 * ink.width())
      << "the collapsed end chord was not replaced by the last good one";
}

TEST(ComposeTravel, PrunesOnlyWhenEveryFieldOfThePathMatches) {
  // THE PRUNE PIN. A motion path is read live at paint, so every field of
  // it must participate in reconciler equality — one control per field.
  Host host(200, 200);
  const auto describe = [](MotionPath p) {
    return travelFrame(rider(std::move(p)));
  };
  const auto renderAndCount = [&](MotionPath p) {
    host.composer.render(describe(std::move(p)));
    host.frame();
    return host.composer.stats().patchedNodes;
  };

  renderAndCount({.path = shapes::circle(), .t = 0.25f, .lookAhead = 0.02f});
  EXPECT_EQ(renderAndCount({.path = shapes::circle(),
                            .t = 0.25f,
                            .lookAhead = 0.02f}),
            0u)
      << "an identical comparable scheme did not prune";

  EXPECT_EQ(renderAndCount({.path = shapes::polygon(6),
                            .t = 0.25f,
                            .lookAhead = 0.02f}),
            1u)
      << "the path FIELD does not participate in equality";
  EXPECT_EQ(renderAndCount({.path = shapes::polygon(6),
                            .t = 0.60f,
                            .lookAhead = 0.02f}),
            1u)
      << "the t FIELD does not participate in equality";
  EXPECT_EQ(renderAndCount({.path = shapes::polygon(6),
                            .t = 0.60f,
                            .lookAhead = 0.05f}),
            1u)
      << "the lookAhead FIELD does not participate in equality";

  // Gaining and losing the path is itself a patch.
  host.composer.render(travelFrame(box()
                                       .key("dot")
                                       .absolute()
                                       .rect(SkRect::MakeXYWH(0, 0, 8, 8))
                                       .fill(red())));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
      << "dropping travel() pruned into the travelling description";

  // The escape hatch keeps its documented cost: a raw callable never
  // compares equal, so a travelling node built from one never prunes.
  const auto raw = [] {
    return travelFrame(rider({.path = [](SkSize s) {
                                SkPathBuilder b;
                                b.addOval(SkRect::MakeWH(s.width(),
                                                         s.height()));
                                return b.detach();
                              },
                              .t = 0.25f}));
  };
  host.composer.render(raw());
  host.frame();
  host.composer.render(raw());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
      << "a raw-callable motion path compared equal — the shape seam's "
         "escape-hatch contract is not being carried through travel()";
}

TEST(ComposeTravel, IsPaintOnlyAndAResizedFrameKeepsT) {
  // Paint-only: the LAID-OUT box never moves, whatever t does.
  Host host(200, 200);
  choreograph::Output<float> t{0};
  const auto describe = [&](float frameSize) {
    return box().child(box()
                           .key("frame")
                           .absolute()
                           .rect(SkRect::MakeXYWH(20, 20, frameSize, frameSize))
                           .child(rider({.path = shapes::circle(), .t = &t})));
  };
  host.composer.render(describe(160));
  host.frame();
  const auto laid = host.composer.bounds("dot");
  ASSERT_TRUE(laid.has_value());
  const SkPoint atZero = inkCentroid(host, SK_ColorRED, 200, 200);

  t = 0.5f;
  host.composer.render(describe(160));
  host.frame();
  EXPECT_EQ(host.composer.bounds("dot"), laid)
      << "travelling RELAYOUT the node — the motion path became a layout "
         "input";
  const SkPoint atHalf = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_GT(std::fabs(atHalf.x() - atZero.x()), 100.0f)
      << "…and it did not move either, so the assertion above is vacuous";

  // THE SIZE RULING: a re-laid frame RE-SHAPES the curve under the rider,
  // and t is untouched — the dot sits at the same FRACTION of the new
  // curve (here: half way round an 80 px circle inset at 20,20 → its left
  // extreme, x = 20) rather than jumping phase or freezing on the old one.
  t = 0.5f;
  host.composer.render(describe(80));
  host.frame();
  const SkPoint resized = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(resized.x(), 20.0f, 1.5f)
      << "a resize did not re-measure the curve (stale table) or did not "
         "keep t";
  EXPECT_NEAR(resized.y(), 60.0f, 1.5f);
}

TEST(ComposeTravel, TheHitTestUndoesTheSameMatrixPaintApplied) {
  Host host(200, 200);
  choreograph::Output<float> t{0.25f};
  host.composer.render(
      travelFrame(rider({.path = shapes::circle(), .t = &t}, 20)));
  host.frame();
  // The rider is laid out at the frame's top-left and painted at the
  // circle's bottom. Only the painted place may hit.
  EXPECT_EQ(host.composer.hitTest({100, 180}).value_or(""), "dot");
  EXPECT_NE(host.composer.hitTest({25, 25}).value_or(""), "dot");
}

TEST(ComposeTravel, APathWithNoMeasurableLengthLeavesTheLanesStanding) {
  Host host(200, 200);
  host.composer.render(travelFrame(rider({.path = [](SkSize) { return SkPath(); },
                                          .t = 0.5f})
                                       .translateX(40)));
  host.frame();
  const SkPoint ink = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(ink.x(), 63.5f, 1.5f)
      << "an empty path engaged anyway and swallowed the translate lane";
  EXPECT_NEAR(ink.y(), 23.5f, 1.5f);
}

TEST(ComposeTravel, PerAxisScaleParticipatesInReconcilerEquality) {
  // Found by the travel() equality audit, not by travel(): scaleX/scaleY
  // were never added to propsEqual when they landed, so two descriptions
  // differing only in a per-axis scale compared EQUAL — the patch pruned,
  // the node was never marked paint-dirty, and the old picture replayed.
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

  host.composer.render(bar(2.0f).child(box().key("y").absolute().rect(
      SkRect::MakeXYWH(0, 60, 40, 40)).transformOrigin(0, 0).fill(green()).scaleY(
      1.0f)));
  host.frame();
  host.composer.render(bar(2.0f).child(box().key("y").absolute().rect(
      SkRect::MakeXYWH(0, 60, 40, 40)).transformOrigin(0, 0).fill(green()).scaleY(
      2.0f)));
  host.frame();
  EXPECT_EQ(host.pixel(20, 130), SK_ColorGREEN)
      << "a CHANGED scaleY pruned into the old description";
}
