#include <include/core/SkBBHFactory.h>
#include <include/core/SkPictureRecorder.h>
#include <sigilcompose/Console.h>

#include "ComposeTestSupport.h"

TEST(ComposeConsole, AppendCostsOneMountNotOneRerecordPerLine) {
  // Lines are keyed by their sequence id, not their position in the visible
  // window, so an append shifts nothing: surviving lines prune and keep
  // their pictures, and only the new tail mounts while the scrolled-out head
  // unmounts. Position keys would give every visible line a new key on every
  // append and re-patch the whole window.
  console::LineRing ring;
  for (int i = 0; i < 30; ++i)
    ring.append(
        sigil::compose::util::toU8("boot sequence line " + std::to_string(i)));
  console::Style st;
  st.text = styleAt(12);
  st.visibleLines = 10;
  Host host(200, 400);
  auto describe = [&] {
    return box().padding(6).child(console::console(ring, st));
  };
  host.composer.render(describe());
  host.frame();  // records the visible window
  ring.append(sigil::compose::util::toU8("intrusion detected"));
  host.composer.render(describe());
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);  // the new tail only
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
  host.composer.setView({});  // pass-through again
  host.frame();
  const uint32_t plain = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(plain, 118u);  // ≈ 128
  EXPECT_LT(plain, 138u);
}
#endif  // SIGILCOMPOSE_ENABLE_OCIO

