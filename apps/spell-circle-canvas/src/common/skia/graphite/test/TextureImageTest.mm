/** @file
 * A Metal texture wrapped as a Graphite image: a texture painted through a
 * surface read back as an image without a copy, no image without a
 * texture, a refused wrap that leaves the retain count where it was, and
 * planes wrapped as one image that hand their release over exactly once.
 */

#import <Metal/Metal.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <include/core/SkYUVAInfo.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/TextureImage.h>

#include <array>

#include <gtest/gtest.h>

#include "GraphiteReadback.h"
#include "MetalFixtures.h"

using sigil::skia::GraphiteContext;
using sigil::skia::OffscreenSurface;
using namespace sigil::skia::test;

namespace {

/** An 8x8 BGRA texture no shader may sample: a render target and nothing
 *  else, which is what a wrap refuses after it has taken the release on. */
MTLTextureDescriptor *sampleFreeDescriptor() {
  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:8
                                                        height:8
                                                     mipmapped:NO];
  desc.usage = MTLTextureUsageRenderTarget;
  desc.storageMode = MTLStorageModePrivate;
  return desc;
}

/** The chroma plane beside it: half the size, two channels, and equally
 *  unsampleable. */
MTLTextureDescriptor *sampleFreeChromaDescriptor() {
  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG8Unorm
                                                         width:4
                                                        height:4
                                                     mipmapped:NO];
  desc.usage = MTLTextureUsageRenderTarget;
  desc.storageMode = MTLStorageModePrivate;
  return desc;
}

}  // namespace

// THE OTHER DIRECTION of the same texture: painted through the surface
// wrap, then read back as an image and drawn onto a second surface. The
// pixels arrive without a copy, and the image outlives every reference
// the test still holds to the texture, because the wrap retains it.
TEST(SigilSkiaGraphite, WrapsATexturePaintedOnItAsAnImage) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const int size = 8;
  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:size
                                                        height:size
                                                     mipmapped:NO];
  desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  desc.storageMode = MTLStorageModeShared;
  id<MTLTexture> texture = [device() newTextureWithDescriptor:desc];
  ASSERT_NE(texture, nil);
  {
    OffscreenSurface painted(*ctx, (__bridge void *)texture, size, size);
    ASSERT_NE(painted.canvas(), nullptr);
    painted.canvas()->clear(SkColorSetARGB(255, 0, 0, 255));
    painted.submit();
  }

  sk_sp<SkImage> image =
      sigil::skia::wrapImage(*ctx->recorder(), (__bridge void *)texture, size, size);
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), size);
  EXPECT_EQ(image->height(), size);

  const SkImageInfo info = SkImageInfo::MakeN32Premul(size, size);
  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  target->getCanvas()->drawImage(image, 0, 0);
  const SkBitmap pixels = readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(0, 0), SkColorSetARGB(255, 0, 0, 255));
  EXPECT_EQ(pixels.getColor(size - 1, size - 1), SkColorSetARGB(255, 0, 0, 255));
}

TEST(SigilSkiaGraphite, WrapsNoImageWithoutATexture) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  EXPECT_EQ(sigil::skia::wrapImage(*ctx->recorder(), nullptr, 8, 8), nullptr);
  EXPECT_EQ(sigil::skia::wrapImage(*ctx->recorder(), (__bridge void *)device(), 0, 8), nullptr);
}

// A REFUSED WRAP OWES THE TEXTURE NOTHING: the wrap binds its release to
// the retained texture before it validates anything, and runs it itself
// on the way out, so a texture a refusal touched is left exactly as
// retained as it was handed over. The guard retain keeps a second release
// from freeing the texture under the case.
TEST(SigilSkiaGraphite, ARefusedWrapLeavesTheRetainCountWhereItWas) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  id<MTLTexture> texture = [device() newTextureWithDescriptor:sampleFreeDescriptor()];
  ASSERT_NE(texture, nil);
  CFTypeRef guard = CFRetain((__bridge CFTypeRef)texture);
  const CFIndex before = CFGetRetainCount(guard);

  // Not sampleable, so the wrap refuses after it has taken the release on.
  EXPECT_EQ(sigil::skia::wrapImage(*ctx->recorder(), (__bridge void *)texture, 8, 8), nullptr);

  EXPECT_EQ(CFGetRetainCount(guard), before);
  CFRelease(guard);
}

