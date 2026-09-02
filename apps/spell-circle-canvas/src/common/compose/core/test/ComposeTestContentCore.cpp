// The core binary's share of ComposeTestContent.cpp: the suites whose subjects
// are core-tier values, cut from that file so each test binary links only the
// target it exercises.

#include <include/core/SkBBHFactory.h>
#include <include/core/SkFont.h>
#include <include/core/SkPictureRecorder.h>
#include <sigilcompose/core/Feed.h>

#include <numeric>

#include "support/CoreTestSupport.h"

namespace {

/** Plain rows, white on the Host's black ground so ink reads as brightness. */
feed::TextOptions feedOptions(size_t visible, float size = 12.0f) {
  feed::TextOptions options;
  options.styles.base(whiteStyle(size));
  options.window.visible = visible;
  options.window.gap = 2.0f;
  return options;
}

/** The brightest ink inside a rect — how lit a row is, whatever it says. */
int brightestIn(Host& host, SkRect r) {
  int best = 0;
  for (int y = (int)r.top(); y < (int)r.bottom(); ++y)
    for (int x = (int)r.left(); x < (int)r.right(); ++x)
      best = std::max(best, (int)SkColorGetR(host.pixel(x, y)));
  return best;
}

}  // namespace

TEST(ComposeFeed, AnAppendCostsOneMountAndNeverRerecordsTheRowsAboveIt) {
  // Rows are keyed by their sequence id, not their position in the visible
  // window, so an append shifts nothing: surviving rows prune and keep their
  // pictures, and only the new tail mounts while the scrolled-out head
  // unmounts. Position keys would give every visible row a new key on every
  // append and re-patch the whole window.
  feed::TextRing ring;
  for (int i = 0; i < 30; ++i)
    ring.append({toU8("boot sequence line " + std::to_string(i))});
  const feed::TextOptions options = feedOptions(10);
  Host host(200, 400);
  auto describe = [&] {
    return box().padding(6).child(feed::feed(ring, options));
  };
  host.composer.render(describe());
  host.frame();  // records the visible window

  ring.append({toU8("intrusion detected")});
  host.composer.render(describe());
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);  // the new tail only
  host.frame();
  // Ancestor chain re-records + the tail's own picture; the nine surviving
  // rows replay their cached pictures untouched.
  EXPECT_LE(host.composer.stats().picturesRecorded, 4u);

  // The price is CONSTANT, which is the whole claim: a second append costs
  // exactly what the first did, and the retained tree does not grow.
  const size_t liveAfterFirst = host.composer.stats().instances;
  ring.append({toU8("second intrusion")});
  host.composer.render(describe());
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
  host.frame();
  EXPECT_LE(host.composer.stats().picturesRecorded, 4u);
  EXPECT_EQ(host.composer.stats().instances, liveAfterFirst)
      << "the window is bounded: one mount in, one unmount out";
}

TEST(ComposeFeed, ASurvivingRowKeepsItsInstanceRatherThanReentering) {
  // The sharper form of the same property. A row whose factory declares a
  // mount entrance re-runs that entrance whenever it MOUNTS, so a row that
  // was silently remounted by an append would flash back to nothing. Every
  // row here is fully lit before the append, and must still be after it.
  feed::TextRing ring;
  for (int i = 0; i < 4; ++i) ring.append({toU8("row")});
  const feed::TextOptions options = feedOptions(6, 16.0f);
  auto lit = [&](const feed::TextRow& row) {
    return feed::textRow(row, options.styles)
        .opacity(animate(motion::from(0.0f).to(1.0f),
                         {200ms, &choreograph::easeNone}));
  };
  Host host(160, 200);
  auto describe = [&] {
    return box().padding(4).child(feed::feed(ring, options.window, lit));
  };

  host.composer.render(describe());
  host.frame(0.4);  // every mounted row has finished its entrance
  for (uint64_t seq = 1; seq <= 4; ++seq) {
    const std::optional<SkRect> band = host.composer.bounds(feed::rowKey(seq));
    ASSERT_TRUE(band.has_value()) << seq;
    EXPECT_GT(brightestIn(host, *band), 150) << "row " << seq;
  }

  ring.append({toU8("tail")});
  host.composer.render(describe());
  host.frame(0.016);
  for (uint64_t seq = 1; seq <= 4; ++seq) {
    const std::optional<SkRect> band = host.composer.bounds(feed::rowKey(seq));
    ASSERT_TRUE(band.has_value()) << seq;
    EXPECT_GT(brightestIn(host, *band), 150)
        << "row " << seq << " re-entered: the append remounted it";
  }
  // …and the new row really is new — it is mid-entrance, not already lit.
  const std::optional<SkRect> tail = host.composer.bounds(feed::rowKey(5));
  ASSERT_TRUE(tail.has_value());
  EXPECT_LT(brightestIn(host, *tail), 120);
}