TEST(ComposeMaterial, UnknownUniformNamesWarnAndIgnore) {
  // A typo'd uniform name must never abort (SkDEBUGFAIL kills the sketch
  // host in debug): unknown names are warned and dropped, at sksl() and at
  // uniform(), constant and bound alike.
  Material m = Material::sksl(ukEffect(), {{"uTypo", 1.0f}});
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

TEST(ComposeDerive, FlowAroundCycleIsIgnored) {
  Host host;
  host.composer.render(box().child(
      text(u8"self reference", whiteStyle(16)).key("self").flowAround("self")));
  host.frame();  // must not hang or exclude itself into nothing
  EXPECT_NE(host.composer.paragraphLayout("self"), nullptr);
}

TEST(ComposeDerive, ConnectorTracksMovedEndpoints) {
  Host host;
  PathFormat wire;
  wire.width = 4;
  wire.strokeFill = Fill::color({1, 1, 0, 1});

  auto tree = [&](float bLeft) {
    return stack()
        .child(box()
                   .key("a")
                   .width(20)
                   .height(20)
                   .inset(10, 10, 170, 170)
                   .absolute()
                   .fill(red()))
        .child(box()
                   .key("b")
                   .width(20)
                   .height(20)
                   .inset(bLeft, 160, 180 - bLeft, 20)
                   .absolute()
                   .fill(green()))
        .child(connector("a", "b").inset(0).foreground(wire).zIndex(-1));
  };

  host.composer.render(tree(10.0f));
  host.frame();
  // Vertical wire at x=20 between the stacked boxes.
  EXPECT_EQ(host.pixel(20, 100), SK_ColorYELLOW);

  host.composer.render(tree(160.0f));  // move b to the right
  host.frame();
  EXPECT_EQ(host.pixel(20, 100), SK_ColorBLACK);  // old route gone
  EXPECT_NE(host.pixel(95, 95), SK_ColorBLACK);   // new diagonal route
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
      box().width(100).height(100).clip().shape(diamond).fill(red()).child(
          box().inset(0).absolute().fill(green())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorGREEN);  // clipped child inside
  EXPECT_EQ(host.pixel(3, 3), SK_ColorBLACK);    // box corner outside shape
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
  host.composer.render(box().child(box()
                                       .width(100)
                                       .height(100)
                                       .shape(shapes::rounded(diamond, 20))
                                       .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);   // body intact
  EXPECT_EQ(host.pixel(50, 3), SK_ColorBLACK);  // sharp tip rounded away
  EXPECT_EQ(host.pixel(50, 12), SK_ColorRED);   // rounded apex below y≈7

  host.composer.render(box().child(
      box().width(100).height(100).shape(shapes::star(5)).fill(red())));
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
      stack().child(text(u8"WWWW", style).absolute().inset(10, 10, 10, 120)));
  host.frame();
  int lit = 0;
  for (int x = 10; x < 190; x += 4)
    for (int y = 10; y < 70; y += 4)
      if (host.pixel(x, y) != SK_ColorBLACK) lit++;
  EXPECT_GT(lit, 20);  // glyph coverage, not empty
}

TEST(TextLayout, AlignItemsCentersTextLeaf) {
  Host host;
  sigil::weave::TextStyle style = styleAt(40);
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(box()
                           .width(200)
                           .height(60)
                           .alignItems(Align::Center)
                           .child(text(u8"W", style)));
  host.frame();

  int litLeft = 0, litMiddle = 0;
  for (int x = 0; x < 50; x += 2)
    for (int y = 0; y < 60; y += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) litLeft++;
  for (int x = 75; x < 125; x += 2)
    for (int y = 0; y < 60; y += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) litMiddle++;
  EXPECT_EQ(litLeft, 0);    // nothing hugging the start edge
  EXPECT_GT(litMiddle, 5);  // the glyph sits in the middle
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

TEST(Shape, PerCornerRadiiIndependent) {
  Host host;
  // Sharp top-left, heavily rounded top-right.
  host.composer.render(box().child(
      box().width(100).height(100).corners({0, 40, 0, 0}).fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(2, 2), SK_ColorRED);     // sharp TL corner filled
  EXPECT_EQ(host.pixel(97, 2), SK_ColorBLACK);  // rounded TR corner empty
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);
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

TEST(TextLayout, ParagraphOverloadPaintsMixedSpans) {
  Host host(400, 200);
  auto para = std::make_shared<sigil::weave::Paragraph>();
  sigil::weave::TextStyle big = styleAt(40);
  big.paint.foreground.setColor(SK_ColorWHITE);
  sigil::weave::TextStyle small = styleAt(16);
  small.paint.foreground.setColor(SK_ColorWHITE);
  para->appendText(std::u8string(u8"BIG"), big);
  para->appendText(std::u8string(u8" and small"), small);

  host.composer.render(box().padding(10).child(text(para).key("spans")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("spans");
  ASSERT_NE(layout, nullptr);
  int lit = 0;
  for (int x = 10; x < 390; x += 3)
    for (int y = 10; y < 70; y += 3)
      if (host.pixel(x, y) != SK_ColorBLACK) lit++;
  EXPECT_GT(lit, 15);  // both spans shaped and painted
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
  // `PaintContext::animating` looks dead from inside the library: Paint.cpp
  // assigns it from `ticker.active()` (and the Brushes.h wrappers copy that
  // forward rather than a constant), but nothing in the library ever reads
  // it back. Its only consumer is a paint program written by a caller, so
  // this test is the only thing keeping the field wired up.
  Host host;
  bool seen = false;
  host.composer.render(
      box()
          .child(box().width(40).height(40).fill(red()).opacity(
              animate(from(0.0f).to(1.0f), {400ms})))
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
// Shape kit (Shapes.h): organic generators, per-edge extraction.

TEST(Shape, PolygonAndSquircleSilhouettes) {
  Host host;
  host.composer.render(
      box()
          .row()
          .child(
              box().width(90).height(90).shape(shapes::polygon(6)).fill(red()))
          .child(box()
                     .width(90)
                     .height(90)
                     .shape(shapes::squircle(4))
                     .fill(green())));
  host.frame();
  EXPECT_EQ(host.pixel(45, 45), SK_ColorRED);     // hexagon body
  EXPECT_EQ(host.pixel(2, 2), SK_ColorBLACK);     // hexagon corner cut
  EXPECT_EQ(host.pixel(135, 45), SK_ColorGREEN);  // squircle body
  EXPECT_EQ(host.pixel(92, 2), SK_ColorBLACK);    // squircle corner soft
  EXPECT_EQ(host.pixel(135, 3), SK_ColorGREEN);   // but edge midpoints full
}

TEST(Shape, BlobIsDeterministicOrganicAndBounded) {
  auto probe = [](uint32_t seed) {
    Host host;
    host.composer.render(box().child(box()
                                         .width(120)
                                         .height(120)
                                         .shape(shapes::blob(seed, 0.3f, 9))
                                         .fill(red())));
    host.frame();
    std::vector<SkColor> px;
    for (int y = 0; y < 130; y += 4)
      for (int x = 0; x < 130; x += 4) px.push_back(host.pixel(x, y));
    return px;
  };
  std::vector<SkColor> a1 = probe(7), a2 = probe(7), b = probe(8);
  EXPECT_EQ(a1, a2);  // same seed → identical pixels (cacheable chaos)
  EXPECT_NE(a1, b);   // different seed → different blob

  Host host;
  host.composer.render(box().child(box()
                                       .width(120)
                                       .height(120)
                                       .shape(shapes::blob(7, 0.3f, 9))
                                       .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(60, 60), SK_ColorRED);  // center always covered
  int outside = 0;
  for (int x = 121; x < 200; x += 4)
    for (int y = 0; y < 200; y += 4)
      if (host.pixel(x, y) != SK_ColorBLACK) outside++;
  EXPECT_EQ(outside, 0);  // never escapes its layout box
}

TEST(ComposeDecorations, EdgeSliceStrokesSelectedEdgesOnly) {
  Host host;
  host.composer.render(
      box().child(box().width(100).height(100).fill(blue()).foreground(
          shapes::onEdges(shapes::Edge::Top | shapes::Edge::Left,
                          util::stroke(8, Fill::color({1, 1, 1, 1}))))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorWHITE);  // top edge stroked
  EXPECT_EQ(host.pixel(1, 50), SK_ColorWHITE);  // left edge stroked
  EXPECT_EQ(host.pixel(98, 50), SK_ColorBLUE);  // right edge bare
  EXPECT_EQ(host.pixel(50, 98), SK_ColorBLUE);  // bottom edge bare
}

TEST(ComposeDecorations, EdgesSplitRoundedCornersDiagonally) {
  // A rounded rect's corner arcs divide between their adjacent edges at
  // the diagonal — the top run must include the upper half of the
  // top-left arc but none of the left flank.
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).corners({30}).fill(blue()).foreground(
          shapes::onEdges(shapes::Edge::Top,
                          util::stroke(8, Fill::color({1, 1, 1, 1}))))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorWHITE);  // top run center
  EXPECT_EQ(host.pixel(1, 50), SK_ColorBLUE);   // left flank untouched
  EXPECT_EQ(host.pixel(50, 98), SK_ColorBLUE);  // bottom untouched
}

// ---------------------------------------------------------------------------
// Shape VALUES: an outline generator is a comparable scheme, so a shaped
// node can prune. This matters more than it looks — a node whose outline
// cannot compare re-patches on every describe, which throws away its
// recording and its bake, so a single such node in an otherwise static tree
// puts the whole tree back on the live-paint path.

TEST(ComposeShapeValues, AStockGeneratorShapePrunes) {
  // The core case: a shaped node re-described identically must patch nothing
  // and re-record nothing. If structural equality refuses shaped nodes
  // outright, this tree re-records every frame for its entire life and
  // nothing reports it.
  Host host;
  auto tree = [] {
    return box().child(box()
                           .width(100)
                           .height(100)
                           .shape(shapes::star(5, 0.5f, 0.12f))
                           .fill(red()));
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);  // the star is really there

  host.composer.render(tree());  // brand-new Elements, identical values
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeShapeValues, AChangedParameterPatchesAndMovesPixels) {
  // The other half of the prune contract: equality must be HONEST. A
  // different parameter is a different value, patches, and redraws.
  Host host;
  auto tree = [](int sides) {
    return box().child(
        box().width(100).height(100).shape(shapes::polygon(sides)).fill(red()));
  };
  host.composer.render(tree(4));  // diamond: box corners empty
  host.frame();
  EXPECT_EQ(host.pixel(6, 6), SK_ColorBLACK);
  host.composer.render(tree(40));  // ~circle: still empty corners, more ink
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
  host.frame();
  // A 40-gon covers (25, 12); a diamond does not.
  EXPECT_EQ(host.pixel(25, 12), SK_ColorRED);
}

TEST(ComposeShapeValues, ARawCallableIsTheEscapeHatchAndStaysConservative) {
  // A hand-rolled OutlineFn cannot compare, so its node re-patches on every
  // describe. That is the conservative answer and it is deliberate: claiming
  // equality for two callables would prune a node whose outline had in fact
  // changed. An author who needs the prune wraps the node in memo().
  Host host;
  auto tree = [] {
    return box().child(box()
                           .width(100)
                           .height(100)
                           .shape([](SkSize s) {
                             SkPathBuilder b;
                             b.addOval(SkRect::MakeWH(s.width(), s.height()));
                             return b.detach();
                           })
                           .fill(red()));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree());
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "an incomparable callable pruned — equality is lying";
}

TEST(ComposeShapeValues, CopiesOfOneShapeCompareEqualEvenWhenRaw) {
  // Two copies of one Shape share state, and shared state IS identity — so
  // a caller who builds a raw callable once and holds it gets a real prune,
  // where one who re-mints an equivalent lambda each describe does not.
  const Shape raw = [](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
  const Shape copy = raw;
  EXPECT_TRUE(raw == copy);
  // But two separate constructions from equivalent lambdas cannot know
  // they agree, and must not claim to.
  const Shape other = [](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
  EXPECT_FALSE(raw == other);
}

TEST(ComposeShapeValues, WrappersAreComparableWhenTheirInnerIs) {
  // rounded() composes: value in, value out. Wrapping the escape hatch
  // stays the escape hatch.
  EXPECT_TRUE(Shape(shapes::rounded(shapes::star(5), 8)) ==
              Shape(shapes::rounded(shapes::star(5), 8)));
  EXPECT_FALSE(Shape(shapes::rounded(shapes::star(5), 8)) ==
               Shape(shapes::rounded(shapes::star(5), 9)));
  EXPECT_FALSE(Shape(shapes::rounded(shapes::star(5), 8)) ==
               Shape(shapes::rounded(shapes::star(6), 8)));
  auto lambda = [](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
  EXPECT_FALSE(Shape(shapes::rounded(lambda, 8)) ==
               Shape(shapes::rounded(lambda, 8)));
}

TEST(ComposeShapeValues, SvgShapesAreValuesNow) {
  // svg() parses its d-string once into an SkPath, and SkPath has structural
  // equality — so an svg() silhouette compares by geometry and prunes,
  // unlike the raw-callable hatch above.
  EXPECT_TRUE(Shape(shapes::svg("M0 0L10 0L10 10Z")) ==
              Shape(shapes::svg("M0 0L10 0L10 10Z")));
  EXPECT_FALSE(Shape(shapes::svg("M0 0L10 0L10 10Z")) ==
               Shape(shapes::svg("M0 0L10 0L5 10Z")));
}

TEST(ComposeShapeValues, KeyedParametricIsAValueUnkeyedIsNot) {
  auto fig8 = [](float t) { return SkPoint{std::sin(2 * t), std::sin(t)}; };
  // Unkeyed: the callable is the identity and cannot compare.
  EXPECT_FALSE(Shape(shapes::parametric(fig8, 0, 6.2832f, 720)) ==
               Shape(shapes::parametric(fig8, 0, 6.2832f, 720)));
  // Keyed: (key, window, samples) is the identity — the author's contract
  // that one key names one curve.
  EXPECT_TRUE(Shape(shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
              Shape(shapes::parametric("fig8", fig8, 0, 6.2832f, 720)));
  EXPECT_FALSE(Shape(shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
               Shape(shapes::parametric("fig8", fig8, 0, 6.2832f, 360)));
  EXPECT_FALSE(Shape(shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
               Shape(shapes::parametric("orbit", fig8, 0, 6.2832f, 720)));
  // The named families carry their identity in their parameters.
  EXPECT_TRUE(Shape(shapes::lissajous(3, 2)) == Shape(shapes::lissajous(3, 2)));
  EXPECT_FALSE(Shape(shapes::lissajous(3, 2)) ==
               Shape(shapes::lissajous(5, 4)));
  EXPECT_TRUE(Shape(shapes::rose(3)) == Shape(shapes::rose(3)));
  EXPECT_FALSE(Shape(shapes::spiral(3.0f)) == Shape(shapes::spiral(4.0f)));
}

TEST(ComposeShapeValues, TextOnAComparableBaselinePrunes) {
  // A TextPath's baseline is a Shape, so TextPath compares and a curved run
  // prunes. Without this every radial label in a figure re-records on every
  // render(), and a ring of labels is exactly where a figure has the most of
  // them.
  Host host(240, 240);
  auto ring = [](float at) {
    return text(u8"HHHHHHHHHH", whiteStyle(22))
        .width(240)
        .height(240)
        .absolute()
        .left(0)
        .top(0)
        .onPath({.path = shapes::arc(180.0f, 359.9f),
                 .at = at,
                 .align = TextPath::Align::Center});
  };
  host.composer.render(box().child(ring(0.25f)));
  host.frame();
  host.composer.render(box().child(ring(0.25f)));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical curved run re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  // …and the equality is honest: moving `at` IS a change. Omit `at` from
  // textEqual and a run that slides along its baseline compares equal to
  // where it was, prunes, and keeps the OLD placement forever with no
  // diagnostic — so this half of the case is the load-bearing one.
  host.composer.render(box().child(ring(0.75f)));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
}

TEST(ComposeShapeValues, ABandWithAComparableSpinePrunes) {
  // A band's authored spine is a Shape too, so deriveEqual must compare it
  // rather than refusing any authored spine outright.
  Host host;
  auto tree = [] {
    return box().child(band(shapes::circle(), across(8.0f))
                           .width(100)
                           .height(100)
                           .fill(red()));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical authored band spine re-patched";
}

TEST(ComposeShapeValues, TheChevreulScenarioKeepsItsBake) {
  // The steady state the prune exists for: a texture-cached node whose shape
  // is a generator, re-described every frame. If the shape does not compare,
  // the node patches, the patch drops the bake, and the node re-rasterizes
  // every frame — a single such node dominates a frame that is otherwise
  // entirely cached.
  Host host;
  auto tree = [] {
    return box().child(box()
                           .width(120)
                           .height(120)
                           .shape(shapes::circle())
                           .fill(red())
                           .cache(Cache::Texture));
  };
  host.composer.render(tree());
  host.frame();  // records + bakes once
  for (int i = 0; i < 3; ++i) {
    host.composer.render(tree());
    host.frame();
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
    EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
        << "the bake was thrown away by an identical re-describe";
  }
}

// ---------------------------------------------------------------------------
// Element stamps + snapshot().

TEST(ComposeStamps, SnapshotBakesIntrinsicSize) {
  sk_sp<SkPicture> pic =
      snapshot(box()
                   .row()
                   .gap(4)
                   .child(box().width(20).height(12).fill(red()))
                   .child(box().width(20).height(12).fill(green())),
               fonts());
  ASSERT_NE(pic, nullptr);
  EXPECT_FLOAT_EQ(pic->cullRect().width(), 44.0f);   // 20 + 4 + 20
  EXPECT_FLOAT_EQ(pic->cullRect().height(), 12.0f);  // content height
}

// ---------------------------------------------------------------------------
// tiles::window() / tiles::sliceable() — slicing one long bake into a run
// of tile rasters. These pin the ORIENTATION CONVENTION IN PIXELS, which is
// the whole reason the door exists. Slice arithmetic derived by hand goes
// wrong at the mirror, and it goes wrong invisibly: a strip sliced with the
// mirror on the wrong axis still produces a plausible-looking run of tiles.

namespace {

constexpr int kTileW = 40;
constexpr int kTileH = 20;
constexpr int kTileCount = 3;
// The mark sits near the tile's top-left and is small enough that its own
// mirror image never overlaps it — on EITHER axis, which is what lets a
// single sample point tell a flip from no flip.
constexpr float kMark = 3.0f;
constexpr float kMarkSize = 6.0f;
constexpr int kProbe = 5;  ///< inside the mark
constexpr int kFarX = kTileW - kProbe;
constexpr int kFarY = kTileH - kProbe;

SkColor stripMark(int index) {
  static const SkColor marks[3] = {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE};
  return marks[index % 3];
}

/** A strip whose every tile carries ONE mark, near the tile's top-LEFT and
 *  in a per-tile colour — so a rendered tile reports its index by colour and
 *  its handedness by which side the mark landed on. `flow` picks whether the
 *  strip runs down (a column of tiles) or across (a row). */
sk_sp<SkPicture> markedStrip(tiles::Flow flow) {
  const bool down = flow == tiles::Flow::Down;
  const float w = (float)(down ? kTileW : kTileW * kTileCount);
  const float h = (float)(down ? kTileH * kTileCount : kTileH);
  auto strip = box().width(w).height(h);
  for (int j = 0; j < kTileCount; ++j)
    strip.child(box()
                    .absolute()
                    .left(kMark + (down ? 0.0f : (float)(j * kTileW)))
                    .top(kMark + (down ? (float)(j * kTileH) : 0.0f))
                    .width(kMarkSize)
                    .height(kMarkSize)
                    .fill(Fill::color(SkColor4f::FromColor(stripMark(j)))));
  // Shell box: snapshot() sizes by the ROOT's children, not its own dims.
  return snapshot(box().child(std::move(strip)), fonts());
}

sk_sp<SkSurface> renderTile(const sk_sp<SkPicture>& pic, int index,
                            tiles::Flow flow, tiles::Facing facing) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kTileW, kTileH));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorBLACK);
  canvas->concat(tiles::window({kTileW, kTileH}, index, flow, facing));
  canvas->drawPicture(pic);
  return surface;
}

SkColor tilePixel(SkSurface& surface, int x, int y) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  surface.readPixels(bm.pixmap(), x, y);
  return bm.getColor(0, 0);
}

}  // namespace

TEST(ComposeStripTiles, ForwardWindowSlicesInOrderAndDoesNotMirror) {
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Down);
  ASSERT_NE(strip, nullptr);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> tile =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Forward);
    // The mark keeps its own side and its own offset within the tile: the
    // colour names the tile, so an off-by-one or a reversed step shows up
    // as the WRONG colour here.
    EXPECT_EQ(tilePixel(*tile, kProbe, kProbe), stripMark(k)) << "tile " << k;
    EXPECT_EQ(tilePixel(*tile, kFarX, kProbe), SK_ColorBLACK)
        << "tile " << k << " picked up a mirror it was not asked for";
  }
}

TEST(ComposeStripTiles, MirroredWindowFlipsAcrossTheStripNotAlongIt) {
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Down);
  ASSERT_NE(strip, nullptr);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> tile =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Mirrored);
    // ACROSS: x is reflected (4..12 becomes 28..36) …
    EXPECT_EQ(tilePixel(*tile, kFarX, kProbe), stripMark(k)) << "tile " << k;
    EXPECT_EQ(tilePixel(*tile, kProbe, kProbe), SK_ColorBLACK) << "tile " << k;
    // … and NOT along: y is untouched, so the mark stays near the top. A
    // flip on the flow axis would put it at kTileH - 8 and reverse the run.
    EXPECT_EQ(tilePixel(*tile, kFarX, kFarY), SK_ColorBLACK)
        << "tile " << k << " was mirrored along the flow, not across it";
  }
}

TEST(ComposeStripTiles, MirroredTileReadsForwardUnderMirroredSampling) {
  // What Facing::Mirrored actually promises: bake mirrored, sample mirrored,
  // and the strip reads exactly as the forward bake does. A caller drawing
  // the back face of a ribbon relies on this being an exact reflection
  // rather than an approximately-similar one.
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Down);
  ASSERT_NE(strip, nullptr);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> forward =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Forward);
    sk_sp<SkSurface> mirrored =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Mirrored);
    for (int y = 1; y < kTileH; y += 3)
      for (int x = 1; x < kTileW; x += 3)
        ASSERT_EQ(tilePixel(*forward, x, y),
                  tilePixel(*mirrored, kTileW - 1 - x, y))
            << "tile " << k << " at " << x << "," << y;
  }
}

