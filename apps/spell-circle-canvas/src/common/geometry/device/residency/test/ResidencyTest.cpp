/** @file
 * What the device keeps between draws: the depth of a map's chain, the
 * buffers a named mesh is uploaded into once and drawn from after, the
 * streaming pair a mesh nobody can name is written through, a page that
 * stands on the device worn in the format it was painted in and with a
 * chain, and the letting go that keeps a scene from holding everything
 * it ever cooked.
 */

#include <Graphics/GraphicsEngine/interface/Buffer.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <sigilgpu/GpuDevice.h>
#include <sigilmedia/advanced/Device.h>
#include <sigilmedia/core/Frame.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "Meshes.h"
#include "OnDevice.h"
#include "Pipelines.h"
#include "Textures.h"

using namespace sigil::geometry;

namespace {

namespace gpu = ::sigil::gpu;

/** A SOURCE WHOSE PIXELS STAND ON THE DEVICE AND NOWHERE ELSE: its frame
 *  names the texture and carries no image, so a map sampled from it is
 *  the texture wrapped or nothing. */
struct StandingSource {
  const void* device = nullptr;
  gpu::NativeTexture image;
  /** How many times the page has been painted, as its painter counts;
   *  zero for a painter that does not count. */
  uint64_t revision = 0;
  sigil::media::Frame frameAt(std::chrono::duration<double>) const {
    sigil::media::Frame frame;
    frame.device.kind = sigil::media::DeviceFrame::Kind::Texture;
    frame.device.device = device;
    frame.device.handle = image.vkImage;
    frame.device.format = image.vkFormat;
    frame.device.layout = image.vkLayout;
    frame.device.width = image.width;
    frame.device.height = image.height;
    frame.device.mipLevels = image.mipLevels;
    frame.device.revision = revision;
    return frame;
  }
  bool isRunning() const { return false; }
  bool operator==(const StandingSource& other) const {
    return device == other.device && image.vkImage == other.image.vkImage &&
           revision == other.revision;
  }
};

/** A page of @p side in @p format, with @p levels levels, painted one
 *  colour at level zero on @p on's device, or an empty handle where the
 *  device carries no 2D side. */
gpu::TextureHandle paintedPage(device::Device& on, gpu::TextureFormat format,
                               int side, int levels = 1) {
  gpu::GpuDevice* gpu = on.gpu();
  if (!gpu || !on.graphite()) return {};
  gpu::TextureDescription description;
  description.width = description.height = side;
  description.format = format;
  description.mipLevels = levels;
  const gpu::TextureHandle page = gpu->createTexture(description);
  if (!page) return {};
  const device::Device::QueueLock lock(on);
  sigil::skia::OffscreenSurface surface(*on.graphite(), *gpu, page);
  if (!surface.canvas()) return page;
  surface.canvas()->clear(SkColor4f{0.25f, 0.5f, 0.75f, 1.0f});
  surface.submit();
  return page;
}

/** Two triangles, enough to have vertices and indices and nothing more. */
mesh::Mesh quad() {
  mesh::Mesh out;
  out.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
  out.indices = {0, 1, 2, 0, 2, 3};
  return out;
}

mesh::Mesh triangle() {
  mesh::Mesh out;
  out.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}};
  out.indices = {0, 1, 2};
  return out;
}

}  // namespace

/** A PROGRAM THE DEVICE CANNOT BUILD IS REFUSED, never an abort. The
 *  build runs on the device's workers, through the driver's own compiler;
 *  a program with no entry point is one no driver can turn into a
 *  pipeline. The build ends refused, the cache answers no pipeline for it
 *  — which is what a frame stands a body in for — and remembers the
 *  refusal, so the program is not attempted again on every draw. */
TEST(PipelineCache, AProgramThatCannotBeBuiltIsRefusedAndRemembered) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::PipelineCache cache(*on);
  // A module header and nothing after it: well formed enough to reach the
  // driver's compiler, with nothing in it to compile.
  sigil::material::slang::Compiled broken;
  broken.vertex = {0x07230203u, 0x00010000u, 0u, 1u, 0u};
  broken.fragment = broken.vertex;
  device::PipelineKey key;
  key.program = &broken;
  key.fullscreen = true;

  cache.prepare(key);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (cache.building() > 0 && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(cache.ready(key), nullptr);
  EXPECT_EQ(cache.pipeline(key), nullptr)
      << "a refused build answered a pipeline";
  EXPECT_EQ(cache.building(), 0u);
  EXPECT_EQ(cache.pipeline(key), nullptr) << "the refusal was not remembered";
}

