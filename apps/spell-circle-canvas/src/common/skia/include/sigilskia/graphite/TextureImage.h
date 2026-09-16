#pragma once

/** @file
 * A texture someone else owns, read as an SkImage a draw can SAMPLE —
 * the other direction from `OffscreenSurface`, which is the same texture
 * as a surface a draw can WRITE.
 *
 * TWO WAYS TO READ ONE, and which is possible is a fact about the
 * devices rather than a preference. A WRAP copies nothing: what crosses
 * is a name, the API's own texture object read where it stands, and the
 * image belongs to the recorder that wrapped it and to no other. A READ
 * copies the pixels into host memory, which is the only thing left when
 * the drawing that must sample them stands on a different device — or on
 * a different graphics API — from the one the texture was made on.
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
 * THE WHOLE OF @p mtlTexture, at the size the texture itself reports —
 * for the caller handed a texture somebody else made, whose extent is
 * that texture's own fact and not one worth restating. Everything else
 * is the wrap above.
 */
sk_sp<SkImage> wrapImage(skgpu::graphite::Recorder& recorder, void* mtlTexture,
                         SkAlphaType alphaType = kPremul_SkAlphaType,
                         sk_sp<SkColorSpace> colorSpace = nullptr);

/**
 * @p mtlTexture's pixels READ BACK INTO HOST MEMORY, as an image any
 * canvas and any renderer can draw — the answer for a picture that has
 * to reach a device the texture does not stand on, where a wrap has
 * nothing either side could hand the other.
 *
 * THIS ONE COPIES and the wraps do not, which is the whole difference
 * between them. An image from here names bytes this process owns, so it
 * is uploaded again wherever it is drawn and costs a frame of pixels
 * every time one is made; a wrapped one costs nothing and is drawable on
 * exactly one recorder. Ask for a wrap wherever a wrap will do.
 *
 * The copy runs on a command queue this call keeps for the texture's own
 * device — one queue held for the process, since a queue made per read
 * would be a queue allocated per read — and the calling thread blocks
 * until it has finished, which is what having the pixels means.
 *
 * The texels are read as the format the texture declares, with the
 * channels in that format's own order; @p alphaType says what its alpha
 * channel means and @p colorSpace what its colours do, with null asking
 * for no conversion. Null when there is no texture, when the copy did
 * not complete, or when the format is not four eight-bit channels, which
 * is the only shape read here.
 */
sk_sp<SkImage> readImage(void* mtlTexture,
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
 *
 * Named apart from `wrapImage` because the ownership is the other way
 * round: that one retains the texture it is given, this one retains
 * nothing and hands the planes back through @p release.
 */
sk_sp<SkImage> wrapPlanarImage(skgpu::graphite::Recorder& recorder,
                               std::span<const TexturePlane> planes,
                               const SkYUVAInfo& info,
                               sk_sp<SkColorSpace> colorSpace,
                               TextureRelease release, void* releaseContext);
#endif

}  // namespace sigil::skia
