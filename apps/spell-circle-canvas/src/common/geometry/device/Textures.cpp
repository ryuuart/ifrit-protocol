/** @file
 * The map crossing: a wrap where the pixels already stand, an upload
 * where they do not, and the panorama chain a lit surface reflects.
 */

#include "sigilgeometry/device/Textures.h"

// clang-format off
// ORDER IS LOAD-BEARING HERE, which is why the sorter is held off: the
// engine's Vulkan interface names Vulkan's handle types and does not
// include the header that declares them, so an alphabetical sort of the
// two leaves every one of those names unknown.
#include <vulkan/vulkan.h>
#include <Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h>
// clang-format on

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Resources.h>
#include <sigilskia/graphite/Pixels.h>

#include <Graphics/GraphicsAccessories/interface/GraphicsAccessories.hpp>
#include <cstdint>
#include <vector>

namespace sigil::geometry::device {

namespace dg = Diligent;

namespace {

/** How long a map stays resident after the last draw that named it. */
constexpr uint64_t kMapLifetime = 2;

/** The description a WRAPPED map is given: a plain two-dimensional
 *  colour texture a shader reads, at the one level the image someone
 *  else painted actually has. A wrap describes what is there and cannot
 *  ask for anything more. */
dg::TextureDesc wrappedMapDesc(const char* label, int width, int height) {
  dg::TextureDesc desc;
  desc.Name = label;
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)width;
  desc.Height = (dg::Uint32)height;
  desc.MipLevels = 1;
  desc.Format = kColorFormat;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_DEFAULT;
  return desc;
}

/** …and the description an UPLOADED one is given: the same texture with
 *  whatever chain `mapMipLevels` says it has. The render-target bind and
 *  the generate flag are what let the device fill the levels below zero,
 *  which nothing else here asks it for, and they are asked for only
 *  where there is a level to fill: the count is stated rather than left
 *  to the device to work out, so one piece of arithmetic decides both
 *  what is asked for and whether there is anything to derive. */
dg::TextureDesc uploadedMapDesc(const char* label, int width, int height) {
  dg::TextureDesc desc = wrappedMapDesc(label, width, height);
  desc.MipLevels = (dg::Uint32)mapMipLevels(width, height);
  if (desc.MipLevels > 1) {
    desc.BindFlags = dg::BIND_SHADER_RESOURCE | dg::BIND_RENDER_TARGET;
    desc.MiscFlags = dg::MISC_TEXTURE_FLAG_GENERATE_MIPS;
  }
  return desc;
}

/** One level of a half-float panorama, as the device takes it. */
dg::TextureSubResData halfFloatLevel(const std::vector<uint16_t>& pixels,
                                     int width) {
  dg::TextureSubResData data;
  data.pData = pixels.data();
  data.Stride = (dg::Uint64)width * 4 * sizeof(uint16_t);
  return data;
}

}  // namespace

int mapMipLevels(int width, int height) {
  if (width <= 0 || height <= 0) return 1;
  return (int)dg::ComputeMipLevelsCount((dg::Uint32)width, (dg::Uint32)height);
}

TextureResidency::TextureResidency(Device& device) : m_device(&device) {}

