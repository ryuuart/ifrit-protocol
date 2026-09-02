/** @file
 * The Metal driver's texture interop beyond Ultralight's own commands:
 * publish and slot textures imported into the device as handles,
 * external textures registered under Ultralight ids, blits and uploads,
 * painting through the web thread's Graphite recorder, and wrapping a
 * texture as an SkImage that keeps it alive.
 */

#import <Metal/Metal.h>

#include "MetalDriver.h"
#include "MetalDriverState.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Image.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>

#include <cstdio>
#include <mutex>

namespace sigil::scry {

namespace {

void copyMetalTextures(id<MTLCommandQueue> queue, id<MTLTexture> src, id<MTLTexture> dst, int width,
                       int height) {
  NSUInteger copyWidth = std::min<NSUInteger>(width, src.width);
  copyWidth = std::min(copyWidth, dst.width);
  NSUInteger copyHeight = std::min<NSUInteger>(height, src.height);
  copyHeight = std::min(copyHeight, dst.height);

  @autoreleasepool {
    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
    [blit copyFromTexture:src
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(copyWidth, copyHeight, 1)
                toTexture:dst
         destinationSlice:0
         destinationLevel:0
        destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];
    [commandBuffer commit];
  }
}

}  // namespace

sigil::core::hardware::TextureHandle MetalDriver::createPublishTexture(int width, int height) {
  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:width
                                                        height:height
                                                     mipmapped:NO];
  desc.usage = MTLTextureUsageShaderRead;
  desc.storageMode = MTLStorageModePrivate;
  return m_state->import([m_state->device newTextureWithDescriptor:desc], width, height);
}

void MetalDriver::releaseTexture(sigil::core::hardware::TextureHandle handle) {
  // The device forgets the borrowed handle at once; the driver's own +1
  // from import() goes with it. Command buffers in flight and wraps hold
  // their own references, so the texture lives as long as anything draws
  // it.
  const sigil::core::hardware::NativeTexture native = m_state->gpuDevice->exportNative(handle);
  if (!native) return;
  m_state->gpuDevice->destroy(handle);
  CFRelease(native.mtlTexture);
}

sigil::core::hardware::TextureHandle MetalDriver::createImageTexture(int width, int height) {
  MTLTextureDescriptor *desc =
      [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                         width:width
                                                        height:height
                                                     mipmapped:NO];
  desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  // Shared storage: render-target capable on Apple GPUs while still
  // accepting replaceRegion uploads from the CPU.
  desc.storageMode = MTLStorageModeShared;
  return m_state->import([m_state->device newTextureWithDescriptor:desc], width, height);
}

uint32_t MetalDriver::registerExternalTexture(sigil::core::hardware::TextureHandle handle) {
  uint32_t textureId = m_state->nextTextureId++;
  m_state->textures[textureId] = m_state->texture(handle);
  return textureId;
}

void MetalDriver::unregisterExternalTexture(uint32_t textureId) {
  m_state->textures.erase(textureId);
}

bool MetalDriver::paintTexture(sigil::core::hardware::TextureHandle handle, int width, int height,
                               const std::function<void(SkCanvas &)> &painter) {
  id<MTLTexture> mtlTexture = m_state->texture(handle);
  skgpu::graphite::Recorder *recorder = m_state->webRecorder.get();
  if (!mtlTexture || !recorder) return false;

  skgpu::graphite::BackendTexture backendTexture = skgpu::graphite::BackendTextures::MakeMetal(
      SkISize::Make(width, height), (__bridge CFTypeRef)mtlTexture);
  sk_sp<SkSurface> surface =
      SkSurfaces::WrapBackendTexture(recorder, backendTexture, SkColorSpace::MakeSRGB(), nullptr);
  if (!surface) return false;

  painter(*surface->getCanvas());
  surface.reset();

  // Snapped on this thread, inserted and submitted under the context's
  // lock: the host may be using the same context from its own thread.
  std::unique_ptr<skgpu::graphite::Recording> recording = recorder->snap();
  skgpu::graphite::Context *context = m_state->graphite->context();
  const std::unique_lock<std::mutex> lock = m_state->graphite->lockContext();
  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = recording.get();
  const skgpu::graphite::InsertStatus status =
      recording ? context->insertRecording(info) : skgpu::graphite::InsertStatus();
  if (!recording || !status) {
    // The status is checked, never assumed. This recorder replays in
    // order, so a Recording snapped and not inserted — a null snap
    // included — skips an ID and kills the recorder: every later insert
    // fails and nothing it records is ever drawn again. The failure is
    // otherwise invisible (the painted image simply never updates), so it
    // is reported rather than swallowed, once per process because the
    // cause is a property of the context and repeating it every paint
    // would only flood the log.
    static bool warned = false;
    if (!warned) {
      warned = true;
      std::fprintf(
          stderr,
          "[SigilScry:warning] Graphite paint failed (%s%d%s%s); WebImage "
          "frames will not render\n",
          recording ? "insert status " : "empty snap",
          recording ? static_cast<int>(static_cast<skgpu::graphite::InsertStatus::V>(status)) : 0,
          status.message().empty() ? "" : ": ", status.message().c_str());
    }
    return false;
  }
  context->submit();
  return true;
}

void MetalDriver::uploadToTexture(sigil::core::hardware::TextureHandle handle, const void *pixels, int width,
                                  int height, size_t rowBytes) {
  id<MTLTexture> texture = m_state->texture(handle);
  if (!texture) return;
  [texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
             mipmapLevel:0
               withBytes:pixels
             bytesPerRow:rowBytes];
}

bool MetalDriver::copyDeviceTexture(sigil::core::hardware::TextureHandle src, sigil::core::hardware::TextureHandle dst,
                                    int width, int height) {
  id<MTLTexture> srcTexture = m_state->texture(src);
  id<MTLTexture> dstTexture = m_state->texture(dst);
  if (!srcTexture || !dstTexture) return false;
  copyMetalTextures(m_state->queue, srcTexture, dstTexture, width, height);
  return true;
}

void MetalDriver::copyTexture(uint32_t srcTextureId, sigil::core::hardware::TextureHandle dst, int width,
                              int height) {
  auto it = m_state->textures.find(srcTextureId);
  id<MTLTexture> dstTexture = m_state->texture(dst);
  if (it == m_state->textures.end() || !dstTexture) return;
  copyMetalTextures(m_state->queue, it->second, dstTexture, width, height);
}

sk_sp<SkImage> MetalDriver::wrapTexture(skgpu::graphite::Recorder *recorder,
                                        sigil::core::hardware::TextureHandle handle, int width, int height) {
  id<MTLTexture> texture = m_state->texture(handle);
  if (!recorder || !texture) return nullptr;
  // The wrapped image retains the MTLTexture so it stays valid even if
  // the owning view/image resizes or is destroyed while the image lives.
  CFTypeRef retained = CFRetain((__bridge CFTypeRef)texture);
  skgpu::graphite::BackendTexture backendTexture =
      skgpu::graphite::BackendTextures::MakeMetal(SkISize::Make(width, height), retained);
  return SkImages::WrapTexture(
      recorder, backendTexture, kPremul_SkAlphaType, SkColorSpace::MakeSRGB(),
      [](void *context) { CFRelease(static_cast<CFTypeRef>(context)); },
      const_cast<void *>(static_cast<const void *>(retained)));
}

}  // namespace sigil::scry
