/** @file
 * The sampled slots past the base colour, and the door a foreign texture
 * comes in by.
 *
 * WHAT EACH TEST HOLDS THE DEVICE TO is what its slot MEANS, not a
 * plate: an emissive map lights, a cutout
 * drops texels outright, and a normal map tilts the shading — each read
 * off the pixels of the same card rendered with and without the map, so
 * what is asserted is the map's effect and not a number a rasteriser
 * happens to produce.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/ocio/Ocio.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilworld/World.h>
#include <sigilworld/diligent/Import.h>
#include <sigilworld/diligent/Runtime.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <glm/geometric.hpp>
#include <string>
#include <utility>
#include <vector>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;
using ::sigil::world::diligent::at;

namespace {

constexpr glm::ivec2 kExtent{120, 120};

/** A texture of @p width x @p height, drawn by @p paint. Named by a key
 *  so two identical asks are one texture. */
material::Texture drawnTexture(const std::string& key, int width, int height,
                               const std::function<void(SkCanvas&)>& paint) {
  return material::Texture(
      sigil::media::PixelSource::produce(key, [width, height, paint] {
        sk_sp<SkSurface> surface =
            SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
        surface->getCanvas()->clear(SK_ColorTRANSPARENT);
        paint(*surface->getCanvas());
        return surface->makeImageSnapshot();
      }));
}

/** One flat colour, one texel. */
material::Texture flat(const std::string& key, SkColor4f colour) {
  return drawnTexture(key, 1, 1,
                      [colour](SkCanvas& canvas) { canvas.clear(colour); });
}

/** One frame, rendered on @p runtime and photographed square on. */
SkBitmap plateOf(const Frame& frame, const Runtime& runtime) {
  return diligent::photograph(frame, runtime, kExtent);
}

/** …and a card wearing @p surface, on the same terms. */
SkBitmap cardOn(const material::Material& surface, const Runtime& runtime) {
  return plateOf(diligent::card(surface, kExtent), runtime);
}

float luma(SkColor4f c) { return c.fR * 0.3f + c.fG * 0.59f + c.fB * 0.11f; }

}  // namespace

TEST(SurfaceSlots, AnEmissiveMapCarriesItsOwnColour) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;

  // Emission is the map TIMES the strength, so a surface with no
  // strength emits nothing whatever its slot holds — which is what makes
  // the stock parameters say "not an emitter" rather than "a white one".
  material::surface::SurfaceParameters parameters;
  parameters.baseColor = {0.1f, 0.1f, 0.1f, 1.0f};
  parameters.emissive = {1, 1, 1, 1};
  material::Material dark = material::surface::program(parameters);
  parameters.emissiveStrength = 1;
  material::Material glowing = material::surface::program(parameters);
  glowing.slot(material::surface::kEmissiveSlot,
               flat("world.test.emissive", {0.9f, 0.2f, 0.2f, 1.0f}));

  const SkColor4f bare = at(cardOn(dark, on.runtime), 0.5f, 0.5f);
  const SkColor4f lit = at(cardOn(glowing, on.runtime), 0.5f, 0.5f);
  EXPECT_GT(lit.fR, bare.fR + 0.3f) << "the emission reaches the pixels";
  EXPECT_GT(lit.fR, lit.fG + 0.3f) << "and it is the MAP\'s colour";
}

namespace {

/** A surface whose base is black and whose light is all its own, as a
 *  page of content that takes no light arrives: the emission is what the
 *  surface shows, at the value the map holds. */
material::Material glowingOnBlack() {
  material::texture::TextureMaps maps;
  maps.maps.emplace(material::texture::Role::BaseColor,
                    flat("world.test.black-base", {0, 0, 0, 1}));
  maps.maps.emplace(material::texture::Role::Emissive,
                    flat("world.test.own-light", {0.7f, 0.2f, 0.1f, 1}));
  return material::surface::program(maps);
}

void expectOwnLight(const SkColor4f& centre) {
  EXPECT_NEAR(centre.fR, 0.7f, 0.03f);
  EXPECT_NEAR(centre.fG, 0.2f, 0.03f);
  EXPECT_NEAR(centre.fB, 0.1f, 0.03f);
}

}  // namespace

TEST(SurfaceSlots, EmissionShowsOverABlackBaseAsAuthored) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // With nothing lighting it, a camera that reads the picture with no
  // curve shows the light the surface gives off of its own at the value
  // it was authored. (Under a light, a black dielectric still reflects
  // its few per cent.)
  const Group root =
      group({mesh(::sigil::geometry::mesh::quad(120, 120))
                 .key("card")
                 .fill(glowingOnBlack()),
             camera(diligent::levelEye(), {.toneCurve = ToneCurve::Linear})
                 .key("camera")})
          .key("set");
  Frame plain(root);
  plain.extent(kExtent).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  expectOwnLight(at(plateOf(plain, on.runtime), 0.5f, 0.5f));
}

