// Metal arm of the image wrap: an id<MTLTexture> as an SkImage on a
// Metal recorder, so a draw samples the texture where it stands.

#import <Metal/Metal.h>

#include <include/core/SkColorSpace.h>
#include <include/core/SkImage.h>
#include <include/core/SkSize.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Image.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/YUVABackendTextures.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>
#include <sigilskia/graphite/TextureImage.h>

#include <array>
#include <utility>

namespace sigil::skia {

sk_sp<SkImage> wrapImage(skgpu::graphite::Recorder &recorder, void *mtlTexture, int width,
                         int height, SkAlphaType alphaType, sk_sp<SkColorSpace> colorSpace) {
  if (!mtlTexture || width <= 0 || height <= 0) return nullptr;

  // RETAINED FOR THE IMAGE'S LIFE, and released by the wrap's own release
  // proc: an image is sampled at draw time and submitted later, and the
  // view or the frame that owned the texture may have resized or gone by
  // then. Without the retain the draw would reach whatever now holds the
  // slot.
  CFTypeRef retained = CFRetain(static_cast<CFTypeRef>(mtlTexture));
  const skgpu::graphite::BackendTexture backendTexture =
      skgpu::graphite::BackendTextures::MakeMetal(SkISize::Make(width, height), retained);
  sk_sp<SkImage> image = SkImages::WrapTexture(
      &recorder, backendTexture, alphaType, std::move(colorSpace),
      [](void *context) { CFRelease(static_cast<CFTypeRef>(context)); },
      const_cast<void *>(static_cast<const void *>(retained)));
  // A wrap that failed never took the release proc on, so the retain is
  // this function's to undo.
  if (!image) CFRelease(retained);
  return image;
}

sk_sp<SkImage> wrapImage(skgpu::graphite::Recorder &recorder,
                         std::span<const TexturePlane> planes, const SkYUVAInfo &info,
                         sk_sp<SkColorSpace> colorSpace, TextureRelease release,
                         void *releaseContext) {
  // THE RELEASE RUNS ON EVERY PATH OUT: the caller handed its planes over
  // once, and a wrap that never happened must not leave them held.
  const auto refuse = [&]() -> sk_sp<SkImage> {
    if (release) release(releaseContext);
    return nullptr;
  };
  if (planes.empty() || planes.size() > SkYUVAInfo::kMaxPlanes) return refuse();

  std::array<skgpu::graphite::BackendTexture, SkYUVAInfo::kMaxPlanes> textures;
  for (size_t i = 0; i < planes.size(); ++i) {
    const TexturePlane &plane = planes[i];
    if (!plane.texture || plane.width <= 0 || plane.height <= 0) return refuse();
    textures[i] = skgpu::graphite::BackendTextures::MakeMetal(
        SkISize::Make(plane.width, plane.height), static_cast<CFTypeRef>(plane.texture));
  }

  const skgpu::graphite::YUVABackendTextures yuva(
      info, SkSpan<const skgpu::graphite::BackendTexture>(textures.data(), planes.size()));
  if (!yuva.isValid()) return refuse();

  sk_sp<SkImage> image = SkImages::TextureFromYUVATextures(
      &recorder, yuva, std::move(colorSpace), release, releaseContext);
  if (!image) return refuse();
  return image;
}

}  // namespace sigil::skia