dg::ITexture* TextureResidency::sample(const material::Texture& map) {
  if (!m_device->renderDevice()) return nullptr;

  // ZERO COPY: the pixels already stand on this very device, so what a
  // draw needs is a name for them and not a second copy of them.
  const material::DeviceImage where = map.deviceImage();
  if (where && where.device == m_device->gpu() && where.handle != 0) {
    SampledImage& held = m_wrapped[where.handle];
    held.used = m_frame;
    if (!held.texture) {
      auto* vk = static_cast<dg::IRenderDeviceVk*>(m_device->renderDevice());
      // The image was drawn into and then submitted, so what a sampler
      // reads is what it was left as: a shader resource.
      // The image arrives as a NUMBER, because the value that carried it
      // here belongs to a library that cannot spell a Vulkan type.
      vk->CreateTextureFromVulkanImage(
          reinterpret_cast<VkImage>(  // NOLINT(performance-no-int-to-ptr)
              where.handle),
          wrappedMapDesc("sampled map", where.width, where.height),
          dg::RESOURCE_STATE_SHADER_RESOURCE, &held.texture);
    }
    if (held.texture) return held.texture;
    // The wrap was refused; the pixels are still readable the long way.
  }

  const sk_sp<SkImage> image = map.image();
  if (!image) return nullptr;
  SampledImage& held = m_uploaded[image->uniqueID()];
  held.used = m_frame;
  if (held.texture) return held.texture;

  SkBitmap bytes;
  if (!bytes.tryAllocPixels(SkImageInfo::Make(image->width(), image->height(),
                                              kRGBA_8888_SkColorType,
                                              kPremul_SkAlphaType)))
    return nullptr;
  if (!image->readPixels(nullptr, bytes.pixmap(), 0, 0)) return nullptr;

  // NO INITIAL DATA: a texture is created with as many subresources as
  // it has levels or the device refuses it, and only level zero is
  // known here. It is written after the fact and whatever levels stand
  // under it derived from it, once, on the frame the map first appears.
  m_device->renderDevice()->CreateTexture(
      uploadedMapDesc("sampled map", image->width(), image->height()), nullptr,
      &held.texture);
  if (!held.texture) return nullptr;

  dg::TextureSubResData level;
  level.pData = bytes.getPixels();
  level.Stride = (dg::Uint64)bytes.rowBytes();
  dg::Box whole;
  whole.MaxX = (dg::Uint32)image->width();
  whole.MaxY = (dg::Uint32)image->height();
  dg::IDeviceContext* context = m_device->context();
  context->UpdateTexture(held.texture, 0, 0, whole, level,
                         dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                         dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  if (held.texture->GetDesc().MipLevels > 1)
    context->GenerateMips(
        held.texture->GetDefaultView(dg::TEXTURE_VIEW_SHADER_RESOURCE));
  return held.texture;
}

dg::ITexture* TextureResidency::environment(
    const material::EnvironmentMap& map) {
  if (!m_device->renderDevice() || !map.valid()) return nullptr;
  const sk_sp<SkImage> base = map.image(0);
  if (!base) return nullptr;
  SampledImage& held = m_environments[base->uniqueID()];
  held.used = m_frame;
  if (held.texture) return held.texture;

  const std::vector<sk_sp<SkImage>> levels = map.chain();
  if (levels.empty() || !levels.front()) return nullptr;

  // Half floats keep the range a sky needs and are filterable
  // everywhere; the thirty-two-bit form the panorama was blurred in is
  // not, on an Apple GPU.
  std::vector<std::vector<uint16_t>> pixels;
  std::vector<dg::TextureSubResData> subresources;
  pixels.reserve(levels.size());
  subresources.reserve(levels.size());
  for (const sk_sp<SkImage>& level : levels) {
    pixels.push_back(skia::halfFloatPixels(level));
    if (pixels.back().empty()) return nullptr;
    subresources.push_back(halfFloatLevel(pixels.back(), level->width()));
  }

  dg::TextureDesc desc;
  desc.Name = "environment panorama";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)levels.front()->width();
  desc.Height = (dg::Uint32)levels.front()->height();
  desc.MipLevels = (dg::Uint32)levels.size();
  desc.Format = dg::TEX_FORMAT_RGBA16_FLOAT;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_IMMUTABLE;
  dg::TextureData data;
  data.pSubResources = subresources.data();
  data.NumSubresources = (dg::Uint32)subresources.size();
  m_device->renderDevice()->CreateTexture(desc, &data, &held.texture);
  return held.texture;
}

dg::ITexture* TextureResidency::irradiance(
    const material::EnvironmentMap& map) {
  if (!m_device->renderDevice() || !map.valid()) return nullptr;
  const sk_sp<SkImage> base = map.image(0);
  if (!base) return nullptr;
  SampledImage& held = m_irradiances[base->uniqueID()];
  held.used = m_frame;
  if (held.texture) return held.texture;

  const sk_sp<SkImage> lobe = map.irradiance();
  if (!lobe) return nullptr;
  const std::vector<uint16_t> pixels = skia::halfFloatPixels(lobe);
  if (pixels.empty()) return nullptr;
  dg::TextureSubResData level = halfFloatLevel(pixels, lobe->width());
  dg::TextureDesc desc;
  desc.Name = "irradiance lobe";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)lobe->width();
  desc.Height = (dg::Uint32)lobe->height();
  desc.MipLevels = 1;
  desc.Format = dg::TEX_FORMAT_RGBA16_FLOAT;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_IMMUTABLE;
  dg::TextureData data{&level, 1};
  m_device->renderDevice()->CreateTexture(desc, &data, &held.texture);
  return held.texture;
}

void TextureResidency::endFrame() {
  ++m_frame;
  for (auto it = m_wrapped.begin(); it != m_wrapped.end();) {
    if (m_frame - it->second.used > kMapLifetime)
      it = m_wrapped.erase(it);
    else
      ++it;
  }
  for (auto it = m_uploaded.begin(); it != m_uploaded.end();) {
    if (m_frame - it->second.used > kMapLifetime)
      it = m_uploaded.erase(it);
    else
      ++it;
  }
}

}  // namespace sigil::geometry::device
