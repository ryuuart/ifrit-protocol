/** @file
 * The sampled slots past the base colour, and the door a foreign texture
 * comes in by.
 *
 * WHAT EACH TEST HOLDS THE DEVICE TO is what its slot MEANS, not a
 * plate: an occlusion map darkens, an emissive map lights, a cutout
 * drops texels outright, and a normal map tilts the shading — each read
 * off the pixels of the same card rendered with and without the map, so
 * what is asserted is the map's effect and not a number a rasteriser
 * happens to produce.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilgeometry/device/Device.h>
#include <sigilworld/diligent/Import.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilworld/scene/Scene.h>

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace sigil;
using namespace sigil::world;

namespace {

constexpr SkISize kExtent{120, 120};

struct OnDevice {
  std::unique_ptr<geometry::device::Device> device;
  Runtime runtime;
  std::string error;
  explicit operator bool() const { return (bool)device; }
};

OnDevice onDevice() {
  OnDevice out;
  const geometry::device::DeviceConfig config;
  out.device = geometry::device::Device::create(config, &out.error);
  if (out.device) out.runtime = world::diligent::runtime(*out.device);
  return out;
}

Camera eye() {
  Camera camera;
  camera.eye = {0, 0, 200};
  camera.target = {0, 0, 0};
  return camera;
}

/** A texture of @p width x @p height, painted by @p paint. Named by a
 *  key so two identical asks are one texture. */
material::Texture painted(const std::string& key, int width, int height,
                          const std::function<void(SkCanvas&)>& paint) {
  return material::Texture::produce(key, [width, height, paint] {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    paint(*surface->getCanvas());
    return surface->makeImageSnapshot();
  });
}

/** One flat colour, one texel. */
material::Texture flat(const std::string& key, SkColor4f colour) {
  return painted(key, 1, 1,
                 [colour](SkCanvas& canvas) { canvas.clear(colour); });
}

/** A card facing the camera, wearing @p surface, lit by one sun. */
Frame card(const material::Material& surface) {
  namespace gm = ::sigil::geometry::mesh;
  Element root =
      Element()
          .key("set")
          .child(Element().key("sun").light(light::sun({-0.2f, -0.3f, -1.0f})))
          .child(Element().key("card").mesh(gm::quad(120, 120)).fill(surface));
  Frame frame(root);
  frame.extent(kExtent).camera(eye()).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

SkBitmap photograph(const Frame& frame, const Runtime& runtime) {
  motion::Ticker ticker;
  Scene scene(ticker);
  Frame copy = frame;
  copy.runtime(runtime);
  ticker.tick(1.0 / 60.0);
  scene.render(copy);

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorBLACK);
  scene.draw(canvas, eye());
  return bitmap;
}

SkColor4f at(const SkBitmap& plate, float x, float y) {
  return plate.getColor4f((int)(x * (float)plate.width()),
                          (int)(y * (float)plate.height()));
}

float luma(SkColor4f c) { return c.fR * 0.3f + c.fG * 0.59f + c.fB * 0.11f; }

}  // namespace

TEST(SurfaceSlots, AnOcclusionMapDarkensWhereItIsDark) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  material::Material plain =
      material::kit::surface({.baseColor = {0.9f, 0.9f, 0.9f, 1.0f}});
  material::Material occluded = plain;
  // Two texels: the left half black, the right half white.
  occluded.child(material::kit::kOcclusionSlot,
                 painted("world.test.occlusion", 2, 1, [](SkCanvas& canvas) {
                   canvas.clear(SK_ColorWHITE);
                   SkPaint paint;
                   paint.setColor(SK_ColorBLACK);
                   canvas.drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
                 }));

  const SkBitmap bare = photograph(card(plain), on.runtime);
  const SkBitmap dressed = photograph(card(occluded), on.runtime);
  // The map's dark half darkens and its white half does not.
  EXPECT_LT(luma(at(dressed, 0.35f, 0.5f)), luma(at(bare, 0.35f, 0.5f)) * 0.5f);
  EXPECT_NEAR(luma(at(dressed, 0.65f, 0.5f)), luma(at(bare, 0.65f, 0.5f)),
              0.02f);
}