TEST(MapUpload, AMapTooSmallToHalveAsksForNoChainAtAll) {
  // A map wider than one texel wears the whole chain, because a surface
  // smaller on screen than its map is in texels aliases without one: the
  // count is how many times the wider side can be halved before it
  // arrives at one, and the last one is the whole map.
  EXPECT_EQ(device::mapMipLevels(8, 8), 4);
  EXPECT_EQ(device::mapMipLevels(16, 4), 5);
  // …and a ONE-TEXEL map — how a constant slot such as an emissive tint
  // is spelled — has one level and asks for nothing under it, since
  // halving a single texel arrives nowhere and a device told to fill the
  // levels below a view with one level in it refuses.
  EXPECT_EQ(device::mapMipLevels(1, 1), 1);
  EXPECT_EQ(device::mapMipLevels(2, 1), 2);
}

TEST(MeshResidency, LayoutHasOneFormPerInstanceAndTangentChoice) {
  // Four vertex elements always — position, normal, uv and colour — the
  // six instance elements numbered right after them, and the tangent
  // after those; nothing between the colour and the first instance.
  EXPECT_EQ(device::meshLayout(false, false).size(), 4u);
  EXPECT_EQ(device::meshLayout(true, false).size(), 10u);
  EXPECT_EQ(device::meshLayout(false, true).size(), 5u);
  EXPECT_EQ(device::meshLayout(true, true).size(), 11u);
  EXPECT_EQ(device::kFirstInstanceInput, 4u);
  EXPECT_EQ(device::kTangentInput, device::kFirstInstanceInput + 6);
  for (const bool instanced : {false, true})
    for (const bool tangent : {false, true}) {
      std::vector<unsigned> expectedInputs = {0, 1, 2, 3};
      if (instanced)
        for (unsigned input = 0; input < 6; ++input)
          expectedInputs.push_back(device::kFirstInstanceInput + input);
      if (tangent) expectedInputs.push_back(device::kTangentInput);
      std::vector<unsigned> inputs;
      for (const Diligent::LayoutElement& element :
           device::meshLayout(instanced, tangent)) {
        inputs.push_back(element.InputIndex);
        EXPECT_EQ(element.Stride, element.BufferSlot == 0
                                      ? sizeof(device::MeshVertex)
                                      : sizeof(device::InstanceVertex));
      }
      EXPECT_EQ(inputs, expectedInputs);
    }
  // A vertex is its position, normal, uv, colour and tangent, and no
  // lane besides.
  EXPECT_EQ(sizeof(device::MeshVertex), sizeof(float) * (3 + 3 + 2 + 4 + 4));
}

TEST(MeshResidency, UploadsAMeshOnceUnderTheNameItsArtefactWasGiven) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::MeshResidency resident(*on);

  const device::MeshBuffers* first = resident.upload(7, quad());
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->vertexCount, 4u);
  EXPECT_EQ(first->indexCount, 6u);

  // The SAME buffers on the second ask: a number naming an artefact is
  // what says two frames are looking at the same triangles, so nothing
  // crosses to the device a second time.
  Diligent::IBuffer* vertices = first->vertices;
  const device::MeshBuffers* again = resident.upload(7, quad());
  ASSERT_NE(again, nullptr);
  EXPECT_EQ(again->vertices.RawPtr(), vertices);
}

