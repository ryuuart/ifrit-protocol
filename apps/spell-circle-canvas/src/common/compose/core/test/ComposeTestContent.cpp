// What a leaf carries and what a composer is handed: a slot swapped under
// undisturbed siblings and the content
// kinds a leaf draws from — a held path, a keyed shape, a replayed picture,
// a figure with its own box, a keyed custom whose key must stay honest,
// the sampling an image leaf is magnified with, the box a picture and
// an atlas region meet, the fit a picture keeps in a stated box, and what
// a paint program is handed.

#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/paint/Bases.h>

#include <cstring>  // memcmp — equivalent UTF-8 inputs draw identical pixels
#include <utility>

#include "support/CoreTestSupport.h"

TEST(ComposeSlots, SlotUpdatesWithoutDisturbingSiblings) {
  static int staticRuns;
  staticRuns = 0;
  Host host;
  host.composer.render(box().row().gap(10).children(
      {custom([](sigil::draw::Pen& pen, const PaintContext& ctx) {
         SkCanvas& c = *pen.canvas();
         ++staticRuns;
         SkPaint p;
         p.setColor(SK_ColorRED);
         c.drawRect(SkRect::MakeWH(ctx.size.x, ctx.size.y), p);
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

TEST(ComposeContent, ContentGivenBeforeItsSlotMountsIsShownOnceItMounts) {
  // A setup may fill a slot and then return the tree that holds it: the
  // content waits for the slot and is there at the first frame.
  Host host;
  host.composer.renderSlot(
      "x", box().fill(Fill::color({0, 1, 0, 1})).width(80).height(50));
  host.composer.render(box().children({slot("x").width(80).height(50)}));
  host.frame();
  EXPECT_EQ(host.pixel(40, 25), SK_ColorGREEN)
      << "content given to a slot before its tree mounted was dropped";
  // Given before a describe that brings a second slot, it waits again.
  host.composer.renderSlot(
      "y", box().fill(Fill::color({0, 0, 1, 1})).width(80).height(50));
  host.frame();
  host.composer.render(box().children(
      {slot("x").width(80).height(50), slot("y").width(80).height(50)}));
  host.frame();
  EXPECT_EQ(host.pixel(40, 25), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(40, 75), SK_ColorBLUE);
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
    return box().children({held ? box().width(60).height(60).shape(
                                      heldPath(geometry::path::fromSk(cooked)))
                                : box().width(60).height(60).shape(
                                      skiaShape([cooked] { return cooked; }))});
  };
  Host host;
  host.composer.render(tree(true));
  host.frame();
  host.composer.render(tree(true));  // the same cooked path: prune
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  // A path REBUILT from the same numbers is the same drawing: it carries
  // a new generation, so it is compared verb for verb, and prunes — which
  // is what lets a figure an operator attaches every frame settle.
  SkPathBuilder rebuilt;
  rebuilt.addOval(SkRect::MakeXYWH(10, 10, 40, 40));
  host.composer.render(box().children({box().width(60).height(60).shape(
      heldPath(geometry::path::fromSk(rebuilt.detach())))}));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  // A DIFFERENT path is a change.
  SkPathBuilder moved;
  moved.addOval(SkRect::MakeXYWH(12, 12, 36, 36));
  host.composer.render(box().children({box().width(60).height(60).shape(
      heldPath(geometry::path::fromSk(moved.detach())))}));
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
    return box().children({box().width(60).height(60).fill(red()).shape(
        inset, skiaShape([inset] {
          SkPathBuilder pb;
          pb.addOval(
              SkRect::MakeXYWH(inset, inset, 60 - 2 * inset, 60 - 2 * inset));
          return pb.detach();
        }))});
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
    auto fn = skiaShape([radius](SkSize s) {
      SkPathBuilder pb;
      pb.addRRect(SkRRect::MakeRectXY(SkRect::MakeWH(s.width(), s.height()),
                                      radius, radius));
      return pb.detach();
    });
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
      {pathFigure(geometry::path::fromSk(pb.detach()), 4.0f)
           .key("fig")
           .fill(red())}));
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
    auto program = [shade](sigil::draw::Pen& pen, const PaintContext& ctx) {
      SkCanvas& c = *pen.canvas();
      ++runs;
      SkPaint p;
      p.setColor4f({shade, 0, 0, 1});
      c.drawRect(SkRect::MakeWH(ctx.size.x, ctx.size.y), p);
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
  auto magnified = [&](material::Sampling options) {
    Host host(200, 200);
    host.composer.render(box().children({image(atlas)
                                             .imageRendering(options)
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

  EXPECT_GT(magnified(material::Sampling::Linear), 3);
  EXPECT_LE(magnified(material::Sampling::Nearest), 1);
}

// -------------------------------------------------------------------------
// What a picture and an atlas region meet their box as, and what a
// paint program is handed: the parameters it named, the host scale,
// and the engine's state.

TEST(ComposeContent, APictureMeetsItsBoxTheWayTheFitSays) {
  // A raw picture is a leaf with the wrap written once, and the FIT is
  // said where the picture is rather than as a matrix the caller builds.
  SkBitmap wide;
  wide.allocN32Pixels(40, 10);
  wide.eraseColor(SK_ColorRED);
  const sk_sp<SkImage> picture = wide.asImage();
  const auto shown = [&](material::Fit fit) {
    Host host;
    host.composer.render(
        box().children({box()
                            .width(100)
                            .height(100)
                            .alignItems(Align::Center)
                            .justifyContent(Justify::Center)
                            .children({image(picture, fit).key("fig")})}));
    host.frame();
    const std::optional<geometry::path::Rect> fig = host.composer.bounds("fig");
    return geometry::path::toSk(fig.value_or(geometry::path::Rect{}));
  };
  // Both axes independently: the picture's proportions are the box's.
  EXPECT_EQ(shown(material::Fit::Stretch), SkRect::MakeWH(100, 100));
  // As large as fits, the slack on the long axis: 4:1 in a square box.
  const SkRect held = shown(material::Fit::Contain);
  EXPECT_FLOAT_EQ(held.width(), 100);
  EXPECT_FLOAT_EQ(held.height(), 25);
  // No slack at all: the short axis fills and the long one overflows.
  const SkRect filled = shown(material::Fit::Cover);
  EXPECT_FLOAT_EQ(filled.height(), 100);
  EXPECT_FLOAT_EQ(filled.width(), 400);
  // A picture that is not there draws nothing and takes no room.
  Host bare;
  bare.composer.render(box().children({image(sk_sp<SkImage>()).key("none")}));
  bare.frame();
  const std::optional<geometry::path::Rect> empty =
      bare.composer.bounds("none");
  ASSERT_TRUE(empty.has_value());
  EXPECT_FLOAT_EQ(empty->height(), 0);
}

namespace {

/** Four upright stripes across a 40x10 picture, ten pixels each: red,
 *  green, blue, white. Which stripes show says which part was drawn. */
sk_sp<SkImage> fourStripes() {
  SkBitmap stripes;
  stripes.allocN32Pixels(40, 10);
  const SkColor colours[] = {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE,
                             SK_ColorWHITE};
  for (int index = 0; index < 4; ++index)
    stripes.erase(colours[index], SkIRect::MakeXYWH(index * 10, 0, 10, 10));
  return stripes.asImage();
}

/** @p picture placed by hand with Skia on a black 120x120 ground, through
 *  the same sampling the leaf is given. */
SkBitmap drawnByHand(const sk_sp<SkImage>& picture, const SkRect& source,
                     const SkRect& destination, SkFilterMode filter) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 120));
  surface->getCanvas()->clear(SK_ColorBLACK);
  surface->getCanvas()->drawImageRect(picture, source, destination,
                                      SkSamplingOptions(filter), nullptr,
                                      SkCanvas::kFast_SrcRectConstraint);
  SkBitmap pixels;
  pixels.allocPixels(surface->imageInfo());
  surface->readPixels(pixels.pixmap(), 0, 0);
  return pixels;
}

bool samePixels(const SkBitmap& left, const SkBitmap& right) {
  if (left.dimensions() != right.dimensions()) return false;
  for (int y = 0; y < left.height(); ++y)
    if (std::memcmp(left.getAddr32(0, y), right.getAddr32(0, y),
                    (size_t)left.width() * 4) != 0)
      return false;
  return true;
}

/** A 4:1 picture under @p fit, given a square 80x80 box at (20, 20) by
 *  `rect()`, drawn with nearest sampling on a black 120x120 ground. */
SkBitmap squareBoxed(const sk_sp<SkImage>& picture, material::Fit fit) {
  Host host(120, 120);
  host.composer.render(
      box().children({image(picture, fit)
                          .imageRendering(material::Sampling::Nearest)
                          .absolute()
                          .rect(20, 20, 80, 80)}));
  host.frame();
  return host.pixels();
}

}  // namespace

TEST(ComposeContent, ACoverLeafInAStatedBoxShowsTheCentredCrop) {
  // The box is square and the picture is 4:1, so covering it takes the
  // picture at eight times and keeps its middle ten pixels: the green and
  // blue stripes, half the box each. Stretched, all four would show.
  const sk_sp<SkImage> picture = fourStripes();
  const SkBitmap shown = squareBoxed(picture, material::Fit::Cover);
  EXPECT_TRUE(samePixels(
      shown,
      drawnByHand(picture, SkRect::MakeXYWH(15, 0, 10, 10),
                  SkRect::MakeXYWH(20, 20, 80, 80), SkFilterMode::kNearest)));
  EXPECT_EQ(shown.getColor(30, 60), SK_ColorGREEN);
  EXPECT_EQ(shown.getColor(90, 60), SK_ColorBLUE);
  for (int x = 20; x < 100; ++x)
    ASSERT_NE(shown.getColor(x, 60), SK_ColorRED) << "stretched at " << x;
}

TEST(ComposeContent, AContainLeafInAStatedBoxIsLetterboxed) {
  // Held whole at twice its size, 80x20, centred: thirty pixels of the box
  // above and below stay the ground.
  const sk_sp<SkImage> picture = fourStripes();
  const SkBitmap shown = squareBoxed(picture, material::Fit::Contain);
  EXPECT_TRUE(samePixels(shown, drawnByHand(picture, SkRect::MakeWH(40, 10),
                                            SkRect::MakeXYWH(20, 50, 80, 20),
                                            SkFilterMode::kNearest)));
  for (int y : {21, 49, 70, 99})
    EXPECT_EQ(shown.getColor(60, y), SK_ColorBLACK) << "margin at " << y;
  EXPECT_EQ(shown.getColor(25, 60), SK_ColorRED);
  EXPECT_EQ(shown.getColor(95, 60), SK_ColorWHITE);
}

TEST(ComposeContent, AStretchLeafInAStatedBoxStretches) {
  const sk_sp<SkImage> picture = fourStripes();
  const SkBitmap shown = squareBoxed(picture, material::Fit::Stretch);
  EXPECT_TRUE(samePixels(shown, drawnByHand(picture, SkRect::MakeWH(40, 10),
                                            SkRect::MakeXYWH(20, 20, 80, 80),
                                            SkFilterMode::kNearest)));
  EXPECT_EQ(shown.getColor(25, 60), SK_ColorRED);
  EXPECT_EQ(shown.getColor(95, 60), SK_ColorWHITE);
}

TEST(ComposeContent, AFreeCoverLeafDrawsTheWholePictureIntoItsBox) {
  // Free, the leaf is the picture's shape up to the layout's rounding, and
  // the whole picture is drawn into the whole box with linear sampling,
  // exactly as a hand-placed draw into the same box.
  SkBitmap odd;
  odd.allocN32Pixels(30, 7);
  odd.eraseColor(SK_ColorBLUE);
  odd.erase(SK_ColorYELLOW, SkIRect::MakeXYWH(0, 0, 13, 3));
  const sk_sp<SkImage> picture = odd.asImage();
  Host host(120, 120);
  host.composer.render(box().width(120).height(60).children(
      {image(picture, material::Fit::Cover).key("free")}));
  host.frame();
  const SkRect bounds = require(host.composer.bounds("free"));
  EXPECT_FLOAT_EQ(bounds.height(), 60);
  EXPECT_NEAR(bounds.width(), 60.0f * 30.0f / 7.0f, 0.5f);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(120, 120));
  surface->getCanvas()->clear(SK_ColorBLACK);
  surface->getCanvas()->drawImageRect(picture, bounds,
                                      SkSamplingOptions{SkFilterMode::kLinear});
  SkBitmap expected;
  expected.allocPixels(surface->imageInfo());
  surface->readPixels(expected.pixmap(), 0, 0);
  EXPECT_TRUE(samePixels(host.pixels(), expected));
}

TEST(ComposeContent, ARegionIsTheSourceTheFitApplies) {
  // The region straddles the atlas seam, half red and half green. Held
  // inside a 2:1 box it is a centred square with a margin either side.
  auto atlas = twoCellAtlas();
  Host host(120, 120);
  host.composer.render(box().children(
      {image(atlas, material::Fit::Contain)
           .imageRegion(geometry::path::Rect::of({8, 0}, {16, 16}))
           .absolute()
           .rect(0, 0, 100, 50)}));
  host.frame();
  EXPECT_EQ(host.pixel(10, 25), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(35, 25), SK_ColorRED);
  EXPECT_EQ(host.pixel(65, 25), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(90, 25), SK_ColorBLACK);
}

TEST(ComposeContent, ImageRegionDrawsAtlasCell) {
  Host host;
  auto atlas = twoCellAtlas();
  host.composer.render(box().row().children(
      {image(atlas, material::Fit::Native)
           .imageRegion(geometry::path::Rect::of({16, 0}, {16, 16}))
           .width(50)
           .height(50),
       image(atlas, material::Fit::Native).width(50).height(50)}));
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
  glm::vec2 offered{0, 0};
  host.composer.render(box().row().children(
      {custom([&](sigil::draw::Pen& pen) {
         SkCanvas& canvas = *pen.canvas();
         square(canvas, SK_ColorGREEN);
       })
           .width(20)
           .height(20)
           .cache(Cache::None),
       custom([&](sigil::draw::Pen& pen, const PaintContext& ctx) {
         SkCanvas& canvas = *pen.canvas();
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
  EXPECT_EQ(offered, glm::vec2(20, 20));
  EXPECT_EQ(nullaryRuns, 1) << "a program that reads neither still runs";
}

TEST(ComposePaint, ContentScaleReportsHostScale) {
  Host host;
  float seen = 0.0f;
  host.composer.render(box().children(
      {custom([&seen](sigil::draw::Pen& pen, const PaintContext& ctx) {
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

TEST(ComposePaint, AnimatingReportsTheEnginesState) {
  // `PaintContext::animating` looks dead from inside the library: the painter
  // assigns it from `engine.isRunning()` (and the Brushes.h wrappers copy that
  // forward rather than a constant), but nothing in the library ever reads
  // it back. Its only consumer is a paint program written by a caller, so
  // this test is the only thing keeping the field wired up.
  Host host;
  bool seen = false;
  host.composer.render(box().children(
      {box().width(40).height(40).fill(red()).opacity(
           motion::animate({.from = 0.0f, .to = 1.0f, .duration = 400ms})),
       custom([&seen](sigil::draw::Pen& pen, const PaintContext& ctx) {
         seen = ctx.animating;
       })
           .width(10)
           .height(10)
           .cache(Cache::None)}));
  host.frame(0.016);
  EXPECT_TRUE(seen) << "an entrance is running: the engine is running";
  for (int i = 0; i < 40; ++i)
    host.frame(0.016);  // 640 ms — well past the 400 ms entrance
  EXPECT_FALSE(seen) << "and false again once nothing is moving";
}
