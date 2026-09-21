// The shape a node is cut against as a VALUE: what an outline fills and
// clips, the corners it cuts, the stock generators and the raw escape
// hatch, when two of them compare equal — which is what lets a shaped
// node prune — and the hit test that honours the shape rather than the
// box.

#include <include/core/SkPathBuilder.h>

#include "support/ShapeTestSupport.h"

TEST(ComposeShapeValues, CustomOutlineShapesFillAndClip) {
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
      box()
          .width(100)
          .height(100)
          .overflow(Overflow::Clip)
          .shape(diamond)
          .fill(red())
          .children({box().inset(0).absolute().fill(green())}));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorGREEN);  // clipped child inside
  EXPECT_EQ(host.pixel(3, 3), SK_ColorBLACK);    // box corner outside shape
}

TEST(ComposeShapeValues, RoundedOutlineCutsSharpCorners) {
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
  host.composer.render(
      box().children({box()
                          .width(100)
                          .height(100)
                          .shape(geometry::shapes::rounded(diamond, 20))
                          .fill(red())}));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);   // body intact
  EXPECT_EQ(host.pixel(50, 3), SK_ColorBLACK);  // sharp tip rounded away
  EXPECT_EQ(host.pixel(50, 12), SK_ColorRED);   // rounded apex below y≈7

  host.composer.render(box().children({box()
                                           .width(100)
                                           .height(100)
                                           .shape(geometry::shapes::star(5))
                                           .fill(red())}));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);    // star body
  EXPECT_NE(host.pixel(50, 3), SK_ColorBLACK);   // sharp top point present
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLACK);  // gap between arms
}

TEST(ComposeShapeValues, PerCornerRadiiIndependent) {
  Host host;
  // Sharp top-left, heavily rounded top-right.
  host.composer.render(box().children(
      {box().width(100).height(100).borderRadius({0, 40, 0, 0}).fill(red())}));
  host.frame();
  EXPECT_EQ(host.pixel(2, 2), SK_ColorRED);     // sharp TL corner filled
  EXPECT_EQ(host.pixel(97, 2), SK_ColorBLACK);  // rounded TR corner empty
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);
}

// ---------------------------------------------------------------------------
// A shape VALUE: an outline generator is a comparable scheme, so a shaped
// node can prune. This matters more than it looks — a node whose outline
// cannot compare re-patches on every describe, which throws away its
// recording and its bake, so a single such node in an otherwise static
// tree puts the whole tree back on the live-paint path.

TEST(ComposeShapeValues, PolygonAndSquircleSilhouettes) {
  Host host;
  host.composer.render(
      box().row().children({box()
                                .width(90)
                                .height(90)
                                .shape(geometry::shapes::polygon(6))
                                .fill(red()),
                            box()
                                .width(90)
                                .height(90)
                                .shape(geometry::shapes::squircle(4))
                                .fill(green())}));
  host.frame();
  EXPECT_EQ(host.pixel(45, 45), SK_ColorRED);     // hexagon body
  EXPECT_EQ(host.pixel(2, 2), SK_ColorBLACK);     // hexagon corner cut
  EXPECT_EQ(host.pixel(135, 45), SK_ColorGREEN);  // squircle body
  EXPECT_EQ(host.pixel(92, 2), SK_ColorBLACK);    // squircle corner soft
  EXPECT_EQ(host.pixel(135, 3), SK_ColorGREEN);   // but edge midpoints full
}