TEST(ComposeStripTiles, AcrossFlowStepsRightwardAndMirrorsInY) {
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Across);
  ASSERT_NE(strip, nullptr);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> forward =
        renderTile(strip, k, tiles::Flow::Across, tiles::Facing::Forward);
    EXPECT_EQ(tilePixel(*forward, kProbe, kProbe), stripMark(k))
        << "tile " << k;
    sk_sp<SkSurface> mirrored =
        renderTile(strip, k, tiles::Flow::Across, tiles::Facing::Mirrored);
    // The across-flow mirror is in Y — perpendicular to the flow again, so
    // x keeps its place and the mark drops to the bottom.
    EXPECT_EQ(tilePixel(*mirrored, kProbe, kFarY), stripMark(k))
        << "tile " << k;
    EXPECT_EQ(tilePixel(*mirrored, kProbe, kProbe), SK_ColorBLACK)
        << "tile " << k;
  }
}

TEST(ComposeStripTiles, SliceableFlattensTheOpsAndChangesNoPixel) {
  const sk_sp<SkPicture> strip = markedStrip(tiles::Flow::Down);
  ASSERT_NE(strip, nullptr);
  const sk_sp<SkPicture> sliced = tiles::sliceable(strip);
  ASSERT_NE(sliced, nullptr);
  // The trap this verb exists for: drawPicture() into the recorder would
  // store ONE nested op the hierarchy cannot index into. Counting
  // non-nested ops is what tells the two apart.
  EXPECT_EQ(sliced->approximateOpCount(false), strip->approximateOpCount(false))
      << "sliceable() nested the picture instead of flattening it";
  EXPECT_GT(sliced->approximateOpCount(false), 3);
  for (int k = 0; k < kTileCount; ++k) {
    sk_sp<SkSurface> plain =
        renderTile(strip, k, tiles::Flow::Down, tiles::Facing::Mirrored);
    sk_sp<SkSurface> fast =
        renderTile(sliced, k, tiles::Flow::Down, tiles::Facing::Mirrored);
    for (int y = 0; y < kTileH; ++y)
      for (int x = 0; x < kTileW; ++x)
        ASSERT_EQ(tilePixel(*plain, x, y), tilePixel(*fast, x, y))
            << "tile " << k << " at " << x << "," << y;
  }
}

