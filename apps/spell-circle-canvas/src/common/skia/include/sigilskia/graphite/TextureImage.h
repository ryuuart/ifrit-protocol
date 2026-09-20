#pragma once

/** @file
 * @ingroup skia-graphite
 * A texture someone else owns, read as an SkImage a draw can SAMPLE —
 * the other direction from `OffscreenSurface`, which is the same texture
 * as a surface a draw can WRITE. A WRAP copies nothing and belongs to
 * the recorder that made it; a READ copies the pixels into host memory,
 * which is all that is left when the drawing that must sample them
 * stands on another device, or on another graphics API.
 */

// A colour space is held by value in a defaulted argument, so it is
// spelled whole here; an image is only ever named.
#include <include/core/SkAlphaType.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkYUVAInfo.h>

#include <span>

class SkImage;

namespace skgpu::graphite {
class Recorder;
}  // namespace skgpu::graphite

namespace sigil::skia {

/** ONE PLANE of a wrapped image: the API's own texture object and the
 *  size it is read at. A plane's size is its own — a chroma plane of a
 *  subsampled frame is half the luma's. */
struct TexturePlane {
  /** id<MTLTexture> bridged to void* on Apple. */
  void* texture = nullptr;
  int width = 0;
  int height = 0;
};

/** What frees the textures a planar wrap named, once the image is gone,
 *  and what it is handed. */
using TextureRelease = void (*)(void* context);

#ifdef __APPLE__
/** @p mtlTexture — an id<MTLTexture> bridged to void*, created on
 *  @p recorder's device — as an image drawn through that recorder, with
 *  NO COPY, HOLDING THE TEXTURE for as long as the image lives. Read as
 *  the format the texture declares; @p alphaType and @p colorSpace say
 *  what its alpha and its colours mean, null asking for no conversion.
 *  Null when there is no texture, no recorder, or the wrap failed. */
sk_sp<SkImage> wrapImage(skgpu::graphite::Recorder& recorder, void* mtlTexture,
                         int width, int height,
                         SkAlphaType alphaType = kPremul_SkAlphaType,
                         sk_sp<SkColorSpace> colorSpace = nullptr);

/** THE WHOLE OF @p mtlTexture, at the size the texture itself reports,
 *  for the caller handed a texture somebody else made. Everything else
 *  is the wrap above. */
sk_sp<SkImage> wrapImage(skgpu::graphite::Recorder& recorder, void* mtlTexture,
                         SkAlphaType alphaType = kPremul_SkAlphaType,
                         sk_sp<SkColorSpace> colorSpace = nullptr);

/** @p mtlTexture's pixels READ BACK INTO HOST MEMORY, as an image any
 *  canvas and any renderer can draw. Read as the format the texture
 *  declares, channels in that format's own order; null when there is no
 *  texture, when the copy did not complete, or when the format is not
 *  four eight-bit channels, the only shape read here.
 *  @trap THIS ONE COPIES where the wraps do not: the calling thread
 *  blocks on it, and the pixels upload again wherever they are drawn. */
sk_sp<SkImage> readImage(void* mtlTexture,
                         SkAlphaType alphaType = kPremul_SkAlphaType,
                         sk_sp<SkColorSpace> colorSpace = nullptr);

/** ONE IMAGE OUT OF SEVERAL PLANES that each stand on the device, most
 *  often a frame decoded straight into luma and chroma textures, wrapped
 *  as @p info describes them with no copy and no conversion pass.
 *  @p planes are in the order @p info declares. NOTHING IS RETAINED:
 *  @p release is called with @p releaseContext once the image is gone,
 *  and runs even when the answer is null, so a caller hands its context
 *  over exactly once. */
sk_sp<SkImage> wrapPlanarImage(skgpu::graphite::Recorder& recorder,
                               std::span<const TexturePlane> planes,
                               const SkYUVAInfo& info,
                               sk_sp<SkColorSpace> colorSpace,
                               TextureRelease release, void* releaseContext);
#endif

}  // namespace sigil::skia
