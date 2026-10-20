/** @file
 * One body, drawn: its program and pipeline chosen, its uniforms written,
 * every sampled slot bound by name — the material's maps, the frame's sky,
 * the scene behind it, the shadow atlas and the tint beside it — and its
 * mesh drawn once per instance in one instanced draw.
 */

#include <sigilmaterial/advanced/Combine.h>
#include <sigilmaterial/advanced/Program.h>  // reportOnce
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilworld/advanced/Diligent.h>
#include <sigilworld/advanced/Skia.h>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "GeometryPass.h"

namespace sigil::world::diligent {

namespace {

/** The scaffold's sampled slot and the placement it is read at — the
 *  two names the map a body is dressed with arrives under. */
constexpr std::string_view kMapSlot = "uBaseColorMap";
constexpr std::string_view kMapUv = "uBaseColorMapUv";

/** WHAT A SLOT HOLDS when its texture does not say: a surface's normal,
 *  roughness, metalness, occlusion, opacity, clear-coat, transmission,
 *  thickness, distortion and volume maps are data,
 *  and keep their smaller copies because a surface's map is shrunk on
 *  screen like any other; its base colour and emission are colour; and
 *  anything else — the slots a flipbook or a flow map brought with it — is
 *  colour unless its texture says otherwise, with smaller copies only for
 *  colour. */
device::MapForm formFor(std::string_view slot, const material::Texture& map) {
  namespace surface = material::surface;
  const bool surfaceData =
      slot == surface::kNormalSlot || slot == surface::kRoughnessSlot ||
      slot == surface::kMetallicSlot || slot == surface::kOcclusionSlot ||
      slot == surface::kOpacitySlot || slot == surface::kClearcoatSlot ||
      slot == surface::kClearcoatRoughnessSlot ||
      slot == surface::kClearcoatNormalSlot ||
      slot == surface::kTransmissionSlot || slot == surface::kThicknessSlot ||
      slot == surface::kDistortionSlot || slot == surface::kVolumeSlot;
  device::MapForm form;
  form.colorSpace = map.colorSpace().value_or(
      surfaceData ? media::ColorSpace::Data : media::ColorSpace::Srgb);
  form.mipmaps = map.mipmaps().value_or(
      surfaceData || form.colorSpace == media::ColorSpace::Srgb);
  form.filtered = map.sampling() == material::Sampling::Linear;
  return form;
}

/** Whether @p slot is one of the surface program's own — read through
 *  the sampler the base-colour map decides — rather than one a child
 *  material brought, which is read the way its own texture says. */
bool surfaceSlot(std::string_view slot) {
  namespace surface = material::surface;
  for (std::string_view own :
       {surface::kBaseColorSlot, surface::kNormalSlot, surface::kRoughnessSlot,
        surface::kMetallicSlot, surface::kOcclusionSlot, surface::kEmissiveSlot,
        surface::kOpacitySlot, surface::kClearcoatSlot,
        surface::kClearcoatRoughnessSlot, surface::kClearcoatNormalSlot})
    if (slot == own) return true;
  return false;
}

/** The scaffold's slot the opaque depth is read through. */
constexpr std::string_view kSceneDepthSlot = "uSceneDepth";
/** …and the one the scene behind a surface that sees through itself is. */
constexpr std::string_view kSceneColorSlot = "uSceneColor";
/** …and the one every light's shadow map is read through. */
constexpr std::string_view kShadowSlot = "uShadowAtlas";
/** …and the one the same atlas is compared through. */
constexpr std::string_view kShadowCompareSlot = "uShadowCompare";

}  // namespace

/** One body, drawn. @p map is the texture it is dressed with, or null;
 *  @p sceneDepth the copy of the opaque depth, or null; @p sceneColor the
 *  copy of the scene a body that sees through itself reads, or null;
 *  @p caster, where the body is drawn into a shadow map rather than the
 *  picture; @p receives, whether the shadows the pass drew darken it. */
void drawBody(Gpu& gpu, const glm::mat4& viewProj, const glm::mat4& view,
              const Shape& shape, const glm::mat4& model, glm::vec4 baseColor,
              const material::Material* worn, const material::Texture* map,
              std::span<const PlacedLight* const> lights,
              const Environment& sky, bool lit, const Blending& blending,
              bool cull, dg::ITexture* sceneDepth, dg::ITexture* sceneColor,
              const Caster* caster, bool receives) {
  const MeshBuffers* buffers =
      gpu.meshes.upload(shape.artefact, *shape.mesh, shape.primitiveColor);
  if (!buffers) return;
  dg::IBuffer* indexBuffer = buffers->indices;
  uint32_t indexCount = buffers->indexCount;
  if (shape.slot >= 0) {
    indexBuffer = gpu.meshes.slotIndices(shape.artefact, *shape.mesh,
                                         shape.primitiveColor,
                                         (size_t)shape.slot, &indexCount);
    if (!indexBuffer) return;
  }
  const device::InstanceBuffers* instances =
      shape.instances ? gpu.meshes.instances(shape.artefact, *shape.instances)
                      : gpu.meshes.single();
  if (!instances) return;
  // A CHILD MATERIAL IN A SLOT is written into the body, so the device
  // compiles one program that plays it; a material holding none is
  // itself.
  const std::optional<material::Material> inlined =
      worn ? std::optional<material::Material>(material::inlined(*worn))
           : std::nullopt;
  const material::Material* material = inlined ? &*inlined : nullptr;
  // INTO A SHADOW MAP, a body whose surface does not decide where it
  // covers is drawn with the one depth-only program; one whose surface
  // does is asked, through its own program's shadow build, only that.
  const uint32_t reaching =
      lit && !caster ? lightVariant(gpu, lights, receives) : 0u;
  Surface surface;
  if (caster && caster->tint)
    surface.program = &tintScaffold();
  else if (caster && !caster->coverage)
    surface.program = &shadowScaffold(/*coverage=*/false);
  else
    surface = surfaceOf(material, lit, gpu.seconds, caster != nullptr,
                        gpu.standIns, reaching);
  if (!surface.program || surface.program->empty()) return;
  PipelineKey key = bodyKey(*surface.program, blending, cull, caster);
  // A LIGHT KIND ENTERING, told from a body opening by the light bits its
  // lit program was last drawn whole with.
  const material::Recipe* litRecipe =
      lit && !caster && material ? &material->recipe() : nullptr;
  const auto drawn =
      litRecipe ? gpu.litWith.find(litRecipe) : gpu.litWith.end();
  const bool entering = drawn != gpu.litWith.end() && drawn->second != reaching;
  // THE PIPELINE, built on the device's workers. A frame that waits waits
  // for it. A live frame takes it only once it is built: until then the
  // body stands in, through the stand-in's pipeline every runtime starts
  // ahead of its first frame, and where that one too is still building
  // the body is left out of the frame; either way it is counted as
  // waiting. The one body a live frame waits for is one whose program a
  // light kind entering made it wait for, so that light costs at most one
  // frame of stand-ins.
  const Pipeline* pipeline = nullptr;
  if (!gpu.standIns || (entering && !surface.standIn)) {
    pipeline = gpu.pipelines.pipeline(key);
  } else {
    pipeline = gpu.pipelines.ready(key);
    if (!pipeline && !surface.standIn && !(caster && caster->tint)) {
      surface = Surface{};
      surface.standIn = true;
      surface.lit = false;
      surface.program =
          caster ? &shadowScaffold(/*coverage=*/true) : &scaffold(false);
      key = bodyKey(*surface.program, blending, cull, caster);
      pipeline = gpu.pipelines.ready(key);
    }
    if (!pipeline) {
      ++gpu.programsPending;
      return;
    }
  }
  if (!pipeline) return;
  // The compile a light kind entering made the stand-in start is waited
  // for before the next geometry pass draws.
  if (litRecipe) {
    if (!surface.standIn)
      gpu.litWith[litRecipe] = reaching;
    else if (entering)
      gpu.awaitedPrograms.emplace_back(material->recipePointer(),
                                       kVariantLit | reaching);
  }
  if (surface.standIn) {
    ++gpu.programsPending;
    lit = false;
  }
  if (caster) lit = false;

  // THE PANORAMA, uploaded once per map and kept. Its level count is
  // what says there is a sky at all, so it is asked for before the
  // uniforms are written.
  dg::ITexture* panorama =
      lit ? prefilteredPanorama(gpu, gpu.maps.environment(sky.map)) : nullptr;
  dg::ITexture* panoramaNext =
      lit ? prefilteredPanorama(gpu, gpu.maps.environment(sky.next)) : nullptr;
  dg::ITexture* lobe = lit ? gpu.maps.irradiance(sky.map) : nullptr;
  dg::ITexture* lobeNext = lit ? gpu.maps.irradiance(sky.next) : nullptr;
  if (!panoramaNext) panoramaNext = panorama;
  if (!lobeNext) lobeNext = lobe;
  const int levels = panorama ? (int)panorama->GetDesc().MipLevels : 0;

  material::slang::Uniforms uniforms(*surface.program);
  writeScaffold(uniforms, *surface.program, gpu,
                caster ? caster->viewProjection : viewProj, view, model,
                baseColor, lights, sky, levels, lit, sceneDepth != nullptr,
                receives);
  writeInstancing(uniforms, view, shape.instances ? shape.facing : Facing::None,
                  *instances);
  writeMaterial(uniforms, surface);
  if (caster && caster->tint && material) {
    const TintTerms terms = tintTermsOf(*material);
    uniforms.set("uTintVolume", terms.weight, terms.thickness, terms.distance,
                 0.0f);
    uniforms.set("uTintAttenuation", terms.attenuation.r, terms.attenuation.g,
                 terms.attenuation.b, 1.0f);
  }
  // THE SCENE BEHIND, for a body that sees through itself: its size, its
  // levels, and the projection that carries a point onto it.
  uniforms.set("uSceneColorDials",
               gpu.extent.width() ? 1.0f / (float)gpu.extent.width() : 0.0f,
               gpu.extent.height() ? 1.0f / (float)gpu.extent.height() : 0.0f,
               sceneColor ? (float)sceneColor->GetDesc().MipLevels : 0.0f,
               sceneColor ? 1.0f : 0.0f);
  uniforms.set("uProjection", viewProj * glm::inverse(view));

  // THE SAMPLED SLOTS, bound by NAME rather than by position, because a
  // material's body declares slots of its own and the compiler is free
  // to report them in whatever order it laid them out. An unbound slot
  // reads one white texel, which is the neutral for every map a scalar
  // multiplies — and the one value a tangent-space normal cannot mean,
  // so a body can tell an undressed slot from a dressed one exactly.
  std::vector<dg::ITexture*> textures(surface.program->textures.size(),
                                      nullptr);
  std::vector<dg::ISampler*> samplers(surface.program->textures.size(),
                                      nullptr);
  std::vector<dg::ITextureView*> views(surface.program->textures.size(),
                                       nullptr);
  // A STACK RUNNING ITS OWN BODY OWNS EVERY MAP IN IT. The frame extracts
  // the map of the material at the bottom of a stack, because that is
  // what a tier with no compiler can answer with; a composed body samples
  // both operands' maps itself, through slots of its own, so handing the
  // scaffold the bottom's map as well would land it a second time and
  // over the whole face rather than where the mask says.
  if (worn && surface.recipe && material::stackDepth(*worn) > 0) map = nullptr;
  const Sampling sampling = map ? samplingOf(*map) : Sampling{};
  uniforms.set(kMapUv, mapMatrix(toSk(sampling.uv)));
  for (size_t i = 0; i < textures.size(); ++i) {
    const std::string& slot = surface.program->textures[i];
    // THE BASE COLOUR MAP IS THE SCAFFOLD'S, and only the scaffold's. It
    // multiplies whatever colour the body answers, so the body's own base
    // colour slot is left reading white and the map lands once.
    if (slot == kMapSlot) {
      // Asked of the device and not of the host image: a source whose
      // pixels stand on this device has no host image to check for.
      if (map)
        textures[i] = gpu.maps.sample(
            *map, formFor(material::surface::kBaseColorSlot, *map),
            std::chrono::duration<double>(gpu.seconds));
      continue;
    }
    // …the scene behind is the frame's, read between its texels and
    // across its levels…
    if (slot == kSceneColorSlot) {
      textures[i] = sceneColor;
      samplers[i] = gpu.shared.samplerFor(SkFilterMode::kLinear, false);
      continue;
    }
    // …the scene's depth is the frame's, read texel by texel…
    if (slot == kSceneDepthSlot) {
      textures[i] = sceneDepth;
      if (sceneDepth) views[i] = gpu.opaqueDepthView;
      samplers[i] = gpu.shared.samplerFor(SkFilterMode::kNearest, false);
      continue;
    }
    // …and the four panorama slots are the FRAME'S, not any material's:
    // a sky is a property of the scene every body in it reads, so it is
    // bound beside the lights rather than dressed onto a surface.
    if (slot == kEnvironmentSlots[0]) {
      textures[i] = panorama;
      continue;
    }
    if (slot == kEnvironmentSlots[1]) {
      textures[i] = panoramaNext;
      continue;
    }
    if (slot == kEnvironmentSlots[2]) {
      textures[i] = lobe;
      continue;
    }
    if (slot == kEnvironmentSlots[3]) {
      textures[i] = lobeNext;
      continue;
    }
    if (slot == kBrdfSlot) {
      textures[i] = gpu.maps.sample(brdfTable());
      continue;
    }
    // …and so is what transmissive bodies let through of each light,
    // read texel by texel beside it…
    if (slot == "uShadowTint") {
      if (gpu.shadowTinted) textures[i] = gpu.shadowTint;
      samplers[i] = gpu.shared.samplerFor(SkFilterMode::kNearest, false);
      continue;
    }
    if (slot == "uShadowGlass") {
      if (gpu.shadowTinted) {
        textures[i] = gpu.shadowGlass;
        views[i] = gpu.shadowGlassView;
      }
      samplers[i] = gpu.shared.samplerFor(SkFilterMode::kNearest, false);
      continue;
    }
    // …and so is the shadow atlas, read texel by texel through a view of
    // its depth format.
    if (slot == kShadowSlot) {
      if (!gpu.shadows.empty() && gpu.shadowAtlas) {
        textures[i] = gpu.shadowAtlas;
        views[i] = gpu.shadowAtlasView;
      }
      samplers[i] = gpu.shared.samplerFor(SkFilterMode::kNearest, false);
      continue;
    }
    // …and through the comparison sampler, four depths compared in one
    // gather. A comparison reads only a depth texture, so a pass with no
    // map binds the atlas as it stands, or a one-texel one — which the
    // dials say nothing reads.
    if (slot == kShadowCompareSlot) {
      if (gpu.shadowAtlas || gpu.shadowAtlasAt(1)) {
        textures[i] = gpu.shadowAtlas;
        views[i] = gpu.shadowCompareView;
      }
      samplers[i] = gpu.shared.comparisonSampler();
      continue;
    }
    // …and a transmissive body's own transmission, thickness, volume and
    // attenuation maps, which its tint beside the shadow maps reads.
    if (slot == "uTintTransmissionMap" || slot == "uTintThicknessMap" ||
        slot == "uTintVolumeMap" || slot == "uTintAttenuationMap") {
      const std::string_view own =
          slot == "uTintTransmissionMap" ? material::surface::kTransmissionSlot
          : slot == "uTintThicknessMap"  ? material::surface::kThicknessSlot
          : slot == "uTintVolumeMap"     ? material::surface::kVolumeSlot
                                         : material::surface::kAttenuationSlot;
      if (material)
        if (const material::Texture* map =
                material::surface::map(*material, own))
          textures[i] =
              gpu.maps.sample(*map, formFor(own, *map),
                              std::chrono::duration<double>(gpu.seconds));
      samplers[i] = gpu.shared.samplerFor(SkFilterMode::kLinear, true);
      continue;
    }
    if (!material) continue;
    // …and every OTHER slot is the material's own. `material::surface::map`
    // answers null for a slot still holding the neutral dressing a
    // surface is built with, which is what keeps an undressed body
    // reading the one white texel rather than a map that says nothing.
    const material::Texture* own = material::surface::map(*material, slot);
    if (own) {
      if (own != map)
        textures[i] =
            gpu.maps.sample(*own, formFor(slot, *own),
                            std::chrono::duration<double>(gpu.seconds));
      // A slot a child material brought is read the way its own texture
      // asks — a flipbook's sheet held at its edge, a flow map repeating.
      if (!surfaceSlot(slot)) {
        const Sampling read = samplingOf(*own);
        samplers[i] = gpu.shared.samplerFor(toSk(read.filter), read.tile);
      }
      continue;
    }
    // A slot filled with a MATERIAL with no body this device can compile
    // has nothing to sample, so it reads the same white texel — and says
    // so, once per slot, since the picture alone would show a flat
    // surface and no reason for it.
    if (material->slot(slot))
      material::reportOnce(
          "world.diligent.slot:" + slot,
          "surface slot \"" + slot +
              "\" holds a material with no body the device can compile; "
              "the slot reads as empty. Supply an image texture there, or "
              "a material with a Slang body.");
  }

  dg::IDeviceContext* context = gpu.device->context();
  context->SetPipelineState(pipeline->state);
  // The WRAP is the map's own, not a default: one wrap serves both axes
  // and every surface slot bound here, and clamping an axis that was
  // asked to repeat drags one edge's texels across the whole face.
  device::bindDraw(gpu.shared, *pipeline, *surface.program, uniforms, textures,
                   toSk(sampling.filter), sampling.tile, &isEnvironmentSlot,
                   samplers, views);
  dg::IBuffer* streams[2] = {buffers->vertices, instances->instances};
  const dg::Uint64 offsets[2] = {0, 0};
  context->SetVertexBuffers(0, 2, streams, offsets,
                            dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                            dg::SET_VERTEX_BUFFERS_FLAG_RESET);
  context->SetIndexBuffer(indexBuffer, 0,
                          dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  dg::DrawIndexedAttribs draw;
  draw.NumIndices = indexCount;
  draw.NumInstances = instances->count;
  draw.IndexType = dg::VT_UINT32;
  draw.Flags = dg::DRAW_FLAG_VERIFY_ALL;
  context->DrawIndexed(draw);
}

}  // namespace sigil::world::diligent
