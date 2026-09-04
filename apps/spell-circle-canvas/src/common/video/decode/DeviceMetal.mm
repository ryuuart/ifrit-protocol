#import <CoreVideo/CoreVideo.h>
#import <Metal/Metal.h>

#include "Device.h"

#include <include/core/SkColorSpace.h>
#include <include/core/SkYUVAInfo.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Image.h>
#include <include/gpu/graphite/YUVABackendTextures.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

#include <array>
#include <mutex>

namespace sigil::video::device {
namespace {

CVPixelBufferRef pixelBuffer(const NativeFrame& frame) {
  if (frame.kind != NativeFrame::Kind::VideoToolboxPixelBuffer) return nullptr;
  return static_cast<CVPixelBufferRef>(frame.handle());
}

SkYUVColorSpace colorSpace(const NativeFrame& frame) {
  switch (frame.yuvMatrix) {
    case NativeFrame::YuvMatrix::Bt2020:
      return frame.fullRange ? kBT2020_8bit_Full_SkYUVColorSpace
                             : kBT2020_8bit_Limited_SkYUVColorSpace;
    case NativeFrame::YuvMatrix::Rec709:
      return frame.fullRange ? kRec709_Full_SkYUVColorSpace : kRec709_Limited_SkYUVColorSpace;
    case NativeFrame::YuvMatrix::Rec601:
      return frame.fullRange ? kJPEG_Full_SkYUVColorSpace : kRec601_Limited_SkYUVColorSpace;
  }
}

struct WrappedPlanes {
  CVPixelBufferRef buffer = nullptr;
  CVMetalTextureRef y = nullptr;
  CVMetalTextureRef uv = nullptr;
};

class MetalContext final : public Context {
 public:
  explicit MetalContext(CVMetalTextureCacheRef cache) : m_cache(cache) {}
  ~MetalContext() override { CFRelease(m_cache); }

  CVMetalTextureCacheRef cache() const { return m_cache; }

 private:
  CVMetalTextureCacheRef m_cache;
};

void releasePlanes(void* context) {
  auto* planes = static_cast<WrappedPlanes*>(context);
  if (planes->uv) CFRelease(planes->uv);
  if (planes->y) CFRelease(planes->y);
  if (planes->buffer) CVPixelBufferRelease(planes->buffer);
  delete planes;
}

}  // namespace

std::shared_ptr<Context> makeContext(void* metalDevice) {
  id<MTLDevice> device =
      metalDevice ? (__bridge id<MTLDevice>)metalDevice : MTLCreateSystemDefaultDevice();
  if (!device) return nullptr;
  static std::mutex mutex;
  static std::weak_ptr<Context> shared;
  static void* sharedDevice = nullptr;
  std::lock_guard lock(mutex);
  if (sharedDevice == (__bridge void*)device)
    if (std::shared_ptr<Context> existing = shared.lock()) return existing;
  CVMetalTextureCacheRef cache = nullptr;
  if (CVMetalTextureCacheCreate(kCFAllocatorDefault, nullptr, device, nullptr, &cache) !=
      kCVReturnSuccess)
    return nullptr;
  std::shared_ptr<Context> result = std::make_shared<MetalContext>(cache);
  sharedDevice = (__bridge void*)device;
  shared = result;
  return result;
}

NativeFrame retainNativeFrame(const AVFrame* frame) {
  if (!frame || frame->format != AV_PIX_FMT_VIDEOTOOLBOX) return {};
  CVPixelBufferRef buffer = reinterpret_cast<CVPixelBufferRef>(frame->data[3]);
  if (!buffer) return {};
  CVPixelBufferRetain(buffer);
  NativeFrame::YuvMatrix matrix = NativeFrame::YuvMatrix::Rec601;
  if (frame->colorspace == AVCOL_SPC_BT709)
    matrix = NativeFrame::YuvMatrix::Rec709;
  else if (frame->colorspace == AVCOL_SPC_BT2020_NCL || frame->colorspace == AVCOL_SPC_BT2020_CL)
    matrix = NativeFrame::YuvMatrix::Bt2020;
  const OSType format = CVPixelBufferGetPixelFormatType(buffer);
  return {
      .kind = NativeFrame::Kind::VideoToolboxPixelBuffer,
      .storage = std::shared_ptr<void>(
          buffer, [](void* value) { CVPixelBufferRelease(static_cast<CVPixelBufferRef>(value)); }),
      .width = frame->width,
      .height = frame->height,
      .yuvMatrix = matrix,
      .fullRange = format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
  };
}

sk_sp<SkImage> wrapNativeFrame(const NativeFrame& frame, skgpu::graphite::Recorder* recorder,
                               Context& context) {
  CVPixelBufferRef buffer = pixelBuffer(frame);
  if (!buffer || !recorder || CVPixelBufferGetPlaneCount(buffer) != 2) return nullptr;

  const OSType format = CVPixelBufferGetPixelFormatType(buffer);
  const bool fullRange = format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
  if (!fullRange && format != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) return nullptr;

  auto* metal = dynamic_cast<MetalContext*>(&context);
  if (!metal) return nullptr;

  auto* planes = new WrappedPlanes;
  planes->buffer = CVPixelBufferRetain(buffer);
  const size_t yWidth = CVPixelBufferGetWidthOfPlane(buffer, 0);
  const size_t yHeight = CVPixelBufferGetHeightOfPlane(buffer, 0);
  const size_t uvWidth = CVPixelBufferGetWidthOfPlane(buffer, 1);
  const size_t uvHeight = CVPixelBufferGetHeightOfPlane(buffer, 1);

  const CVReturn yStatus = CVMetalTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, metal->cache(), buffer, nullptr, MTLPixelFormatR8Unorm, yWidth, yHeight,
      0, &planes->y);
  const CVReturn uvStatus = CVMetalTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, metal->cache(), buffer, nullptr, MTLPixelFormatRG8Unorm, uvWidth,
      uvHeight, 1, &planes->uv);
  if (yStatus != kCVReturnSuccess || uvStatus != kCVReturnSuccess || !planes->y || !planes->uv) {
    releasePlanes(planes);
    return nullptr;
  }

  id<MTLTexture> yTexture = CVMetalTextureGetTexture(planes->y);
  id<MTLTexture> uvTexture = CVMetalTextureGetTexture(planes->uv);
  if (!yTexture || !uvTexture) {
    releasePlanes(planes);
    return nullptr;
  }

  std::array<skgpu::graphite::BackendTexture, 2> textures = {
      skgpu::graphite::BackendTextures::MakeMetal(
          SkISize::Make(static_cast<int>(yWidth), static_cast<int>(yHeight)),
          (__bridge CFTypeRef)yTexture),
      skgpu::graphite::BackendTextures::MakeMetal(
          SkISize::Make(static_cast<int>(uvWidth), static_cast<int>(uvHeight)),
          (__bridge CFTypeRef)uvTexture),
  };
  const SkYUVAInfo info(SkISize::Make(frame.width, frame.height), SkYUVAInfo::PlaneConfig::kY_UV,
                        SkYUVAInfo::Subsampling::k420, colorSpace(frame));
  const skgpu::graphite::YUVABackendTextures yuva(info, textures);
  if (!yuva.isValid()) {
    releasePlanes(planes);
    return nullptr;
  }

  sk_sp<SkImage> image = SkImages::TextureFromYUVATextures(
      recorder, yuva, SkColorSpace::MakeSRGB(), releasePlanes, planes, "SigilVideo frame");
  if (!image) releasePlanes(planes);
  return image;
}

}  // namespace sigil::video::device