TEST(SurfaceSlots, AnEmissiveMapCarriesItsOwnColour) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  // Emission is the map TIMES the strength, so a surface with no
  // strength emits nothing whatever its slot holds — which is what makes
  // the stock params say "not an emitter" rather than "a white one".
  material::kit::SurfaceParams params;
  params.baseColor = {0.1f, 0.1f, 0.1f, 1.0f};
  params.emissive = {1, 1, 1, 1};
  material::Material dark = material::kit::surface(params);
  params.emissiveStrength = 1;
  material::Material glowing = material::kit::surface(params);
  glowing.child(material::kit::kEmissiveSlot,
                flat("world.test.emissive", {0.9f, 0.2f, 0.2f, 1.0f}));

  const SkColor4f bare = at(photograph(card(dark), on.runtime), 0.5f, 0.5f);
  const SkColor4f lit = at(photograph(card(glowing), on.runtime), 0.5f, 0.5f);
  EXPECT_GT(lit.fR, bare.fR + 0.3f) << "the emission reaches the pixels";
  EXPECT_GT(lit.fR, lit.fG + 0.3f) << "and it is the MAP\'s colour";
}

TEST(SurfaceSlots, AnOpacityCutoutDropsTexelsOutright) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  material::kit::SurfaceParams params;
  params.baseColor = {0.9f, 0.5f, 0.2f, 1.0f};
  params.alphaCutoff = 0.5f;
  material::Material cut = material::kit::surface(params);
  cut.child(material::kit::kOpacitySlot,
            painted("world.test.opacity", 2, 1, [](SkCanvas& canvas) {
              canvas.clear(SK_ColorWHITE);
              SkPaint paint;
              paint.setColor(SK_ColorBLACK);
              canvas.drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
            }));

  const SkBitmap plate = photograph(card(cut), on.runtime);
  // Below the threshold the surface is ABSENT, so the pass's clear
  // stands there; above it the card is its own colour.
  EXPECT_LT(luma(at(plate, 0.35f, 0.5f)), 0.02f);
  EXPECT_GT(luma(at(plate, 0.65f, 0.5f)), 0.2f);
}

TEST(SurfaceSlots, ANormalMapTiltsTheShading) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  material::Material plain =
      material::kit::surface({.baseColor = {0.8f, 0.8f, 0.8f, 1.0f}});
  material::Material bumped = plain;
  // Two texels, each a tangent normal leaning hard the opposite way, so
  // the two halves of one flat card face two different directions and
  // the sun reaches them differently.
  bumped.child(material::kit::kNormalSlot,
               painted("world.test.normal", 2, 1, [](SkCanvas& canvas) {
                 SkPaint paint;
                 paint.setColor(SkColor4f{0.05f, 0.5f, 0.6f, 1}.toSkColor());
                 canvas.drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
                 paint.setColor(SkColor4f{0.95f, 0.5f, 0.6f, 1}.toSkColor());
                 canvas.drawRect(SkRect::MakeXYWH(1, 0, 1, 1), paint);
               }));

  const SkBitmap bare = photograph(card(plain), on.runtime);
  const SkBitmap tilted = photograph(card(bumped), on.runtime);
  // A flat card with no map shades alike across its whole face…
  EXPECT_NEAR(luma(at(bare, 0.3f, 0.5f)), luma(at(bare, 0.7f, 0.5f)), 0.02f);
  // …and with one, the two halves part company.
  EXPECT_GT(
      std::abs(luma(at(tilted, 0.3f, 0.5f)) - luma(at(tilted, 0.7f, 0.5f))),
      0.05f);
}