TEST(ComposeFeed, TheWindowNeverMountsTheRowsOutsideIt) {
  // Virtualization is the window, not a separate mechanism: rows older than
  // Options::visible are never built, so they have no instance, no bounds
  // and no layout cost, and a ring that keeps growing does not.
  feed::TextRing ring{600};
  for (int i = 0; i < 300; ++i)
    ring.append({toU8("line " + std::to_string(i))});
  const feed::TextOptions options = feedOptions(8);
  Host host(200, 200);
  host.composer.render(box().child(feed::feed(ring, options)));
  host.frame();

  EXPECT_FALSE(host.composer.bounds(feed::rowKey(1)).has_value())
      << "a row outside the window was mounted";
  EXPECT_FALSE(host.composer.bounds(feed::rowKey(292)).has_value());
  EXPECT_TRUE(host.composer.bounds(feed::rowKey(293)).has_value())
      << "the window is the NEWEST visible rows";
  EXPECT_TRUE(host.composer.bounds(feed::rowKey(300)).has_value());

  const size_t live = host.composer.stats().instances;
  for (int i = 0; i < 200; ++i)
    ring.append({toU8("more " + std::to_string(i))});
  host.composer.render(box().child(feed::feed(ring, options)));
  host.frame();
  EXPECT_EQ(host.composer.stats().instances, live)
      << "the retained tree grew with the ring instead of with the window";
}

TEST(ComposeFeed, TheEntranceStaggerDelaysOnlyTheRowsThatMount) {
  // Options::entrance is a motion::Spread — the same value the glyph
  // engine and staggerChildren speak — with a ROW as the beat. The initial
  // describe cascades the window; an append is the only new mount in its patch,
  // so it enters AT ONCE instead of inheriting a full window's worth of steps,
  // and no row already on screen re-enters.
  feed::TextRing ring;
  for (int i = 0; i < 3; ++i) ring.append({toU8("row")});
  feed::TextOptions options = feedOptions(6, 16.0f);
  options.window.entrance = {.eachMs = 400};
  auto lit = [&](const feed::TextRow& row) {
    return feed::textRow(row, options.styles)
        .opacity(animate(motion::from(0.0f).to(1.0f),
                         {200ms, &choreograph::easeNone}));
  };
  Host host(160, 200);
  auto describe = [&] {
    return box().padding(4).child(feed::feed(ring, options.window, lit));
  };

  host.composer.render(describe());
  host.frame(0.25);  // row 1's 200 ms is done; row 2 waits out its 400 ms
  const std::optional<SkRect> r1 = host.composer.bounds(feed::rowKey(1));
  const std::optional<SkRect> r2 = host.composer.bounds(feed::rowKey(2));
  ASSERT_TRUE(r1.has_value());
  ASSERT_TRUE(r2.has_value());
  EXPECT_GT(brightestIn(host, *r1), 150);
  EXPECT_LT(brightestIn(host, *r2), 40) << "the cascade did not delay row 2";
  host.frame(0.5);  // t = 0.75 — row 2 is past its 400 ms delay
  EXPECT_GT(brightestIn(host, *r2), 150);

  // The append: one new mount, so no extra delay at all. Waiting only its
  // own 200 ms entrance is what proves the cascade counts MOUNTS and not
  // positions — an ordinal-based delay would hold this row for 1.2 s.
  ring.append({toU8("tail")});
  host.composer.render(describe());
  host.frame(0.25);
  const std::optional<SkRect> r4 = host.composer.bounds(feed::rowKey(4));
  ASSERT_TRUE(r4.has_value());
  EXPECT_GT(brightestIn(host, *r4), 150)
      << "the appended row inherited the window's cascade";
}

TEST(ComposeMaterial, UnknownUniformNamesWarnAndIgnore) {
  // A typo'd uniform name must never abort (SkDEBUGFAIL kills the sketch
  // host in debug): unknown names are warned and dropped, at sksl() and at
  // uniform(), constant and bound alike.
  material::skia::Paint m =
      material::skia::Paint::sksl(ukEffect(), {{"uTypo", 1.0f}});
  choreograph::Output<float> o{1.0f};
  m.uniform("uAlsoMissing", &o);  // dropped → still not live
  EXPECT_FALSE(m.isAnimated());
  Host host;
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(m)));
  host.frame();  // paints with uK at its SkSL default (0) — and does not crash
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 40u);
}