namespace {

/** A grey card facing the camera, its normal map one texel tilted 45
 *  degrees toward @p tilt (x right, y up the picture, green up), lit by
 *  one sun shining from @p from. */
float tiltedBrightness(glm::vec2 tilt, glm::vec3 from, const Runtime& runtime) {
  const glm::vec3 n = glm::normalize(glm::vec3{tilt.x, tilt.y, 1});
  material::Material surface =
      material::surface::program({.baseColor = {.8f, .8f, .8f, 1}});
  surface.slot(material::surface::kNormalSlot,
               flat("world.test.tilt." + std::to_string(tilt.x) + "." +
                        std::to_string(tilt.y),
                    {n.x * .5f + .5f, n.y * .5f + .5f, n.z * .5f + .5f, 1}));
  const Group root = group({directionalLight(-from).key("sun"),
                            mesh(::sigil::geometry::mesh::quad(120, 120))
                                .key("card")
                                .fill(surface)})
                         .key("set");
  Frame frame(root);
  frame.extent(kExtent)
      .defaultCamera(diligent::levelEye())
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return luma(at(plateOf(frame, runtime), .5f, .5f));
}

}  // namespace

TEST(SurfaceSlots, ANormalMapTurnsTowardTheLightItIsTiltedToward) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // Red is right and green is up the picture: a face tilted toward the
  // side a light shines from takes more of it than one tilted away.
  const glm::vec3 right{1, 0, .6f}, above{0, 1, .6f};
  EXPECT_GT(tiltedBrightness({1, 0}, right, on.runtime),
            tiltedBrightness({-1, 0}, right, on.runtime) + .05f);
  EXPECT_GT(tiltedBrightness({0, 1}, above, on.runtime),
            tiltedBrightness({0, -1}, above, on.runtime) + .05f);
}

TEST(SurfaceSlots, AnOpacityCutoutDropsTexelsOutright) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;

  material::surface::SurfaceParameters parameters;
  parameters.baseColor = {0.9f, 0.5f, 0.2f, 1.0f};
  parameters.alphaCutoff = 0.5f;
  material::Material cut = material::surface::program(parameters);
  cut.slot(material::surface::kOpacitySlot,
           drawnTexture("world.test.opacity", 2, 1, [](SkCanvas& canvas) {
             canvas.clear(SK_ColorWHITE);
             SkPaint paint;
             paint.setColor(SK_ColorBLACK);
             canvas.drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
           }));

  const SkBitmap plate = cardOn(cut, on.runtime);
  // Below the threshold the surface is ABSENT, so the pass's clear
  // stands there; above it the card is its own colour.
  EXPECT_LT(luma(at(plate, 0.35f, 0.5f)), 0.02f);
  EXPECT_GT(luma(at(plate, 0.65f, 0.5f)), 0.2f);
}

TEST(SurfaceSlots, ANormalMapTiltsTheShading) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;

  material::Material plain =
      material::surface::program({.baseColor = {0.8f, 0.8f, 0.8f, 1.0f}});
  material::Material bumped = plain;
  // Two texels, each a tangent normal leaning hard the opposite way, so
  // the two halves of one flat card face two different directions and
  // the sun reaches them differently.
  bumped.slot(material::surface::kNormalSlot,
              drawnTexture("world.test.normal", 2, 1, [](SkCanvas& canvas) {
                SkPaint paint;
                paint.setColor(SkColor4f{0.05f, 0.5f, 0.6f, 1}.toSkColor());
                canvas.drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
                paint.setColor(SkColor4f{0.95f, 0.5f, 0.6f, 1}.toSkColor());
                canvas.drawRect(SkRect::MakeXYWH(1, 0, 1, 1), paint);
              }));

  const SkBitmap bare = cardOn(plain, on.runtime);
  const SkBitmap tilted = cardOn(bumped, on.runtime);
  // A flat card with no map shades ALMOST alike across its face — a
  // light standing off to one side falls a little more steeply on one
  // half than the other, which is the card's own falloff and not a map…
  const float falloff =
      std::abs(luma(at(bare, 0.3f, 0.5f)) - luma(at(bare, 0.7f, 0.5f)));
  EXPECT_LT(falloff, 0.05f);
  // …and with a map, the two halves part company by much more than that.
  const float split =
      std::abs(luma(at(tilted, 0.3f, 0.5f)) - luma(at(tilted, 0.7f, 0.5f)));
  EXPECT_GT(split, falloff + 0.05f);
}

