// A compose tree painted as the maps of a surface: what each map reads
// under a lit fill, lit glyph ink, a relief and a flat label, where
// nothing paints, when it stands still and when it paints again, that
// what a composer holds for one map is never what it draws for another,
// and that the same tree drawn as a page paints every material's colours
// flat, reading none of its surface channels.

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/testing/Scene.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/PixelSource.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "support/Host.h"

namespace {

using sigil::material::texture::Role;

constexpr int kWidth = 128;
constexpr int kHeight = 112;

material::Material plateFinish(material::Color colour) {
  return material::from(colour).surface({.metallic = .75f, .roughness = .25f});
}

material::Material glyphFinish() {
  // A constant tilted normal: (0.5, 0, 0.8) normalised, green up.
  return material::from(material::Color{.2f, .6f, .4f, 1})
      .surface({.roughness = .9f,
                .normal = material::from(material::Color{.75f, .5f, .9f, 1})});
}

material::Material reliefFinish() {
  return material::from(material::Color{.6f, .6f, .6f, 1})
      .surface({.roughness = .8f});
}

/** A lit plate with a flat label on it, lit glyph ink, a relief, and an
 *  empty band along the bottom. */
Element page(material::Color plate = {.8f, .4f, .2f, 1}) {
  return box().width(kWidth).height(kHeight).children({
      box().absolute().rect(0, 0, 128, 48).fill(plateFinish(plate)),
      box()
          .absolute()
          .rect(96, 4, 24, 24)
          .fill(material::Color{.9f, .1f, .1f, 1}),
      text(u8"HH", whiteStyle(44))
          .absolute()
          .rect(0, 48, 64, 48)
          .ink(glyphFinish(), PaintBox::Glyph),
      box()
          .absolute()
          .rect(64, 48, 64, 48)
          .background(relief(reliefFinish(), {.shoulder = 6, .depth = 3})),
  });
}

/** A picture that counts the times it is read. */
struct CountedPicture {
  std::shared_ptr<int> reads;
  sk_sp<SkImage> image;
  sigil::media::Frame frameAt(std::chrono::duration<double>) const {
    ++*reads;
    sigil::media::Frame frame;
    frame.image = image;
    return frame;
  }
  bool isRunning() const { return false; }
  SkISize size() const { return image->dimensions(); }
  bool operator==(const CountedPicture& other) const {
    return reads == other.reads;
  }
};

/** A page whose fill, glyph ink, keyline and relief carry @p finish,
 *  which states a surface or does not. */
Element finishedPage(const material::Material& finish) {
  return box().width(kWidth).height(kHeight).children({
      box()
          .absolute()
          .rect(0, 0, 128, 48)
          .fill(finish)
          .stroke(finish,
                  {.width = 4, .position = material::StrokePosition::Inside}),
      text(u8"HH", whiteStyle(44))
          .absolute()
          .rect(0, 48, 64, 48)
          .ink(finish, PaintBox::Glyph),
      box()
          .absolute()
          .rect(64, 48, 64, 48)
          .background(relief(finish, {.shoulder = 6, .depth = 3})),
  });
}

SkBitmap pixelsOf(const SurfaceScene& scene, Role role) {
  SkBitmap pixels;
  const TextureScene* map = scene.scene(role);
  if (!map || !map->image()) return pixels;
  pixels.allocN32Pixels(map->image()->width(), map->image()->height());
  map->image()->readPixels(nullptr, pixels.pixmap(), 0, 0);
  return pixels;
}

/** One channel of the packed occlusion-roughness-metallic map, as the
 *  grey map that scalar would have been on its own: 0 occlusion, 1
 *  roughness, 2 metallic. */
SkBitmap channelOf(const SurfaceScene& scene, int channel) {
  const SkBitmap packed = pixelsOf(scene, Role::Packed);
  SkBitmap grey;
  if (packed.empty()) return grey;
  grey.allocN32Pixels(packed.width(), packed.height());
  for (int y = 0; y < packed.height(); ++y)
    for (int x = 0; x < packed.width(); ++x) {
      const SkColor4f c = packed.getColor4f(x, y);
      const float v = channel == 0 ? c.fR : channel == 1 ? c.fG : c.fB;
      grey.erase(SkColor4f{v, v, v, c.fA}.toSkColor(),
                 SkIRect::MakeXYWH(x, y, 1, 1));
    }
  return grey;
}

void expectNear(const SkColor4f& actual, float red, float green, float blue,
                float alpha = 1, float tolerance = .012f) {
  EXPECT_NEAR(actual.fR, red, tolerance);
  EXPECT_NEAR(actual.fG, green, tolerance);
  EXPECT_NEAR(actual.fB, blue, tolerance);
  EXPECT_NEAR(actual.fA, alpha, tolerance);
}

}  // namespace