TEST(ComposeDerive, FlowAroundWrapsTextAroundFrame) {
  const std::u8string body =
      u8"the quick brown fox jumps over the lazy dog and keeps running "
      u8"through the tall summer grass until the river bend appears and "
      u8"the evening light settles over the water in long amber bands";

  auto tree = [&](bool flow) {
    auto t = text(body, whiteStyle(18)).key("body");
    if (flow) t.flowAround("frame", 6);
    return stack()
        .child(box()
                   .key("frame")
                   .width(150)
                   .height(140)
                   .inset(200, 10, 10, 210)
                   .absolute()
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

namespace {

const std::u8string& flowBody() {
  // Long enough to run past the obstacle on every geometry, so a height
  // comparison reads room-per-line and not "the text stopped early".
  static const std::u8string body = [] {
    std::u8string one =
        u8"the quick brown fox jumps over the lazy dog and keeps running "
        u8"through the tall summer grass until the river bend appears and "
        u8"the evening light settles over the water in long amber bands "
        u8"while the swallows turn above the reeds and the mill wheel "
        u8"grinds on into the blue hour without hurry or complaint ";
    std::u8string all;
    for (int i = 0; i < 4; ++i) all += one;
    return all;
  }();
  return body;
}

/** One paragraph flowing around one keyed target of the caller's making. */
Element flowScene(Element target, float margin) {
  return stack()
      .child(std::move(target))
      .child(box()
                 .inset(0)
                 .child(text(flowBody(), whiteStyle(15))
                            .key("body")
                            .flowAround("obstacle", margin))
                 .zIndex(1));
}

Element obstacleBox(Shape silhouette) {
  Element el = box()
                   .key("obstacle")
                   .width(160)
                   .height(160)
                   .left(100)
                   .top(40)
                   .fill(Fill::color({0, 0.4f, 0, 1}));
  if (silhouette) el.shape(std::move(silhouette));
  return el;
}

}  // namespace

TEST(ComposeDerive, FlowAroundShapelessTargetKeepsItsBox) {
  // The pin: a target with no silhouette of its own is subtracted by its
  // BOX, exactly as it always was. Every line the box crosses is cut to
  // the box's full width, whatever the type does.
  Host host(360, 460);
  host.composer.render(flowScene(obstacleBox({}), 6));
  host.frame();
  EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeLTRB(106, 46, 254, 194)));
}

TEST(ComposeDerive, FlowAroundCycleIsIgnored) {
  Host host;
  host.composer.render(box().child(
      text(u8"self reference", whiteStyle(16)).key("self").flowAround("self")));
  host.frame();  // must not hang or exclude itself into nothing
  EXPECT_NE(host.composer.paragraphLayout("self"), nullptr);
}

TEST(ComposeCaching, TextureBakeScaleQuantized) {
  // A continuously changing canvas scale (live window resize, pinch
  // zoom) must not re-bake Cache::Texture nodes every frame: the bake
  // scale quantizes up to a coarse step.
  Host host;
  host.composer.render(box()
                           .width(60)
                           .height(60)
                           .cache(Cache::Texture)
                           .fill(red())
                           .child(box().width(20).height(20).fill(green())));
  auto drawAt = [&](float s) {
    SkCanvas& canvas = *host.surface->getCanvas();
    canvas.save();
    canvas.scale(s, s);
    host.composer.draw(canvas);
    canvas.restore();
  };
  drawAt(1.6f);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 1u);  // first bake
  drawAt(1.7f);
  drawAt(1.9f);
  drawAt(2.0f);  // still within the 2.0 step: the bake is reused
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  drawAt(2.2f);  // crossed into the 3.0 step: one re-bake
  EXPECT_EQ(host.composer.stats().picturesRecorded, 1u);
}

