#pragma once

/** @file
 * WHAT EVERY EXECUTOR ON THIS DEVICE STANDS ON, made once and shared:
 * the buffer a draw's uniforms go into, the samplers a map is read
 * through, the one white texel an unfilled slot reads, and the staging
 * copy that brings a texture's pixels home.
 *
 * None of it is a frame's. A frame's targets, its meshes and its
 * pipelines belong to whatever draws frames; these four belong to the
 * device, and two executors sharing a device share them.
 */

#include <Graphics/GraphicsEngine/interface/Buffer.h>
#include <Graphics/GraphicsEngine/interface/Sampler.h>
#include <Graphics/GraphicsEngine/interface/Texture.h>
#include <include/core/SkImage.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilgeometry/device/Device.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <cstddef>

namespace sigil::geometry::device {

namespace dg = Diligent;

/** THE COLOUR FORMAT every target on this device holds. One format for
 *  every resource is what lets two resources whose lives do not overlap
 *  be handed one texture. */
inline constexpr dg::TEXTURE_FORMAT kColorFormat = dg::TEX_FORMAT_RGBA8_UNORM;
inline constexpr dg::TEXTURE_FORMAT kDepthFormat = dg::TEX_FORMAT_D32_FLOAT;

/**
 * The device-wide resources, made once per device and shared by every
 * executor standing on it.
 */
class Resources {
 public:
  /** Makes the samplers and the white texel on @p device. */
  explicit Resources(Device& device);

  Device& device() const { return *m_device; }

  /** The uniform buffer, grown to hold at least @p bytes. Null when the
   *  device refused it. */
  dg::IBuffer* uniformBuffer(size_t bytes);

  /** The sampler @p filter asks for, wrapping outside the image when
   *  @p tile. */
  dg::ISampler* samplerFor(SkFilterMode filter, bool tile = false) const;

  /** …and ONE MORE, for a panorama, which none of the four above can
   *  read: an equirect map repeats in azimuth and clamps at the poles,
   *  so its two axes want different wraps, and its prefiltered levels
   *  are different images rather than a filtering aid, so it reads
   *  linearly ACROSS them as well as within one. */
  dg::ISampler* panoramaSampler() const { return m_panorama.RawPtr(); }

  /** What an unfilled sampled slot reads: one white texel, so a body
   *  multiplied by a map it was not given is the body. */
  dg::ITexture* white() const { return m_white.RawPtr(); }

  /** @p texture's pixels, read back through a staging texture of the
   *  texture's own size. Null when there is nothing to read. */
  sk_sp<SkImage> read(dg::ITexture* texture);

 private:
  Device* m_device = nullptr;
  /** Every draw's uniforms, discarded and rewritten per draw. */
  dg::RefCntAutoPtr<dg::IBuffer> m_uniforms;
  size_t m_uniformCapacity = 0;
  /** HOW A MAP IS READ BETWEEN TEXELS, one sampler per answer. A
   *  texture states which it wants and a body's draw picks; everything
   *  with no texture to ask — a target a post stage reads, the one white
   *  texel — takes the linear one. …and the same two for a map that
   *  repeats outside its coordinates, because wrapping is the sampler's
   *  answer and not the lookup's. */
  dg::RefCntAutoPtr<dg::ISampler> m_linear;
  dg::RefCntAutoPtr<dg::ISampler> m_nearest;
  dg::RefCntAutoPtr<dg::ISampler> m_linearTiled;
  dg::RefCntAutoPtr<dg::ISampler> m_nearestTiled;
  dg::RefCntAutoPtr<dg::ISampler> m_panorama;
  dg::RefCntAutoPtr<dg::ITexture> m_white;
};

}  // namespace sigil::geometry::device