TEST(ComposeSurfaceScene, EachMapReadsWhatTheTreePaintedThere) {
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  scene->render(page());
  const SkBitmap base = pixelsOf(*scene, Role::BaseColor);
  const SkBitmap normal = pixelsOf(*scene, Role::Normal);
  const SkBitmap roughness = channelOf(*scene, 1);
  const SkBitmap metallic = channelOf(*scene, 2);
  const SkBitmap occlusion = channelOf(*scene, 0);
  const SkBitmap emissive = pixelsOf(*scene, Role::Emissive);
  ASSERT_FALSE(base.empty());

  // The lit plate: its colour, numbers and a flat normal; it emits nothing.
  expectNear(base.getColor4f(10, 10), .8f, .4f, .2f);
  expectNear(normal.getColor4f(10, 10), .5f, .5f, 1);
  expectNear(roughness.getColor4f(10, 10), .25f, .25f, .25f);
  expectNear(metallic.getColor4f(10, 10), .75f, .75f, .75f);
  expectNear(occlusion.getColor4f(10, 10), 1, 1, 1);
  expectNear(emissive.getColor4f(10, 10), 0, 0, 0);

  // The flat label over it states no surface: it is the stock surface, its
  // colour the base colour, and it covers the plate and emits nothing.
  expectNear(base.getColor4f(108, 16), .9f, .1f, .1f);
  expectNear(emissive.getColor4f(108, 16), 0, 0, 0);
  expectNear(normal.getColor4f(108, 16), .5f, .5f, 1);
  expectNear(roughness.getColor4f(108, 16), .5f, .5f, .5f);
  expectNear(metallic.getColor4f(108, 16), 0, 0, 0);

  // Lit glyph ink: somewhere inside a glyph the base is the ink's colour
  // and the normal its tilted one.
  bool inked = false;
  for (int y = 50; y < 96 && !inked; ++y)
    for (int x = 0; x < 64 && !inked; ++x) {
      const SkColor4f colour = base.getColor4f(x, y);
      if (colour.fA < .99f) continue;
      inked = true;
      expectNear(colour, .2f, .6f, .4f);
      expectNear(normal.getColor4f(x, y), .765f, .5f, .924f);
      expectNear(roughness.getColor4f(x, y), .9f, .9f, .9f);
      expectNear(emissive.getColor4f(x, y), 0, 0, 0);
    }
  EXPECT_TRUE(inked);

  // The relief: its colour throughout, flat across its middle and tilted
  // along its shoulder.
  expectNear(base.getColor4f(96, 72), .6f, .6f, .6f);
  expectNear(normal.getColor4f(96, 72), .5f, .5f, 1, 1, .03f);
  const SkColor4f shoulder = normal.getColor4f(66, 72);
  EXPECT_GT(std::abs(shoulder.fR - .5f), .08f);

  // Where nothing paints, every map reads its ground.
  expectNear(base.getColor4f(5, 105), 0, 0, 0, 0);
  expectNear(normal.getColor4f(5, 105), .5f, .5f, 1);
  expectNear(roughness.getColor4f(5, 105), 1, 1, 1);
  expectNear(metallic.getColor4f(5, 105), 0, 0, 0);
  expectNear(emissive.getColor4f(5, 105), 0, 0, 0);

  const material::texture::TextureMaps maps = scene->maps();
  // Five maps: the three scalars share one and the coat's two another.
  EXPECT_EQ(maps.maps.size(), 5u);
  EXPECT_FALSE(maps.normalDirectX);
  EXPECT_NE(maps.map(Role::Emissive), nullptr);
  EXPECT_NE(maps.map(Role::Packed), nullptr);
  EXPECT_NE(maps.map(Role::Clearcoat), nullptr);
  // A plate stating no coat has none.
  expectNear(pixelsOf(*scene, Role::Clearcoat).getColor4f(10, 10), 0, 0, 0);
}