TEST(ComposeCaching, TextureBakeReusedUnderAMovingAncestor) {
  // The same guarantee from the side the test above cannot see. A bake
  // taken in DEVICE space is exact but pinned to one device rect, so it
  // may only be taken while the node is holding still — and "still" has
  // two independent measures that are easy to mistake for one:
  //
  //   * the node's own transform is not declared as animating, and
  //   * the device rect it LANDS on has not moved.
  //
  // This node declares nothing. It is dragged across the canvas by an
  // ancestor, through a Cache::None parent so no recording intervenes —
  // the one arrangement where a moving rect reaches a node that looks
  // static from every declaration available to it. A device-pinned bake
  // would re-rasterize every frame here, which is precisely the cost the
  // quantized local bake exists to avoid.
  //
  // Note this cannot be a pixel assertion: every arrangement below draws
  // the correct picture. Only the bake COUNT tells them apart.
  Host host(300, 300);
  choreograph::Output<float> slide{0.0f};
  host.composer.render(
      box()
          .cache(Cache::None)
          .child(box()
                     .cache(Cache::None)
                     .absolute()
                     .translateX(&slide)
                     .child(box()
                                .width(60)
                                .height(60)
                                .cache(Cache::Texture)
                                .fill(red())
                                .child(box().width(20).height(20).fill(
                                    green())))));
  host.frame();
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);  // the first bake
  // The still -> moving transition costs exactly one re-bake, because the
  // held image is in the wrong space for the path now being taken. That is
  // inherent to having two bake spaces and is not what this test guards.
  slide = 7.0f;
  host.frame();
  // From here the guarantee is absolute: a moving node reuses ONE local
  // bake and blits it through its transform, however far it travels.
  for (int i = 2; i <= 5; ++i) {
    slide = (float)i * 7.0f;  // whole-pixel slides: the rect really moves
    host.frame();
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u)
        << "frame " << i
        << ": the bake was re-rasterized while the node slid, instead of "
           "being reused and blitted through the transform";
  }
}

// ---------------------------------------------------------------------------
// Layout and leaf surface: wrap, per-edge spacing, per-corner radii,
// Dim literals, atlas regions, the Paragraph overload, contentScale.

TEST(ComposeLayout, WrapLinesFlowsToSecondRow) {
  Host host;
  host.composer.render(
      box().child(box()
                      .row()
                      .wrapLines()
                      .width(200)
                      .child(box().width(80).height(40).fill(red()))
                      .child(box().width(80).height(40).fill(green()))
                      .child(box().width(80).height(40).fill(blue()))));
  host.frame();
  EXPECT_EQ(host.pixel(40, 20), SK_ColorRED);
  EXPECT_EQ(host.pixel(120, 20), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(40, 60), SK_ColorBLUE);  // wrapped to the next line
}

TEST(ComposeLayout, PerEdgePaddingAndMargin) {
  Host host;
  host.composer.render(box().child(
      box()
          .padding(10, 20, 30, 40)
          .key("outer")
          .child(box().margin(5, 6, 7, 8).width(50).height(50).key("inner"))));
  host.frame();
  auto inner = host.composer.bounds("inner");
  ASSERT_TRUE(inner.has_value());
  EXPECT_FLOAT_EQ(inner->left(), 10 + 5);  // padding.left + margin.left
  EXPECT_FLOAT_EQ(inner->top(), 20 + 6);   // padding.top + margin.top
}

TEST(ComposeLayout, DimLiteralsResolvePercent) {
  Host host;
  host.composer.render(
      box().child(box().width(50_pct).height(25_pct).fill(red()).key("half")));
  host.frame();
  auto rect = host.composer.bounds("half");
  ASSERT_TRUE(rect.has_value());
  EXPECT_FLOAT_EQ(rect->width(), 100.0f);  // 50% of the 200px host
  EXPECT_FLOAT_EQ(rect->height(), 50.0f);  // 25% of 200px
}

TEST(ComposeContent, ImageRegionDrawsAtlasCell) {
  Host host;
  auto atlas = twoCellAtlas();
  host.composer.render(box()
                           .row()
                           .child(image(atlas)
                                      .region(SkRect::MakeXYWH(16, 0, 16, 16))
                                      .width(50)
                                      .height(50))
                           .child(image(atlas).width(50).height(50)));
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorGREEN);  // region: right cell only
  EXPECT_EQ(host.pixel(60, 25), SK_ColorRED);    // whole atlas: left half
}

TEST(ComposePaint, ContentScaleReportsHostScale) {
  Host host;
  float seen = 0.0f;
  host.composer.render(
      box().child(custom([&seen](SkCanvas&, const PaintContext& ctx) {
                    seen = ctx.contentScale;
                  })
                      .width(50)
                      .height(50)
                      .cache(Cache::None)));
  SkCanvas& canvas = *host.surface->getCanvas();
  canvas.save();
  canvas.scale(2.0f, 2.0f);
  host.composer.draw(canvas);
  canvas.restore();
  EXPECT_FLOAT_EQ(seen, 2.0f);
}

