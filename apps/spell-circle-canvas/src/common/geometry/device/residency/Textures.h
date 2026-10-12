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
#include <include/core/SkImage.h>
#include <sigilgeometry/device/Device.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Texture.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <boost/container/map.hpp>
#include <chrono>
#include <cstdint>
#include <vector>

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

/** HOW A MAP IS PUT ON THE DEVICE, decided by whoever binds it from what
 *  the texture states and what the slot it fills is for.
 *
 *  COLOUR is uploaded as eight bits a channel with its alpha multiplied
 *  in. DATA is uploaded exactly: its alpha kept out of the other three
 *  channels, no colour conversion, and at the image's own precision —
 *  floating point as floating point, sixteen bits as sixteen — save that
 *  thirty-two-bit floats a device cannot filter are uploaded as half
 *  floats when the map is to be read between its texels. */
struct MapForm {
  media::ColorSpace colorSpace = media::ColorSpace::Srgb;
  /** Whether the smaller copies are made. */
  bool mipmaps = true;
  /** Whether the map is read between its texels, which decides only
   *  whether a format the device cannot filter may be used. */
  bool filtered = true;
  bool operator==(const MapForm&) const = default;
};

/** A MAP ON THE DEVICE: either an image uploaded from host memory, or a
 *  texture someone else painted on THIS device, wrapped without a copy.
 *  `used` is the frame it was last drawn with, so a map no draw names
 *  any more is let go. */
struct SampledImage {
  Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
  uint64_t used = 0;
};

/** A PAGE WRAPPED WHERE IT STANDS, in its own format, and the chain a
 *  body wearing it at a distance reads: the page's level zero copied
 *  under levels the device derives, refilled on the first draw of every
 *  frame that wears it, since a page may be painted again between two
 *  frames and nothing says when. `filled` is the frame it was last filled
 *  on; `used` the frame it was last drawn with. */
struct WrappedImage {
  Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
  Diligent::RefCntAutoPtr<Diligent::ITexture> chain;
  uint64_t used = 0;
  uint64_t filled = UINT64_MAX;
};

/**
 * The maps standing on one device, and what it takes to put one there.
 *
 * `endFrame()` closes the frame and lets go of what no draw has named
 * lately — the panoramas among them, on the same beat as everything
 * else. A prefiltered sky is the largest thing here, and a residency
 * that kept every one a scene ever sampled would grow for as long as the
 * scene ran.
 */
class TextureResidency {
 public:
  explicit TextureResidency(Device& device);

  /** THE MAP @p map IS AT @p time, on this device — wrapped where its
   *  pixels already stand, or brought over once in @p form and held. Each
   *  is held under the image the source answers at that time, so a video
   *  or an animated image crosses once per frame of its own and a still
   *  crosses once. Null when the texture yields no image or the device
   *  refused it. One image put on the device in two forms is two
   *  textures.
   *
   *  A WRAP IS DESCRIBED AS THE PAGE IS: in the format the page was made
   *  in, so a half-float page keeps the values above one it holds. Where
   *  @p form asks for the smaller copies and the page is wide enough to
   *  halve, what a draw samples is the page's chain — its level zero
   *  copied on the device under the levels the device derives, once a
   *  frame — so a body wearing a page at a distance reads a level its
   *  size calls for rather than aliasing across level zero. Nothing
   *  crosses through host memory for either. */
  Diligent::ITexture* sample(const material::Texture& map,
                             const MapForm& form = {},
                             std::chrono::duration<double> time = {});

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

  /** THE SAME TWO for a caller holding a panorama as images rather than
   *  as a map: @p levels is the prefiltered chain, sharpest first, and
   *  @p lobe the cosine convolution. Each is held under the id of the
   *  image it was built from, which is the same key the two above use,
   *  so a sky reaching this residency both ways crosses once. */
  Diligent::ITexture* panorama(const std::vector<sk_sp<SkImage>>& levels);
  Diligent::ITexture* convolution(const sk_sp<SkImage>& lobe);

  /** Closes the frame: the maps no draw has named lately are released. */
  void endFrame();

 private:
  /** The chain @p held is worn through, filled for this frame; null when
   *  the device refused it. */
  Diligent::ITexture* chainOf(WrappedImage& held);

  Device* m_device = nullptr;
  uint64_t m_frame = 0;
  /** Maps whose pixels already stand on this device, under the name the
   *  API gave them. Nothing crosses through host memory for one of
   *  these. */
  boost::container::map<uint64_t, WrappedImage> m_wrapped;
  /** …and maps that had to be brought over, under the id of the image
   *  they were brought from and the form they were brought in. */
  boost::container::map<uint64_t, SampledImage> m_uploaded;
  /** Prefiltered panoramas, under the id of the panorama they were built
   *  from, and the cosine convolutions under the same key. */
  boost::container::map<uint32_t, SampledImage> m_environments;
  boost::container::map<uint32_t, SampledImage> m_irradiances;
};

}  // namespace sigil::geometry::device
