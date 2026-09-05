#pragma once

/** @file
 * A texture someone else owns, read as an SkImage a draw can SAMPLE —
 * the other direction from `OffscreenSurface`, which is the same texture
 * as a surface a draw can WRITE.
 *
 * Nothing is copied either way. What crosses is a name: the API's own
 * texture object, wrapped where it stands.
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
/**
 * @p mtlTexture — an id<MTLTexture> bridged to void*, created on the
 * device @p recorder was made from — as an image drawn through that
 * recorder, with no copy.
 *
 * THE IMAGE HOLDS THE TEXTURE for as long as it lives, so a texture its
 * owner resizes away or drops under a draw stays valid until the last
 * image naming it is gone. The texels are read as the format the texture
 * itself declares; @p alphaType says what its alpha channel means and
 * @p colorSpace what its colours do, with null asking for no conversion.
 *
 * Null when there is no texture, no recorder, or the wrap failed. The
 * recorder is the one whose thread the draw is recorded on — the
 * context's own, or the one that thread took for itself.
 */
sk_sp<SkImage> wrapImage(skgpu::graphite::Recorder& recorder, void* mtlTexture,
                         int width, int height,
                         SkAlphaType alphaType = kPremul_SkAlphaType,
                         sk_sp<SkColorSpace> colorSpace = nullptr);

/**
 * ONE IMAGE OUT OF SEVERAL PLANES that each stand on the device — a
 * frame decoded straight into luma and chroma textures, most often —
 * wrapped as @p info describes them, with no copy and no conversion
 * pass: the shader that samples the image does the arithmetic @p info
 * names.
 *
 * @p planes are in the order @p info declares, and there are as many of
 * them as it says. NOTHING IS RETAINED HERE: the planes live as long as
 * @p releaseContext keeps them, and @p release is called with it once
 * the image is gone — which is the shape a decoder wants, holding the
 * one buffer every plane was made from rather than each plane in turn.
 *
 * Null when a plane is missing, the description does not fit the planes,
 * or the wrap failed; the release runs either way, so a caller hands its
 * context over exactly once.
 */
sk_sp<SkImage> wrapImage(skgpu::graphite::Recorder& recorder,
                         std::span<const TexturePlane> planes,
                         const SkYUVAInfo& info, sk_sp<SkColorSpace> colorSpace,
                         TextureRelease release, void* releaseContext);
#endif

}  // namespace sigil::skia