TEST(ComposePaint, AnimatingReportsTheTickersState) {
  // `PaintContext::animating` looks dead from inside the library: the painter
  // assigns it from `ticker.active()` (and the Brushes.h wrappers copy that
  // forward rather than a constant), but nothing in the library ever reads
  // it back. Its only consumer is a paint program written by a caller, so
  // this test is the only thing keeping the field wired up.
  Host host;
  bool seen = false;
  host.composer.render(
      box()
          .child(box().width(40).height(40).fill(red()).opacity(
              animate(motion::from(0.0f).to(1.0f), {400ms})))
          .child(custom([&seen](SkCanvas&, const PaintContext& ctx) {
                   seen = ctx.animating;
                 })
                     .width(10)
                     .height(10)
                     .cache(Cache::None)));
  host.frame(0.016);
  EXPECT_TRUE(seen) << "an entrance is running: the ticker is active";
  for (int i = 0; i < 40; ++i)
    host.frame(0.016);  // 640 ms — well past the 400 ms entrance
  EXPECT_FALSE(seen) << "and false again once nothing is moving";
}

// ---------------------------------------------------------------------------
// hitTest: paint order, transforms, shapes.

TEST(ComposeQueries, HitTestRespectsPaintOrderAndKeys) {
  Host host;
  host.composer.render(
      stack()
          .child(box().key("under").inset(0).fill(red()))
          .child(box()
                     .key("over")
                     .width(60)
                     .height(60)
                     .inset(20, 20, 120, 120)
                     .absolute()
                     .fill(green()))
          .child(box()
                     .width(30)
                     .height(30)
                     .inset(150, 150, 20, 20)
                     .absolute()
                     .fill(blue())));  // keyless → falls to root
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

TEST(ComposeTransform, SkewLeansPaintAndHits) {
  // skewX(−12°) leans the card's top to the right about its centre. The
  // point of the case is the second half: hit-testing must walk the shear
  // backwards, so a point that is inside the leaning card but outside its
  // unsheared box still hits it.
  Host host;
  host.composer.render(box().child(box()
                                       .key("card")
                                       .width(40)
                                       .height(40)
                                       .inset(60, 60, 100, 100)
                                       .absolute()
                                       .fill(red())
                                       .skewX(-12.0f)));
  host.frame();
  EXPECT_EQ(host.pixel(101, 64), SK_ColorRED);   // top leaned right
  EXPECT_EQ(host.pixel(61, 64), SK_ColorBLACK);  // vacated top-left
  EXPECT_EQ(host.pixel(58, 97), SK_ColorRED);    // bottom leaned left
  EXPECT_EQ(host.pixel(98, 97), SK_ColorBLACK);  // vacated bottom-right
  auto hit = host.composer.hitTest({101, 64});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, "card");  // transform-aware hit through the shear
  EXPECT_FALSE(host.composer.hitTest({61, 64}).has_value());
}

TEST(ComposeTransform, SkewXPositiveLeansTheTopTowardNegativeX) {
  // THE SIGN PIN. skewX shears about the box centre in screen space, y
  // down, by tan(skewX degrees): a POSITIVE angle displaces the top edge
  // toward NEGATIVE x relative to the bottom edge — the top leans left.
  // The sign is easy to state backwards, so the runtime's answer is
  // pinned here in pixels.
  Host host;
  host.composer.render(box().child(box()
                                       .key("card")
                                       .width(40)
                                       .height(40)
                                       .inset(60, 60, 100, 100)
                                       .absolute()
                                       .fill(red())
                                       .skewX(30.0f)));
  host.frame();
  // The unsheared box is x in [60, 100], y in [60, 100], centre (80, 80).
  // At y = 64 (16 above centre) the shift is tan(30) * -16 ~ -9.2, so the
  // top row spans about [50.8, 90.8]; at y = 97 (17 below) the shift is
  // +9.8, spanning about [69.8, 109.8].
  EXPECT_EQ(host.pixel(54, 64), SK_ColorRED);    // top edge left of the box
  EXPECT_EQ(host.pixel(97, 64), SK_ColorBLACK);  // vacated top-right
  EXPECT_EQ(host.pixel(106, 97), SK_ColorRED);   // bottom edge leaned right
  EXPECT_EQ(host.pixel(63, 97), SK_ColorBLACK);  // vacated bottom-left
  // And hit-testing walks the same shear: the leaned top-left corner is
  // inside the card, the vacated top-right is not.
  auto hit = host.composer.hitTest({54, 64});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, "card");
  EXPECT_FALSE(host.composer.hitTest({97, 64}).has_value());
}