TEST(MeshResidency, UploadsAgainWhenAnAskNamesADifferentPrimitiveLane) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::MeshResidency resident(*on);

  mesh::Mesh tinted = quad();
  tinted.primitive("Color") = {{1, 0, 0, 1}, {0, 1, 0, 1}};

  // WITHOUT A LANE the mesh's own indices stand and its four vertices
  // are shared. WITH ONE they cannot be: the value belongs to a
  // triangle, and a vertex two triangles meet at cannot hold two of
  // them, so each triangle carries its own three.
  const device::MeshBuffers* shared = resident.upload(11, tinted);
  ASSERT_NE(shared, nullptr);
  EXPECT_EQ(shared->vertexCount, 4u);

  const device::MeshBuffers* unwelded = resident.upload(11, tinted, "Color");
  ASSERT_NE(unwelded, nullptr);
  EXPECT_EQ(unwelded->vertexCount, 6u);

  // …and back, so the lane is part of the key in both directions rather
  // than a one-way repack.
  const device::MeshBuffers* again = resident.upload(11, tinted);
  ASSERT_NE(again, nullptr);
  EXPECT_EQ(again->vertexCount, 4u);
}

TEST(MeshResidency, HoldsBothPackingsOfAMeshDrawnTintedAndUntinted) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::MeshResidency resident(*on);

  mesh::Mesh tinted = quad();
  tinted.primitive("Color") = {{1, 0, 0, 1}, {0, 1, 0, 1}};

  // ONE FRAME draws the mesh both ways: through its colour lane and
  // without it. Each is packed once…
  const device::MeshBuffers* untintedFirst = resident.upload(13, tinted);
  const device::MeshBuffers* tintedFirst = resident.upload(13, tinted, "Color");
  ASSERT_NE(untintedFirst, nullptr);
  ASSERT_NE(tintedFirst, nullptr);
  Diligent::IBuffer* untintedVertices = untintedFirst->vertices;
  Diligent::IBuffer* tintedVertices = tintedFirst->vertices;
  EXPECT_NE(untintedVertices, tintedVertices);
  resident.endFrame();

  // …and the SECOND FRAME, drawing the same two, is answered by the
  // buffers already held for each rather than a repack of either.
  const device::MeshBuffers* untintedSecond = resident.upload(13, tinted);
  const device::MeshBuffers* tintedSecond =
      resident.upload(13, tinted, "Color");
  ASSERT_NE(untintedSecond, nullptr);
  ASSERT_NE(tintedSecond, nullptr);
  EXPECT_EQ(untintedSecond->vertices.RawPtr(), untintedVertices);
  EXPECT_EQ(untintedSecond->vertexCount, 4u);
  EXPECT_EQ(tintedSecond->vertices.RawPtr(), tintedVertices);
  EXPECT_EQ(tintedSecond->vertexCount, 6u);
}

TEST(MeshResidency, LetsGoOfAMeshNoDrawHasNamedLately) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::MeshResidency resident(*on);
  ASSERT_NE(resident.upload(3, quad()), nullptr);

  // Frames pass and nothing asks for it again, so the buffers go; the
  // next ask uploads afresh rather than answering with what a scene
  // stopped drawing long ago.
  for (int i = 0; i < 8; ++i) resident.endFrame();
  const device::MeshBuffers* again = resident.upload(3, triangle());
  ASSERT_NE(again, nullptr);
  EXPECT_EQ(again->vertexCount, 3u);
  EXPECT_EQ(again->indexCount, 3u);
}

TEST(MeshResidency, StreamsAMeshNobodyCanName) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::MeshResidency resident(*on);

  const device::MeshBuffers* wide = resident.stream(quad());
  ASSERT_NE(wide, nullptr);
  EXPECT_EQ(wide->vertexCount, 4u);
  Diligent::IBuffer* vertices = wide->vertices;

  // GROWN, NEVER SHRUNK: a smaller mesh after a larger one writes into
  // the buffer that is already big enough, so a stream that has settled
  // allocates nothing and the counts are this draw's, not the buffers'.
  const device::MeshBuffers* narrow = resident.stream(triangle());
  ASSERT_NE(narrow, nullptr);
  EXPECT_EQ(narrow->vertexCount, 3u);
  EXPECT_EQ(narrow->indexCount, 3u);
  EXPECT_EQ(narrow->vertices, vertices);
}

TEST(MeshResidency, RefusesAMeshWithNoTriangles) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::MeshResidency resident(*on);
  EXPECT_EQ(resident.upload(1, mesh::Mesh{}), nullptr);
  EXPECT_EQ(resident.stream(mesh::Mesh{}), nullptr);
}