TEST(ComposeSurfaceScene, AStatedLobeAddsItsMapsInTheRenderThatStatesIt) {
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  // A page whose materials state no optional lobe paints the five maps
  // and nothing else.
  scene->render(page());
  EXPECT_EQ(scene->maps().maps.size(), 5u);
  EXPECT_EQ(scene->lobes(), material::Lobes{});

  // A sheened, brushed plate: the render that paints it adds the sheen's
  // two maps and the brushing's, and paints them at once.
  const Element lobed = box().width(kWidth).height(kHeight).children({
      box()
          .absolute()
          .rect(0, 0, 64, 48)
          .fill(material::from(material::Color{.3f, .1f, .1f, 1})
                    .surface({.sheenColor = material::Color{.8f, .6f, .4f, 1},
                              .sheenRoughness = .5f})),
      box()
          .absolute()
          .rect(64, 0, 64, 48)
          .fill(material::from(material::Color{.9f, .9f, .9f, 1})
                    .surface({.metallic = 1.0f,
                              .anisotropy = .75f,
                              .anisotropyRotation = 90})),
  });
  scene->render(lobed);
  EXPECT_EQ(scene->lobes(),
            (material::Lobes{.sheen = true, .anisotropy = true}));
  const material::texture::TextureMaps maps = scene->maps();
  EXPECT_EQ(maps.maps.size(), 8u);
  EXPECT_NE(maps.map(Role::Sheen), nullptr);
  EXPECT_NE(maps.map(Role::SheenSpecular), nullptr);
  EXPECT_NE(maps.map(Role::Anisotropy), nullptr);
  EXPECT_EQ(maps.map(Role::Iridescence), nullptr);
  expectNear(pixelsOf(*scene, Role::Sheen).getColor4f(20, 20), .8f, .6f, .4f);
  // The sheen's roughness in red, the stock specular strength in green.
  expectNear(pixelsOf(*scene, Role::SheenSpecular).getColor4f(20, 20), .5f, 1,
             0);
  // A quarter turn from the page's width: the direction (0, 1), encoded,
  // and the strength in blue; where nothing brushes, no strength.
  const SkBitmap brushing = pixelsOf(*scene, Role::Anisotropy);
  expectNear(brushing.getColor4f(96, 20), .5f, 1, .75f);
  expectNear(brushing.getColor4f(5, 105), 1, .5f, 0);
  // The maps stay once added, though the next tree states no lobe.
  scene->render(page());
  EXPECT_EQ(scene->maps().maps.size(), 8u);
}

TEST(ComposeSurfaceScene, AStillTreeSettlesAndAChangedMaterialPaintsAgain) {
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  const Element still = page();
  scene->render(still);
  const uint64_t painted = scene->revision();
  EXPECT_EQ(painted, 1u);
  for (int frame = 0; frame < 6; ++frame)
    scene->render(still, (frame + 1) / 60.0);
  EXPECT_EQ(scene->revision(), painted);
  EXPECT_FALSE(scene->isRunning());

  scene->render(page({.1f, .3f, .9f, 1}), 1);
  EXPECT_EQ(scene->revision(), painted + 1);
  expectNear(pixelsOf(*scene, Role::BaseColor).getColor4f(10, 10), .1f, .3f,
             .9f);
}