TEST(ComposeStamps, StampRecordsOnceReplaysPerSample) {
  static int stampDescribes;
  stampDescribes = 0;
  Host host;

  ContourWalk vine;
  vine.spacing = 25.0f;
  vine.stamp =
      custom([](SkCanvas& c, const PaintContext& ctx) {
        ++stampDescribes;
        SkPaint p;
        p.setColor(SK_ColorYELLOW);
        c.drawRect(SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
      })
          .width(12)
          .height(12);

  host.composer.render(box().child(box()
                                       .width(100)
                                       .height(100)
                                       .inset(50, 50, 50, 50)
                                       .absolute()
                                       .fill(blue())
                                       .foreground(vine)));
  host.frame();
  host.frame();
  EXPECT_EQ(stampDescribes, 1);  // baked once, replayed at every sample

  // Stamps are centered on the outline: the top-left corner sample
  // lands half outside the box.
  EXPECT_EQ(host.pixel(50, 50), SK_ColorYELLOW);   // corner sample center
  EXPECT_EQ(host.pixel(100, 46), SK_ColorYELLOW);  // top edge, above box
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLUE);   // interior untouched
}

TEST(ComposeStamps, RecursiveStampWalksItsOwnContour) {
  // Two levels of recursion: the stamp is itself decorated by a ContourWalk
  // dotting its own outline. This terminates because a stamp may only
  // decorate art that is already baked, never itself — so the nesting is
  // finite by construction rather than by a depth limit.
  Host host;
  ContourWalk dots;
  dots.spacing = 6.0f;
  dots.draw = [](SkCanvas& c, const PathSample&, const PaintContext&) {
    SkPaint p;
    p.setColor(SK_ColorCYAN);
    c.drawRect(SkRect::MakeXYWH(-1, -1, 2, 2), p);
  };

  ContourWalk outer;
  outer.spacing = 40.0f;
  outer.stamp = box().width(16).height(16).fill(red()).foreground(dots);

  host.composer.render(box().child(box()
                                       .width(120)
                                       .height(120)
                                       .inset(40, 40, 40, 40)
                                       .absolute()
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
  EXPECT_GT(redPx, 20);   // stamps landed
  EXPECT_GT(cyanPx, 10);  // and their own walked borders too
}

TEST(ComposeStamps, CustomLeafDrawsNestedComposer) {
  // A custom() leaf hosting an entire nested Composer: the recursion closes
  // at the paint phase, with the inner composer owning its own ticker and
  // size and drawing straight onto the outer canvas.
  Host host;
  auto nestedTicker = std::make_shared<sigil::motion::Ticker>();
  auto nested = std::make_shared<Composer>(*nestedTicker, fonts());
  nested->setSize({60, 60});
  nested->render(
      box().padding(10).fill(green()).child(box().grow(1).fill(red())));

  host.composer.render(box().child(
      custom([nested, nestedTicker](SkCanvas& c, const PaintContext&) {
        nested->draw(c);
      })
          .width(60)
          .height(60)
          .cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorGREEN);  // nested padding ring
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);  // nested content
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

TEST(ComposeQueries, HitTestHonorsShapeAndRotation) {
  Host host;
  host.composer.render(box()
                           .child(box()
                                      .key("star")
                                      .width(100)
                                      .height(100)
                                      .shape(shapes::star(5))
                                      .fill(red()))
                           .child(box()
                                      .key("spun")
                                      .width(80)
                                      .height(20)
                                      .inset(60, 140, 60, 40)
                                      .absolute()
                                      .rotate(90.0f)
                                      .fill(green())));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({50, 50}).value_or(""), "star");
  // Between the star's arms: inside the box, outside the silhouette.
  EXPECT_FALSE(host.composer.hitTest({20, 20}).has_value());

  // The 80x20 bar at (60,140) rotated 90° about its center paints as a
  // 20x80 bar centered at (100,150): x∈[90,110], y∈[110,190].
  EXPECT_EQ(host.composer.hitTest({100, 115}).value_or(""), "spun");
  EXPECT_FALSE(host.composer.hitTest({70, 150}).has_value());  // unrotated
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
  host.composer.render(stack()
                           .child(box()
                                      .key("a")
                                      .width(20)
                                      .height(20)
                                      .inset(10, 10, 170, 170)
                                      .absolute()
                                      .fill(red()))
                           .child(box()
                                      .key("b")
                                      .width(20)
                                      .height(20)
                                      .inset(160, 160, 20, 20)
                                      .absolute()
                                      .fill(green()))
                           .child(connector("a", "b", routers::orthogonal())
                                      .inset(0)
                                      .foreground(wire)
                                      .zIndex(-1)));
  host.frame();
  // Centers (20,20) and (170,170); midX = 95: H leg at y=20, V leg at
  // x=95, H leg at y=170.
  EXPECT_EQ(host.pixel(60, 20), SK_ColorYELLOW);    // first horizontal leg
  EXPECT_EQ(host.pixel(95, 100), SK_ColorYELLOW);   // vertical run
  EXPECT_EQ(host.pixel(130, 170), SK_ColorYELLOW);  // final horizontal leg
  EXPECT_EQ(host.pixel(60, 100), SK_ColorBLACK);    // nowhere near diagonal
}

TEST(ComposeDerive, ArcRouterBowsOffTheChord) {
  Host host;
  PathFormat wire;
  wire.width = 4;
  wire.strokeFill = Fill::color({1, 1, 0, 1});
  host.composer.render(stack()
                           .child(box()
                                      .key("a")
                                      .width(10)
                                      .height(10)
                                      .inset(20, 95, 170, 95)
                                      .absolute()
                                      .fill(red()))
                           .child(box()
                                      .key("b")
                                      .width(10)
                                      .height(10)
                                      .inset(170, 95, 20, 95)
                                      .absolute()
                                      .fill(green()))
                           .child(connector("a", "b", routers::arc(0.3f))
                                      .inset(0)
                                      .foreground(wire)
                                      .zIndex(-1)));
  host.frame();
  // Horizontal chord from (25,100) to (175,100), bulge 0.3×150 = 45 px
  // toward +normal (downward-left convention: normal of (+x,0) is
  // (0,+y) → the bow lands at y ≈ 145).
  EXPECT_EQ(host.pixel(100, 145), SK_ColorYELLOW);  // bowed midpoint
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);   // chord midpoint empty
}

TEST(ComposeDerive, ConnectorGapPullsTheWireOffTheEndpoints) {
  // A route runs to the node BOX's centre, and a box is often much larger
  // than the shape drawn inside it — an sdf:: panel, for instance, reserves
  // room for its glow. Without a terminal gap the wire is drawn straight
  // through the visible terminal to a centre nobody can see. The gap is the
  // same pull-back Anchor takes, spelled on connector().
  const auto scene = [](float gap) {
    PathFormat wire;
    wire.width = 4;
    wire.strokeFill = Fill::color({1, 1, 0, 1});
    return stack()
        .child(box()
                   .key("a")
                   .width(20)
                   .height(20)
                   .inset(10, 90, 170, 90)
                   .absolute()
                   .fill(red()))
        .child(box()
                   .key("b")
                   .width(20)
                   .height(20)
                   .inset(170, 90, 10, 90)
                   .absolute()
                   .fill(green()))
        .child(
            connector("a", "b", {}, gap).inset(0).foreground(wire).zIndex(1));
  };
  // Control: with gap 0 the wire runs centre to centre, (20,100) → (180,100),
  // and paints OVER both terminal boxes. Without this arm, "the gapped wire
  // does not reach the terminal" would also pass on a wire that was never
  // drawn.
  Host flush;
  flush.composer.render(scene(0.0f));
  flush.frame();
  EXPECT_EQ(flush.pixel(25, 100), SK_ColorYELLOW)
      << "the gapless wire must still reach into its near terminal";
  EXPECT_EQ(flush.pixel(175, 100), SK_ColorYELLOW)
      << "…and pierce the far one (that IS the complaint)";
  EXPECT_EQ(flush.pixel(100, 100), SK_ColorYELLOW);
  // The gap: 30 px pulled back at EACH end — the wire now runs x ∈
  // [50, 150], both terminals show their own fill, the middle survives.
  Host gapped;
  gapped.composer.render(scene(30.0f));
  gapped.frame();
  EXPECT_EQ(gapped.pixel(25, 100), SK_ColorRED)
      << "the gap did not pull the wire off its near terminal";
  EXPECT_EQ(gapped.pixel(175, 100), SK_ColorGREEN)
      << "the gap did not pull the wire off its far terminal";
  EXPECT_EQ(gapped.pixel(145, 100), SK_ColorYELLOW)
      << "the wire ends before its gap says to";
  EXPECT_EQ(gapped.pixel(160, 100), SK_ColorBLACK)
      << "the wire overran its gap";
  EXPECT_EQ(gapped.pixel(100, 100), SK_ColorYELLOW);  // the run survives
}

#include <sigilcompose/Layouts.h>

// ---------------------------------------------------------------------------
// Organic layout schemes (Layouts.h).

TEST(ComposeLayouts, RadialPlacesChildrenOnTheRing) {
  Host host;
  std::vector<Element> dots;
  for (int i = 0; i < 4; ++i)
    dots.push_back(
        box().width(10).height(10).fill(red()).key("d" + std::to_string(i)));
  host.composer.render(
      box().child(layout(layouts::Radial{.radiusFraction = 0.8f})
                      .width(200)
                      .height(200)
                      .children(dots)));
  host.frame();
  // Radius 80 from center (100,100), starting up, clockwise quarters.
  auto center = [&](const char* k) {
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
    beads.push_back(
        box().width(6).height(6).fill(green()).key("b" + std::to_string(i)));
  host.composer.render(
      box().child(layout(layouts::AlongPath{.path = shapes::star(5)})
                      .width(180)
                      .height(180)
                      .children(beads)));
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

// ---- kinetic typography
// ------------------------------------------------------

#include <sigilcompose/Kinetic.h>

TEST(ComposeKinetic, StaggeredRiseRevealsInOrder) {
  // The stagger law: at mid-progress the early glyphs are fully revealed
  // while the late ones haven't started — the canonical staggered reveal,
  // rendered through batched RSXform draws.
  Host host;
  auto tree = [](Animatable<float> progress) {
    GlyphFx fx;
    fx.effect = glyphfx::rise(24);
    fx.stagger = {.eachMs = 40, .durationMs = 200};
    fx.progress = std::move(progress);
    return box().padding(10).child(
        text(u8"IIIIIIIIIIII", whiteStyle(32)).key("k").glyphFx(std::move(fx)));
  };
  host.composer.render(tree(0.0f));
  host.frame();
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  const SkIRect leftEdge = SkIRect::MakeLTRB(
      (int)b->left(), (int)b->top(), (int)b->left() + 24, (int)b->bottom());
  const SkIRect rightEdge = SkIRect::MakeLTRB(
      (int)b->right() - 24, (int)b->top(), (int)b->right(), (int)b->bottom());
  EXPECT_FALSE(anyWhiteIn(host, leftEdge));  // progress 0: nothing revealed
  host.composer.render(tree(0.45f));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, leftEdge));    // head fully in
  EXPECT_FALSE(anyWhiteIn(host, rightEdge));  // tail not started
  host.composer.render(tree(1.0f));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, rightEdge));  // everything landed
}

