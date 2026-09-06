// What a leaf carries and what a composer is handed: a slot swapped under
// undisturbed siblings, the declared input colour space, and the content
// kinds a leaf draws from — a held path, a keyed shape, a replayed picture,
// a figure with its own box, and a keyed custom whose key must stay honest.

#include <cstring>  // memcmp — for the no-conversion control
#include <utility>

#include "support/CoreTestSupport.h"

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
    EXPECT_EQ(host.composer.declaredInputSpace(),
              Composer::InputSpace::EncodedSRGB);  // the default IS the truth
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

TEST(ComposeContent, AHeldPathShapePrunesWhereALambdaNeverCan) {
  // The commonest escape hatch in the tree: a path cooked in the author's
  // own coordinates handed to a node through a lambda. The lambda is
  // incomparable, so the node re-patches every describe however static the
  // drawing is; heldPath() hands the same path over as a value.
  SkPathBuilder pb;
  pb.addOval(SkRect::MakeXYWH(10, 10, 40, 40));
  const SkPath cooked = pb.detach();
  auto tree = [&cooked](bool held) {
    return box().child(held ? box().width(60).height(60).shape(heldPath(cooked))
                            : box().width(60).height(60).shape(
                                  [cooked](SkSize) { return cooked; }));
  };
  Host host;
  host.composer.render(tree(true));
  host.frame();
  host.composer.render(tree(true));  // the same cooked path: prune
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  // A DIFFERENT path is a change, even one drawn from the same numbers:
  // a rebuild carries a new generation, so equality stays conservative.
  SkPathBuilder rebuilt;
  rebuilt.addOval(SkRect::MakeXYWH(10, 10, 40, 40));
  const SkPath other = rebuilt.detach();
  host.composer.render(
      box().child(box().width(60).height(60).shape(heldPath(other))));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
  // The lambda spelling never settles.
  Host raw;
  raw.composer.render(tree(false));
  raw.frame();
  raw.composer.render(tree(false));
  EXPECT_GE(raw.composer.stats().patchedNodes, 1u);
}

TEST(ComposeContent, AKeyedShapeSettlesOnTheValueItClosesOver) {
  // A generator that is a function of a few numbers is a value wearing a
  // lambda's clothes. keyedShape() hands the numbers over as the identity:
  // equal keys prune, a changed key re-patches, and the keyless form is
  // conservative forever.
  auto tree = [](bool keyed, float radius) {
    auto fn = [radius](SkSize s) {
      SkPathBuilder pb;
      pb.addRRect(SkRRect::MakeRectXY(SkRect::MakeWH(s.width(), s.height()),
                                      radius, radius));
      return pb.detach();
    };
    auto leaf = keyed ? box().width(60).height(60).shape(radius, fn)
                      : box().width(60).height(60).shape(fn);
    return box().child(leaf.fill(red()));
  };
  Host host;
  host.composer.render(tree(true, 2.0f));
  host.frame();
  EXPECT_EQ(host.pixel(1, 1), SK_ColorRED);  // a 2 px corner keeps it
  host.composer.render(tree(true, 2.0f));    // same key: prune
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  // A changed key IS a change, and the new outline reaches the paint.
  host.composer.render(tree(true, 28.0f));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
  host.frame();
  EXPECT_EQ(host.pixel(1, 1), SK_ColorBLACK)
      << "the new radius never reached the outline";
  Host raw;
  raw.composer.render(tree(false, 2.0f));
  raw.frame();
  raw.composer.render(tree(false, 2.0f));
  EXPECT_GE(raw.composer.stats().patchedNodes, 1u);
}

TEST(ComposeContent, APictureLeafReplaysABakeAndStillPrunes) {
  // snapshot() hands back a picture and image() takes an asset, so a
  // caller who has baked a subtree had only custom() to draw it back
  // through — forfeiting the pruning and the caching the bake was taken
  // for. The picture leaf keeps both.
  Host host;
  sk_sp<SkPicture> baked =
      snapshot(box().child(box().width(40).height(40).fill(red())), fonts());
  ASSERT_NE(baked, nullptr);
  auto tree = [&baked] {
    return box().child(picture(baked, SkSize::Make(40, 40)).key("bake"));
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorRED);
  // It sizes itself from what it was recorded at, so a snapshot drops
  // into a layout without being measured again.
  const auto placed = host.composer.bounds("bake");
  ASSERT_TRUE(placed.has_value());
  EXPECT_EQ(placed->width(), 40.0f);
  EXPECT_EQ(placed->height(), 40.0f);
  // The same picture is the same drawing: the node settles.
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  // Given other dims the picture is scaled to them.
  host.composer.render(box().child(
      picture(baked, SkSize::Make(40, 40)).key("bake").width(80).height(80)));
  host.frame();
  EXPECT_EQ(host.pixel(70, 70), SK_ColorRED);
}

TEST(ComposeContent, APathFigureCarriesItsOwnBox) {
  // An absolute path re-based into its own bounds: the node's rect is the
  // path's bounds grown by the bleed, and the shape is the path moved to
  // that corner — so the figure lands where it was worked out.
  SkPathBuilder pb;
  pb.addRect(SkRect::MakeXYWH(30, 40, 20, 10));
  Host host;
  host.composer.render(
      positioned().child(pathFigure(pb.detach(), 4.0f).key("fig").fill(red())));
  host.frame();
  const auto placed = host.composer.bounds("fig");
  ASSERT_TRUE(placed.has_value());
  EXPECT_EQ(placed->left(), 26.0f);
  EXPECT_EQ(placed->top(), 36.0f);
  EXPECT_EQ(placed->width(), 28.0f);
  EXPECT_EQ(placed->height(), 18.0f);
  EXPECT_EQ(host.pixel(40, 45), SK_ColorRED);    // inside the figure
  EXPECT_EQ(host.pixel(28, 38), SK_ColorBLACK);  // the bleed is not the mark
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
