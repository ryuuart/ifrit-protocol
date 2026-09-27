/** @file
 * The frame a texture samples, the region cut out of it, its size, and
 * where its pixels stand on a device.
 */

#include "sigilmaterial/texture/Texture.h"

#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <sigilmedia/advanced/Device.h>

namespace sigil::material {

media::Frame Texture::frameAt(std::chrono::duration<double> time) const {
  return frameAt(time, nullptr);
}

media::Frame Texture::frameAt(std::chrono::duration<double> time,
                              skgpu::graphite::Recorder* recorder) const {
  media::Frame frame = m_source.frameAt(time);
  // A frame standing on a device is bound for the recorder that draws
  // it, and read back where there is none: a texture is sampled by
  // whichever renderer draws the material, and a renderer that shares
  // the device without a recorder reads `deviceImage()` instead.
  if (!frame.image && frame.device) {
    frame.image = media::deviceImage(frame, recorder);
    frame.device = {};
  }
  if (!frame.image || !m_region) return frame;
  if (m_cut.image && m_cutFrom.image.get() == frame.image.get()) return m_cut;
  SkIRect rect = SkIRect::MakeXYWH(m_region->x, m_region->y, m_region->width,
                                   m_region->height);
  if (!rect.intersect(
          SkIRect::MakeWH(frame.image->width(), frame.image->height())))
    return {};
  m_cutFrom = frame;
  m_cut = frame;
  m_cut.image = frame.image->makeSubset(nullptr, rect, {});
  return m_cut;
}

glm::ivec2 Texture::size() const {
  const SkISize whole = m_source.size();
  if (!m_region) return {whole.width(), whole.height()};
  SkIRect rect = SkIRect::MakeXYWH(m_region->x, m_region->y, m_region->width,
                                   m_region->height);
  if (!rect.intersect(SkIRect::MakeSize(whole))) return {0, 0};
  return {rect.width(), rect.height()};
}

DeviceImage Texture::deviceImage() const {
  const media::Frame frame = m_source.frameAt({});
  if (frame.device.kind != media::DeviceFrame::Kind::Texture) return {};
  return {.device = frame.device.device,
          .pointer = frame.device.pointer,
          .handle = frame.device.handle,
          .format = frame.device.format,
          .layout = frame.device.layout,
          .width = frame.device.width,
          .height = frame.device.height};
}

bool Texture::operator==(const Texture& other) const {
  return m_source == other.m_source && m_tileX == other.m_tileX &&
         m_tileY == other.m_tileY && m_uv == other.m_uv &&
         m_region == other.m_region && m_sampling == other.m_sampling;
}

}  // namespace sigil::material