TEST(ComposeReconcile, StructuralPruneNeedsNoMemo) {
  // memo() is an optimisation for expensive DESCRIBES, not the thing that
  // makes pruning work. A subtree whose new description equals its old one
  // is skipped wholesale either way, so plain boxes, text and images built
  // from value-comparable props re-render for free.
  Host host;
  auto tree = [] {
    return box()
        .row()
        .gap(8)
        .padding(12)
        .child(box().width(40).height(40).corners({6}).fill(red()))
        .child(text(u8"static", styleAt(18)).key("t"))
        .child(box().grow(1).fill(blue()).opacity(0.9f));
  };
  host.composer.render(tree());
  host.frame();

  host.composer.render(tree());  // brand-new Elements, identical values
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());  // hosts may skip the redraw
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

// ---------------------------------------------------------------------------
// Mount entrances, trim wrap, per-side insets, overflow-safe recording,
// stroke align, measure(), presets, marquee.

// ---------------------------------------------------------------------------
// The authoring grammar: animate(motion::from(a).to(b)) /
// animate(through({…})). What is pinned is the VALUE each argument shape
// builds, because that value is the only thing the engine ever sees — the
// argument spellings are pure sugar over it.

TEST(ComposeMotion, EachArgumentShapeBuildsItsOwnTransitioned) {
  const sigil::motion::Transition spec{200ms, &choreograph::easeNone, 40ms};

  const sigil::motion::Transitioned<float> ramp =
      animate(sigil::motion::to(1.0f), spec);
  EXPECT_EQ(ramp.value, 1.0f);
  EXPECT_FALSE(ramp.from.has_value()) << "to() alone is not an entrance";
  EXPECT_TRUE(ramp.waypoints.empty());
  EXPECT_EQ(ramp.spec.duration, 200ms);
  EXPECT_EQ(ramp.spec.delay, 40ms);

  const sigil::motion::Transitioned<float> entrance =
      animate(motion::from(0.0f).to(1.0f), spec);
  EXPECT_EQ(entrance.value, 1.0f);
  ASSERT_TRUE(entrance.from.has_value());
  EXPECT_EQ(*entrance.from, 0.0f);
  EXPECT_TRUE(entrance.waypoints.empty());
  EXPECT_EQ(entrance.spec.duration, 200ms);
  EXPECT_EQ(entrance.spec.delay, 40ms);
  EXPECT_FLOAT_EQ(entrance.spec.easing()(0.25f), 0.25f);

  const std::vector<std::pair<std::chrono::milliseconds, float>> path{
      {0ms, 40.0f}, {200ms, -20.0f}, {400ms, 0.0f}};
  const sigil::motion::Transitioned<float> phrasedPath =
      animate(sigil::motion::through(path), &choreograph::easeNone);
  EXPECT_EQ(phrasedPath.value, 0.0f);
  ASSERT_TRUE(phrasedPath.from.has_value());
  EXPECT_EQ(*phrasedPath.from, 40.0f);
  EXPECT_EQ(phrasedPath.waypoints, path);
  EXPECT_EQ(phrasedPath.spec.duration, 400ms);
  // The ease is the one field the waypoint overload writes itself —
  // dropping it would default to easeOutQuad silently.
  EXPECT_FLOAT_EQ(phrasedPath.spec.easing()(0.25f), 0.25f);
}

// A guard, not a reproduction: an indeterminate value can happen to hold the
// number this test wants, so a passing run is weaker evidence than usual.
// Both spellings are checked, and the pixel arm at the bottom is what makes
// the claim about behaviour rather than about one struct field.
TEST(ComposeMotion, AnEmptyKeyframePathIsDETERMINATE) {
  // An empty waypoint list is a degenerate ask that must still produce a
  // definite answer. `Transitioned<T>::value` has to be value-initialized:
  // default-initialized, `animate(through({}))` would leave a float property
  // reading whatever was on the stack — once, silently, with no failure to
  // observe anywhere. Zero is the answer.
  const sigil::motion::Transitioned<float> empty =
      animate(sigil::motion::through({}));
  EXPECT_EQ(empty.value, 0.0f);
  EXPECT_FALSE(empty.from.has_value());
  EXPECT_TRUE(empty.waypoints.empty());

  const std::vector<std::pair<std::chrono::milliseconds, float>> none;
  const sigil::motion::Transitioned<float> phrased =
      animate(sigil::motion::through(none));
  EXPECT_EQ(phrased.value, 0.0f);

  // And through the property slot: the node paints AT that determinate
  // value rather than at a number nobody chose.
  Host host;
  host.composer.render(
      box().child(box().width(80).height(80).fill(red()).opacity(
          animate(sigil::motion::through({})))));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLACK);  // opacity 0, not garbage
}