TEST(ComposeShapeValues, AStockGeneratorShapePrunes) {
  // The core case: a shaped node re-described identically must patch nothing
  // and re-record nothing. If structural equality refuses shaped nodes
  // outright, this tree re-records every frame for its entire life and
  // nothing reports it.
  Host host;
  auto tree = [] {
    return box().children({box()
                               .width(100)
                               .height(100)
                               .shape(geometry::shapes::star(5, 0.5f, 0.12f))
                               .fill(red())});
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
    return box().children({box()
                               .width(100)
                               .height(100)
                               .shape(geometry::shapes::polygon(sides))
                               .fill(red())});
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
  // A hand-rolled OutlineFunction cannot compare, so its node re-patches on
  // every describe. That is the conservative answer and it is deliberate:
  // claiming equality for two callables would prune a node whose outline had in
  // fact changed. An author who needs the prune wraps the node in memo().
  Host host;
  auto tree = [] {
    return box().children({box()
                               .width(100)
                               .height(100)
                               .shape([](SkSize s) {
                                 SkPathBuilder b;
                                 b.addOval(
                                     SkRect::MakeWH(s.width(), s.height()));
                                 return b.detach();
                               })
                               .fill(red())});
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
  // the copy is what the test compares
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
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
  EXPECT_TRUE(Shape(geometry::shapes::rounded(geometry::shapes::star(5), 8)) ==
              Shape(geometry::shapes::rounded(geometry::shapes::star(5), 8)));
  EXPECT_FALSE(Shape(geometry::shapes::rounded(geometry::shapes::star(5), 8)) ==
               Shape(geometry::shapes::rounded(geometry::shapes::star(5), 9)));
  EXPECT_FALSE(Shape(geometry::shapes::rounded(geometry::shapes::star(5), 8)) ==
               Shape(geometry::shapes::rounded(geometry::shapes::star(6), 8)));
  auto lambda = [](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
  EXPECT_FALSE(Shape(geometry::shapes::rounded(lambda, 8)) ==
               Shape(geometry::shapes::rounded(lambda, 8)));
}

TEST(ComposeShapeValues, AnSvgSilhouetteComparesByItsGeometry) {
  // svg() parses its d-string once into an SkPath, and SkPath has
  // structural equality, so an svg() silhouette compares by geometry and
  // prunes where the raw-callable hatch above cannot.
  EXPECT_TRUE(Shape(geometry::shapes::svg("M0 0L10 0L10 10Z")) ==
              Shape(geometry::shapes::svg("M0 0L10 0L10 10Z")));
  EXPECT_FALSE(Shape(geometry::shapes::svg("M0 0L10 0L10 10Z")) ==
               Shape(geometry::shapes::svg("M0 0L10 0L5 10Z")));
}

TEST(ComposeShapeValues, KeyedParametricIsAValueUnkeyedIsNot) {
  auto fig8 = [](float t) { return SkPoint{std::sin(2 * t), std::sin(t)}; };
  // Unkeyed: the callable is the identity and cannot compare.
  EXPECT_FALSE(Shape(geometry::shapes::parametric(fig8, 0, 6.2832f, 720)) ==
               Shape(geometry::shapes::parametric(fig8, 0, 6.2832f, 720)));
  // Keyed: (key, window, samples) is the identity — the author's contract
  // that one key names one curve.
  EXPECT_TRUE(
      Shape(geometry::shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
      Shape(geometry::shapes::parametric("fig8", fig8, 0, 6.2832f, 720)));
  EXPECT_FALSE(
      Shape(geometry::shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
      Shape(geometry::shapes::parametric("fig8", fig8, 0, 6.2832f, 360)));
  EXPECT_FALSE(
      Shape(geometry::shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
      Shape(geometry::shapes::parametric("orbit", fig8, 0, 6.2832f, 720)));
  // The named families carry their identity in their parameters.
  EXPECT_TRUE(Shape(geometry::shapes::lissajous(3, 2)) ==
              Shape(geometry::shapes::lissajous(3, 2)));
  EXPECT_FALSE(Shape(geometry::shapes::lissajous(3, 2)) ==
               Shape(geometry::shapes::lissajous(5, 4)));
  EXPECT_TRUE(Shape(geometry::shapes::rose(3)) ==
              Shape(geometry::shapes::rose(3)));
  EXPECT_FALSE(Shape(geometry::shapes::spiral(3.0f)) ==
               Shape(geometry::shapes::spiral(4.0f)));
}

TEST(ComposeShapeValues,
     AGeneratedSilhouetteReDescribedEveryFrameKeepsItsBake) {
  // The steady state the prune exists for: a texture-cached node whose shape
  // is a generator, re-described every frame. If the shape does not compare,
  // the node patches, the patch drops the bake, and the node re-rasterizes
  // every frame — a single such node dominates a frame that is otherwise
  // entirely cached.
  Host host;
  auto tree = [] {
    return box().children({box()
                               .width(120)
                               .height(120)
                               .shape(geometry::shapes::circle())
                               .fill(red())
                               .cache(Cache::Texture)});
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

TEST(ComposeQueries, HitTestHonorsShapeAndRotation) {
  Host host;
  host.composer.render(box().children(
      {box()
           .key("star")
           .width(100)
           .height(100)
           .shape(geometry::shapes::star(5))
           .fill(red()),
       box()
           .key("spun")
           .width(80)
           .height(20)
           .inset({.top = 140, .right = 60, .bottom = 40, .left = 60})
           .absolute()
           .rotate(90.0f)
           .fill(green())}));
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
