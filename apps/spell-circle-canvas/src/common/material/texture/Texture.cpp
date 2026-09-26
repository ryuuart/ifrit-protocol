/** @file
 * The frame a texture samples, the region cut out of it, the image
 * shader it samples through, and where its pixels stand on a device.
 */

#include "sigilmaterial/texture/Texture.h"

#include <sigilmedia/advanced/Device.h>

namespace sigil::material {

sk_sp<SkImage> Texture::image(std::chrono::duration<double> time) const {
  // A frame standing on a device is read back: a texture is sampled by
  // whichever renderer draws the material, and a renderer that shares
  // the device reads `deviceImage()` instead.
  sk_sp<SkImage> full = media::deviceImage(m_source.frameAt(time), nullptr);
  if (!full || !m_region) return full;
  if (m_cut && m_cutFrom.get() == full.get()) return m_cut;
  SkIRect rect = *m_region;
  if (!rect.intersect(SkIRect::MakeWH(full->width(), full->height())))
    return nullptr;
  m_cutFrom = full;
  m_cut = full->makeSubset(nullptr, rect, {});
  return m_cut;
}

SkISize Texture::size() const {
  sk_sp<SkImage> img = image();
  return img ? img->dimensions() : SkISize::MakeEmpty();
}

sk_sp<SkShader> Texture::shader() const {
  sk_sp<SkImage> img = image();
  if (!img) return nullptr;
  return img->makeShader(m_tileX, m_tileY, SkSamplingOptions(m_filter), m_uv);
}

sk_sp<SkShader> Texture::shaderAt(const FrameData& frame) const {
  if (!m_source.isRunning()) return shader();
  sk_sp<SkImage> img = image(std::chrono::duration<double>(frame.seconds));
  if (!img) return nullptr;
  return img->makeShader(m_tileX, m_tileY, SkSamplingOptions(m_filter), m_uv);
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
         m_region == other.m_region && m_filter == other.m_filter;
}

}  // namespace sigil::material
