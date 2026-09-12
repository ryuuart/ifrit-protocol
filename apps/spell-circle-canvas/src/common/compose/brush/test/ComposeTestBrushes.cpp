// The brush kinds as values: the ribbon, the pattern, the scatter and the
// art, what each paints along a run, the corner tile that sits on a bend,
// and the corner shapes a silhouette is cut to.

#include <utility>

#include "support/BrushTestSupport.h"

namespace {

Element shapedPanel(std::function<SkPath(SkSize)> outline, Decoration dec) {
  return box().child(box()
                         .width(100)
                         .height(100)
                         .shape(std::move(outline))
                         .fill(blue())
                         .foreground(std::move(dec)));
}

}  // namespace

// ---------------------------------------------------------------------------
// geometry::shapes::chamfered / geometry::shapes::notched — the two corner cuts
// the kernel's corners() (which only rounds) could not express.
TEST(ComposeShapes, ChamferCutsTheCornerAtFortyFiveDegrees) {
  Host host;
  host.composer.render(
      shapedPanel(geometry::shapes::chamfered(30), PathFormat{}));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);   // corner cut away
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);  // body intact
  EXPECT_EQ(host.pixel(5, 95), SK_ColorBLACK);  // all four by default
}

TEST(ComposeShapes, ChamferMaskCutsOnlyTheSelectedCorners) {
  Host host;
  host.composer.render(shapedPanel(
      geometry::shapes::chamfered(30, geometry::shapes::Corner::Diagonal),
      PathFormat{}));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);    // top-left cut
  EXPECT_EQ(host.pixel(95, 95), SK_ColorBLACK);  // bottom-right cut
  EXPECT_EQ(host.pixel(95, 5), SK_ColorBLUE);    // top-right square
  EXPECT_EQ(host.pixel(5, 95), SK_ColorBLUE);    // bottom-left square
}

TEST(ComposeShapes, NotchBitesARectangleOutOfTheCorner) {
  Host host;
  host.composer.render(shapedPanel(
      geometry::shapes::notched(20, 10, geometry::shapes::Corner::TopLeft),
      PathFormat{}));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);  // inside the bite
  EXPECT_EQ(host.pixel(5, 15), SK_ColorBLUE);  // below it
  EXPECT_EQ(host.pixel(25, 5), SK_ColorBLUE);  // right of it
  EXPECT_EQ(host.pixel(95, 5), SK_ColorBLUE);  // other corners untouched
}

// ---------------------------------------------------------------------------
// The Illustrator brush model: scatter/pattern/ribbon + the ops pipeline.
TEST(ComposeBrushes, ScatterInstancesArtAlongThePath) {
  Host host;
  host.composer.render(straightRun([] {
    brush::Scatter b;
    b.art = box().width(6).height(6).fill(red());
    b.spacing = 40;
    b.alignToPath = true;
    return b;
  }()));
  host.frame();
  // 160px run, spacing 40 → stamps at d = 20, 60, 100, 140 (x = 20+d).
  EXPECT_EQ(host.pixel(40, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(80, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(60, 100), SK_ColorBLACK);  // between stamps
}

TEST(ComposeBrushes, ScatterModSkipsAndLifts) {
  Host host;
  brush::Scatter b;
  b.art = box().width(6).height(6).fill(red());
  b.spacing = 40;
  b.modifier = [](const PathSample&, size_t i, size_t) {
    brush::StampModifier m;
    if (i % 2)
      m.skip = true;  // drop every other slot
    else
      m.dNormal = -20;  // lift the kept ones off the axis
    return m;
  };
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  EXPECT_EQ(host.pixel(40, 80), SK_ColorRED);    // slot 0 lifted (d=20)
  EXPECT_EQ(host.pixel(80, 80), SK_ColorBLACK);  // slot 1 skipped (d=60)
  EXPECT_EQ(host.pixel(80, 100), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(120, 80), SK_ColorRED);  // slot 2 lifted (d=100)
}

TEST(ComposeBrushes, RibbonTapersAndNibVariesWithAngle) {
  Host taperHost;
  taperHost.composer.render(straightRun(brush::presets::taper(16, 2, green())));
  taperHost.frame();
  auto bandHeight = [](Host& h, int x) {
    int lit = 0;
    for (int y = 70; y < 130; ++y) lit += h.pixel(x, y) == SK_ColorGREEN;
    return lit;
  };
  EXPECT_GT(bandHeight(taperHost, 30), 12);  // wide near the start
  EXPECT_LT(bandHeight(taperHost, 170), 6);  // narrow near the end

  // Calligraphic nib at 0°: a horizontal run lies ALONG the nib → thin.
  Host nib;
  nib.composer.render(
      straightRun(brush::presets::calligraphic(0, 16, green(), 0.2f)));
  nib.frame();
  EXPECT_LT(bandHeight(nib, 100), 6);
}

TEST(ComposeBrushes, RestyleWavesAnyDecoration) {
  Host host;
  host.composer.render(straightRun(
      brush::restyle(geometry::shapers::Wave{8, 24}, stroke(2, green()), 12)));
  host.frame();
  int offAxis = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7}) offAxis += host.pixel(x, 100 + dy) == SK_ColorGREEN;
  EXPECT_GT(offAxis, 10);  // the stroke followed the waved geometry
}