TEST(TextureResidency, ReadsNothingFromAMapThatYieldsNoImage) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  device::TextureResidency resident(*on);
  EXPECT_EQ(resident.sample(sigil::material::Texture{}), nullptr);
  EXPECT_EQ(resident.environment(sigil::material::EnvironmentMap{}), nullptr);
  EXPECT_EQ(resident.irradiance(sigil::material::EnvironmentMap{}), nullptr);
}

// A PAGE IS DESCRIBED AS IT WAS MADE: in the format it was painted in —
// a half-float page is half floats to the device, which keeps whatever
// it holds above one — and, worn without smaller copies, as the one
// level it was painted at.
TEST(TextureResidency, APageIsWornInTheFormatItWasPaintedIn) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  gpu::GpuDevice* gpu = on->gpu();
  if (!gpu) GTEST_SKIP() << "2D on the 3D device is unavailable";
  const struct {
    gpu::TextureFormat made;
    Diligent::TEXTURE_FORMAT described;
  } formats[] = {
      {gpu::TextureFormat::RGBA8Unorm, Diligent::TEX_FORMAT_RGBA8_UNORM},
      {gpu::TextureFormat::BGRA8Unorm, Diligent::TEX_FORMAT_BGRA8_UNORM},
      {gpu::TextureFormat::RGBA16Float, Diligent::TEX_FORMAT_RGBA16_FLOAT},
  };
  device::TextureResidency resident(*on);
  for (const auto& format : formats) {
    const gpu::TextureHandle page = paintedPage(*on, format.made, 32);
    ASSERT_TRUE(gpu->isValid(page));
    const sigil::material::Texture map(sigil::media::PixelSource(
        StandingSource{gpu, gpu->exportNative(page)}));
    Diligent::ITexture* worn = resident.sample(map, {.mipmaps = false});
    ASSERT_NE(worn, nullptr);
    EXPECT_EQ(worn->GetDesc().Format, format.described);
    EXPECT_EQ(worn->GetDesc().MipLevels, 1u);
    gpu->destroy(page);
  }
  on->context()->WaitForIdle();
}

// A PAGE MADE WITH A CHAIN IS WORN THROUGH THAT CHAIN, as deep as an
// uploaded map of its size carries and in the page's own format, its
// levels filled in place: one draw after another in the same frame reads
// the page itself, and nothing else is kept for it. Worn without smaller
// copies, a draw reads its level zero alone.
TEST(TextureResidency, APageMadeWithAChainIsWornThroughItsOwnChain) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  gpu::GpuDevice* gpu = on->gpu();
  if (!gpu) GTEST_SKIP() << "2D on the 3D device is unavailable";
  const gpu::TextureHandle page = paintedPage(
      *on, gpu::TextureFormat::RGBA16Float, 64, device::mapMipLevels(64, 64));
  ASSERT_TRUE(gpu->isValid(page));
  const gpu::NativeTexture image = gpu->exportNative(page);
  ASSERT_EQ(image.mipLevels, device::mapMipLevels(64, 64));
  const sigil::material::Texture map(
      sigil::media::PixelSource(StandingSource{gpu, image}));
  device::TextureResidency resident(*on);
  resident.prepare(map);
  Diligent::ITexture* worn = resident.sample(map);
  ASSERT_NE(worn, nullptr);
  EXPECT_EQ(worn->GetDesc().MipLevels,
            (Diligent::Uint32)device::mapMipLevels(64, 64));
  EXPECT_EQ(worn->GetDesc().Format, Diligent::TEX_FORMAT_RGBA16_FLOAT);
  EXPECT_EQ(resident.sample(map), worn);
  // THE PAGE ITSELF: the very image the page was painted into.
  EXPECT_EQ(worn->GetNativeHandle(), image.vkImage);
  // …and worn without them, a draw reads level zero alone.
  Diligent::ITexture* flat = resident.sample(map, {.mipmaps = false});
  ASSERT_NE(flat, nullptr);
  EXPECT_NE(flat, worn);
  EXPECT_EQ(flat->GetDesc().MipLevels, 1u);
  resident.endFrame();
  on->context()->Flush();
  on->context()->WaitForIdle();
  gpu->destroy(page);
}