TEST(SurfaceSlots, WhiteIsTheNEUTRALEverySlotFallsBackTo) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;

  // Every slot of a surface nobody dressed holds the neutral fill, which
  // the renderer leaves unbound so the shader reads one white texel. So
  // an undressed surface and one dressed with white in every slot a
  // scalar multiplies must be the SAME picture — not a near one — which
  // is the whole of what "white means no map here" claims.
  const material::kit::SurfaceParams params{
      .baseColor = {0.7f, 0.55f, 0.3f, 1.0f}};
  const material::Material plain = material::kit::surface(params);
  material::Material white = material::kit::surface(params);
  const material::Texture texel = flat("world.test.white", SkColors::kWhite);
  white.child(material::kit::kRoughnessSlot, texel);
  white.child(material::kit::kMetallicSlot, texel);
  white.child(material::kit::kOcclusionSlot, texel);
  white.child(material::kit::kOpacitySlot, texel);

  const SkBitmap bare = photograph(card(plain), on.runtime);
  const SkBitmap dressed = photograph(card(white), on.runtime);
  for (int y = 0; y < bare.height(); ++y)
    ASSERT_EQ(0, std::memcmp(bare.getAddr32(0, y), dressed.getAddr32(0, y),
                             (size_t)bare.width() * 4))
        << "row " << y;
  const SkColor4f centre = at(bare, 0.5f, 0.5f);
  EXPECT_GT(centre.fR, centre.fG);
  EXPECT_GT(centre.fG, centre.fB);
}

TEST(SurfaceSlots, AnImportedNativeTextureLandsWhereARasterOneWould) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;
  core::hardware::GpuDevice* gpu = on.device->gpu();
  skia::GraphiteContext* graphite = on.device->graphite();
  if (!gpu || !graphite) GTEST_SKIP() << "2D on the 3D device is unavailable";

  // A texture painted with the graphics API on this very device, handed
  // back as the API's own object…
  core::hardware::TextureDesc desc;
  desc.width = desc.height = 16;
  desc.format = core::hardware::TextureFormat::RGBA8Unorm;
  const core::hardware::TextureHandle painted = gpu->createTexture(desc);
  ASSERT_TRUE((bool)painted);
  const SkColor4f colour{0.15f, 0.75f, 0.35f, 1.0f};
  {
    const geometry::device::Device::QueueLock lock(*on.device);
    skia::OffscreenSurface surface(*graphite, *gpu, painted);
    ASSERT_NE(surface.canvas(), nullptr);
    surface.canvas()->clear(colour);
    surface.submit();
  }
  const core::hardware::NativeTexture native = gpu->exportNative(painted);
  ASSERT_TRUE((bool)native);

  // …comes back in through the one door, as an ordinary texture value.
  const material::Texture imported =
      world::diligent::importNative(*on.device, native);
  ASSERT_TRUE(imported.valid());
  // NO HOST IMAGE AT ALL: a picture carrying this colour cannot have
  // come from a copy of it.
  EXPECT_EQ(imported.image(), nullptr);
  EXPECT_EQ(imported.deviceImage().device, gpu);

  material::Material dressed =
      material::kit::unlit({.baseColor = {1, 1, 1, 1}});
  dressed.child(material::kit::kBaseColorSlot, imported);
  const SkBitmap plate = photograph(card(dressed), on.runtime);
  const SkColor4f centre = at(plate, 0.5f, 0.5f);
  EXPECT_GT(centre.fG, centre.fR + 0.2f);
  EXPECT_GT(centre.fG, centre.fB + 0.2f);

  // …and it stands where a raster texture of the same colour would.
  material::Material raster = material::kit::unlit({.baseColor = {1, 1, 1, 1}});
  raster.child(material::kit::kBaseColorSlot,
               flat("world.test.raster", colour));
  const SkColor4f other = at(photograph(card(raster), on.runtime), 0.5f, 0.5f);
  EXPECT_NEAR(centre.fR, other.fR, 0.02f);
  EXPECT_NEAR(centre.fG, other.fG, 0.02f);
  EXPECT_NEAR(centre.fB, other.fB, 0.02f);

  // An import a device has no adopted GpuDevice for, or one the device
  // refuses, is an empty value rather than a texture that lies.
  EXPECT_FALSE(
      world::diligent::importNative(*on.device, core::hardware::NativeTexture{}).valid());
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
  return material::EnvironmentMap::fromEquirect(SkImages::RasterFromPixmapCopy(
      {info, pixels.data(), (size_t)kWidth * 4 * sizeof(float)}));
}

/** A set carrying @p sky and nothing else — no body, no emitter — so
 *  what a plate of it shows is the backdrop alone. */