TEST(ComposeKinetic, TransitionedProgressPaintsLive) {
  // The master progress takes the full Animatable treatment: a with()
  // transition animates the reveal and the node paints live while moving.
  Host host;
  auto tree = [](Animatable<float> progress) {
    GlyphFx fx;
    fx.effect = glyphfx::pop();
    fx.stagger = {.eachMs = 20, .durationMs = 150};
    fx.progress = std::move(progress);
    return box().padding(10).child(
        text(u8"POP", whiteStyle(40)).key("k").glyphFx(std::move(fx)));
  };
  host.composer.render(tree(0.001f));
  host.frame();
  host.composer.render(
      tree(animate(to(1.0f), {400ms, &choreograph::easeNone})));
  host.frame(0.2);                                    // mid-ramp
  EXPECT_GT(host.composer.stats().nodesPainted, 0u);  // live while animating
  host.frame(0.3);                                    // settle
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  EXPECT_TRUE(
      anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left(), (int)b->top(),
                                         (int)b->right(), (int)b->bottom())));
}

TEST(ComposeKinetic, ABoundProgressRevealsWithoutARedescribe) {
  // Glyph progress must be classified as CONTENT volatility, not paint-only:
  // it rebuilds glyph geometry, so the node's own recording is invalid the
  // moment it ticks. Classified paint-only, computeVolatile leaves
  // `ownContent` and `subtreeVolatile` false, the picture is never reset,
  // and the reveal FREEZES at whatever progress the last describe recorded.
  //
  // Driving the reveal from a BOUND Output is the only way to see that. Any
  // case that moves the reveal by RE-DESCRIBING marks the node paint-dirty
  // and hides the question entirely, and a case that only checks for ink
  // after settling is satisfied by a frozen half-revealed recording. Hence
  // both halves here: the tail must be dark before, and lit after, with no
  // describe in between.
  Host host;
  choreograph::Output<float> progress{0.0f};
  GlyphFx fx;
  fx.effect = glyphfx::rise(24);
  fx.stagger = {.eachMs = 40, .durationMs = 200};
  fx.progress = &progress;
  host.composer.render(box().padding(10).child(
      text(u8"IIIIIIIIIIII", whiteStyle(32)).key("k").glyphFx(std::move(fx))));
  host.frame();
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  const SkIRect tail = SkIRect::MakeLTRB((int)b->right() - 24, (int)b->top(),
                                         (int)b->right(), (int)b->bottom());
  ASSERT_FALSE(anyWhiteIn(host, tail))
      << "the tail was already revealed at progress 0, so the check below "
         "cannot tell a live reveal from a frozen one";

  progress = 1.0f;  // ONE describe, and the value moves underneath it
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, tail))
      << "the tail never appeared: the node replayed the recording it made at "
         "progress 0, so glyph progress is not invalidating its own picture";
}

