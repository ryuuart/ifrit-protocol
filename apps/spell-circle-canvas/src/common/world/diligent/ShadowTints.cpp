/** @file
 * The tint beside the shadow maps: what of a light a transmissive surface
 * lets through, which bodies tint it, and the nearest transmissive depth
 * and the multiplied colour drawn for every map the stage laid out.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Parameters.h>

#include <algorithm>
#include <cmath>

#include "GeometryPass.h"

namespace sigil::world::diligent {

TintTerms tintTermsOf(const material::Material& worn) {
  const material::Schema& fields = worn.recipe().parameters();
  const auto number = [&](std::string_view name, float otherwise) {
    const material::Field* field = fields.find(name);
    return field && field->floats == 1 ? worn.get<float>(name) : otherwise;
  };
  // Typed as a display shows it, and carried as light.
  const auto decoded = [](float code) {
    return code <= 0.04045f ? code / 12.92f
                            : std::pow((code + 0.055f) / 1.055f, 2.4f);
  };
  const auto colourOf = [&](std::string_view name) {
    const material::Field* field = fields.find(name);
    if (!field || field->floats != 4) return glm::vec3(1.0f);
    const material::Color c = worn.get<material::Color>(name);
    return glm::vec3(decoded(c.r), decoded(c.g), decoded(c.b));
  };
  TintTerms out;
  out.colour = colourOf("baseColor");
  out.weight = std::clamp(number("transmission", 1.0f), 0.0f, 1.0f);
  out.thickness = std::max(number("thickness", 0.0f), 0.0f);
  out.distance = std::max(number("attenuationDistance", 0.0f), 0.0f);
  out.attenuation = colourOf("attenuationColor");
  return out;
}

/** WHETHER @p body TINTS A LIGHT PASSING THROUGH IT: it says it casts, it
 *  is lit, it is laid over normally, and its surface transmits. */
bool tints(const Draw& body) {
  if (!body.mesh || !body.castShadow || !body.lit || !body.material)
    return false;
  if (!seesThrough(body, false)) return false;
  const Blending blending = blendingOf(body, body.baseColor, false);
  if (blending.blend != SkBlendMode::kSrcOver || blending.fades) return false;
  return statedLobes(*body.material).transmission;
}

/** THE TINT BESIDE THE SHADOW MAPS: for every map the stage laid out, the
 *  nearest transmissive body's depth, and the colour every transmissive
 *  body nearer the light than what blocks it lets through, multiplied
 *  together. Drawn whole in a frame that holds such a body and in which
 *  something it is a function of moved — a map the stage drew again
 *  (@p mapsDrawn), or a body that tints — and kept from the frame before
 *  otherwise; in no frame without such a body. Answers how many maps'
 *  tints it drew: every map's, or none. */
int64_t drawTints(Gpu& gpu, const View& view, const glm::mat4& cameraView,
                  int64_t mapsDrawn) {
  gpu.shadowTinted = false;
  if (gpu.shadows.empty() || !gpu.shadowAtlas) return 0;
  bool any = false;
  for (const Draw& body : view.draws) any = any || tints(body);
  if (!any || !gpu.shadowTintAt(gpu.shadowAtlasSide)) return 0;
  // WHAT THE TINT IS A FUNCTION OF besides the maps' depth: each map's
  // place and projection, and every body that tints — where it stands,
  // which triangles it is, its instances, the eye's turn where it faces
  // the eye, its surface and what that surface lets through, and the
  // moment a running surface is read at.
  Fold fold;
  for (const ShadowMap& map : gpu.shadows) {
    fold.add(map.viewProjection);
    fold.add(map.x);
    fold.add(map.y);
    fold.add(map.size);
  }
  for (size_t i = 0; i < view.draws.size(); ++i) {
    const Draw& body = view.draws[i];
    if (!tints(body)) continue;
    fold.add(i);
    fold.add(body.world);
    fold.add(body.geometry);
    fold.add(body.mesh);
    fold.add(body.mesh->positions.size());
    if (body.instances) {
      fold.all(body.instances->positions);
      fold.all(body.instances->orientations);
      fold.all(body.instances->scales);
    }
    if (body.facing != Facing::None) fold.add(cameraView);
    fold.add(body.material);
    fold.add(body.baseColor);
    const TintTerms terms = tintTermsOf(*body.material);
    fold.add(terms.colour);
    fold.add(terms.weight);
    fold.add(terms.thickness);
    fold.add(terms.distance);
    fold.add(terms.attenuation);
    if (body.material->isRunning()) fold.add(gpu.seconds);
  }
  if (mapsDrawn == 0 && fold.value == gpu.shadowTintFold &&
      gpu.shadowTintFoldTexture == gpu.shadowTint.RawPtr()) {
    gpu.shadowTinted = true;
    return 0;
  }
  const int64_t pendingBefore = gpu.programsPending;
  dg::IDeviceContext* context = gpu.device->context();
  const dg::Uint32 side = (dg::Uint32)gpu.shadowAtlasSide;
  const auto eachMap = [&](bool tint) {
    for (const ShadowMap& map : gpu.shadows) {
      dg::Viewport viewport;
      viewport.TopLeftX = (float)map.x;
      viewport.TopLeftY = (float)map.y;
      viewport.Width = viewport.Height = (float)map.size;
      viewport.MinDepth = 0.0f;
      viewport.MaxDepth = 1.0f;
      context->SetViewports(1, &viewport, side, side);
      const Collection& link = map.light->light.shadowLink;
      for (const Draw& body : view.draws) {
        if (!tints(body)) continue;
        if (!link.everything() && !link.contains(memberOf(body))) continue;
        Caster caster{map.viewProjection, false, tint};
        drawBody(gpu, map.viewProjection, cameraView,
                 Shape{body.geometry, body.mesh, body.instances, body.facing,
                       -1, body.primitiveColor},
                 body.world,
                 tint ? glm::vec4(tintTermsOf(*body.material).colour, 1.0f)
                      : body.baseColor,
                 tint ? body.material : nullptr, nullptr, {}, view.environment,
                 /*lit=*/false, Blending{}, /*cull=*/false, nullptr, nullptr,
                 &caster);
      }
    }
  };
  // THE NEAREST TRANSMISSIVE DEPTH, which says what stands behind one.
  dg::ITextureView* glass =
      gpu.shadowGlass->GetDefaultView(dg::TEXTURE_VIEW_DEPTH_STENCIL);
  context->SetRenderTargets(0, nullptr, glass,
                            dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->ClearDepthStencil(glass, dg::CLEAR_DEPTH_FLAG, 1.0f, 0,
                             dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  eachMap(false);
  // THE TINT, against the map's own depth, so a transmissive body behind
  // what blocks the light tints nothing.
  dg::ITextureView* tint =
      gpu.shadowTint->GetDefaultView(dg::TEXTURE_VIEW_RENDER_TARGET);
  dg::ITextureView* blockers =
      gpu.shadowAtlas->GetDefaultView(dg::TEXTURE_VIEW_DEPTH_STENCIL);
  context->SetRenderTargets(1, &tint, blockers,
                            dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float white[4] = {1, 1, 1, 1};
  context->ClearRenderTarget(tint, white,
                             dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  eachMap(true);
  gpu.shadowTinted = true;
  // A tint a body stood in for, or was left out of, is drawn again on the
  // next frame, whatever it is a function of.
  gpu.shadowTintFold =
      gpu.programsPending != pendingBefore ? ~fold.value : fold.value;
  gpu.shadowTintFoldTexture = gpu.shadowTint.RawPtr();
  return (int64_t)gpu.shadows.size();
}

}  // namespace sigil::world::diligent
