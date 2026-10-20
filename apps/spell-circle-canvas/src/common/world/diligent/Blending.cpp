/** @file
 * How a body composites: the blend state its surface's mode is on this
 * device, whether it writes depth or fades into the scene, and so which
 * phase of the frame it is drawn in — laid over, seen through, or blended.
 */

#include <sigilmaterial/advanced/Program.h>  // reportOnce
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/surface/Surface.h>

#include <string>

#include "GeometryPass.h"

namespace sigil::world::diligent {

namespace {

/** THE BLEND STATE @p mode IS on this device. The modes a blend state can
 *  express over premultiplied light are taken as they are; every other
 *  one would have to read the target it draws into, and is laid over
 *  instead, said once. */
SkBlendMode deviceBlend(material::BlendMode mode) {
  switch (mode) {
    case material::BlendMode::Normal:
      return SkBlendMode::kSrcOver;
    case material::BlendMode::PlusLighter:
      return SkBlendMode::kPlus;
    case material::BlendMode::Multiply:
      return SkBlendMode::kMultiply;
    case material::BlendMode::Screen:
      return SkBlendMode::kScreen;
    case material::BlendMode::Modulate:
      return SkBlendMode::kModulate;
    default:
      material::reportOnce(
          std::string("world.diligent.blend:") +
              std::string(material::name(mode)),
          "a body composited \"" + std::string(material::name(mode)) +
              "\" would need to read the picture it is drawn over, which a "
              "blend state cannot; it is laid over normally");
      return SkBlendMode::kSrcOver;
  }
}

}  // namespace

/** HOW @p body COMPOSITES, off its material: the blend its surface
 *  states, and depth written by an opaque body composited normally and by
 *  nothing else unless the surface says. A flat colour drawn over the body
 *  — a coverage, a variant — composites by its own alpha. */
Blending blendingOf(const Draw& body, glm::vec4 colour, bool flat) {
  Blending out;
  const material::surface::Compositing stated =
      !flat && body.material ? material::surface::compositing(*body.material)
                             : material::surface::Compositing{};
  out.blend = deviceBlend(stated.blend);
  const bool opaque = stated.blend == material::BlendMode::Normal &&
                      colour.a >= 1.0f && stated.depthFade <= 0.0f;
  out.depthWrite = stated.depthWrite.value_or(opaque);
  out.fades = stated.depthFade > 0.0f;
  return out;
}

/** Whether @p body draws in the second phase, over every body laid over
 *  normally: one that does not composite normally, fades into the scene,
 *  or states that it writes no depth. */
bool blended(const Draw& body, const Blending& blending, bool flat) {
  const material::surface::Compositing stated =
      !flat && body.material ? material::surface::compositing(*body.material)
                             : material::surface::Compositing{};
  return blending.blend != SkBlendMode::kSrcOver || blending.fades ||
         stated.depthWrite == false;
}

/** Whether @p body's surface sees through itself: it transmits or
 *  distorts, and so reads the copy of the scene behind it. A flat colour
 *  drawn over the body never does. */
bool seesThrough(const Draw& body, bool flat) {
  if (flat || !body.material) return false;
  const material::Lobes stated = statedLobes(*body.material);
  return stated.transmission || stated.distortion;
}

}  // namespace sigil::world::diligent