TEST(SurfaceSlots, ASlotDressedInWhiteIsTheSamePictureAsOneDressedInNothing) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;

  // Every slot of a surface nobody dressed holds the neutral fill, which
  // the renderer leaves unbound so the shader reads one white texel. So
  // an undressed surface and one dressed with white in every slot a
  // scalar multiplies must be the SAME picture — not a near one — which
  // is the whole of what "white means no map here" claims.
  const material::surface::SurfaceParameters parameters{
      .baseColor = {0.7f, 0.55f, 0.3f, 1.0f}};
  const material::Material plain = material::surface::program(parameters);
  material::Material white = material::surface::program(parameters);
  const material::Texture texel = flat("world.test.white", SkColors::kWhite);
  white.slot(material::surface::kRoughnessSlot, texel);
  white.slot(material::surface::kMetallicSlot, texel);
  white.slot(material::surface::kOcclusionSlot, texel);
  white.slot(material::surface::kOpacitySlot, texel);

  const SkBitmap bare = cardOn(plain, on.runtime);
  const SkBitmap dressed = cardOn(white, on.runtime);
  for (int y = 0; y < bare.height(); ++y)
    ASSERT_EQ(0, std::memcmp(bare.getAddr32(0, y), dressed.getAddr32(0, y),
                             (size_t)bare.width() * 4))
        << "row " << y;
  const SkColor4f centre = at(bare, 0.5f, 0.5f);
  EXPECT_GT(centre.fR, centre.fG);
  EXPECT_GT(centre.fG, centre.fB);
}

TEST(SurfaceSlots, AChannelThatIsAMaterialReadsAsEmptyAndSaysSo) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;

  // A normal given as luminance grain is a material the device has no body
  // for, so its slot binds the same white texel an undressed one does —
  // the card is the plain card exactly — and the slot is named once on
  // the diagnostic stream, because the picture alone would show a flat
  // surface and no reason for it.
  const material::Color grey{0.8f, 0.8f, 0.8f, 1.0f};
  const material::Material plain = material::from(grey).surface({});
  const material::Material derived =
      material::from(grey).surface({.normal = material::field::grain(0.05f)});

  const SkBitmap bare = cardOn(plain, on.runtime);
  testing::internal::CaptureStderr();
  const SkBitmap dressed = cardOn(derived, on.runtime);
  const std::string said = testing::internal::GetCapturedStderr();

  EXPECT_GT(luma(at(dressed, 0.5f, 0.5f)), 0.05f) << "the card is drawn";
  EXPECT_EQ(diligent::worstChannel(bare, dressed), 0);
  EXPECT_NE(said.find(std::string(material::surface::kNormalSlot)),
            std::string::npos)
      << said;
  EXPECT_NE(said.find("image texture"), std::string::npos) << said;
}

// ---- the door a foreign texture comes in by -------------------------

namespace {

/** The colour the texture below is painted in on the device, and the
 *  one a raster texture is held against it in. */
constexpr SkColor4f kImportedColour{0.15f, 0.75f, 0.35f, 1.0f};

/** A card wearing @p map and its own light, so the map alone decides
 *  every pixel of it. */
material::Material dressedWith(material::Texture map) {
  material::Material surface =
      material::surface::unlit({.baseColor = {1, 1, 1, 1}});
  surface.slot(material::surface::kBaseColorSlot, std::move(map));
  return surface;
}

}  // namespace

TEST(SurfaceSlots, AnImportedNativeTextureCarriesItsColourAndNoHostImage) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const diligent::PaintedTexture painted =
      diligent::paintOnDevice(*on.device, kImportedColour);
  if (!painted.native) GTEST_SKIP() << "2D on the 3D device is unavailable";

  // A texture painted with the graphics API on this very device comes
  // back in through the one door, as an ordinary texture value.
  const material::Texture imported =
      world::diligent::importNative(*on.device, painted.native);
  ASSERT_TRUE(imported.valid());
  // NO HOST IMAGE AT ALL: a picture carrying this colour cannot have
  // come from a copy of it.
  EXPECT_EQ(imported.frameAt().image, nullptr);
  EXPECT_EQ(imported.deviceImage().device, on.device->gpu());

  const SkColor4f centre =
      at(cardOn(dressedWith(imported), on.runtime), 0.5f, 0.5f);
  EXPECT_GT(centre.fG, centre.fR + 0.2f);
  EXPECT_GT(centre.fG, centre.fB + 0.2f);
  on.device->gpu()->destroy(painted.handle);
}

TEST(SurfaceSlots, AnImportedNativeTextureStandsWhereARasterOneWould) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const diligent::PaintedTexture painted =
      diligent::paintOnDevice(*on.device, kImportedColour);
  if (!painted.native) GTEST_SKIP() << "2D on the 3D device is unavailable";

  const material::Texture imported =
      world::diligent::importNative(*on.device, painted.native);
  ASSERT_TRUE(imported.valid());
  const SkColor4f bound =
      at(cardOn(dressedWith(imported), on.runtime), 0.5f, 0.5f);
  const SkColor4f raster =
      at(cardOn(dressedWith(flat("world.test.raster", kImportedColour)),
                on.runtime),
         0.5f, 0.5f);

  // WHICH DOOR A TEXTURE CAME IN BY IS NOT SOMETHING A PICTURE OF IT
  // SHOWS: bound where its pixels already stand, it reads as a copy of
  // them would.
  EXPECT_NEAR(bound.fR, raster.fR, 0.02f);
  EXPECT_NEAR(bound.fG, raster.fG, 0.02f);
  EXPECT_NEAR(bound.fB, raster.fB, 0.02f);
  on.device->gpu()->destroy(painted.handle);
}

