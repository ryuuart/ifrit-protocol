#pragma once

/** @file
 * @ingroup material-surface
 *
 * A MATERIAL NO DEVICE PROGRAM PLAYS, AS THE PIXELS OF A MAP: the source
 * `surface::lower` fills a slot with when the material in it has no Slang
 * body — a gradient, a colour stack, a base with layers, a program only
 * Skia runs. It is painted by Skia over a square that stands for the unit
 * square a body's texture coordinates span, kept, and painted again at a
 * new time only while the material runs. A renderer standing on a device
 * hands it that device, and from then on it is painted there, by Graphite,
 * into a texture of the device's own that the renderer samples where it
 * stands.
 *
 *     const auto* painted = map.source().as<surface::PaintedMap>();
 *     if (painted) painted->useDevice(*device.gpu(), *device.graphite());
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmedia/core/Frame.h>

#include <chrono>
#include <cstdint>
#include <glm/vec2.hpp>
#include <memory>

namespace sigil::core::hardware {
class GpuDevice;
}  // namespace sigil::core::hardware

namespace sigil::skia {
class GraphiteContext;
}  // namespace sigil::skia

namespace sigil::material::surface {

/** A MATERIAL PAINTED INTO A SQUARE MAP, as a pixel source. Copies share
 *  what was painted, and two equal materials are one source, so a
 *  renderer holds one map for both. */
class PaintedMap {
 public:
  /** HOW MANY PIXELS ACROSS a material is painted at: the square stands
   *  for the unit square a body's texture coordinates span, so a material
   *  laid out in box units covers the body once. A plain colour is one
   *  pixel. */
  static constexpr int kSide = 512;

  /** @p material as a map. A map a data slot reads — numbers rather than
   *  colour — is @p straightAlpha: its values are read with alpha
   *  straight, as a data map is uploaded, so where it is painted on a
   *  device its texels are stored that way. */
  explicit PaintedMap(Material material, bool straightAlpha = false);

  /** The map at @p time: standing on the device once `useDevice` was
   *  handed one, as a picture painted on the host otherwise. Painted
   *  again only when the material runs and @p time is new. */
  media::Frame frameAt(std::chrono::duration<double> time) const;
  bool isRunning() const;
  glm::ivec2 size() const;
  /** How many paintings have stood, on the host and on the device. */
  uint64_t revision() const;
  bool operator==(const PaintedMap& other) const;

  /** The material the map is painted from. */
  const Material& material() const { return m_material; }

  /** PAINTS ON @p device FROM HERE ON, through @p context — Graphite on
   *  that device — into a texture the device makes, with the chain its
   *  size carries for whoever samples it to fill. Handed the device it
   *  already paints on, nothing changes. False where the device makes no
   *  texture the context can paint, which leaves the map painting on the
   *  host. */
  bool useDevice(core::hardware::GpuDevice& device,
                 sigil::skia::GraphiteContext& context) const;

  /** How many times the map was painted by Skia's CPU rasteriser: every
   *  painting before a device was handed it, and every picture a reader
   *  with no recorder of the painting context asked for after. */
  uint64_t hostPaintings() const;

 private:
  struct Kept;
  Material m_material;
  bool m_straightAlpha;
  int m_side;
  std::shared_ptr<Kept> m_kept;
};

}  // namespace sigil::material::surface
