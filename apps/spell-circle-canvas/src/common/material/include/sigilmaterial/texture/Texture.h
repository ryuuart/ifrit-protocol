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
 * read, and how it is read between pixels. Nothing here names a
 * renderer: the Skia executor turns a texture into an image shader, a
 * device renderer binds its frame as a texture.
 */

#include <sigilmaterial/core/Gradient.h>
#include <sigilmaterial/core/Leaf.h>
#include <sigilmedia/core/Frame.h>
#include <sigilmedia/core/PixelSource.h>

#include <chrono>
#include <cstdint>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <optional>
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

/** HOW A TEXTURE IS READ BETWEEN ITS PIXELS: the nearest pixel, or a
 *  blend of the four around the sample. Nearest is right for anything on
 *  a pixel grid and wrong for anything organic. */
enum class Sampling : uint8_t { Nearest, Linear };

/** A RECTANGLE OF WHOLE PIXELS in an image: its top-left corner and its
 *  size. Empty when either side is not positive. */
struct PixelRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  bool empty() const { return width <= 0 || height <= 0; }
  bool operator==(const PixelRect&) const = default;
};

/** An image and its sampling. A value: copy it, change a dial on the
 *  copy, keep both. As a Leaf it fills a material's slot, and whichever
 *  renderer draws the material binds it: the Skia executor as an image
 *  shader, a device renderer as a texture. */
class Texture : public Leaf {
 public:
  Texture() = default;
  /** A texture over @p source — a picture, an `Image`, a `Video`, frames
   *  another application publishes, a producer baked on first use, a
   *  rendered scene — sampled at the time the material is drawn at. */
  explicit Texture(media::PixelSource source) : m_source(std::move(source)) {}

  /** What is sampled past the image's (or the region's) edges, per axis.
   *  Unstated, the edge pixels carry on (`Repeat::Pad`). */
  Texture& tile(Repeat horizontal, Repeat vertical) {
    m_tileX = horizontal;
    m_tileY = vertical;
    return *this;
  }
  Texture& tile(Repeat both) { return tile(both, both); }
  /** Texture space into the sampled space: where pixel (0, 0) lands and
   *  how the image is scaled and turned. */
  Texture& uv(const glm::mat3& matrix) {
    m_uv = matrix;
    return *this;
  }
  /** The image's corner at @p origin, and no other placing. */
  Texture& at(glm::vec2 origin) {
    glm::mat3 translation(1.0f);
    translation[2][0] = origin.x;
    translation[2][1] = origin.y;
    return uv(translation);
  }
  /** Reads only @p rect of the image, whose corner becomes pixel (0, 0). */
  Texture& region(PixelRect rect) {
    m_region = rect;
    m_cutFrom = {};
    m_cut = {};
    return *this;
  }
  /** How the image is read between pixels; linear unless stated. */
  Texture& sampling(Sampling mode) {
    m_sampling = mode;
    return *this;
  }

  const media::PixelSource& source() const { return m_source; }
  Repeat tileX() const { return m_tileX; }
  Repeat tileY() const { return m_tileY; }
  const glm::mat3& uv() const { return m_uv; }
  const std::optional<PixelRect>& region() const { return m_region; }
  Sampling sampling() const { return m_sampling; }

  bool valid() const { return static_cast<bool>(m_source); }
  /** The frame sampled at @p time: the source's frame, read back to host
   *  memory when it stands on a device, cut to the region when one is
   *  set. Empty when the source yields nothing or the region misses the
   *  image. */
  media::Frame frameAt(std::chrono::duration<double> time = {}) const;
  /** The same, with a frame standing on a device bound for @p recorder
   *  where it stands rather than read back — what a renderer drawing on
   *  that device reads. A null @p recorder reads it back. */
  media::Frame frameAt(std::chrono::duration<double> time,
                       skgpu::graphite::Recorder* recorder) const;
  /** The sampled size in pixels — the region's, clipped to the image,
   *  when one is set — zero when there is no image. */
  glm::ivec2 size() const;

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
  Repeat m_tileX = Repeat::Pad;
  Repeat m_tileY = Repeat::Pad;
  glm::mat3 m_uv{1.0f};
  std::optional<PixelRect> m_region;
  Sampling m_sampling = Sampling::Linear;
  // The region cut from the frame it was cut from, so a texture sampled
  // every frame does not copy its pixels every frame. Derived state: not
  // part of equality.
  mutable media::Frame m_cutFrom;
  mutable media::Frame m_cut;
};

}  // namespace sigil::material
