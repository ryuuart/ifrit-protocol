/** @file
 * Which program a body is drawn with: its material's own, specialised on
 * the kinds of light that reach it, or — while that program compiles, or
 * where the material has none — the scaffold standing in for it.
 */

#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/core/Material.h>
#include <sigilworld/advanced/Diligent.h>
#include <sigilworld/advanced/Light.h>

#include <algorithm>
#include <cstdint>
#include <span>

#include "GeometryPass.h"

namespace sigil::world::diligent {

/** THE KINDS OF LIGHT a lit program is specialised on, of the first
 *  sixteen of @p lights — the ones the scaffold is written with: whether
 *  one of them has a size, and, where the body reads the pass's shadows,
 *  whether one of them is read through a directional light's cascades or
 *  a point light's six faces. The tests are the ones the scaffold makes
 *  of the same records, so a program built without a kind is never drawn
 *  under it. */
uint32_t lightVariant(const Gpu& gpu,
                      std::span<const PlacedLight* const> lights,
                      bool receives) {
  uint32_t bits = 0u;
  const size_t count = std::min(lights.size(), kLights);
  for (size_t i = 0; i < count; ++i) {
    const LightKind kind = lights[i]->light.kind;
    if (kind == LightKind::Rect || kind == LightKind::Sphere ||
        kind == LightKind::Disk)
      bits |= kVariantAreaLights;
  }
  // A BODY NO CASTING LIGHT'S MAP CAN REACH is built without the shadow
  // terms: no light in the frame casts, or the body receives no shadow.
  if (!receives || gpu.shadows.empty() || gpu.shadowAtlasSide <= 0)
    return bits | kVariantUnshadowed;
  for (size_t i = 0; i < count; ++i)
    for (const ShadowMap& map : gpu.shadows)
      if (map.light == lights[i]) {
        if (map.ends.x > 0.0f)
          bits |= kVariantCascadedShadows;
        else if (map.cube)
          bits |= kVariantPointShadows;
        if (gpu.shadowTinted) bits |= kVariantTintedShadows;
        break;
      }
  return bits;
}

/** Whether @p material's program for @p variant can be had without
 *  waiting — held, or not a compile at all — starting it on a worker when
 *  it cannot. */
bool programReady(const material::Material& material, uint32_t variant) {
  return material::ProgramCache::shared().prepare(material.recipePointer(),
                                                  material::Target::Slang,
                                                  material::Variant{variant});
}

/** THE STAND-IN, with @p standIns: a body whose program is still
 *  compiling is drawn in its base colour, unlit — or, into a shadow map,
 *  through the depth scaffold that reads that colour's coverage — and is
 *  drawn whole on the first frame after the program arrives. */
Surface surfaceOf(const material::Material* material, bool lit, double seconds,
                  bool shadow, bool standIns, uint32_t lights) {
  Surface out;
  out.lit = lit && !shadow;
  const auto fallback = [&] {
    return shadow ? &shadowScaffold(/*coverage=*/true)
                  : &scaffold(out.lit, lights);
  };
  if (!material) {
    out.program = fallback();
    return out;
  }
  const material::Variant variant{shadow ? kVariantShadow
                                         : (lit ? kVariantLit | lights : 0u)};
  if (standIns && !programReady(*material, variant.bits)) {
    out.standIn = true;
    out.lit = false;
    out.program =
        shadow ? &shadowScaffold(/*coverage=*/true) : &scaffold(false);
    return out;
  }
  // THE FRAME'S CLOCK reaches the body: a material that reads time — a
  // flipbook, a flow — is shown at the time the frame was described at.
  material::FrameData frame;
  frame.seconds = seconds;
  out.upload = material->resolve(material::Target::Slang, frame, variant);
  const auto* program =
      out.upload.program
          ? out.upload.program->as<material::slang::SlangProgram>()
          : nullptr;
  if (!program) {
    // The cache has already reported the recipe and the target; the body
    // it would have painted is drawn in the colour the frame extracted,
    // which is what the tier with no compiler answers with too.
    out.program = fallback();
    return out;
  }
  out.program = &program->compiled();
  out.recipe = &material->recipe();
  return out;
}

}  // namespace sigil::world::diligent