namespace {

/** THE FINEST PATTERN A PAGE HOLDS: one-pixel squares of black and white.
 *  Read from a level its size on screen calls for it is grey; read from
 *  its own level at a distance it is whichever squares the pixels happen
 *  to land on. */
void checkerboard(SkCanvas& canvas) {
  canvas.clear(SK_ColorBLACK);
  SkPaint white;
  white.setColor(SK_ColorWHITE);
  for (int y = 0; y < 64; ++y)
    for (int x = y % 2; x < 64; x += 2)
      canvas.drawRect(SkRect::MakeXYWH((float)x, (float)y, 1, 1), white);
}

/** A card of @p size wearing @p map square on, far enough that each of
 *  its pixels covers several of the map's. */
SkBitmap atADistance(const material::Texture& map, const Runtime& runtime) {
  const Group root = group({mesh(::sigil::geometry::mesh::quad(9.3f, 9.3f))
                                .key("card")
                                .fill(dressedWith(map))})
                         .key("set");
  Frame frame(root);
  frame.extent(kExtent)
      .defaultCamera(diligent::levelEye())
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return plateOf(frame, runtime);
}

/** How far apart the lightest and darkest pixels of the three by three
 *  block at the middle of @p plate stand, in the green channel. */
float spreadAtTheMiddle(const SkBitmap& plate) {
  float lightest = 0, darkest = 1;
  for (int y = -1; y <= 1; ++y)
    for (int x = -1; x <= 1; ++x) {
      const float green =
          plate.getColor4f(plate.width() / 2 + x, plate.height() / 2 + y).fG;
      lightest = std::max(lightest, green);
      darkest = std::min(darkest, green);
    }
  return lightest - darkest;
}

}  // namespace

// A PAGE WORN AT A DISTANCE IS READ FROM A SMALLER LEVEL: a page standing
// on the device, made with its chain and painted at level zero, worn by a
// card a few of its pixels across, reads as the grey its squares average
// to, as the same page uploaded from host memory does — and worn with no
// smaller copies, as the squares it lands on.
TEST(SurfaceSlots, APageWornAtADistanceIsReadFromASmallerLevel) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const diligent::PaintedTexture painted = diligent::paintOnDevice(
      *on.device, 64, core::hardware::TextureFormat::RGBA8Unorm, checkerboard,
      core::hardware::mipLevelsFor(64, 64));
  if (!painted.native) GTEST_SKIP() << "2D on the 3D device is unavailable";
  const material::Texture page =
      world::diligent::importNative(*on.device, painted.native);
  ASSERT_TRUE(page.valid());

  const SkBitmap worn = atADistance(page, on.runtime);
  const SkBitmap uploaded =
      atADistance(drawnTexture("world.test.checkerboard", 64, 64, checkerboard),
                  on.runtime);
  material::Texture flat = page;
  flat.mipmaps(false);
  const SkBitmap aliased = atADistance(flat, on.runtime);

  const SkColor4f middle = at(worn, 0.5f, 0.5f);
  EXPECT_GT(middle.fG, 0.3f) << "the card is not grey";
  EXPECT_LT(middle.fG, 0.7f) << "the card is not grey";
  EXPECT_LT(spreadAtTheMiddle(worn), 0.08f)
      << "the page is read from its own level at a distance";
  EXPECT_NEAR(middle.fG, at(uploaded, 0.5f, 0.5f).fG, 0.04f)
      << "a page standing on the device and the same page uploaded differ";
  EXPECT_GT(spreadAtTheMiddle(aliased), 0.2f)
      << "the pattern does not tell a chain from none";
  on.device->gpu()->destroy(painted.handle);
}

TEST(SurfaceSlots, AnImportOfNothingIsNoTextureRatherThanOneThatLies) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A device with no adopted 2D side, or a native texture the device
  // refuses, answers an empty value: a body wearing it is undressed
  // rather than wearing something the renderer invented.
  EXPECT_FALSE(
      world::diligent::importNative(*on.device, core::hardware::NativeTexture{})
          .valid());
}

// ---- the environment map -------------------------------------------
// A panorama both tiers sample, and the two questions a picture of one
// has to answer: is it the right way up, and does it reach a body.