TEST(ComposeLayouts, BaselineGridRendersInsideStackedAbsoluteColumn) {
  // Text inside a BaselineGrid, nested in an absolute column, inside a
  // stack(). A custom layout scheme writes back into Yoga out of band, and
  // an absolute ancestor changes how its subtree is sized, so this is the
  // combination most likely to leave the text laid out at zero size and
  // therefore invisible.
  Host host;
  host.composer.render(stack().child(
      box()
          .column()
          .absolute()
          .inset(10, 10, 10, 10)
          .child(layout(layouts::BaselineGrid{.rhythm = 24})
                     .width(pct(100))
                     .child(text(u8"probe", whiteStyle(28)).key("p")))));
  host.frame();
  auto b = host.composer.bounds("p");
  ASSERT_TRUE(b.has_value());
  EXPECT_GT(b->width(), 5.0f) << "placed rect " << b->left() << "," << b->top()
                              << " " << b->width() << "x" << b->height();
  EXPECT_GT(b->height(), 5.0f);
  EXPECT_TRUE(
      anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left(), (int)b->top(),
                                         (int)b->right(), (int)b->bottom())))
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
  host.composer.render(box().child(layout(grid)
                                       .width(pct(100))
                                       .grow(1)
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
  EXPECT_NEAR(a->width(), 44 * 2 + 8, 0.01f);  // 2-module span + gutter
  EXPECT_NEAR(a->left(), 0, 0.01f);
  EXPECT_NEAR(b->left(), (44 + 8) * 3, 0.01f);   // 4th column
  EXPECT_NEAR(b->height(), 44 * 3 + 16, 0.01f);  // 3 rows + 2 gutters
  EXPECT_NEAR(c->left(), 0, 0.01f);  // auto-flow starts at (0,0)… of the flow
  EXPECT_NEAR(c->width(), 44, 0.01f);
  EXPECT_NEAR(d->left(), 44 + 8, 0.01f);  // next module across
}

TEST(ComposeLayouts, BaselineGridSnapsBottomsAndBaselines) {
  // Non-text children anchor by BOTTOM: heights 15 & 27 on rhythm 20 land
  // their bottoms on grid lines 20 and 60 (flow 20+27=47 rounds up).
  Host host;
  host.composer.render(box().child(
      layout(layouts::BaselineGrid{.rhythm = 20})
          .width(pct(100))
          .grow(1)
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
  host.composer.render(
      box().child(layout(layouts::BaselineGrid{.rhythm = 200})
                      .width(pct(100))
                      .grow(1)
                      .child(text(u8"Xylograph", styleAt(40)).key("t"))));
  host.frame();
  auto t = host.composer.bounds("t");
  ASSERT_TRUE(t.has_value());
  EXPECT_GT(200.0f - t->top(), 10.0f);               // sane baseline
  EXPECT_LT(200.0f - t->top(), t->height() - 0.5f);  // baseline, not bottom
}

TEST(ComposeLayouts, ScatterIsDeterministicAndContained) {
  auto centers = [&](uint32_t seed) {
    Host host;
    std::vector<Element> bits;
    for (int i = 0; i < 9; ++i)
      bits.push_back(
          box().width(12).height(12).fill(blue()).key("s" + std::to_string(i)));
    host.composer.render(box().child(layout(layouts::Scatter{.seed = seed})
                                         .width(200)
                                         .height(200)
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
  EXPECT_EQ(a1, a2);  // same seed → same scatter
  EXPECT_NE(a1, b);   // new seed → new chaos
}

// ---------------------------------------------------------------------------
// Tile maps: atlas regions + chunked cache invalidation.

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
  std::vector<int> tiles;  // 4x4 tile ids
  int chunkX = 0, chunkY = 0;
  bool operator==(const ChunkProps&) const = default;
};

constexpr float kTilePx = 12.0f;

Element tileChunk(const ChunkProps& p) {
  static auto atlas = fourTileAtlas();
  auto chunk = box().width(4 * kTilePx).height(4 * kTilePx);
  for (int i = 0; i < (int)p.tiles.size(); ++i) {
    const int id = p.tiles[(size_t)i];
    const float sx = (float)(id % 2) * 8, sy = (float)(id / 2) * 8;
    chunk.child(
        image(atlas)
            .region(SkRect::MakeXYWH(sx, sy, 8, 8))
            .absolute()
            .inset((float)(i % 4) * kTilePx, (float)(i / 4) * kTilePx, 0, 0)
            .width(kTilePx)
            .height(kTilePx));
  }
  return chunk;
}

}  // namespace

TEST(ComposeTiling, OnlyTouchedChunkRerecords) {
  Host host;
  // 2x2 chunks of 4x4 tiles; a checker-ish rule fills the ids.
  std::vector<ChunkProps> chunks(4);
  for (int c = 0; c < 4; ++c) {
    chunks[(size_t)c].chunkX = c % 2;
    chunks[(size_t)c].chunkY = c / 2;
    for (int i = 0; i < 16; ++i) chunks[(size_t)c].tiles.push_back((i + c) % 4);
  }
  auto maze = [&] {
    auto grid = box().row().wrapLines().width(2 * 4 * kTilePx);
    for (int c = 0; c < 4; ++c)
      grid.child(
          memo(chunks[(size_t)c], tileChunk).key("chunk" + std::to_string(c)));
    return box().child(grid);
  };

  host.composer.render(maze());
  host.frame();
  const size_t coldRecords = host.composer.stats().picturesRecorded;
  EXPECT_GE(coldRecords, 4u);  // every chunk baked (plus ancestors)

  // Pixel sanity: chunk 0 tile 0 is id 0 (red); chunk 1 tile 0 is id 1
  // (green) at x = 48.
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(53, 5), SK_ColorGREEN);

  host.composer.render(maze());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);  // all memo-warm

  chunks[0].tiles[0] = 3;  // mutate ONE tile in ONE chunk
  host.composer.render(maze());
  host.frame();
  // Only chunk 0 and its ancestor chain re-record; the other three
  // chunks' pictures replay untouched.
  EXPECT_LE(host.composer.stats().picturesRecorded, 3u);
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorYELLOW);  // the mutated tile
  EXPECT_EQ(host.pixel(53, 5), SK_ColorGREEN);  // neighbors intact
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
// The authoring grammar: animate(from(a).to(b)) / animate(through({…})).
// What is pinned is the VALUE each argument shape builds, because that value
// is the only thing the engine ever sees — the argument spellings are pure
// sugar over it.

TEST(ComposeMotion, EachArgumentShapeBuildsItsOwnTransitioned) {
  const Transition spec{200ms, &choreograph::easeNone, 40ms};

  const Transitioned<float> ramp = animate(to(1.0f), spec);
  EXPECT_EQ(ramp.value, 1.0f);
  EXPECT_FALSE(ramp.from.has_value()) << "to() alone is not an entrance";
  EXPECT_TRUE(ramp.waypoints.empty());
  EXPECT_EQ(ramp.spec.duration, 200ms);
  EXPECT_EQ(ramp.spec.delay, 40ms);

  const Transitioned<float> entrance = animate(from(0.0f).to(1.0f), spec);
  EXPECT_EQ(entrance.value, 1.0f);
  ASSERT_TRUE(entrance.from.has_value());
  EXPECT_EQ(*entrance.from, 0.0f);
  EXPECT_TRUE(entrance.waypoints.empty());
  EXPECT_EQ(entrance.spec.duration, 200ms);
  EXPECT_EQ(entrance.spec.delay, 40ms);
  EXPECT_FLOAT_EQ(entrance.spec.easing()(0.25f), 0.25f);

  const std::vector<std::pair<std::chrono::milliseconds, float>> path{
      {0ms, 40.0f}, {200ms, -20.0f}, {400ms, 0.0f}};
  const Transitioned<float> phrasedPath =
      animate(through(path), &choreograph::easeNone);
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
  const Transitioned<float> empty = animate(through({}));
  EXPECT_EQ(empty.value, 0.0f);
  EXPECT_FALSE(empty.from.has_value());
  EXPECT_TRUE(empty.waypoints.empty());

  const std::vector<std::pair<std::chrono::milliseconds, float>> none;
  const Transitioned<float> phrased = animate(through(none));
  EXPECT_EQ(phrased.value, 0.0f);

  // And through the property slot: the node paints AT that determinate
  // value rather than at a number nobody chose.
  Host host;
  host.composer.render(box().child(
      box().width(80).height(80).fill(red()).opacity(animate(through({})))));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLACK);  // opacity 0, not garbage
}

TEST(ComposeMotion, AnimateThroughDeducesAFloatPath) {
  // A nested braced list is a non-deduced context, so the generic form
  // normally has to be told `<float>`. This overload exists so it does not.
  // Compiling with no explicit template argument IS the test — the
  // assertions below only confirm it deduced the right thing.
  const Transitioned<float> t = animate(through({{0ms, 0.0f}, {100ms, 1.0f}}));
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
        animate(from(0.0f).to(1.0f), {200ms, &choreograph::easeNone})));
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
  host.composer.render(box().child(box().width(80).height(80).fill(
      Animatable<Fill>(animate(from(Fill::color({1, 1, 1, 1})).to(red()),
                               {200ms, &choreograph::easeNone})))));
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorWHITE);  // the declared "from"
  host.frame(0.3);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);
}

