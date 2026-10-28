// The brush kinds as values: the ribbon, the pattern, the scatter and the
// art, what each paints along a run, the corner tile that sits on a bend,
// the corner shapes a silhouette is cut to, and how far a ribbon
// reaches when its profile rather than its own defaults decides.

#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/skia/Paint.h>

#include <utility>

#include "cache/StampCache.h"
#include "support/BrushTestSupport.h"
#include "support/Rows.h"

namespace {

Element shapedPanel(Shape outline, Decoration dec) {
  return box().children({box()
                             .width(100)
                             .height(100)
                             .shape(std::move(outline))
                             .fill(blue())
                             .foreground(std::move(dec))});
}

}  // namespace

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
TEST(ComposeBrushEngine, EveryLayerRidesTheSharedPipelineAndItsOwnSuffix) {
  sigil::compose::test::checkRows({
      {"ARestyleWavesAnyDecoration",
       [&] {
         Host host;
         host.composer.render(straightRun(brush::restyle(
             geometry::shapers::Wave{8, 24}, stroke(2, green()), 12)));
         host.frame();
         int offAxis = 0;
         for (int x = 30; x < 170; x += 2)
           for (int dy : {-7, 7})
             offAxis += host.pixel(x, 100 + dy) == SK_ColorGREEN;
         EXPECT_GT(offAxis, 10);  // the stroke followed the waved geometry
       }},
      // Skia capabilities surfaced as compose values: sketchy jitter, SVG path
      // data as an outline, the quantized clock and Perlin noise as a fill.
      {"ARestyleReachesAJitteredHand",
       [&] {
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
       }},
  });
}

TEST(ComposeShapeWords,
     AShapeIsTheSilhouetteTheFillTheStrokeAndTheHitTestTrace) {
  // A right triangle authored as an SVG d-string, stretched to the node.
  Host host;
  host.composer.render(box().children(
      {box()
           .absolute()
           .inset(50)
           .shape(geometry::shapes::svg("M0 0 L100 0 L100 100 Z"))
           .fill(red())}));
  host.frame();
  EXPECT_EQ(host.pixel(140, 70), SK_ColorRED);    // inside the hypotenuse
  EXPECT_EQ(host.pixel(60, 130), SK_ColorBLACK);  // outside it
  // Hit-testing follows the silhouette too.
  host.composer.render(box().children(
      {box()
           .absolute()
           .inset(50)
           .shape(geometry::shapes::svg("M0 0 L100 0 L100 100 Z"))
           .fill(red())
           .key("tri")}));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({140, 70}).value_or(""), "tri");
  EXPECT_FALSE(host.composer.hitTest({60, 130}).has_value());
}