TEST(ComposeSurfaceScene, WhatAComposerHoldsForOneMapIsNotDrawnForAnother) {
  Host host(kWidth, kHeight);
  host.composer.render(box()
                           .width(kWidth)
                           .height(kHeight)
                           .cache(Cache::Picture)
                           .children({
                               box()
                                   .absolute()
                                   .rect(0, 0, 128, 48)
                                   .cache(Cache::Picture)
                                   .fill(plateFinish({.8f, .4f, .2f, 1})),
                               text(u8"HH", whiteStyle(44))
                                   .absolute()
                                   .rect(0, 48, 64, 48)
                                   .cache(Cache::Picture)
                                   .ink(glyphFinish()),
                           }));
  const auto at = [&](int x, int y) {
    return SkColor4f::FromColor(host.pixel(x, y));
  };
  // The strongest red across the glyphs, over the black the host clears to.
  const auto inkedRed = [&] {
    float strongest = 0;
    for (int y = 50; y < 96; ++y)
      for (int x = 0; x < 64; ++x) strongest = std::max(strongest, at(x, y).fR);
    return strongest;
  };
  host.composer.setSurfaceMap(Role::Roughness);
  host.frame();
  host.frame();
  expectNear(at(10, 10), .25f, .25f, .25f);
  EXPECT_NEAR(inkedRed(), .9f, .012f);
  host.composer.setSurfaceMap(Role::Metallic);
  host.frame();
  expectNear(at(10, 10), .75f, .75f, .75f);
  EXPECT_NEAR(inkedRed(), 0, .012f);  // the ink states no metal
  host.composer.setSurfaceMap(Role::BaseColor);
  host.frame();
  expectNear(at(10, 10), .8f, .4f, .2f);
  EXPECT_NEAR(inkedRed(), .2f, .012f);
  host.composer.setSurfaceMap(std::nullopt);
  host.frame();
  // With no map and no lighting the plate paints its colour stack flat.
  expectNear(at(10, 10), .8f, .4f, .2f);
}

TEST(ComposeSurfaceScene, APageDrawPaintsAStatedSurfaceAsItsColoursFlat) {
  auto colourReads = std::make_shared<int>(0);
  auto normalReads = std::make_shared<int>(0);
  auto roughnessReads = std::make_shared<int>(0);
  const auto picture = [](const std::shared_ptr<int>& reads, SkColor colour) {
    SkBitmap pixels;
    pixels.allocN32Pixels(8, 8, true);
    pixels.eraseColor(colour);
    pixels.setImmutable();
    return material::image(
        sigil::media::PixelSource(CountedPicture{reads, pixels.asImage()}),
        {.repeat = material::Repeat::Pad});
  };
  // A strongly tilted normal, a metal and an emission: none of them
  // changes what a page paints.
  // The colours are a picture, so the plain material and the surfaced one
  // both paint as a paint rather than one of them as a plain colour.
  const material::Material colours =
      picture(colourReads, SkColorSetRGB(77, 140, 204));
  const material::Material surfaced = material::from(colours).surface(
      {.metallic = 1.0f,
       .roughness = picture(roughnessReads, SkColorSetRGB(40, 40, 40)),
       .normal = picture(normalReads, SkColorSetRGB(230, 128, 160)),
       .emission = {1, 0, 0, 1},
       .emissionStrength = 1});
  Host lit(kWidth, kHeight), plain(kWidth, kHeight);
  lit.composer.render(finishedPage(surfaced));
  plain.composer.render(finishedPage(colours));
  lit.frame();
  plain.frame();
  EXPECT_TRUE(identicalPixels(lit, plain, kWidth, kHeight));
  EXPECT_EQ(*normalReads, 0);
  EXPECT_EQ(*roughnessReads, 0);

  // The same tree through a surface scene still yields its maps.
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  scene->render(finishedPage(surfaced));
  EXPECT_GT(*normalReads, 0);
  EXPECT_GT(*roughnessReads, 0);
  const SkBitmap base = pixelsOf(*scene, Role::BaseColor);
  const SkBitmap normal = pixelsOf(*scene, Role::Normal);
  const SkBitmap metallic = channelOf(*scene, 2);
  const SkBitmap emissive = pixelsOf(*scene, Role::Emissive);
  ASSERT_FALSE(base.empty());
  expectNear(base.getColor4f(20, 20), 77 / 255.f, 140 / 255.f, 204 / 255.f);
  EXPECT_GT(normal.getColor4f(20, 20).fR, .8f);
  expectNear(metallic.getColor4f(20, 20), 1, 1, 1);
  expectNear(emissive.getColor4f(20, 20), 1, 0, 0);
}