namespace {

/** A panorama whose two hemispheres are two colours, with a hard
 *  horizon: the one sky whose reading is legible from a single pixel,
 *  which is what makes an orientation testable at all. */
material::EnvironmentMap hemispheres(SkColor4f above, SkColor4f below) {
  constexpr int kWidth = 64, kHeight = 32;
  std::vector<float> pixels((size_t)kWidth * kHeight * 4);
  for (int y = 0; y < kHeight; ++y) {
    const SkColor4f colour = y < kHeight / 2 ? above : below;
    for (int x = 0; x < kWidth; ++x) {
      float* px = &pixels[((size_t)y * kWidth + x) * 4];
      px[0] = colour.fR;
      px[1] = colour.fG;
      px[2] = colour.fB;
      px[3] = 1;
    }
  }
  const SkImageInfo info = SkImageInfo::Make(
      kWidth, kHeight, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  return material::EnvironmentMap::fromEquirectangular(
      SkImages::RasterFromPixmapCopy(
          {info, pixels.data(), (size_t)kWidth * 4 * sizeof(float)}));
}

/** A scene standing in @p map read under @p sky and holding nothing else —
 *  no body, no light — so what a plate of it shows is the panorama shown
 *  behind it alone. */
Frame skyAlone(const world::EnvironmentMap& map,
               const world::EnvironmentOptions& sky) {
  Frame frame(group().key("set"));
  frame.environment(map, sky);
  frame.extent(kExtent)
      .defaultCamera(diligent::levelEye())
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

}  // namespace

TEST(Environment, TheBackdropPutsTheZenithAtTheTopOfTheFrame) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // v = 0 is the zenith, and a camera looking along -z from above the
  // origin has the upper hemisphere in the upper half of its frame. A
  // sky read with its second axis the wrong way round puts the ground
  // where the zenith is and every reflection in the scene with it, and
  // nothing about a smooth panorama would show that.
  world::EnvironmentOptions sky;
  world::EnvironmentMap map;
  map = hemispheres({0.9f, 0.1f, 0.1f, 1}, {0.1f, 0.1f, 0.9f, 1});
  sky.shown = 1.0f;
  const SkBitmap plate = plateOf(skyAlone(map, sky), on.runtime);

  const SkColor4f top = at(plate, 0.5f, 0.08f);
  const SkColor4f bottom = at(plate, 0.5f, 0.92f);
  EXPECT_GT(top.fR, top.fB) << "the zenith is red and it belongs at the top";
  EXPECT_GT(bottom.fB, bottom.fR)
      << "the nadir is blue and it belongs at the bottom";
}

TEST(Environment, APartOfTheSkyIsThatPartOfTheWholeSky) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The sky is a fill over every pixel of the target, read through the
  // projection a pixel turns into a ray with — so a frame whose targets
  // are a part of a larger picture reads the rays the whole picture
  // reads there, and its horizon stands on the whole picture's row.
  constexpr glm::ivec2 kWhole{kExtent.x * 4, kExtent.y * 4};
  constexpr glm::ivec2 kCorner{100, 180};
  world::EnvironmentOptions sky;
  world::EnvironmentMap map;
  map = hemispheres({0.9f, 0.1f, 0.1f, 1}, {0.1f, 0.1f, 0.9f, 1});
  sky.shown = 1.0f;
  Frame whole = skyAlone(map, sky);
  whole.extent(kWhole);
  Frame part = skyAlone(map, sky);
  part.viewOffset({kWhole, kCorner});
  const SkIRect region =
      SkIRect::MakeXYWH(kCorner.x, kCorner.y, kExtent.x, kExtent.y);

  const SkBitmap all = diligent::photograph(whole, on.runtime, kWhole);
  const SkBitmap piece = diligent::photograph(part, on.runtime, kWhole);
  // The part straddles the horizon, so both hemispheres are in it.
  const SkColor4f top = piece.getColor4f(kCorner.x + 8, kCorner.y + 2);
  const SkColor4f bottom =
      piece.getColor4f(kCorner.x + 8, kCorner.y + kExtent.y - 2);
  EXPECT_GT(top.fR, top.fB);
  EXPECT_GT(bottom.fB, bottom.fR);
  EXPECT_LE(diligent::pixelsApart(all, piece, region, 2),
            kExtent.x * kExtent.y / 500);
}

TEST(Environment, ABackdropAtZeroStrengthDrawsNothing) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The strength is also the switch, so there is no flag that can
  // disagree with it: a scene that carries a panorama and shows none of it
  // stands against whatever the pass cleared to.
  world::EnvironmentOptions sky;
  world::EnvironmentMap map;
  map = hemispheres({0.9f, 0.1f, 0.1f, 1}, {0.1f, 0.1f, 0.9f, 1});
  const SkBitmap plate = plateOf(skyAlone(map, sky), on.runtime);
  EXPECT_LT(luma(at(plate, 0.5f, 0.08f)), 0.02f);
  EXPECT_LT(luma(at(plate, 0.5f, 0.92f)), 0.02f);
}

