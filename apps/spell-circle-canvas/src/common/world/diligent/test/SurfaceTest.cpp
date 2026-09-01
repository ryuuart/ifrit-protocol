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
#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilworld/diligent/Device.h>
#include <sigilworld/diligent/Import.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <cstring>
#include <functional>
#include <memory>
#include <string>

using namespace sigil;
using namespace sigil::world;

namespace {

constexpr SkISize kExtent{120, 120};

struct OnDevice {
  std::unique_ptr<world::diligent::Device> device;
  Runtime runtime;
  std::string error;
  explicit operator bool() const { return (bool)device; }
};

OnDevice onDevice() {
  OnDevice out;
  const world::diligent::DeviceConfig config;
  out.device = world::diligent::Device::create(config, &out.error);
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
  skia::GpuDevice* gpu = on.device->gpu();
  skia::GraphiteContext* graphite = on.device->graphite();
  if (!gpu || !graphite) GTEST_SKIP() << "2D on the 3D device is unavailable";

  // A texture painted with the graphics API on this very device, handed
  // back as the API's own object…
  skia::TextureDesc desc;
  desc.width = desc.height = 16;
  desc.format = skia::TextureFormat::RGBA8Unorm;
  const skia::TextureHandle painted = gpu->createTexture(desc);
  ASSERT_TRUE((bool)painted);
  const SkColor4f colour{0.15f, 0.75f, 0.35f, 1.0f};
  {
    const world::diligent::Device::QueueLock lock(*on.device);
    skia::OffscreenSurface surface(*graphite, *gpu, painted);
    ASSERT_NE(surface.canvas(), nullptr);
    surface.canvas()->clear(colour);
    surface.submit();
  }
  const skia::NativeTexture native = gpu->exportNative(painted);
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
      world::diligent::importNative(*on.device, skia::NativeTexture{}).valid());
}