// ---------------------------------------------------------------------------
// Skia capabilities surfaced as compose values: sketchy jitter, SVG path
// data as an outline, the quantized clock and Perlin noise as a fill.
TEST(ComposeSeams, SketchyJitterLeavesTheAxis) {
  Host host, plain;
  host.composer.render(straightRun(brush::restyle(
      geometry::shapers::Jitter{8, 3.0f, 11}, stroke(2, green()))));
  plain.composer.render(straightRun(stroke(2, green())));
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
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(50, 50, 50, 50)
                      .shape(geometry::shapes::svg("M0 0 L100 0 L100 100 Z"))
                      .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(140, 70), SK_ColorRED);    // inside the hypotenuse
  EXPECT_EQ(host.pixel(60, 130), SK_ColorBLACK);  // outside it
  // Hit-testing follows the silhouette too.
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(50, 50, 50, 50)
                      .shape(geometry::shapes::svg("M0 0 L100 0 L100 100 Z"))
                      .fill(red())
                      .key("tri")));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({140, 70}).value_or(""), "tri");
  EXPECT_FALSE(host.composer.hitTest({60, 130}).has_value());
}

TEST(ComposeMaterials, QuantizeTimeStepsTheClock) {
  // uTime → red channel; with quantizeTime(2) samples inside one half-
  // second step resolve identically, and differ across steps.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uTime; half4 main(float2 p) {"
               "  return half4(fract(uTime), 0, 0, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  material::skia::Paint stepped =
      material::skia::Paint::sksl(fx).quantizeTime(2.0f);
  auto sampleAt = [&](material::skia::Paint& m, double seconds) {
    PaintContext ctx;
    ctx.size = {8, 8};
    ctx.elapsedSeconds = seconds;
    Fill f = resolveFill(m, ctx);
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
  material::skia::Paint continuous = material::skia::Paint::sksl(fx);
  EXPECT_NE(sampleAt(continuous, 0.6), sampleAt(continuous, 0.9));
}

// ---------------------------------------------------------------------------
// Stamped-brush bakes live with the INSTANCE, not inside the brush value.
TEST(ComposeBrushes, AStampBakeSurvivesABrushRebuiltEveryDescribe) {
  // A brush is a value an author rebuilds freely, so any cache stored in the
  // brush itself is empty on every describe. Here a FRESH Scatter value is
  // constructed each frame around pointer-stable art, on a node that
  // repaints every frame. The bake has to be keyed on the instance for the
  // art to rasterize once; keyed on the brush, re-describing a scene would
  // cost raster work, which nothing else in the library does.
  static int bakes;
  bakes = 0;
  const Element art =  // stable: its node pointer is the cache key
      box().width(8).height(8).child(
          custom([](SkCanvas& c, const PaintContext&) {
            ++bakes;
            SkPaint p;
            p.setColor(SK_ColorRED);
            c.drawRect(SkRect::MakeWH(8, 8), p);
          })
              .width(8)
              .height(8)
              .cache(Cache::None));
  Host host;
  auto tree = [&] {
    brush::Scatter s;  // fresh VALUE: empty member cache, on purpose
    s.art = art;
    s.spacing = 20;
    s.alignToPath = false;
    return box().child(box()
                           .width(120)
                           .height(120)
                           .cache(Cache::None)  // repaints every frame
                           .stroke(std::move(s)));
  };
  for (int i = 0; i < 5; ++i) {
    host.composer.render(tree());
    host.frame();
  }
  EXPECT_EQ(bakes, 1)
      << "a pointer-stable art re-baked under rebuilt brush values";
  int reds = 0;  // and the stamps really draw, wherever spacing lands them
  for (int x = 0; x < 130; ++x)
    for (int y = 0; y < 10; ++y) reds += host.pixel(x, y) == SK_ColorRED;
  EXPECT_GT(reds, 20);
}

TEST(ComposeBrushes, AFreshArtNodePerDescribeRebakesByContract) {
  // The same boundary from the other side (the case above is the hit
  // direction). The stamp cache is keyed on the art Element's NODE, so art
  // constructed INSIDE the describe is a fresh node every frame and re-bakes
  // every frame. That is the author's half of the bargain: keep the art
  // pointer-stable if you want the bake.
  //
  // It is also the safety direction. Because the key is node identity, a
  // fresh node is never served another node's bake, so art whose content
  // genuinely differs always reaches pixels. Any cache that hit here on
  // something weaker than real content identity would show the previous
  // frame's art instead — a stale picture with no diagnostic. A
  // content-identity bake cache could legitimately turn the
  // identical-content half of this into a hit, but only deliberately, and
  // the differing-content half below must keep failing it.
  static int bakes;
  bakes = 0;
  Host host;
  auto tree = [&](SkColor color) {
    Element art =  // fresh node EVERY call, on purpose — the contract's cost
        box().width(8).height(8).child(
            custom([color](SkCanvas& c, const PaintContext&) {
              ++bakes;
              SkPaint p;
              p.setColor(color);
              c.drawRect(SkRect::MakeWH(8, 8), p);
            })
                .width(8)
                .height(8)
                .cache(Cache::None));
    brush::Scatter s;
    s.art = std::move(art);
    s.spacing = 20;
    s.alignToPath = false;
    return box().child(box()
                           .width(120)
                           .height(120)
                           .cache(Cache::None)  // repaints every frame
                           .stroke(std::move(s)));
  };
  for (int i = 0; i < 4; ++i) {
    host.composer.render(tree(SK_ColorRED));
    host.frame();
  }
  EXPECT_EQ(bakes, 4) << "a fresh art node was served a bake it does not own";
  // And content that genuinely differs lands on pixels: the fifth
  // describe's art is GREEN, and green is what draws.
  host.composer.render(tree(SK_ColorGREEN));
  host.frame();
  EXPECT_EQ(bakes, 5);
  int greens = 0, reds = 0;
  for (int x = 0; x < 130; ++x)
    for (int y = 0; y < 10; ++y) {
      greens += host.pixel(x, y) == SK_ColorGREEN;
      reds += host.pixel(x, y) == SK_ColorRED;
    }
  EXPECT_GT(greens, 20);
  EXPECT_EQ(reds, 0) << "stale art survived a content change";
}

TEST(ComposeBrushes, PatternIntegerFitNeverTearsTheLastTile) {
  // 160px run, 25px tile → 6 slots stretched to 26.67px: coverage reaches
  // BOTH ends with no torn tail (the Illustrator fit rule).
  Host host;
  brush::Pattern b;
  b.side = box().width(25).height(8).fill(red());
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  EXPECT_EQ(host.pixel(42, 100), SK_ColorRED);   // first slot starts at run 0
  EXPECT_EQ(host.pixel(178, 100), SK_ColorRED);  // last slot ends at run end
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);  // continuous through middle
}

