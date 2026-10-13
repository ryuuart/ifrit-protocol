/** @file
 * The map crossing: a wrap where the pixels already stand, an upload
 * where they do not, and the panorama chain a lit surface reflects.
 */

#include "Textures.h"

// clang-format off
// ORDER IS LOAD-BEARING HERE, which is why the sorter is held off: the
// engine's Vulkan interface names Vulkan's handle types and does not
// include the header that declares them, so an alphabetical sort of the
// two leaves every one of those names unknown.
#include <vulkan/vulkan.h>
#include <Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h>
#include <Graphics/GraphicsEngineVulkan/interface/TextureVk.h>
// clang-format on

#include <include/core/SkBitmap.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImageInfo.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilmedia/advanced/Device.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilskia/graphite/Pixels.h>

#include <Graphics/GraphicsAccessories/interface/GraphicsAccessories.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>

#include "Resources.h"

namespace sigil::geometry::device {

namespace dg = Diligent;

namespace {

/** How long a map stays resident after the last draw that named it. */
constexpr uint64_t kMapLifetime = 2;

/** The description a WRAPPED map starts from: a plain two-dimensional
 *  texture a shader reads, in @p format, at the one level someone else
 *  painted. A wrap describes what is there and cannot ask for anything
 *  more, so a caller wrapping an image made with a chain states its
 *  levels on top of this. */
dg::TextureDesc wrappedMapDesc(const char* label, int width, int height,
                               dg::TEXTURE_FORMAT format = kColorFormat) {
  dg::TextureDesc desc;
  desc.Name = label;
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)width;
  desc.Height = (dg::Uint32)height;
  desc.MipLevels = 1;
  desc.Format = format;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_DEFAULT;
  return desc;
}

/** THE FORMAT A PAGE STANDING ON THE DEVICE IS IN, as the device names
 *  it, for the formats a texture on the device is made in; a frame that
 *  states none is the eight-bit colour every target here holds.
 *  Undefined for any other, which no wrap may claim. */
dg::TEXTURE_FORMAT wrappedFormat(uint32_t vulkanFormat) {
  switch (vulkanFormat) {
    case VK_FORMAT_UNDEFINED:
    case VK_FORMAT_R8G8B8A8_UNORM:
      return dg::TEX_FORMAT_RGBA8_UNORM;
    case VK_FORMAT_B8G8R8A8_UNORM:
      return dg::TEX_FORMAT_BGRA8_UNORM;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
      return dg::TEX_FORMAT_RGBA16_FLOAT;
    default:
      return dg::TEX_FORMAT_UNKNOWN;
  }
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

/** WHAT AN IMAGE BECOMES ON THE DEVICE in one form: the device's format,
 *  the host layout its pixels are read into for it, and whether the device
 *  can derive smaller copies of it. */
struct Layout {
  dg::TEXTURE_FORMAT format = kColorFormat;
  SkColorType colorType = kRGBA_8888_SkColorType;
  SkAlphaType alpha = kPremul_SkAlphaType;
  bool derivable = true;
};

/** Whether @p format can be read between its texels on @p device. */
bool filterable(Device& device, dg::TEXTURE_FORMAT format) {
  return device.renderDevice()->GetTextureFormatInfoExt(format).Filterable;
}

/** THE LAYOUT @p image TAKES IN @p form. Colour is eight bits a channel,
 *  premultiplied. Data keeps its alpha straight and its precision: a
 *  thirty-two-bit float image stays thirty-two bits unless it is to be
 *  filtered where the device cannot filter that format, a half-float one
 *  stays half, sixteen-bit integers stay sixteen-bit, and everything else
 *  is eight bits a channel. */
Layout layoutFor(const SkImage& image, const MapForm& form, Device& device) {
  Layout out;
  if (form.colorSpace == media::ColorSpace::Srgb) return out;
  out.alpha = kUnpremul_SkAlphaType;
  switch (image.colorType()) {
    case kRGBA_F32_SkColorType:
      if (!form.filtered || filterable(device, dg::TEX_FORMAT_RGBA32_FLOAT)) {
        out.format = dg::TEX_FORMAT_RGBA32_FLOAT;
        out.colorType = kRGBA_F32_SkColorType;
        out.derivable = filterable(device, dg::TEX_FORMAT_RGBA32_FLOAT);
        return out;
      }
      out.format = dg::TEX_FORMAT_RGBA16_FLOAT;
      out.colorType = kRGBA_F16_SkColorType;
      return out;
    case kRGBA_F16_SkColorType:
    case kRGBA_F16Norm_SkColorType:
      out.format = dg::TEX_FORMAT_RGBA16_FLOAT;
      out.colorType = kRGBA_F16_SkColorType;
      return out;
    case kR16G16B16A16_unorm_SkColorType:
      out.format = dg::TEX_FORMAT_RGBA16_UNORM;
      out.colorType = kR16G16B16A16_unorm_SkColorType;
      return out;
    default:
      return out;
  }
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

WrappedImage* TextureResidency::wrap(const media::DeviceFrame& where) {
  const dg::TEXTURE_FORMAT format = wrappedFormat(where.format);
  if (where.kind != media::DeviceFrame::Kind::Texture ||
      where.device != m_device->gpu() || where.handle == 0 ||
      format == dg::TEX_FORMAT_UNKNOWN)
    return nullptr;
  // THE PAINTING BEFORE THE SAMPLING, once a frame: a page another queue
  // paints is finished there before anything this frame submits reads
  // it. A wait queued while a pass is still being recorded holds that
  // pass too, since nothing recorded is submitted until later.
  if (m_waited != m_frame) {
    m_device->waitForPainting();
    m_waited = m_frame;
  }
  m_woreAPage = true;
  const dg::Uint32 levels = (dg::Uint32)std::max(1, where.mipLevels);
  WrappedImage& held = m_wrapped[where.handle];
  held.used = m_frame;
  // A name the device reused for another image is wrapped again.
  if (held.texture) {
    const dg::TextureDesc& was = held.texture->GetDesc();
    if (was.Width != (dg::Uint32)where.width ||
        was.Height != (dg::Uint32)where.height || was.Format != format ||
        was.MipLevels != levels)
      held = WrappedImage{.used = m_frame};
  }
  if (!held.texture) {
    auto* vk = static_cast<dg::IRenderDeviceVk*>(m_device->renderDevice());
    // DESCRIBED AS IT WAS MADE: its format and its levels. The levels
    // below zero are this residency's to fill, which the render-target
    // bind and the generate flag let the device do; a format the device
    // cannot filter is described without them and read at level zero.
    dg::TextureDesc desc =
        wrappedMapDesc("sampled map", where.width, where.height, format);
    desc.MipLevels = levels;
    if (levels > 1 && filterable(*m_device, format)) {
      desc.BindFlags = dg::BIND_SHADER_RESOURCE | dg::BIND_RENDER_TARGET;
      desc.MiscFlags = dg::MISC_TEXTURE_FLAG_GENERATE_MIPS;
    }
    // The image was drawn into and then submitted, so what a sampler
    // reads is what it was left as: a shader resource.
    // The image arrives as a NUMBER, because the value that carried it
    // here belongs to a library that cannot spell a Vulkan type.
    vk->CreateTextureFromVulkanImage(
        reinterpret_cast<VkImage>(  // NOLINT(performance-no-int-to-ptr)
            where.handle),
        desc, dg::RESOURCE_STATE_SHADER_RESOURCE, &held.texture);
  }
  return held.texture ? &held : nullptr;
}

bool TextureResidency::fill(WrappedImage& held) {
  if ((held.texture->GetDesc().MiscFlags &
       dg::MISC_TEXTURE_FLAG_GENERATE_MIPS) == 0)
    return false;
  // ONCE A FRAME, AFTER THE PAINTING: the page may be painted again
  // between two frames and nothing says when, so its levels are filled
  // from level zero on the first use of every frame that wears it. The
  // fill is recorded after the wait for the painting, so it reads the
  // page as painted.
  if (held.filled != m_frame) {
    m_device->context()->GenerateMips(
        held.texture->GetDefaultView(dg::TEXTURE_VIEW_SHADER_RESOURCE));
    held.filled = m_frame;
  }
  return true;
}

dg::ITexture* TextureResidency::levelZeroOf(WrappedImage& held) {
  const dg::TextureDesc& page = held.texture->GetDesc();
  if (page.MipLevels <= 1) return held.texture;
  if (!held.levelZero) {
    auto* vk = static_cast<dg::IRenderDeviceVk*>(m_device->renderDevice());
    dg::RefCntAutoPtr<dg::ITextureVk> image{held.texture, dg::IID_TextureVk};
    if (!image) return held.texture;
    vk->CreateTextureFromVulkanImage(
        image->GetVkImage(),
        wrappedMapDesc("sampled map level zero", (int)page.Width,
                       (int)page.Height, page.Format),
        dg::RESOURCE_STATE_SHADER_RESOURCE, &held.levelZero);
  }
  return held.levelZero ? held.levelZero.RawPtr() : held.texture.RawPtr();
}

void TextureResidency::prepare(const material::Texture& map,
                               std::chrono::duration<double> time) {
  if (!m_device->renderDevice()) return;
  if (WrappedImage* held = wrap(map.source().frameAt(time).device))
    (void)fill(*held);
}

dg::ITexture* TextureResidency::sample(const material::Texture& map,
                                       const MapForm& form,
                                       std::chrono::duration<double> time) {
  if (!m_device->renderDevice()) return nullptr;

  // ZERO COPY: the pixels already stand on this very device, so what a
  // draw needs is a name for them and not a second copy of them. The
  // source is asked at the frame's time, and a frame of it standing on a
  // device is named rather than read back.
  if (WrappedImage* held = wrap(map.source().frameAt(time).device)) {
    if (form.mipmaps && fill(*held)) return held->texture;
    return levelZeroOf(*held);
  }
  // A page the wrap was refused is still readable the long way.

  const sk_sp<SkImage> image = map.frameAt(time).image;
  if (!image) return nullptr;
  const Layout layout = layoutFor(*image, form, *m_device);
  const uint64_t key = ((uint64_t)image->uniqueID() << 8u) |
                       ((uint64_t)layout.format << 1u) |
                       (form.mipmaps ? 1u : 0u);
  SampledImage& held = m_uploaded[key];
  held.used = m_frame;
  if (held.texture) return held.texture;

  SkBitmap bytes;
  if (!bytes.tryAllocPixels(SkImageInfo::Make(image->width(), image->height(),
                                              layout.colorType, layout.alpha,
                                              nullptr)))
    return nullptr;
  if (!image->readPixels(nullptr, bytes.pixmap(), 0, 0)) return nullptr;

  // NO INITIAL DATA: a texture is created with as many subresources as
  // it has levels or the device refuses it, and only level zero is
  // known here. It is written after the fact and whatever levels stand
  // under it derived from it, once, on the frame the map first appears.
  dg::TextureDesc desc =
      uploadedMapDesc("sampled map", image->width(), image->height());
  desc.Format = layout.format;
  if (!form.mipmaps || !layout.derivable) {
    desc.MipLevels = 1;
    desc.BindFlags = dg::BIND_SHADER_RESOURCE;
    desc.MiscFlags = dg::MISC_TEXTURE_FLAG_NONE;
  }
  m_device->renderDevice()->CreateTexture(desc, nullptr, &held.texture);
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
  if (!map.valid()) return nullptr;
  std::vector<sk_sp<SkImage>> levels;
  for (const material::Texture& level : map.chain())
    levels.push_back(level.frameAt().image);
  return panorama(levels);
}

dg::ITexture* TextureResidency::panorama(
    const std::vector<sk_sp<SkImage>>& levels) {
  if (!m_device->renderDevice()) return nullptr;
  if (levels.empty() || !levels.front()) return nullptr;
  SampledImage& held = m_environments[levels.front()->uniqueID()];
  held.used = m_frame;
  if (held.texture) return held.texture;

  // A CHAIN IS A MIP PYRAMID here — level k half of level k-1 — which is
  // what lets one texture hold the whole prefiltered set and a
  // fractional level read across two of them. A caller that assembled
  // its levels some other way is taken at its sharpest one alone rather
  // than refused by the device, which will not take a level whose size
  // is not the size the chain says.
  size_t usable = 1;
  for (size_t i = 1; i < levels.size(); ++i) {
    if (!levels[i] ||
        levels[i]->width() != std::max(1, levels[i - 1]->width() / 2) ||
        levels[i]->height() != std::max(1, levels[i - 1]->height() / 2))
      break;
    ++usable;
  }

  // Half floats keep the range a sky needs and are filterable
  // everywhere; the thirty-two-bit form the panorama was blurred in is
  // not, on an Apple GPU.
  std::vector<std::vector<uint16_t>> pixels;
  std::vector<dg::TextureSubResData> subresources;
  pixels.reserve(usable);
  subresources.reserve(usable);
  for (size_t i = 0; i < usable; ++i) {
    pixels.push_back(skia::halfFloatPixels(levels[i]));
    if (pixels.back().empty()) return nullptr;
    subresources.push_back(halfFloatLevel(pixels.back(), levels[i]->width()));
  }

  dg::TextureDesc desc;
  desc.Name = "environment panorama";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = (dg::Uint32)levels.front()->width();
  desc.Height = (dg::Uint32)levels.front()->height();
  desc.MipLevels = (dg::Uint32)usable;
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
  if (!map.valid()) return nullptr;
  return convolution(map.irradiance().frameAt().image);
}

dg::ITexture* TextureResidency::convolution(const sk_sp<SkImage>& lobe) {
  if (!m_device->renderDevice() || !lobe) return nullptr;
  SampledImage& held = m_irradiances[lobe->uniqueID()];
  held.used = m_frame;
  if (held.texture) return held.texture;

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
  // THE NEXT PAINTING AFTER THE SAMPLING: a page is repainted in place,
  // over what a pass of this frame may still be reading.
  if (m_woreAPage) {
    m_device->holdPaintingUntilSampled();
    m_woreAPage = false;
  }
  ++m_frame;
  // ALL FOUR MAPS AGE ON THE SAME BEAT. A panorama is the largest thing
  // this residency holds, so leaving the two environment maps out would
  // keep every sky a scene ever sampled for the residency's life.
  const auto age = [this](auto& held) {
    for (auto it = held.begin(); it != held.end();) {
      if (m_frame - it->second.used > kMapLifetime)
        it = held.erase(it);
      else
        ++it;
    }
  };
  age(m_wrapped);
  age(m_uploaded);
  age(m_environments);
  age(m_irradiances);
}

}  // namespace sigil::geometry::device
