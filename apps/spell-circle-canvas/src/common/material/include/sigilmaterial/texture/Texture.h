#pragma once

/** @file
 * @ingroup material-texture
 *
 * Texture — an image and how it is sampled, as a comparable value a
 * material tree can hold in a slot. The pixels come from a
 * `media::PixelSource`: a picture, a decoded image or animation, a video,
 * a producer that bakes on first use, frames another application
 * publishes, a rendered scene — asked for the frame at the time the
 * material is drawn at. Sampling is the tiling per axis, a uv matrix
 * placing texture space in the sampled space, a region of the image to
 * read, and the filter.
 */

#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkTileMode.h>
#include <sigilmaterial/core/Leaf.h>
#include <sigilmaterial/texture/ShaderLeaf.h>
#include <sigilmedia/core/PixelSource.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace sigil::material {

/** WHERE A TEXTURE'S PIXELS ALREADY LIVE, when they live on a GPU: the
 *  device that owns the texture, and the texture itself as the graphics
 *  API's own object bridged to opaque values, with its format and
 *  layout — read off the frame its source answers when that frame stands
 *  on a device as a texture. Nothing in this library reads any of it: it
 *  is carried unexamined to a renderer standing on the SAME device, and a
 *  renderer holding another reads `image()` instead.
 *  @trap Empty unless the source painted on a device, which most never
 *  do. */
struct DeviceImage {
  const void* device = nullptr;
  const void* pointer = nullptr;
  uint64_t handle = 0;
  uint32_t format = 0;
  uint32_t layout = 0;
  int width = 0;
  int height = 0;

  explicit operator bool() const {
    return device != nullptr && (pointer != nullptr || handle != 0);
  }
  bool operator==(const DeviceImage&) const = default;
};

/** An image and its sampling. A value: copy it, change a dial on the
 *  copy, keep both. As a Leaf it fills a material's slot, and the
 *  Skia backend binds it as the image shader `shader()` builds. */
class Texture : public ShaderLeaf {
 public:
  Texture() = default;
  /** A texture over @p source — a picture, an `Image`, a `Video`, frames
   *  another application publishes, a rendered scene — sampled at the
   *  time the material is drawn at. */
  explicit Texture(media::PixelSource source) : m_source(std::move(source)) {}

  /** A texture over a picture in hand. */
  static Texture of(sk_sp<SkImage> image) {
    return Texture(media::PixelSource(std::move(image)));
  }
  /** A texture baked by @p produce on first use, identified by @p key. */
  static Texture produce(std::string key,
                         std::function<sk_sp<SkImage>()> produce) {
    return Texture(
        media::PixelSource::produce(std::move(key), std::move(produce)));
  }

  /** The tiling per axis outside the image (or the region). */
  Texture& tile(SkTileMode horizontal, SkTileMode vertical) {
    m_tileX = horizontal;
    m_tileY = vertical;
    return *this;
  }
  Texture& tile(SkTileMode both) { return tile(both, both); }
  /** Texture space into the sampled space: where pixel (0, 0) lands and
   *  how the image is scaled and turned. */
  Texture& uv(const SkMatrix& matrix) {
    m_uv = matrix;
    return *this;
  }
  /** `uv(Translate(origin))`: the image's corner at @p origin. */
  Texture& at(SkPoint origin) {
    return uv(SkMatrix::Translate(origin.fX, origin.fY));
  }
  /** Reads only @p rect of the image, whose corner becomes pixel (0, 0). */
  Texture& region(SkIRect rect) {
    m_region = rect;
    return *this;
  }
  /** The filter between samples: linear (the default) or nearest. */
  Texture& filter(SkFilterMode mode) {
    m_filter = mode;
    return *this;
  }

  const media::PixelSource& source() const { return m_source; }
  SkTileMode tileX() const { return m_tileX; }
  SkTileMode tileY() const { return m_tileY; }
  const SkMatrix& uv() const { return m_uv; }
  const std::optional<SkIRect>& region() const { return m_region; }
  SkFilterMode filter() const { return m_filter; }

  bool valid() const { return static_cast<bool>(m_source); }
  /** The image sampled at @p time: the source's frame, read back to host
   *  memory when it stands on a device, cut to the region when one is
   *  set. Null when the source yields nothing. */
  sk_sp<SkImage> image(std::chrono::duration<double> time = {}) const;
  /** The sampled image's size, zero when there is none. */
  SkISize size() const;
  /** The Skia shader: `image()` tiled, filtered and placed by `uv()`.
   *  Null when there is no image. */
  sk_sp<SkShader> shader() const override;
  /** The same, over the frame the source answers at @p frame's time. */
  sk_sp<SkShader> shaderAt(const FrameData& frame) const override;

  bool animated() const override { return m_source.isRunning(); }
  /** Where the sampled pixels already stand, when the source painted
   *  them on a device. A REGION is not applied to it: what the device
   *  holds is the whole image, and a renderer binding it cuts the region
   *  itself. */
  DeviceImage deviceImage() const;
  bool operator==(const Texture& other) const;

 protected:
  bool equals(const Leaf& other) const override {
    return *this == static_cast<const Texture&>(other);
  }

 private:
  media::PixelSource m_source;
  SkTileMode m_tileX = SkTileMode::kClamp;
  SkTileMode m_tileY = SkTileMode::kClamp;
  SkMatrix m_uv = SkMatrix::I();
  std::optional<SkIRect> m_region;
  SkFilterMode m_filter = SkFilterMode::kLinear;
  // The region cut from the source image it was cut from, so a texture
  // sampled every frame does not copy its pixels every frame. Derived
  // state: not part of equality.
  mutable sk_sp<SkImage> m_cutFrom;
  mutable sk_sp<SkImage> m_cut;
};

}  // namespace sigil::material