namespace {

/** A picture split down its middle: its left half the encoding of a
 *  normal leaning right, its right half one leaning left, so the same
 *  texture read as the base colour and as the normal says in both maps
 *  which half stands where. It is @p width pixels across, or the unit
 *  square across where @p width is zero — the square a unit ink
 *  restarts on. */
material::Material splitPicture(int width) {
  const int across = width > 0 ? width : 64;
  SkBitmap pixels;
  pixels.allocN32Pixels(across, 8, true);
  pixels.erase(SkColorSetRGB(191, 128, 230),
               SkIRect::MakeXYWH(0, 0, across / 2, 8));
  pixels.erase(SkColorSetRGB(64, 128, 230),
               SkIRect::MakeXYWH(across / 2, 0, across / 2, 8));
  pixels.setImmutable();
  material::Texture texture(sigil::media::PixelSource(
      CountedPicture{std::make_shared<int>(0), pixels.asImage()}));
  texture.tile(material::Repeat::Pad).sampling(material::Sampling::Nearest);
  if (width == 0) {
    glm::mat3 toUnitSquare(1.0f);
    toUnitSquare[0][0] = 1.0f / across;
    toUnitSquare[1][1] = 1.0f / 8;
    texture.uv(toUnitSquare);
  }
  return material::image(std::move(texture));
}

/** One texture as the colour and as the normal of one surface. */
material::Material splitFinish(int width) {
  return material::from(splitPicture(width))
      .surface({.roughness = .5f, .normal = splitPicture(width)});
}

/** Which half of the split picture a base colour shows: one for the left,
 *  zero for the right, and one half for neither. */
float leftHalfOf(const SkColor4f& colour) {
  if (std::abs(colour.fR - 191 / 255.f) < .05f) return 1;
  if (std::abs(colour.fR - 64 / 255.f) < .05f) return 0;
  return .5f;
}

/** Which way an encoded normal leans: one to the right, zero to the left,
 *  one half upright. */
float leansRight(const SkColor4f& normal) {
  if (normal.fR > .65f) return 1;
  if (normal.fR < .35f) return 0;
  return .5f;
}

}  // namespace

TEST(ComposeSurfaceScene, ACanvasFillPlacesItsNormalWhereItPlacesItsColour) {
  // A card inset in the page, from 32 to 80 across, filled with a split
  // picture as wide as the canvas laid from the canvas's corner: the split
  // falls at 64, the canvas's middle, in the base colour and the normal
  // alike. Laid from the card's own corner it would fall at 96, past the
  // card, which would show only the left half.
  const auto pageWith = [](PaintBox over) {
    return box().width(kWidth).height(kHeight).children(
        {box()
             .absolute()
             .rect(32, 24, 48, 40)
             .fill(splitFinish(kWidth), over)});
  };
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  scene->render(pageWith(PaintBox::Canvas));
  SkBitmap base = pixelsOf(*scene, Role::BaseColor);
  SkBitmap normal = pixelsOf(*scene, Role::Normal);
  ASSERT_FALSE(base.empty());
  for (const int x : {40, 60, 68, 76}) {
    const float expected = x < 64 ? 1.0f : 0.0f;
    EXPECT_EQ(leftHalfOf(base.getColor4f(x, 44)), expected) << "x " << x;
    EXPECT_EQ(leansRight(normal.getColor4f(x, 44)), expected) << "x " << x;
  }
  // On the card's own box both maps move together with the card.
  scene->render(pageWith(PaintBox::Element), 1);
  base = pixelsOf(*scene, Role::BaseColor);
  normal = pixelsOf(*scene, Role::Normal);
  for (const int x : {40, 60, 68, 76}) {
    EXPECT_EQ(leftHalfOf(base.getColor4f(x, 44)), 1) << "x " << x;
    EXPECT_EQ(leansRight(normal.getColor4f(x, 44)), 1) << "x " << x;
  }
}