// THE PLANAR WRAP HANDS ITS PLANES OVER EXACTLY ONCE, whichever way it
// ends: a refusal it makes itself, a refusal the wrap makes, and a wrap
// that succeeds and holds them until the image is gone.
TEST(SigilSkiaGraphite, PlanarWrapReleasesThePlanesOnce) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const auto count = [](void *context) { ++*static_cast<int *>(context); };

  int released = 0;
  const SkYUVAInfo info({8, 8}, SkYUVAInfo::PlaneConfig::kY_UV, SkYUVAInfo::Subsampling::k420,
                        kRec709_Limited_SkYUVColorSpace);
  EXPECT_EQ(sigil::skia::wrapPlanarImage(*ctx->recorder(), {}, info, nullptr, count, &released),
            nullptr);
  EXPECT_EQ(released, 1);

  int missing = 0;
  const std::array<sigil::skia::TexturePlane, 2> noTexture{
      sigil::skia::TexturePlane{nullptr, 8, 8}, sigil::skia::TexturePlane{nullptr, 4, 4}};
  EXPECT_EQ(
      sigil::skia::wrapPlanarImage(*ctx->recorder(), noTexture, info, nullptr, count, &missing),
      nullptr);
  EXPECT_EQ(missing, 1);

  // Planes of the right shape that no shader may sample: the refusal is
  // the wrap's own, and it runs the release on its way out.
  int unsampleable = 0;
  id<MTLTexture> luma = [device() newTextureWithDescriptor:sampleFreeDescriptor()];
  id<MTLTexture> chroma = [device() newTextureWithDescriptor:sampleFreeChromaDescriptor()];
  ASSERT_NE(luma, nil);
  ASSERT_NE(chroma, nil);
  const std::array<sigil::skia::TexturePlane, 2> planes{
      sigil::skia::TexturePlane{(__bridge void *)luma, 8, 8},
      sigil::skia::TexturePlane{(__bridge void *)chroma, 4, 4}};
  EXPECT_EQ(
      sigil::skia::wrapPlanarImage(*ctx->recorder(), planes, info, nullptr, count, &unsampleable),
      nullptr);
  EXPECT_EQ(unsampleable, 1);
}

TEST(SigilSkiaGraphite, WrapsPlanesAsOneImage) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const auto count = [](void *context) { ++*static_cast<int *>(context); };

  MTLTextureDescriptor *lumaDesc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                         width:8
                                                        height:8
                                                     mipmapped:NO];
  lumaDesc.usage = MTLTextureUsageShaderRead;
  lumaDesc.storageMode = MTLStorageModeShared;
  MTLTextureDescriptor *chromaDesc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG8Unorm
                                                         width:4
                                                        height:4
                                                     mipmapped:NO];
  chromaDesc.usage = MTLTextureUsageShaderRead;
  chromaDesc.storageMode = MTLStorageModeShared;
  id<MTLTexture> luma = [device() newTextureWithDescriptor:lumaDesc];
  id<MTLTexture> chroma = [device() newTextureWithDescriptor:chromaDesc];
  ASSERT_NE(luma, nil);
  ASSERT_NE(chroma, nil);

  const SkYUVAInfo info({8, 8}, SkYUVAInfo::PlaneConfig::kY_UV, SkYUVAInfo::Subsampling::k420,
                        kRec709_Limited_SkYUVColorSpace);
  const std::array<sigil::skia::TexturePlane, 2> planes{
      sigil::skia::TexturePlane{(__bridge void *)luma, 8, 8},
      sigil::skia::TexturePlane{(__bridge void *)chroma, 4, 4}};
  int released = 0;
  sk_sp<SkImage> image =
      sigil::skia::wrapPlanarImage(*ctx->recorder(), planes, info, nullptr, count, &released);
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->width(), 8);
  EXPECT_EQ(image->height(), 8);
  // The planes are the image's for as long as it lives.
  EXPECT_EQ(released, 0);
}