TEST(ComposeFields, AMaterialFieldReachesTheFillAndTheFilterItIsGiven) {
  // uTime → red channel; with quantizeTime(2) samples inside one half-
  // second step resolve identically, and differ across steps.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uTime; half4 main(float2 p) {"
               "  return half4(fract(uTime), 0, 0, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  material::Paint stepped = material::skia::sksl(fx).quantizeTime(2.0f);
  auto sampleAt = [&](material::Paint& m, double seconds) {
    PaintContext ctx;
    ctx.size = {8, 8};
    ctx.elapsedSeconds = seconds;
    Fill f = resolveFill(material::skia::base(m), ctx);
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
    SkPaint p;
    p.setShader(
        material::skia::staticShader(material::skia::paint(*f.material())));
    s->getCanvas()->drawPaint(p);
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    s->readPixels(bm.pixmap(), 1, 1);
    return bm.getColor(0, 0);
  };
  EXPECT_EQ(sampleAt(stepped, 0.6), sampleAt(stepped, 0.9));  // same step
  EXPECT_NE(sampleAt(stepped, 0.6), sampleAt(stepped, 1.1));  // next step
  material::Paint continuous = material::skia::sksl(fx);
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
      box().width(8).height(8).children({custom([](sigil::draw::Pen& pen) {
                                           SkCanvas& c = *pen.canvas();
                                           ++bakes;
                                           SkPaint p;
                                           p.setColor(SK_ColorRED);
                                           c.drawRect(SkRect::MakeWH(8, 8), p);
                                         })
                                             .width(8)
                                             .height(8)
                                             .cache(Cache::None)});
  Host host;
  auto tree = [&] {
    brush::Scatter s;  // fresh VALUE: empty member cache, on purpose
    s.art = art;
    s.spacing = 20;
    s.alignToPath = false;
    return box().children({box()
                               .width(120)
                               .height(120)
                               .cache(Cache::None)  // repaints every frame
                               .stroke(std::move(s))});
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
        box().width(8).height(8).children(
            {custom([color](sigil::draw::Pen& pen) {
               SkCanvas& c = *pen.canvas();
               ++bakes;
               SkPaint p;
               p.setColor(color);
               c.drawRect(SkRect::MakeWH(8, 8), p);
             })
                 .width(8)
                 .height(8)
                 .cache(Cache::None)});
    brush::Scatter s;
    s.art = std::move(art);
    s.spacing = 20;
    s.alignToPath = false;
    return box().children({box()
                               .width(120)
                               .height(120)
                               .cache(Cache::None)  // repaints every frame
                               .stroke(std::move(s))});
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

// ---------------------------------------------------------------------------
// What a composite hands the brushes inside it
namespace {

/** A mark that records the context it was painted with, so a case can ask
 *  what a nested brush was handed as against what the node was handed. */
struct ContextProbe {
  struct Seen {
    int paints = 0;
    const StampCache* stamps = nullptr;
    geometry::path::Transform toRoot;
    glm::vec2 rootSize{0, 0};
    SkRect outline = SkRect::MakeEmpty();
    double elapsedSeconds = 0.0;
  };
  std::shared_ptr<Seen> seen = std::make_shared<Seen>();

  bool operator==(const ContextProbe& o) const { return seen == o.seen; }
  void paint(sigil::draw::Pen& pen, const PaintContext& ctx) const {
    ++seen->paints;
    seen->stamps = ctx.stamps;
    seen->toRoot = ctx.toRoot;
    seen->rootSize = ctx.rootSize;
    seen->outline = geometry::path::toSk(ctx.outline).getBounds();
    seen->elapsedSeconds = ctx.elapsedSeconds;
  }
};

/** A mark that says it blends and paints nothing. */
struct BlendingMark {
  bool operator==(const BlendingMark&) const = default;
  bool blends() const { return true; }
  void paint(sigil::draw::Pen& pen, const PaintContext&) const {}
};

/** The context a composer hands a node: a stamp store, a place in the
 *  root, and the root's size. */
PaintContext nodeContext(StampCache& stamps) {
  PaintContext ctx;
  ctx.size = {100, 60};
  ctx.outline = geometry::path::Outline::rectangle({{0, 0}, {100, 60}});
  ctx.elapsedSeconds = 2.5;
  ctx.contentScale = 2.0f;
  ctx.animating = true;
  ctx.stamps = &stamps;
  ctx.toRoot = geometry::path::Transform::translate({30, 40});
  ctx.rootSize = {800, 600};
  return ctx;
}

}  // namespace

TEST(ComposeBrushValues, ACompositeForwardsWhatItsMarksDeclare) {
  sigil::compose::test::checkRows({
      {"ANestedMarkKeepsEverythingButTheOutline",
       [&] {
         // A composite hands its children the context it was given with one
         // member replaced. Losing the rest costs the stamp cache (every nested
         // stamp re-rasterised each frame) and anchors a world-space material
         // to the node instead of the root — neither of which shows in a pixel
         // of the first frame.
         StampCache stamps;
         const PaintContext ctx = nodeContext(stamps);
         SkCanvas canvas(100,
                         60);  // no device: the probes record, they do not draw

         const ContextProbe woven, layered, restyled;
         paintWithPen(brush::layers({woven}), canvas, ctx);
         paintWithPen(Brush{}.layer(layered), canvas, ctx);
         paintWithPen(
             brush::restyle(geometry::path::Shaper::incomparable(
                                [](const geometry::path::Outline& outline) {
                                  return outline;
                                }),
                            restyled),
             canvas, ctx);

         for (const ContextProbe* probe : {&woven, &layered, &restyled}) {
           ASSERT_EQ(probe->seen->paints, 1);
           EXPECT_EQ(probe->seen->stamps, &stamps);
           EXPECT_EQ(probe->seen->toRoot,
                     geometry::path::Transform::translate({30, 40}));
           EXPECT_EQ(probe->seen->rootSize, glm::vec2(800, 600));
           EXPECT_DOUBLE_EQ(probe->seen->elapsedSeconds, 2.5);
           EXPECT_EQ(probe->seen->outline, SkRect::MakeWH(100, 60));
         }
       }},
      {"ACompositeBlendsWhenAnythingInsideItDoes",
       [&] {
         // The painter reads the top-level decoration alone, so a composite
         // that does not forward the word has its blending mark baked into a
         // layer of its own, where it resolves against transparent black.
         EXPECT_TRUE(Decoration(brush::layers({BlendingMark{}})).blends());
         EXPECT_TRUE(Decoration(Brush{}.layer(BlendingMark{})).blends());
         EXPECT_TRUE(
             Decoration(
                 brush::restyle(geometry::path::Shaper::incomparable(
                                    [](const geometry::path::Outline& outline) {
                                      return outline;
                                    }),
                                BlendingMark{}))
                 .blends());
         EXPECT_TRUE(
             Decoration(onEdges(geometry::path::Edge::Top, BlendingMark{}))
                 .blends());
         EXPECT_TRUE(Decoration(inset(4, BlendingMark{})).blends());
         EXPECT_TRUE(Decoration(DecorationStack{{BlendingMark{}}}).blends());
         // And a composite of marks that do not blend does not.
         EXPECT_FALSE(Decoration(brush::layers({ContextProbe{}})).blends());
         EXPECT_FALSE(Decoration(DecorationStack{{ContextProbe{}}}).blends());
       }},
      {"AStackKeepsMarkWidthSeparateFromOverflow",
       [&] {
         PathFormat wide;
         wide.width = 9.0f;
         wide.align = PathFormat::Align::Inner;
         PathFormat narrow = wide;
         narrow.width = 3.0f;
         const Decoration stack = DecorationStack{{wide, narrow}};
         EXPECT_FLOAT_EQ(stack.bleed({100, 100}), 0.0f);
         EXPECT_FLOAT_EQ(stack.reach({100, 100}), 9.0f);
       }},
  });
}

// -------------------------------------------------------------------------
// How far a ribbon reaches when its profile, not its own defaults,
// decides.

namespace {

/** A flow band's width law: wide at the start, narrow at the end, with the
 *  wide end far larger than any ribbon's default widths. That gap is the
 *  point — it is what makes an under-declared reach visible. */
struct FlowLaw {
  float start = 166.0f, end = 12.0f;
  float across(float along) const { return start + (end - start) * along; }
  float max() const { return std::max(start, end); }
  bool operator==(const FlowLaw&) const = default;
};

}  // namespace

TEST(ComposeBrushes, ARibbonsReachIsDERIVEDFromItsProfile) {
  // bleed() grows the recording cull, so it has to know how far the widest
  // part of a ribbon reaches. A width supplied as a callable cannot be
  // asked, so the declared reach falls back to the fixed widths underneath
  // it and a much wider band is silently CLIPPED — which reads as a
  // rendering bug rather than as a missing declaration.
  //
  // `max()` is REQUIRED by the Profile concept, so the number cannot go
  // unsaid: the trap is structurally impossible rather than merely
  // documented.
  //
  // Asserted on bleed() DIRECTLY. Observing the cull through rendered pixels
  // gives the same answer either way, because what reaches the canvas also
  // depends on cache-mode decisions this test is not pinning.
  brush::Ribbon plain;
  plain.widthStart = 12.0f;
  plain.widthEnd = 4.0f;
  EXPECT_FLOAT_EQ(plain.bleed(), 12.0f);

  brush::Ribbon flow = plain;
  flow.width = geometry::path::Profile(FlowLaw{});
  EXPECT_FLOAT_EQ(flow.bleed(), 166.0f)
      << "the profile knows its own reach; nobody had to declare it";

  // A profile is not optional-with-a-fallback: once set it OWNS the reach,
  // so the fixed widths underneath it never inflate the cull either.
  flow.width = geometry::path::Profile(FlowLaw{2.0f, 2.0f});
  EXPECT_FLOAT_EQ(flow.bleed(), 2.0f);

  // And it participates in equality, so changing the law repatches.
  brush::Ribbon a = plain, b = plain;
  b.width = geometry::path::Profile(FlowLaw{});
  EXPECT_FALSE(a == b);
}