TEST(ComposeMask, WrapWindowCrossesTheSeam) {
  // A wrap window crossing the cycle seam must paint exactly the union of
  // its two clamped pieces — direction-agnostic pixel containment.
  auto strokedBox = [](Spans where) {
    return box().child(box()
                           .absolute()
                           .inset(50, 50, 50, 50)
                           .mask(by::spans(std::move(where)))
                           .foreground(util::stroke(6, green())));
  };
  Host wrap, pieceA, pieceB;
  wrap.composer.render(strokedBox(spans::wrap(0.9f, 1.15f)));
  pieceA.composer.render(strokedBox(spans::range(0.9f, 1.0f)));
  pieceB.composer.render(strokedBox(spans::range(0.0f, 0.15f)));
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
  EXPECT_GT(unionCount, 50);  // the pieces really painted
  EXPECT_LE(missing, 4);      // wrap covers the union (AA slack)
  EXPECT_NEAR(wrapCount, unionCount, unionCount / 5.0 + 8);
}

TEST(ComposeMask, WrapOffsetBindingMarchesTheWindow) {
  Host host;
  choreograph::Output<float> phase{0.0f};
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(50, 50, 50, 50)
                      .mask(by::spans(spans::wrap(0.0f, 0.25f).offset(&phase)))
                      .foreground(util::stroke(6, green()))));
  host.frame();
  std::vector<SkIPoint> lit0;
  for (int y = 40; y < 160; y += 2)
    for (int x = 40; x < 160; x += 2)
      if (host.pixel(x, y) == SK_ColorGREEN) lit0.push_back({x, y});
  ASSERT_GT(lit0.size(), 10u);

  phase = 0.5f;  // march half the cycle — no render()
  host.frame();
  int still = 0;
  for (const SkIPoint& p : lit0)
    still += host.pixel(p.x(), p.y()) == SK_ColorGREEN;
  // The window moved to the far side: (almost) none of the old pixels stay.
  EXPECT_LT((float)still, 0.2f * (float)lit0.size());
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
  // guard in Paint.cpp is justified by promotion, not by op rejection.
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