TEST(Environment, AGroundProjectedBackdropMovesTheHorizonWithTheEye) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // At infinity a sky is the same picture from everywhere; projected
  // onto a ground sphere, an eye that rises above the sphere's centre
  // sees more ground and the horizon drops down the frame.
  world::EnvironmentOptions sky;
  world::EnvironmentMap map;
  map = hemispheres({0.9f, 0.1f, 0.1f, 1}, {0.1f, 0.1f, 0.9f, 1});
  sky.shown = 1.0f;

  const auto horizonRow = [&](float height) {
    geometry::mesh::camera::Camera camera;
    camera.eye = {0, height, 0};
    camera.target = {0, height, -100};
    Frame frame = skyAlone(map, sky);
    frame.defaultCamera(camera);
    const SkBitmap plate = diligent::photograph(frame, on.runtime, kExtent);
    for (int y = 0; y < plate.height(); ++y) {
      const SkColor4f c = plate.getColor4f(plate.width() / 2, y);
      if (c.fB > c.fR) return y;
    }
    return -1;
  };

  const int atInfinityLow = horizonRow(0.0f);
  const int atInfinityHigh = horizonRow(20.0f);
  ASSERT_GE(atInfinityLow, 0);
  EXPECT_EQ(atInfinityLow, atInfinityHigh)
      << "a sky at infinity does not move with the eye";

  sky.groundRadius = 100.0f;
  const int projectedCentre = horizonRow(0.0f);
  const int projectedHigh = horizonRow(20.0f);
  EXPECT_EQ(projectedCentre, atInfinityLow)
      << "an eye at the centre reads the sphere by direction";
  EXPECT_GT(projectedHigh, projectedCentre + 4)
      << "an eye above the centre looks down on the horizon";
}

TEST(Environment, AMirrorWearsTheSkyAndAMatteSurfaceIsLitByIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // What the test pins is the difference between a mirror and a matte
  // surface under one sky, with no emitter anywhere in the scene: the
  // mirror takes the sky's own colour off its reflected view vector,
  // the matte one keeps its albedo and is lit by what falls on it. A
  // scene with neither an emitter nor a sky would be black.
  // A NEUTRAL sky, so what the matte surface shows is its own albedo:
  // under a coloured one a green surface reads by the product of the
  // two, which is a fact about multiplication and not about the tier.
  world::EnvironmentOptions sky;
  world::EnvironmentMap map;
  map = hemispheres({0.8f, 0.8f, 0.8f, 1}, {0.8f, 0.8f, 0.8f, 1});

  const auto photographWith = [&](const material::Material& surface) {
    Frame frame(group({mesh(::sigil::geometry::mesh::quad(120, 120))
                           .key("card")
                           .fill(surface)})
                    .key("set"));
    frame.environment(map, sky);
    frame.extent(kExtent)
        .defaultCamera(diligent::levelEye())
        .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    return plateOf(frame, on.runtime);
  };

  const SkBitmap chrome = photographWith(material::surface::program(
      {.baseColor = {1, 1, 1, 1}, .metallic = 1, .roughness = 0.04f}));
  const SkBitmap matte = photographWith(material::surface::program(
      {.baseColor = {0.1f, 0.6f, 0.1f, 1}, .metallic = 0, .roughness = 0.9f}));

  const SkColor4f mirrored = at(chrome, 0.5f, 0.5f);
  const SkColor4f diffuse = at(matte, 0.5f, 0.5f);
  // The mirror wears the sky, which is neutral, so no channel of it
  // stands out; the matte surface keeps its own green.
  EXPECT_NEAR(mirrored.fR, mirrored.fG, 0.05f);
  EXPECT_GT(diffuse.fG, diffuse.fR + 0.05f);
  // And the sky reaches them both — a body under a lit sky with no
  // emitter in the scene is not black.
  EXPECT_GT(luma(diffuse), 0.02f);
  EXPECT_GT(luma(mirrored), 0.02f);
}

// ---- what a bare body cannot wear ------------------------------------

TEST(SurfaceRefusals, AUnitSourceOnABareBodyIsRefusedByName) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A relief and a brushing read the unit a page paints a material into;
  // a body wearing the material itself has no page and no unit, so it is
  // drawn without them — exactly as the same surface stating neither —
  // and the body and each source are named once.
  const material::Color grey{0.8f, 0.8f, 0.8f, 1.0f};
  const material::Material plain =
      material::from(grey).surface({.metallic = 1.0f, .roughness = .4f});
  const material::Material reading =
      material::from(grey)
          .surface({.metallic = 1.0f, .roughness = .4f})
          .relief({.shoulder = 4, .depth = 1})
          .brushed({.strength = 0});
  const SkBitmap bare = cardOn(plain, on.runtime);
  testing::internal::CaptureStderr();
  const SkBitmap refused = cardOn(reading, on.runtime);
  const std::string said = testing::internal::GetCapturedStderr();
  EXPECT_EQ(diligent::worstChannel(bare, refused), 0);
  EXPECT_NE(said.find("\"card\""), std::string::npos) << said;
  EXPECT_NE(said.find("relief"), std::string::npos) << said;
  EXPECT_NE(said.find("brushing"), std::string::npos) << said;
}

