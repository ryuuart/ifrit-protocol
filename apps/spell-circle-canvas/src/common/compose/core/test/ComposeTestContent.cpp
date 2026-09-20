// What a leaf carries and what a composer is handed: a slot swapped under
// undisturbed siblings, the declared input colour space, and the content
// kinds a leaf draws from — a held path, a keyed shape, a replayed picture,
// a figure with its own box, a keyed custom whose key must stay honest,
// the sampling an image leaf is magnified with, the box a picture and
// an atlas region meet, and what a paint program is handed.

#include <cstring>  // memcmp — for the no-conversion control
#include <utility>

#include "support/CoreTestSupport.h"

TEST(ComposeSlots, SlotUpdatesWithoutDisturbingSiblings) {
  static int staticRuns;
  staticRuns = 0;
  Host host;
  host.composer.render(box().row().gap(10).children(
      {custom([](SkCanvas& c, const PaintContext& ctx) {
         ++staticRuns;
         SkPaint p;
         p.setColor(SK_ColorRED);
         c.drawRect(SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
       })
           .width(50)
           .height(50),
       slot("live").width(80).height(50)}));
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
    h.composer.render(box().children({box().width(160).height(120).fill(
        material::skia::Paint::linear({0, 0}, {160, 120},
                                      {{0.0f, {1, 0, 0, 1}},
                                       {0.5f, {0.25f, 0.5f, 0.25f, 0.8f}},
                                       {1.0f, {0, 0, 1, 1}}}))}));
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

TEST(ComposeContent, EverySpellingOfOneTextDrawsTheSamePixels) {
  // One text type: a `char` literal, a u8 literal, a std::string, a
  // std::u8string and a view of either are the SAME leaf, byte for byte, and
  // no call site widens a string to reach the factory.
  const std::string held = "Wg";
  const std::u8string wide = u8"Wg";
  auto plate = [](Element leaf) {
    Host h;
    h.composer.render(box().children({std::move(leaf)}));
    h.frame();
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
    h.surface->readPixels(bm.pixmap(), 0, 0);
    return bm;
  };
  const SkBitmap plain = plate(text("Wg", whiteStyle(24)));
  for (const SkBitmap& other :
       {plate(text(u8"Wg", whiteStyle(24))), plate(text(held, whiteStyle(24))),
        plate(text(wide, whiteStyle(24))),
        plate(text(std::string_view(held), whiteStyle(24))),
        plate(text(std::u8string_view(wide), whiteStyle(24)))}) {
    ASSERT_EQ(plain.computeByteSize(), other.computeByteSize());
    EXPECT_EQ(0, std::memcmp(plain.getPixels(), other.getPixels(),
                             plain.computeByteSize()))
        << "the spelling of the bytes must not reach the drawing";
  }
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
    return box().children(
        {held ? box().width(60).height(60).shape(heldPath(cooked))
              : box().width(60).height(60).shape([cooked] { return cooked; })});
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
      box().children({box().width(60).height(60).shape(heldPath(other))}));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
  // The lambda spelling never settles.
  Host raw;
  raw.composer.render(tree(false));
  raw.frame();
  raw.composer.render(tree(false));
  EXPECT_GE(raw.composer.stats().patchedNodes, 1u);
}

TEST(ComposeContent, AnOutlineThatIgnoresTheBoxNeedNotNameIt) {
  // The laid-out size is OFFERED to an outline, not demanded: a generator
  // that draws the same path whatever the box is names nothing, and keyed on
  // the value it closes over it compares and prunes exactly as a sized one.
  auto tree = [](float inset) {
    return box().children(
        {box().width(60).height(60).fill(red()).shape(inset, [inset] {
          SkPathBuilder pb;
          pb.addOval(
              SkRect::MakeXYWH(inset, inset, 60 - 2 * inset, 60 - 2 * inset));
          return pb.detach();
        })});
  };
  Host host;
  host.composer.render(tree(0.0f));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
  host.composer.render(tree(0.0f));  // the same key: prune
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  // A changed key is a change, and the new nullary outline reaches the paint.
  host.composer.render(tree(25.0f));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
  host.frame();
  EXPECT_EQ(host.pixel(2, 30), SK_ColorBLACK)
      << "the inset outline never reached the paint";
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
    return box().children({leaf.fill(red())});
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
  sk_sp<SkPicture> baked = snapshot(
      box().children({box().width(40).height(40).fill(red())}), fonts());
  ASSERT_NE(baked, nullptr);
  auto tree = [&baked] {
    return box().children({picture(baked, SkSize::Make(40, 40)).key("bake")});
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
  host.composer.render(box().children(
      {picture(baked, SkSize::Make(40, 40)).key("bake").width(80).height(80)}));
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
  host.composer.render(positioned().children(
      {pathFigure(pb.detach(), 4.0f).key("fig").fill(red())}));
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
    return box().children({key ? custom(key, program).width(60).height(60)
                               : custom(program).width(60).height(60)});
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

// -------------------------------------------------------------------------
// How an image leaf is sampled when it is magnified.

TEST(ComposeContent, SamplingReachesTheImageLeaf) {
  // Every blessed image path hardcoded kLinear, so pixel art, tilemaps
  // and simulation buffers drawn through image() were silently blurred.
  // Material::image() has always taken sampling; the element factory did
  // not, so the fix was discoverable only by diffing two signatures.
  auto atlas = twoCellAtlas();  // 32x16: left half red, right half green
  auto magnified = [&](SkSamplingOptions options) {
    Host host(200, 200);
    host.composer.render(box().children({image(atlas)
                                             .sampling(options)
                                             .absolute()
                                             .left(0)
                                             .top(0)
                                             .width(200)
                                             .height(100)}));
    host.frame();
    // Count columns straddling the red/green seam that are NEITHER pure
    // red nor pure green — the blend band linear filtering invents.
    int blended = 0;
    for (int x = 80; x < 120; ++x) {
      const SkColor c = host.pixel(x, 50);
      const bool pureRed = SkColorGetR(c) > 200 && SkColorGetG(c) < 40;
      const bool pureGreen = SkColorGetG(c) > 200 && SkColorGetR(c) < 40;
      blended += !pureRed && !pureGreen;
    }
    return blended;
  };

  EXPECT_GT(magnified(SkSamplingOptions(SkFilterMode::kLinear)), 3);
  EXPECT_LE(magnified(SkSamplingOptions(SkFilterMode::kNearest)), 1);
}

// -------------------------------------------------------------------------
// What a picture and an atlas region meet their box as, and what a
// paint program is handed: the parameters it named, the host scale,
// and the ticker's state.

TEST(ComposeContent, APictureMeetsItsBoxTheWayTheFitSays) {
  // A raw picture is a leaf with the wrap written once, and the FIT is
  // said where the picture is rather than as a matrix the caller builds.
  SkBitmap wide;
  wide.allocN32Pixels(40, 10);
  wide.eraseColor(SK_ColorRED);
  const sk_sp<SkImage> picture = wide.asImage();
  const auto shown = [&](material::skia::Fit fit) {
    Host host;
    host.composer.render(
        box().children({box()
                            .width(100)
                            .height(100)
                            .alignItems(Align::Center)
                            .justify(Justify::Center)
                            .children({image(picture, fit).key("fig")})}));
    host.frame();
    const std::optional<SkRect> fig = host.composer.bounds("fig");
    return fig.value_or(SkRect::MakeEmpty());
  };
  // Both axes independently: the picture's proportions are the box's.
  EXPECT_EQ(shown(material::skia::Fit::Stretch), SkRect::MakeWH(100, 100));
  // As large as fits, the slack on the long axis: 4:1 in a square box.
  const SkRect held = shown(material::skia::Fit::Contain);
  EXPECT_FLOAT_EQ(held.width(), 100);
  EXPECT_FLOAT_EQ(held.height(), 25);
  // No slack at all: the short axis fills and the long one overflows.
  const SkRect filled = shown(material::skia::Fit::Cover);
  EXPECT_FLOAT_EQ(filled.height(), 100);
  EXPECT_FLOAT_EQ(filled.width(), 400);
  // A picture that is not there draws nothing and takes no room.
  Host bare;
  bare.composer.render(box().children({image(sk_sp<SkImage>()).key("none")}));
  bare.frame();
  const std::optional<SkRect> empty = bare.composer.bounds("none");
  ASSERT_TRUE(empty.has_value());
  EXPECT_FLOAT_EQ(empty->height(), 0);
}

TEST(ComposeContent, ImageRegionDrawsAtlasCell) {
  Host host;
  auto atlas = twoCellAtlas();
  host.composer.render(
      box().row().children({image(atlas)
                                .region(SkRect::MakeXYWH(16, 0, 16, 16))
                                .width(50)
                                .height(50),
                            image(atlas).width(50).height(50)}));
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorGREEN);  // region: right cell only
  EXPECT_EQ(host.pixel(60, 25), SK_ColorRED);    // whole atlas: left half
}

TEST(ComposePaint, APaintProgramNamesOnlyTheParametersItReads) {
  // The canvas and the context are both offered, and a program takes the
  // prefix it reads: the canvas alone, both, or neither. All three paint,
  // so no caller spells a parameter in order to ignore it.
  Host host;
  int nullaryRuns = 0;
  const auto square = [](SkCanvas& canvas, SkColor color) {
    SkPaint paint;
    paint.setColor(color);
    paint.setAntiAlias(false);
    canvas.drawRect(SkRect::MakeWH(20, 20), paint);
  };
  SkSize offered = SkSize::MakeEmpty();
  host.composer.render(box().row().children(
      {custom([&](SkCanvas& canvas) { square(canvas, SK_ColorGREEN); })
           .width(20)
           .height(20)
           .cache(Cache::None),
       custom([&](SkCanvas& canvas, const PaintContext& ctx) {
         offered = ctx.size;
         square(canvas, SK_ColorRED);
       })
           .width(20)
           .height(20)
           .cache(Cache::None),
       custom([&] { ++nullaryRuns; })
           .width(20)
           .height(20)
           .cache(Cache::None)}));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(30, 10), SK_ColorRED);
  EXPECT_EQ(offered, SkSize::Make(20, 20));
  EXPECT_EQ(nullaryRuns, 1) << "a program that reads neither still runs";
}

TEST(ComposePaint, ContentScaleReportsHostScale) {
  Host host;
  float seen = 0.0f;
  host.composer.render(
      box().children({custom([&seen](SkCanvas&, const PaintContext& ctx) {
                        seen = ctx.contentScale;
                      })
                          .width(50)
                          .height(50)
                          .cache(Cache::None)}));
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
      box().children({box().width(40).height(40).fill(red()).opacity(
                          animate(motion::from(0.0f).to(1.0f), {400ms})),
                      custom([&seen](SkCanvas&, const PaintContext& ctx) {
                        seen = ctx.animating;
                      })
                          .width(10)
                          .height(10)
                          .cache(Cache::None)}));
  host.frame(0.016);
  EXPECT_TRUE(seen) << "an entrance is running: the ticker is active";
  for (int i = 0; i < 40; ++i)
    host.frame(0.016);  // 640 ms — well past the 400 ms entrance
  EXPECT_FALSE(seen) << "and false again once nothing is moving";
}