TEST(ComposeSurfaceScene, AGlyphInkRestartsItsNormalOnEachGlyphWithItsColour) {
  // Two capitals inked with the split picture restarting on each glyph:
  // every letter shows both halves, and wherever the base colour shows a
  // half the normal leans that half's way.
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  scene->render(box().width(kWidth).height(kHeight).children(
      {text(u8"HH", whiteStyle(44))
           .absolute()
           .rect(0, 32, 128, 48)
           .ink(splitFinish(0), PaintBox::Glyph)}));
  const SkBitmap base = pixelsOf(*scene, Role::BaseColor);
  const SkBitmap normal = pixelsOf(*scene, Role::Normal);
  ASSERT_FALSE(base.empty());
  // Each letter's columns, split where the second one begins: the inked
  // columns' middle.
  int first = kWidth, last = -1;
  for (int y = 32; y < 80; ++y)
    for (int x = 0; x < kWidth; ++x)
      if (base.getColor4f(x, y).fA > .99f) {
        first = std::min(first, x);
        last = std::max(last, x);
      }
  ASSERT_LT(first, last);
  const int between = (first + last) / 2;
  int halves[2][2] = {{0, 0}, {0, 0}};
  int agreeing = 0, inked = 0;
  for (int y = 32; y < 80; ++y)
    for (int x = 0; x < kWidth; ++x) {
      const SkColor4f colour = base.getColor4f(x, y);
      if (colour.fA < .99f) continue;
      const float half = leftHalfOf(colour);
      if (half == .5f) continue;
      ++inked;
      ++halves[x < between ? 0 : 1][half == 1 ? 0 : 1];
      if (leansRight(normal.getColor4f(x, y)) == half) ++agreeing;
    }
  ASSERT_GT(inked, 50);
  EXPECT_EQ(agreeing, inked);
  for (const auto& letter : halves) {
    EXPECT_GT(letter[0], 10) << "a letter shows no left half";
    EXPECT_GT(letter[1], 10) << "a letter shows no right half";
  }
}

TEST(ComposeSurfaceScene, AStatedCoatNormalAddsItsMapWhereTheCoatIsPainted) {
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  scene->render(page());
  EXPECT_EQ(scene->maps().map(Role::ClearcoatNormal), nullptr);
  // A coated plate whose coat leans right on its own while the surface
  // beneath faces the viewer.
  scene->render(box().width(kWidth).height(kHeight).children(
      {box()
           .absolute()
           .rect(0, 0, 64, 48)
           .fill(material::from(material::Color{.3f, .3f, .3f, 1})
                     .surface({.clearcoat = 1.0f,
                               .clearcoatNormal = material::from(
                                   material::Color{.75f, .5f, .9f, 1})}))}));
  ASSERT_NE(scene->maps().map(Role::ClearcoatNormal), nullptr);
  const SkBitmap coat = pixelsOf(*scene, Role::ClearcoatNormal);
  const SkBitmap normal = pixelsOf(*scene, Role::Normal);
  expectNear(coat.getColor4f(20, 20), .765f, .5f, .924f);
  expectNear(normal.getColor4f(20, 20), .5f, .5f, 1);
  // Where nothing paints, the coat's normal faces the viewer.
  expectNear(coat.getColor4f(100, 100), .5f, .5f, 1);
}

namespace {

/** The columns where row @p y of @p base is inked opaque, as runs of
 *  [first, last]. */
std::vector<std::pair<int, int>> inkedRuns(const SkBitmap& base, int y) {
  std::vector<std::pair<int, int>> runs;
  for (int x = 0; x < base.width(); ++x) {
    if (base.getColor4f(x, y).fA < .99f) continue;
    if (!runs.empty() && runs.back().second == x - 1)
      runs.back().second = x;
    else
      runs.push_back({x, x});
  }
  return runs;
}

}  // namespace