TEST(ComposeBrushes, PatternCornerTileSitsOnTheBend) {
  Host host;
  brush::Pattern b;
  b.side = box().width(20).height(4).fill(red());
  b.corner = brush::CornerArt{box().width(12).height(12).fill(blue()),
                              brush::CornerAlign::Bisector};
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(40, 40, 40, 40)
                      .shape([](SkSize s) {  // an L: right then down
                        SkPathBuilder p;
                        p.moveTo(0, 0);
                        p.lineTo(s.width(), 0);
                        p.lineTo(s.width(), s.height());
                        return p.detach();
                      })
                      .stroke(std::move(b))));
  host.frame();
  EXPECT_EQ(host.pixel(160, 40), SK_ColorBLUE);  // corner tile at the bend
  EXPECT_EQ(host.pixel(100, 40), SK_ColorRED);   // side tiles on the top leg
  EXPECT_EQ(host.pixel(160, 100), SK_ColorRED);  // and down the right leg
}

TEST(ComposeSeams, PerlinNoiseFillsWithVariation) {
  Host host(100, 100);
  host.composer.render(box().child(box().width(100).height(100).fill(
      material::skia::Paint::recipe(material::field::noise(0.05f, 4, 2.0f)))));
  host.frame();
  std::set<SkColor> distinct;
  for (int y = 10; y < 90; y += 8)
    for (int x = 10; x < 90; x += 8) distinct.insert(host.pixel(x, y));
  EXPECT_GT(distinct.size(), 30u);  // organic variation, not a flat fill
}