TEST(SurfaceRefusals, ALayeredStackOnABareBodyIsRefusedByName) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A colour with a second colour laid half over it has no program the
  // device can run as a base colour, so the body says so by name, and
  // what it carries.
  const material::Material layered =
      material::from(material::Color{0.9f, 0.2f, 0.1f, 1})
          .layer(material::from(material::Color{0.1f, 0.2f, 0.9f, 1}),
                 {.opacity = .5f})
          .surface({.roughness = .6f});
  testing::internal::CaptureStderr();
  const SkBitmap drawn = cardOn(layered, on.runtime);
  const std::string said = testing::internal::GetCapturedStderr();
  EXPECT_GT(luma(at(drawn, 0.5f, 0.5f)), 0.05f) << "the card is drawn";
  EXPECT_NE(said.find("\"card\""), std::string::npos) << said;
  EXPECT_NE(said.find("layered colour stack"), std::string::npos) << said;
}

TEST(SurfaceSlots, ANormalDerivedFromAHeightDressesADeviceBody) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  constexpr glm::ivec2 kSide{96, 96};
  // How far the shading strays from its mean across the middle of a card.
  const auto spread = [&](std::optional<material::Material> normal) {
    material::SurfaceOptions options{.roughness = 0.6f};
    options.normal = std::move(normal);
    const SkBitmap plate = diligent::photograph(
        diligent::card(material::from(material::Color{0.7f, 0.7f, 0.7f, 1})
                           .surface(options),
                       kSide),
        on.runtime, kSide);
    double sum = 0;
    double squares = 0;
    int count = 0;
    for (int y = 24; y < 72; ++y)
      for (int x = 24; x < 72; ++x) {
        const double grey = plate.getColor4f(x, y).fG;
        sum += grey;
        squares += grey * grey;
        ++count;
      }
    const double mean = sum / count;
    return std::sqrt(std::max(squares / count - mean * mean, 0.0));
  };
  const double plain = spread(std::nullopt);
  const double dressed = spread(material::surface::normalFromHeight(
      material::field::cellular(6.0f), {.depth = 16.0f}));
  EXPECT_LT(plain, 0.01);
  EXPECT_GT(dressed, plain + 0.03) << "the relief turns the shading across it";
}

TEST(SurfaceSlots, EveryFieldPatternAndBakeVariesAcrossADeviceBody) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  constexpr glm::ivec2 kSide{96, 96};
  // How far the picture strays from its mean across the middle of a card
  // whose base colour is @p base.
  const auto spread = [&](const material::Material& base) {
    const SkBitmap plate = diligent::photograph(
        diligent::card(material::Material(base).surface({.roughness = 0.6f}),
                       kSide),
        on.runtime, kSide);
    double sum = 0;
    double squares = 0;
    int count = 0;
    for (int y = 24; y < 72; ++y)
      for (int x = 24; x < 72; ++x) {
        const double grey = luma(plate.getColor4f(x, y));
        sum += grey;
        squares += grey * grey;
        ++count;
      }
    const double mean = sum / count;
    return std::sqrt(std::max(squares / count - mean * mean, 0.0));
  };
  // A body's region is one unit across, so every length below is a
  // fraction of the card.
  const material::Material cells = material::field::cellular(6.0f);
  std::vector<std::pair<std::string, material::Material>> cards = {
      {"field.noise", material::field::noise(6.0f, 3, 2.0f)},
      {"field.ripple",
       material::field::ripple(0.08f, 0.3f).slot("content", cells)},
      {"field.halftoneRamp",
       material::field::halftoneRamp(0.5f, 0.05f, 0.22f,
                                     material::Color{0.9f, 0.9f, 0.9f, 1})},
      {"pattern.scanlines",
       material::pattern::scanlines(
           {.color = {0.9f, 0.9f, 0.9f, 1}, .period = 0.2f, .on = 0.1f})},
      {"pattern.stipple",
       material::pattern::stipple(
           {.color = {0.9f, 0.9f, 0.9f, 1}, .cell = 0.125f})},
  };
  if (material::ocio::available()) {
    cards.emplace_back("color.response1d",
                       material::ocio::exponent(2.2f).slot("content", cells));
    material::Material volume =
        material::ocio::convert("ocio://default", "ACEScg", "sRGB - Texture");
    if (volume.leaf("lut") != nullptr)
      cards.emplace_back("color.lut3d", volume.slot("content", cells));
  }
  const double plain =
      spread(material::from(material::Color{0.7f, 0.7f, 0.7f, 1}));
  EXPECT_LT(plain, 0.01);
  for (const auto& [name, base] : cards) {
    EXPECT_EQ(base.recipe().name(), name);
    testing::internal::CaptureStderr();
    const double varied = spread(base);
    const std::string said = testing::internal::GetCapturedStderr();
    RecordProperty(name, std::to_string(varied));
    EXPECT_GT(varied, plain + 0.02) << name << " varies across the card";
    EXPECT_EQ(said.find("no body the device can compile"), std::string::npos)
        << name << ": " << said;
  }
}