TEST(ComposeMotion, AnimateThroughDeducesAFloatPath) {
  // A nested braced list is a non-deduced context, so the generic form
  // normally has to be told `<float>`. This overload exists so it does not.
  // Compiling with no explicit template argument IS the test — the
  // assertions below only confirm it deduced the right thing.
  const sigil::motion::Transitioned<float> t =
      animate(sigil::motion::through({{0ms, 0.0f}, {100ms, 1.0f}}));
  ASSERT_EQ(t.waypoints.size(), 2u);
  EXPECT_EQ(t.waypoints.front().second, 0.0f);
  EXPECT_EQ(t.waypoints.back().second, 1.0f);
  ASSERT_TRUE(t.from.has_value());
  EXPECT_EQ(*t.from, 0.0f);
  EXPECT_EQ(t.value, 1.0f);
  EXPECT_EQ(t.spec.duration, 100ms);
}

TEST(ComposeMotion, AnimatePlaysEntranceOnMount) {
  Host host;
  auto tree = [] {
    return box().child(box().width(80).height(80).fill(red()).opacity(
        animate(motion::from(0.0f).to(1.0f), {200ms, &choreograph::easeNone})));
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorBLACK);  // enters invisible
  host.frame(0.1);                               // half the linear ramp
  const SkColor mid = host.pixel(40, 40);
  EXPECT_GT(SkColorGetR(mid), 90u);
  EXPECT_LT(SkColorGetR(mid), 165u);
  EXPECT_EQ(SkColorGetG(mid), 0u);
  host.frame(0.2);  // settled
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);

  host.composer.render(tree());  // identical re-describe prunes clean
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);
}

TEST(ComposeMotion, AnimateColorSweepsOnMount) {
  Host host;
  host.composer.render(
      box().child(box().width(80).height(80).fill(motion::Animatable<Fill>(
          animate(motion::from(Fill::color({1, 1, 1, 1})).to(red()),
                  {200ms, &choreograph::easeNone})))));
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorWHITE);  // the declared "from"
  host.frame(0.3);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);
}

namespace {

/** A red 40x40 rect at x=150 recorded into a picture whose cull rect is
 *  the 100x100 box it escapes; replayed onto a 300x200 white surface.
 *  Returns the pixel the escaped rect would paint. */
sk_sp<SkPicture> escapingPicture(const SkRect& cull, SkBBHFactory* bbh) {
  SkPictureRecorder rec;
  SkCanvas* c = rec.beginRecording(cull, bbh);
  SkPaint p;
  p.setColor(SK_ColorRED);
  c->drawRect(SkRect::MakeXYWH(150, 10, 40, 40), p);
  return rec.finishRecordingAsPicture();
}

SkColor replayPixel(const sk_sp<SkPicture>& pic, int x, int y) {
  auto surf = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(300, 200));
  surf->getCanvas()->clear(SK_ColorWHITE);
  surf->getCanvas()->drawPicture(pic);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  surf->readPixels(bm.pixmap(), x, y);
  return bm.getColor(0, 0);
}

}  // namespace

/** What a picture's cull rect actually does, established by experiment
 *  rather than assumed — because the intuitive reading ("ops outside the
 *  cull rect are dropped") is wrong, and `ownPaintBounds` is sized on the
 *  basis of the real behaviour.
 *
 *  An op outside the cull rect is NOT rejected at record time and NOT culled
 *  at plain playback. The cull rect only bites through a bounding-box
 *  hierarchy. What does clip in the compose paint path is saveLayer bounds
 *  and bake surfaces. Every arm below is asserted against its opposite, so
 *  the test cannot pass by agreeing with itself. */