// A PAGE NOT PAINTED AGAIN COSTS NOTHING TO WEAR AGAIN: the frame that
// first wears a painting of it has the device's queue wait for the
// painting and fills its levels, and a later frame wearing that same
// painting does neither. A new painting is waited for and filled once;
// a page whose painter does not count is waited for and filled in every
// frame that wears it, since nothing says it was not painted again.
TEST(TextureResidency, APageNotPaintedAgainIsNeitherWaitedForNorFilledAgain) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  gpu::GpuDevice* gpu = on->gpu();
  if (!gpu) GTEST_SKIP() << "2D on the 3D device is unavailable";
  const gpu::TextureHandle page = paintedPage(
      *on, gpu::TextureFormat::RGBA8Unorm, 64, device::mapMipLevels(64, 64));
  ASSERT_TRUE(gpu->isValid(page));
  const gpu::NativeTexture image = gpu->exportNative(page);
  device::TextureResidency resident(*on);
  const auto wearFrame = [&](uint64_t revision) {
    const sigil::material::Texture map(
        sigil::media::PixelSource(StandingSource{gpu, image, revision}));
    resident.prepare(map);
    EXPECT_NE(resident.sample(map), nullptr);
    EXPECT_NE(resident.sample(map), nullptr);
    resident.endFrame();
    return resident.pageWork();
  };
  device::PageWork work = wearFrame(1);
  EXPECT_EQ(work.waits, 1u);
  EXPECT_EQ(work.fills, 1u);
  for (int frame = 0; frame < 3; ++frame) {
    work = wearFrame(1);
    EXPECT_EQ(work.waits, 1u) << "a page not painted again was waited for";
    EXPECT_EQ(work.fills, 1u) << "a page not painted again was filled again";
  }
  work = wearFrame(2);
  EXPECT_EQ(work.waits, 2u);
  EXPECT_EQ(work.fills, 2u);
  work = wearFrame(0);
  work = wearFrame(0);
  EXPECT_EQ(work.waits, 4u);
  EXPECT_EQ(work.fills, 4u);
  on->context()->Flush();
  on->context()->WaitForIdle();
  gpu->destroy(page);
}

// A PAGE MADE WITH NO CHAIN IS WORN AS IT STANDS, whatever the draw asks
// for: there is no level below zero to fill, and no copy is made to hold
// one.
TEST(TextureResidency, APageMadeWithNoChainIsWornAsItStands) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  gpu::GpuDevice* gpu = on->gpu();
  if (!gpu) GTEST_SKIP() << "2D on the 3D device is unavailable";
  const gpu::TextureHandle page =
      paintedPage(*on, gpu::TextureFormat::RGBA8Unorm, 64);
  ASSERT_TRUE(gpu->isValid(page));
  const sigil::material::Texture map(
      sigil::media::PixelSource(StandingSource{gpu, gpu->exportNative(page)}));
  device::TextureResidency resident(*on);
  Diligent::ITexture* worn = resident.sample(map);
  ASSERT_NE(worn, nullptr);
  EXPECT_EQ(worn->GetDesc().MipLevels, 1u);
  EXPECT_EQ(resident.sample(map, {.mipmaps = false}), worn);
  resident.endFrame();
  on->context()->Flush();
  on->context()->WaitForIdle();
  gpu->destroy(page);
}

// A FRAME IN A FORMAT NO TEXTURE ON THE DEVICE IS MADE IN is not claimed
// to be one: nothing is wrapped, and with no image beside it nothing is
// sampled.
TEST(TextureResidency, AFrameInAFormatNoPageHasIsNotWrapped) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  gpu::GpuDevice* gpu = on->gpu();
  if (!gpu) GTEST_SKIP() << "2D on the 3D device is unavailable";
  const gpu::TextureHandle page =
      paintedPage(*on, gpu::TextureFormat::RGBA8Unorm, 16);
  ASSERT_TRUE(gpu->isValid(page));
  gpu::NativeTexture image = gpu->exportNative(page);
  image.vkFormat = 9; /* R8_UNORM */
  const sigil::material::Texture map(
      sigil::media::PixelSource(StandingSource{gpu, image}));
  device::TextureResidency resident(*on);
  EXPECT_EQ(resident.sample(map), nullptr);
  gpu->destroy(page);
}