TEST(ComposeSurfaceScene,
     AReliefCutPerGlyphAndPerWordDiffersOnlyBetweenLetters) {
  // One grey that raises the unit it is painted into, inked on two
  // capitals restarting on each glyph and on each word: the shoulders at
  // the word's outer edges fall alike, and between the two letters the
  // word's one height runs across the gap where each glyph's falls away.
  const material::Material raised =
      material::from(material::Color{.6f, .6f, .6f, 1})
          .surface({.roughness = .8f})
          .relief({.shoulder = 16, .depth = .5f});
  const auto normalsUnder = [&](PaintBox unit) {
    const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
    scene->render(box().width(kWidth).height(kHeight).children(
        {text(u8"HH", whiteStyle(64))
             .absolute()
             .rect(4, 16, 120, 80)
             .ink(raised, unit)}));
    return std::pair{pixelsOf(*scene, Role::BaseColor),
                     pixelsOf(*scene, Role::Normal)};
  };
  const auto [glyphBase, glyphNormal] = normalsUnder(PaintBox::Glyph);
  const auto [wordBase, wordNormal] = normalsUnder(PaintBox::Word);
  ASSERT_FALSE(glyphBase.empty());
  // A row through the middle of both letters.
  int top = -1, bottom = -1;
  for (int y = 0; y < kHeight; ++y)
    if (!inkedRuns(glyphBase, y).empty()) {
      if (top < 0) top = y;
      bottom = y;
    }
  ASSERT_GE(top, 0);
  const int row = (top + bottom) / 2;
  const auto runs = inkedRuns(glyphBase, row);
  ASSERT_EQ(runs.size(), 2u);
  // The covered pixels are the same: a relief moves no ink.
  for (int x = 0; x < kWidth; ++x)
    EXPECT_EQ(glyphBase.getColor4f(x, row).fA > .5f,
              wordBase.getColor4f(x, row).fA > .5f)
        << "x " << x;
  // The word's outer edges: the first letter's left side and the last
  // letter's right side lean alike under both units, and lean.
  const int outerLeft = runs[0].first + 1, outerRight = runs[1].second - 1;
  EXPECT_LT(glyphNormal.getColor4f(outerLeft, row).fR, .42f);
  EXPECT_NEAR(glyphNormal.getColor4f(outerLeft, row).fR,
              wordNormal.getColor4f(outerLeft, row).fR, .02f);
  EXPECT_GT(glyphNormal.getColor4f(outerRight, row).fR, .58f);
  EXPECT_NEAR(glyphNormal.getColor4f(outerRight, row).fR,
              wordNormal.getColor4f(outerRight, row).fR, .02f);
  // Between the letters: each glyph falls to its own edge, so its
  // shoulder leans out more steeply than the word's, whose height the
  // neighbouring letter holds up across the gap.
  const int innerLeft = runs[0].second - 1, innerRight = runs[1].first + 1;
  EXPECT_GT(glyphNormal.getColor4f(innerLeft, row).fR,
            wordNormal.getColor4f(innerLeft, row).fR + .04f);
  EXPECT_LT(glyphNormal.getColor4f(innerRight, row).fR,
            wordNormal.getColor4f(innerRight, row).fR - .04f);
}

TEST(ComposeSurfaceScene, ABrushingAlongItsUnitRunsAlongTheUnitInThePage) {
  // A long strip turned a quarter clockwise on the page and brushed along
  // its unit: the brushing runs down the page — green down, encoded — and
  // a strip lying flat brushes across it.
  const auto brushingOf = [&](float turn) {
    const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
    scene->render(box().width(kWidth).height(kHeight).children(
        {box()
             .absolute()
             .rect(24, 46, 80, 20)
             .rotate(turn)
             .fill(material::from(material::Color{.9f, .9f, .9f, 1})
                       .surface({.metallic = 1.0f, .roughness = .3f})
                       .brushed({.strength = .8f}))}));
    return pixelsOf(*scene, Role::Anisotropy);
  };
  expectNear(brushingOf(0).getColor4f(64, 56), 1, .5f, .8f);
  expectNear(brushingOf(90).getColor4f(64, 56), .5f, 0, .8f);
  // A quarter less, the strip leans down to the right and so does its
  // brushing.
  const SkColor4f leaning = brushingOf(45).getColor4f(64, 56);
  EXPECT_NEAR(leaning.fR, .5f + .5f * std::sqrt(.5f), .02f);
  EXPECT_NEAR(leaning.fG, .5f - .5f * std::sqrt(.5f), .02f);
}

TEST(ComposeSurfaceScene, AKeylineBrushedAlongItsUnitRunsAlongItsOutline) {
  // A brushed keyline round a square: along the top and bottom it brushes
  // across the page, along the sides down it.
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  scene->render(box().width(kWidth).height(kHeight).children(
      {box()
           .absolute()
           .rect(24, 16, 80, 80)
           .stroke(
               material::from(material::Color{.9f, .9f, .9f, 1})
                   .surface({.metallic = 1.0f, .roughness = .3f})
                   .brushed({.strength = 1}),
               {.width = 8, .position = material::StrokePosition::Center})}));
  const SkBitmap brushing = pixelsOf(*scene, Role::Anisotropy);
  ASSERT_FALSE(brushing.empty());
  for (const auto& [x, y] : {std::pair{64, 16}, std::pair{64, 96}}) {
    const SkColor4f across = brushing.getColor4f(x, y);
    EXPECT_NEAR(std::abs(across.fR - .5f), .5f, .03f) << x << "," << y;
    EXPECT_NEAR(across.fG, .5f, .03f) << x << "," << y;
  }
  for (const auto& [x, y] : {std::pair{24, 56}, std::pair{104, 56}}) {
    const SkColor4f down = brushing.getColor4f(x, y);
    EXPECT_NEAR(down.fR, .5f, .03f) << x << "," << y;
    EXPECT_NEAR(std::abs(down.fG - .5f), .5f, .03f) << x << "," << y;
  }
}