TEST(ComposeCullRect, PictureCullDoesNotCullWithoutABbh) {
  // (1) recorded: the op survives RECORDING despite sitting wholly
  // outside the cull rect, and the picture keeps the rect it was given.
  sk_sp<SkPicture> pic = escapingPicture(SkRect::MakeWH(100, 100), nullptr);
  EXPECT_EQ(pic->approximateOpCount(true), 1);
  EXPECT_EQ(pic->cullRect(), SkRect::MakeWH(100, 100));
  // (2) and it survives PLAYBACK: the pixels land outside the cull rect.
  EXPECT_EQ(replayPixel(pic, 170, 20), SK_ColorRED);

  // (3) an EMPTY cull rect does not reject either — the zero-size-node
  // guard in StackingPainter.cpp is justified by promotion, not by op
  // rejection.
  sk_sp<SkPicture> empty = escapingPicture(SkRect::MakeWH(0, 0), nullptr);
  EXPECT_EQ(empty->approximateOpCount(true), 1);
  EXPECT_EQ(replayPixel(empty, 170, 20), SK_ColorRED);

  // (4) nor is the whole picture quick-rejected when its cull rect misses
  // the device entirely: an op inside the device still paints.
  {
    SkPictureRecorder rec;
    SkPaint p;
    p.setColor(SK_ColorRED);
    rec.beginRecording(SkRect::MakeXYWH(1000, 1000, 100, 100))
        ->drawRect(SkRect::MakeXYWH(20, 20, 40, 40), p);
    EXPECT_EQ(replayPixel(rec.finishRecordingAsPicture(), 30, 30), SK_ColorRED);
  }

  // (5) WITH a bbh the cull rect finally bites — still recorded, dropped
  // at playback, because the RTree clips op bounds to the cull rect. This
  // is the arm that makes (2) meaningful: same input, opposite outcome.
  SkRTreeFactory bbh;
  sk_sp<SkPicture> tree = escapingPicture(SkRect::MakeWH(100, 100), &bbh);
  EXPECT_EQ(tree->approximateOpCount(true), 1);
  EXPECT_EQ(replayPixel(tree, 170, 20), SK_ColorWHITE);

  // (6) saveLayer bounds, by contrast, are a genuine clip — this is the
  // mechanism recordBounds' child union is actually defending against.
  {
    auto surf = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(300, 200));
    surf->getCanvas()->clear(SK_ColorWHITE);
    const SkRect box = SkRect::MakeWH(100, 100);
    SkPaint layer;
    layer.setAlphaf(0.5f);
    SkPaint p;
    p.setColor(SK_ColorRED);
    surf->getCanvas()->saveLayer(&box, &layer);
    surf->getCanvas()->drawRect(SkRect::MakeXYWH(150, 10, 40, 40), p);
    surf->getCanvas()->restore();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    surf->readPixels(bm.pixmap(), 170, 20);
    EXPECT_EQ(bm.getColor(0, 0), SK_ColorWHITE);
  }
}

TEST(ComposeCache, OverflowingChildSurvivesPictureCaching) {
  // A child translated beyond its parent's box must not be quick-rejected
  // by the parent's recording cull (the recordBounds fix).
  Host host(300, 200);
  host.composer.render(
      box().child(box().width(100).height(100).fill(blue()).child(
          box().width(40).height(40).fill(red()).translateX(150.0f))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 20), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(170, 20), SK_ColorRED);  // fully outside parent's box
  host.frame();                                 // cached replay path
  EXPECT_EQ(host.pixel(170, 20), SK_ColorRED);
}

TEST(ComposeCache, OverflowingChildSurvivesGroupOpacityLayer) {
  // The clip that actually bites: a group opacity opens a saveLayer
  // BOUNDED by recordBounds, and saveLayer bounds are a real clip. Drop
  // the child union from recordBounds and the overflowing child is gone.
  Host host(300, 200);
  host.composer.render(
      box().child(box().width(100).height(100).fill(blue()).opacity(0.5f).child(
          box().width(40).height(40).fill(red()).translateX(150.0f))));
  host.frame();
  EXPECT_GT(SkColorGetB(host.pixel(50, 20)), 100u);   // sanity: the parent
  EXPECT_GT(SkColorGetR(host.pixel(170, 20)), 100u);  // the escaped child
}

TEST(ComposeCache, OverflowingChildSurvivesTextureBake) {
  // The second real clip: Cache::Texture bakes into a surface sized from
  // recordBounds mapped to device, so anything the rect misses is
  // truncated by the surface itself — no picture cull involved.
  Host host(300, 200);
  host.composer.render(box().child(
      box()
          .width(100)
          .height(100)
          .fill(blue())
          .cache(Cache::Texture)
          .child(box().width(40).height(40).fill(red()).translateX(150.0f))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 20), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(170, 20), SK_ColorRED);
  host.frame();  // cached blit path
  EXPECT_EQ(host.pixel(170, 20), SK_ColorRED);
}