TEST(ComposeBrushes, PatternCornerTileAtTheClosedSeam) {
  // The seam of a closed contour is a corner like any other, even though no
  // moveTo/lineTo pair announces it: the last segment turns into the first.
  // A corner tile belongs there too.
  Host host;
  brush::Pattern b;
  b.side = box().width(20).height(4).fill(red());
  b.corner = brush::CornerArt{box().width(12).height(12).fill(blue()),
                              brush::CornerAlign::Bisector};
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(50, 50, 50, 50)
                      .shape([](SkSize s) {  // closed rect starting at (0,0)
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
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);  // the seam corner tile
}

// ---------------------------------------------------------------------------
// What a composite hands the brushes inside it
namespace {

/** A mark that records the context it was painted with, so a case can ask
 *  what a nested brush was handed as against what the node was handed. */
struct ContextProbe {
  struct Seen {
    int paints = 0;
    const StampCache* stamps = nullptr;
    SkMatrix toRoot = SkMatrix::I();
    SkSize rootSize = SkSize::MakeEmpty();
    SkRect outline = SkRect::MakeEmpty();
    double elapsedSeconds = 0.0;
  };
  std::shared_ptr<Seen> seen = std::make_shared<Seen>();

  bool operator==(const ContextProbe& o) const { return seen == o.seen; }
  void paint(SkCanvas&, const PaintContext& ctx) const {
    ++seen->paints;
    seen->stamps = ctx.stamps;
    seen->toRoot = ctx.toRoot;
    seen->rootSize = ctx.rootSize;
    seen->outline = ctx.outline.getBounds();
    seen->elapsedSeconds = ctx.elapsedSeconds;
  }
};

/** A mark that says it blends and paints nothing. */
struct BlendingMark {
  bool operator==(const BlendingMark&) const = default;
  bool blends() const { return true; }
  void paint(SkCanvas&, const PaintContext&) const {}
};

/** The context a composer hands a node: a stamp store, a place in the
 *  root, and the root's size. */
PaintContext nodeContext(StampCache& stamps) {
  PaintContext ctx;
  ctx.size = {100, 60};
  ctx.outline = SkPath::Rect(SkRect::MakeWH(100, 60));
  ctx.elapsedSeconds = 2.5;
  ctx.contentScale = 2.0f;
  ctx.animating = true;
  ctx.stamps = &stamps;
  ctx.toRoot = SkMatrix::Translate(30, 40);
  ctx.rootSize = {800, 600};
  return ctx;
}

}  // namespace

TEST(ComposeBrushes, ANestedBrushKeepsEverythingButTheOutline) {
  // A composite hands its children the context it was given with one
  // member replaced. Losing the rest costs the stamp cache (every nested
  // stamp re-rasterised each frame) and anchors a world-space material to
  // the node instead of the root — neither of which shows in a pixel of
  // the first frame.
  StampCache stamps;
  const PaintContext ctx = nodeContext(stamps);
  SkCanvas canvas(100, 60);  // no device: the probes record, they do not draw

  const ContextProbe woven, layered, restyled;
  brush::layers({woven}).paint(canvas, ctx);
  Brush{}.layer(layered).paint(canvas, ctx);
  brush::restyle(geometry::path::operations::PathOperation(
                     [](const SkPath& p) { return p; }),
                 restyled)
      .paint(canvas, ctx);

  for (const ContextProbe* probe : {&woven, &layered, &restyled}) {
    ASSERT_EQ(probe->seen->paints, 1);
    EXPECT_EQ(probe->seen->stamps, &stamps);
    EXPECT_EQ(probe->seen->toRoot, SkMatrix::Translate(30, 40));
    EXPECT_EQ(probe->seen->rootSize, SkSize::Make(800, 600));
    EXPECT_DOUBLE_EQ(probe->seen->elapsedSeconds, 2.5);
    EXPECT_EQ(probe->seen->outline, SkRect::MakeWH(100, 60));
  }
}

TEST(ComposeBrushes, ACompositeBlendsWhenAnythingInsideItDoes) {
  // The painter reads the top-level decoration alone, so a composite that
  // does not forward the word has its blending mark baked into a layer of
  // its own, where it resolves against transparent black.
  EXPECT_TRUE(Decoration(brush::layers({BlendingMark{}})).blends());
  EXPECT_TRUE(Decoration(Brush{}.layer(BlendingMark{})).blends());
  EXPECT_TRUE(
      Decoration(brush::restyle(geometry::path::operations::PathOperation(
                                    [](const SkPath& p) { return p; }),
                                BlendingMark{}))
          .blends());
  EXPECT_TRUE(
      Decoration(onEdges(geometry::path::Edge::Top, BlendingMark{})).blends());
  EXPECT_TRUE(Decoration(inset(4, BlendingMark{})).blends());
  // And a composite of marks that do not blend does not.
  EXPECT_FALSE(Decoration(brush::layers({ContextProbe{}})).blends());
}