namespace {

/** A page whose fill and keyline, whole-passage ink, per-glyph ink and
 *  ribbon all carry @p finish. */
Element ownColourPage(const material::Material& finish) {
  return box().width(kWidth).height(kHeight).children({
      box()
          .absolute()
          .rect(0, 0, 40, 40)
          .fill(finish)
          .stroke(finish,
                  {.width = 4, .position = material::StrokePosition::Inside}),
      text(u8"HH")
          .font({.size = 40})
          .absolute()
          .rect(44, 0, 84, 48)
          .ink(finish),
      text(u8"HH")
          .font({.size = 40})
          .absolute()
          .rect(0, 48, 64, 48)
          .ink(finish, PaintBox::Glyph),
      box().absolute().rect(76, 60, 40, 40).stroke(brush::ribbon(8, finish)),
  });
}

/** Whether, somewhere in @p region where the page painted at full
 *  coverage, @p shown reads @p colour and @p dark reads black. */
bool showsColourIn(const SkBitmap& shown, const SkBitmap& dark, SkIRect region,
                   material::Color colour) {
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x) {
      const SkColor4f in = shown.getColor4f(x, y);
      const SkColor4f out = dark.getColor4f(x, y);
      if (in.fA < .99f || out.fA < .99f) continue;
      if (std::abs(in.fR - colour.r) < .03f &&
          std::abs(in.fG - colour.g) < .03f &&
          std::abs(in.fB - colour.b) < .03f && out.fR < .03f && out.fG < .03f &&
          out.fB < .03f)
        return true;
    }
  return false;
}

}  // namespace

TEST(ComposeSurfaceScene,
     ContentStatedUnlitIsItsOwnColourAndPlainContentIsLit) {
  // Stated unlit — in either spelling — every fill, keyline, ink and ribbon
  // is its own colour: the colour in the emissive map over a black base.
  // The same content as a plain colour is the base colour and emits
  // nothing, so a scene's light falls on it.
  const material::Color colour{.9f, .3f, .1f, 1};
  struct Case {
    const char* name;
    material::Material finish;
    bool ownColour;
  };
  const std::vector<Case> cases{
      {"unlit surface options", material::from(colour).surface({.unlit = true}),
       true},
      {"unlit surface program", material::surface::unlit({.baseColor = colour}),
       true},
      {"plain colour", material::Material(colour), false},
  };
  for (const Case& each : cases) {
    SCOPED_TRACE(each.name);
    const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
    ASSERT_NE(scene, nullptr);
    scene->render(ownColourPage(each.finish));
    const SkBitmap base = pixelsOf(*scene, Role::BaseColor);
    const SkBitmap emissive = pixelsOf(*scene, Role::Emissive);
    ASSERT_FALSE(base.empty());
    const SkBitmap& shown = each.ownColour ? emissive : base;
    const SkBitmap& dark = each.ownColour ? base : emissive;
    EXPECT_TRUE(
        showsColourIn(shown, dark, SkIRect::MakeLTRB(14, 14, 26, 26), colour))
        << "the fill";
    EXPECT_TRUE(
        showsColourIn(shown, dark, SkIRect::MakeLTRB(0, 14, 3, 26), colour))
        << "the keyline";
    EXPECT_TRUE(
        showsColourIn(shown, dark, SkIRect::MakeLTRB(44, 0, 128, 48), colour))
        << "the whole-passage ink";
    EXPECT_TRUE(
        showsColourIn(shown, dark, SkIRect::MakeLTRB(0, 48, 64, 96), colour))
        << "the per-glyph ink";
    EXPECT_TRUE(
        showsColourIn(shown, dark, SkIRect::MakeLTRB(74, 70, 79, 90), colour))
        << "the ribbon";
  }
}