Frame skyAlone(const world::Environment& sky) {
  Element root =
      Element().key("set").child(Element().key("sky").environmentMap(sky));
  Frame frame(root);
  frame.extent(kExtent).camera(eye()).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

}  // namespace

TEST(Environment, TheBackdropPutsTheZENITHAtTheTOP) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // v = 0 is the zenith, and a camera looking along -z from above the
  // origin has the upper hemisphere in the upper half of its frame. A
  // sky read with its second axis the wrong way round puts the ground
  // where the zenith is and every reflection in the set with it, and
  // nothing about a smooth panorama would show that.
  world::Environment sky;
  sky.map = hemispheres({0.9f, 0.1f, 0.1f, 1}, {0.1f, 0.1f, 0.9f, 1});
  sky.backdrop.intensity = 1.0f;
  const SkBitmap plate = photograph(skyAlone(sky), on.runtime);

  const SkColor4f top = at(plate, 0.5f, 0.08f);
  const SkColor4f bottom = at(plate, 0.5f, 0.92f);
  EXPECT_GT(top.fR, top.fB) << "the zenith is red and it belongs at the top";
  EXPECT_GT(bottom.fB, bottom.fR)
      << "the nadir is blue and it belongs at the bottom";
}

TEST(Environment, ABackdropAtZeroStrengthDrawsNothing) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The strength is also the switch, so there is no flag that can
  // disagree with it: a set that carries a panorama and shows none of it
  // stands against whatever the pass cleared to.
  world::Environment sky;
  sky.map = hemispheres({0.9f, 0.1f, 0.1f, 1}, {0.1f, 0.1f, 0.9f, 1});
  const SkBitmap plate = photograph(skyAlone(sky), on.runtime);
  EXPECT_LT(luma(at(plate, 0.5f, 0.08f)), 0.02f);
  EXPECT_LT(luma(at(plate, 0.5f, 0.92f)), 0.02f);
}

TEST(Environment, AMirrorWearsTheSkyAndAMatteSurfaceIsLitByIt) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // What the test pins is the difference between a mirror and a matte
  // surface under one sky, with no emitter anywhere in the set: the
  // mirror takes the sky's own colour off its reflected view vector,
  // the matte one keeps its albedo and is lit by what falls on it. A
  // set with neither an emitter nor a sky would be black.
  // A NEUTRAL sky, so what the matte surface shows is its own albedo:
  // under a coloured one a green surface reads by the product of the
  // two, which is a fact about multiplication and not about the tier.
  world::Environment sky;
  sky.map = hemispheres({0.8f, 0.8f, 0.8f, 1}, {0.8f, 0.8f, 0.8f, 1});

  const auto photographWith = [&](const material::Material& surface) {
    Element root = Element()
                       .key("set")
                       .child(Element().key("sky").environmentMap(sky))
                       .child(Element()
                                  .key("card")
                                  .mesh(::sigil::geometry::mesh::quad(120, 120))
                                  .fill(surface));
    Frame frame(root);
    frame.extent(kExtent).camera(eye()).pass(
        geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    return photograph(frame, on.runtime);
  };

  material::kit::SurfaceParams mirror = material::kit::SurfaceParams::chrome();
  mirror.baseColor = {1, 1, 1, 1};
  const SkBitmap chrome = photographWith(material::kit::surface(mirror));
  const SkBitmap matte = photographWith(material::kit::surface(
      material::kit::SurfaceParams::dielectric({0.1f, 0.6f, 0.1f, 1}, 0.9f)));

  const SkColor4f mirrored = at(chrome, 0.5f, 0.5f);
  const SkColor4f diffuse = at(matte, 0.5f, 0.5f);
  // The mirror wears the sky, which is neutral, so no channel of it
  // stands out; the matte surface keeps its own green.
  EXPECT_NEAR(mirrored.fR, mirrored.fG, 0.05f);
  EXPECT_GT(diffuse.fG, diffuse.fR + 0.05f);
  // And the sky reaches them both — a body under a lit sky with no
  // emitter in the set is not black.
  EXPECT_GT(luma(diffuse), 0.02f);
  EXPECT_GT(luma(mirrored), 0.02f);
}