TEST(SurfaceSlots,
     AHeightsDerivedNormalShadesAsTheSameNormalSuppliedAsATexture) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  constexpr int kTexels = 256;
  constexpr float kDepth = 6.0f;
  constexpr float kStep = 1.0f;
  constexpr float kWaves = 3.0f;
  const auto height = [&](float u) {
    return 0.5f + 0.5f * std::sin(2.0f * 3.14159265f * kWaves * u);
  };
  // One raking light, crossing the card from its right, so a slope along
  // the card's width shows as light and shade.
  const auto plate = [&](std::optional<material::Material> normal) {
    material::SurfaceOptions options{.roughness = 0.6f};
    options.normal = std::move(normal);
    const Group root =
        group({directionalLight({-1.0f, 0.0f, -0.35f}).key("sun"),
               mesh(::sigil::geometry::mesh::quad(120, 120))
                   .key("card")
                   .fill(material::from(material::Color{0.7f, 0.7f, 0.7f, 1})
                             .surface(options))})
            .key("set");
    Frame frame(root);
    frame.extent(kExtent)
        .defaultCamera(diligent::levelEye())
        .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    return diligent::photograph(frame, on.runtime, kExtent);
  };
  const SkBitmap flat = plate(std::nullopt);
  // How many pixels of the picture the card's width covers, which is the
  // span of the surface the derivation's step is measured in.
  int covered = 0;
  for (int x = 0; x < kExtent.x; ++x)
    if (luma(flat.getColor4f(x, kExtent.y / 2)) > 0.02f) ++covered;
  ASSERT_GT(covered, kExtent.x / 4);

  // The height, as numbers exactly as stored.
  const auto heightTexture =
      drawnTexture("world.test.height-waves", kTexels, 1,
                   [&](SkCanvas& canvas) {
                     for (int x = 0; x < kTexels; ++x) {
                       const float h = height((x + 0.5f) / kTexels);
                       SkPaint paint;
                       paint.setColor4f({h, h, h, 1});
                       canvas.drawRect(SkRect::MakeXYWH((float)x, 0, 1, 1),
                                       paint);
                     }
                   })
          .colorSpace(media::ColorSpace::Data);
  // The same normal, worked out here as the derivation works it out: the
  // central difference one step of the card's own pixels either side,
  // scaled by the depth over the step.
  const float across = kStep / (float)covered;
  const auto normalTexture =
      drawnTexture(
          "world.test.normal-waves", kTexels, 1,
          [&](SkCanvas& canvas) {
            for (int x = 0; x < kTexels; ++x) {
              const float u = (x + 0.5f) / kTexels;
              glm::vec3 n{(height(u - across) - height(u + across)) * kDepth /
                              kStep * 0.5f,
                          0.0f, 1.0f};
              n /= std::max(std::max(std::abs(n.x), std::abs(n.y)), 1.0f);
              n = glm::normalize(n) * 0.5f + 0.5f;
              SkPaint paint;
              paint.setColor4f({n.x, n.y, n.z, 1});
              canvas.drawRect(SkRect::MakeXYWH((float)x, 0, 1, 1), paint);
            }
          })
          .colorSpace(media::ColorSpace::Data);

  const SkBitmap derived = plate(material::surface::normalFromHeight(
      material::image(heightTexture), {.depth = kDepth, .step = kStep}));
  const SkBitmap supplied = plate(material::image(normalTexture));
  float differs = 0;
  float relief = 0;
  for (int x = kExtent.x / 2 - covered / 3; x < kExtent.x / 2 + covered / 3;
       ++x) {
    const float fromHeight = luma(derived.getColor4f(x, kExtent.y / 2));
    const float fromTexture = luma(supplied.getColor4f(x, kExtent.y / 2));
    const float plain = luma(flat.getColor4f(x, kExtent.y / 2));
    differs = std::max(differs, std::abs(fromHeight - fromTexture));
    relief = std::max(relief, std::abs(fromTexture - plain));
  }
  RecordProperty("differs", std::to_string(differs));
  RecordProperty("relief", std::to_string(relief));
  EXPECT_GT(relief, 0.1f) << "the supplied normal turns the shading";
  EXPECT_LT(differs, relief * 0.15f)
      << "the derived normal shades as the supplied one: " << differs
      << " against a relief of " << relief;
}
