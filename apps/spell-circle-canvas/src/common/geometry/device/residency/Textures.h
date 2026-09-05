#pragma once

/** @file
 * MAPS RESIDENT ON THE DEVICE: what a material's texture is when a draw
 * on this device has to read it, and the prefiltered panorama and its
 * cosine convolution beside it.
 *
 * There are two ways a map gets here and the difference is the whole
 * point of the cache. Pixels that ALREADY stand on this very device are
 * wrapped where they are — nothing is copied, and a scene another
 * library painted into a texture on the shared device is sampled exactly
 * as it was painted. Everything else is brought over from host memory
 * once and held under the image it came from.
 *
 * It stands in the device feature because the crossing is the device's,
 * and because two executors sharing a device should share the crossing
 * rather than each making its own copy of every map.
 */

#include <Graphics/GraphicsEngine/interface/Texture.h>
#include <sigilgeometry/device/Device.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Texture.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <boost/container/map.hpp>
#include <cstdint>

namespace sigil::geometry::device {

/**
 * How many mip levels a map of @p width × @p height is uploaded with.
 *
 * A map is minified whenever the surface wearing it is smaller on screen
 * than the map is in texels, and a sampler with one level to read then
 * walks the texels of a shrinking triangle and answers a different one
 * every frame — which is what aliasing is. So a map wide enough to halve
 * carries the whole chain, and the device is asked to derive it.
 *
 * A MAP CAN BE TOO SMALL TO HAVE A CHAIN. Halving a single texel arrives
 * nowhere, so a flat colour handed over as one texel — which is how an
 * emissive tint or any other constant slot is spelled — is the whole
 * texture at its one level, and asking for none below it is not a
 * shortcut: a device handed a view with one level in it and told to fill
 * the levels beneath has nowhere to put them, and refuses.
 *
 * One for a map with no extent at all.
 */
int mapMipLevels(int width, int height);

/** A MAP ON THE DEVICE: either an image uploaded from host memory, or a
 *  texture someone else painted on THIS device, wrapped without a copy.
 *  `used` is the frame it was last drawn with, so a map no draw names
 *  any more is let go. */
struct SampledImage {
  Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
  uint64_t used = 0;
};

/**
 * The maps standing on one device, and what it takes to put one there.
 *
 * `endFrame()` closes the frame and lets go of what no draw has named
 * lately. The panoramas are not aged: a sky is expensive to prefilter
 * and a scene keeps the same one.
 */
class TextureResidency {
 public:
  explicit TextureResidency(Device& device);

  /** THE MAP @p map IS, on this device — wrapped where its pixels
   *  already stand, or brought over once and held. Null when the texture
   *  yields no image or the device refused it. */
  Diligent::ITexture* sample(const material::Texture& map);

  /** THE PANORAMA on the device: one texture whose levels are the map's
   *  prefiltered chain, uploaded once per map and kept.
   *
   *  A SKY IS NOT EIGHT BITS. The values above one are what make a sun a
   *  sun rather than a white disc the same brightness as the sky beside
   *  it, and they are what a reflection is mostly made of — so the
   *  chain is uploaded as half floats, which keep them and are
   *  filterable everywhere. Null when the map carries nothing or the
   *  device refused it. */
  Diligent::ITexture* environment(const material::EnvironmentMap& map);

  /** …and the cosine convolution beside it, one small texture a normal
   *  reads directly. */
  Diligent::ITexture* irradiance(const material::EnvironmentMap& map);

  /** Closes the frame: the maps no draw has named lately are released. */
  void endFrame();

 private:
  Device* m_device = nullptr;
  uint64_t m_frame = 0;
  /** Maps whose pixels already stand on this device, under the name the
   *  API gave them. Nothing is copied for one of these. */
  boost::container::map<uint64_t, SampledImage> m_wrapped;
  /** …and maps that had to be brought over, under the id of the image
   *  they were brought from. */
  boost::container::map<uint32_t, SampledImage> m_uploaded;
  /** Prefiltered panoramas, under the id of the panorama they were built
   *  from, and the cosine convolutions under the same key. */
  boost::container::map<uint32_t, SampledImage> m_environments;
  boost::container::map<uint32_t, SampledImage> m_irradiances;
};

}  // namespace sigil::geometry::device