TEST(ComposeLayouts, RadialRadiusAtGivesEachChildItsOwnRing) {
  // `radiusAt` gives each child its own ring radius, so one Radial can draw
  // nested orbits rather than a single circle. The list may be shorter than
  // the child count: the tail falls back to `radiusFraction`, which is what
  // the second half of this case checks.
  Host host;
  std::vector<Element> dots;
  for (int i = 0; i < 4; ++i)
    dots.push_back(
        box().width(10).height(10).fill(red()).key("r" + std::to_string(i)));
  host.composer.render(box().child(
      layout(layouts::Radial{.radiusFraction = 0.8f, .radiusAt = {0.4f, 0.8f}})
          .width(200)
          .height(200)
          .children(dots)));
  host.frame();
  auto center = [&](const char* k) {
    auto r = host.composer.bounds(k);
    return SkPoint{r->centerX(), r->centerY()};
  };
  EXPECT_NEAR(center("r0").y(), 60, 1);   // top, INNER ring (0.4 → r=40)
  EXPECT_NEAR(center("r1").x(), 180, 1);  // right, outer (0.8 → r=80)
  EXPECT_NEAR(center("r2").y(), 180, 1);  // bottom, fallback 0.8
  EXPECT_NEAR(center("r3").x(), 20, 1);   // left, fallback 0.8
}
