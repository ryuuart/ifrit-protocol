/** @file
 * A geometry pass on the device: the bodies the pass's realisation
 * leaves it, rasterised depth-tested into a device texture from a
 * pipeline built out of each material's own Slang body; the stamps of
 * the point sets it reads; and the coverage a masked pass downstream
 * reads its selection from.
 *
 * EVERY BODY IS ONE INSTANCED DRAW: its mesh once, and a second buffer
 * stepping once per instance — instanced geometry's own instances, or one
 * neutral instance for a body drawn once. The copies of one body are
 * drawn in the order its instances stand in; they are not sorted among
 * themselves.
 *
 * THE ORDER: LAID OVER FIRST, THEN SEEN THROUGH, THEN BLENDED. Bodies
 * arrive sorted back to front by where each one's own origin stands from
 * the camera. Every body laid over normally is drawn first, in that order
 * — an opaque one testing and writing depth, a translucent one testing it
 * only. Then, in a frame that holds a body that SEES THROUGH ITSELF — one
 * whose surface transmits or distorts — the lit colour and the depth are
 * copied, the colour with its chain of blurrier levels, and every such
 * body is drawn back to front reading that one copy: so a transmissive
 * body shows the opaque scene behind it, and never another transmissive
 * body, which is one layer of refraction. Last, every BLENDED one — a
 * surface that adds, multiplies or screens, one that says it writes no
 * depth, one that fades into the scene — back to front over all of them,
 * testing depth and writing it only where its material says; so a glow is
 * not seen through glass standing in front of it. A frame with nothing
 * that sees through itself copies nothing and draws exactly the two
 * phases it would without the middle one. So a blended body is hidden by
 * whatever opaque body stands in front of it, including a card whose texture is
 * transparent in places but which writes depth everywhere it covers: such a
 * card states that it writes no depth. The sort is per body, not per triangle:
 * two blended bodies that pass through each other, or one body whose own
 * triangles overlap, are drawn in whatever order their origins and their
 * triangles give, which an additive surface does not mind and a translucent one
 * laid over may.
 *
 * Every body writes LIGHT into a target holding light; the camera's
 * reading is the final stage's.
 */

#include <include/core/SkBitmap.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/advanced/Combine.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/advanced/Program.h>  // reportOnce
#include <sigilmaterial/advanced/Terms.h>
#include <sigilmaterial/core/Instance.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilworld/advanced/Diligent.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/diligent/Runtime.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/trigonometric.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Gpu.h"

namespace sigil::world::diligent {

namespace {

/** The four panorama slots the scaffold declares, and the sampler they
 *  are read with. Named here because the binding has to tell them from
 *  a material's own slots: an equirect map wants a wrap of its own on
 *  each axis and reads linearly across its levels, and the one sampler
 *  every other slot in a draw shares can be neither. */
constexpr std::string_view kEnvironmentSlots[4] = {
    "uEnvironment", "uEnvironmentNext", "uIrradiance", "uIrradianceNext"};

/** The split sum's table, read with the same sampler: linear, and read
 *  only between its texel centres, so how it wraps never shows. */
constexpr std::string_view kBrdfSlot = "uEnvironmentBrdf";

bool isEnvironmentSlot(std::string_view slot) {
  for (std::string_view name : kEnvironmentSlots)
    if (slot == name) return true;
  return slot == kBrdfSlot;
}

/** THE SPLIT SUM'S TABLE as a texture: SigilMaterial's integral of the
 *  lobe, scale in red and bias in green, and beside them in blue the
 *  sheen's directional albedo over the same square — so a sheen costs no
 *  table of its own — made once for the process. Eight bits a channel hold
 *  it to well under what the shading can show. */
const material::Texture& brdfTable() {
  static const material::Texture table = [] {
    const int size = material::kEnvironmentBrdfSize;
    const std::vector<glm::vec2>& values = material::environmentBrdfTable();
    const std::vector<float>& sheen = material::sheenAlbedoTable();
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::Make(size, size, kRGBA_8888_SkColorType,
                                         kPremul_SkAlphaType));
    for (int y = 0; y < size; ++y)
      for (int x = 0; x < size; ++x) {
        const glm::vec2 ab = values[(size_t)y * size + x];
        uint8_t* texel = (uint8_t*)bitmap.getAddr32(x, y);
        texel[0] = (uint8_t)std::lround(std::clamp(ab.x, 0.0f, 1.0f) * 255.0f);
        texel[1] = (uint8_t)std::lround(std::clamp(ab.y, 0.0f, 1.0f) * 255.0f);
        texel[2] = (uint8_t)std::lround(
            std::clamp(sheen[(size_t)y * size + x], 0.0f, 1.0f) * 255.0f);
        texel[3] = 255;
      }
    bitmap.setImmutable();
    return material::Texture(bitmap.asImage())
        .tile(material::Repeat::Pad)
        .sampling(material::Sampling::Linear);
  }();
  return table;
}

/** The scaffold's sampled slot and the placement it is read at — the
 *  two names the map a body is dressed with arrives under. */
constexpr std::string_view kMapSlot = "uBaseColorMap";
constexpr std::string_view kMapUv = "uBaseColorMapUv";

/** The half of the artefact numbering a STAMPED point set takes. The
 *  scene's store counts up from one, so nothing it hands out ever
 *  reaches here, and the rest of the number is the stamping's own
 *  content key — which is what makes a set that has not moved answer
 *  with the artefact it was already uploaded under. */
constexpr uint64_t kStampArtefact = 1ull << 63u;

/** The number the scaffold reads a light's kind by. */
float kindNumber(LightKind kind) {
  switch (kind) {
    case LightKind::Directional:
      return 0.0f;
    case LightKind::Point:
      return 1.0f;
    case LightKind::Spot:
      return 2.0f;
    case LightKind::Ambient:
      return 3.0f;
    case LightKind::Hemisphere:
      return 4.0f;
  }
  return 0.0f;
}

/** ONE LIGHT AS THE SCAFFOLD READS IT: its record's four rows, carried
 *  into the space the shading is written in by @p view, every colour
 *  decoded and scaled by the intensity. */
void writeLight(material::slang::Uniforms& uniforms, size_t index,
                const PlacedLight& placed, const glm::mat4& view) {
  const LightDescription& light = placed.light;
  const glm::vec3 position = glm::vec3(view * glm::vec4(placed.position, 1.0f));
  // The view is rigid, so its rotation carries a direction as it is.
  const glm::vec3 direction = glm::mat3(view) * light.direction;
  const glm::vec3 colour = radiance(placed);
  const glm::vec3 ground =
      linearColor(light.groundColor.value()) * light.intensity.value();
  const float outer = std::max(light.outerAngle, 0.0f);
  const float cosOuter = std::cos(glm::radians(outer));
  const float cosInner =
      std::cos(glm::radians(std::clamp(light.innerAngle, 0.0f, outer)));
  const float rowPosition[4] = {position.x, position.y, position.z,
                                kindNumber(light.kind)};
  const float rowDirection[4] = {direction.x, direction.y, direction.z,
                                 std::max(light.range, 0.0f)};
  const float rowColour[4] = {colour.r, colour.g, colour.b, cosOuter};
  const float rowGround[4] = {ground.r, ground.g, ground.b, cosInner};
  uniforms.setElement("uLightPosition", index, rowPosition, 4);
  uniforms.setElement("uLightDirection", index, rowDirection, 4);
  uniforms.setElement("uLightColor", index, rowColour, 4);
  uniforms.setElement("uLightGround", index, rowGround, 4);
}

/** What one body is drawn with: its program, the bytes its material
 *  resolved to, and whether the emitters reach it. */
struct Surface {
  const material::slang::Compiled* program = nullptr;
  material::Material::Resolved upload;
  const material::Recipe* recipe = nullptr;
  bool lit = true;
};

Surface surfaceOf(const material::Material* material, bool lit,
                  double seconds) {
  Surface out;
  out.lit = lit;
  if (!material) {
    out.program = &scaffold(lit);
    return out;
  }
  const material::Variant variant{lit ? kVariantLit : 0u};
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
    out.program = &scaffold(lit);
    return out;
  }
  out.program = &program->compiled();
  out.recipe = &material->recipe();
  return out;
}

/** The material's resolved bytes, written at the offsets its program
 *  reported for the same names.
 *
 *  A 3x3 is the one field whose bytes are not already in the order the
 *  shader reads them in: the parameters hold it column by column, the way
 *  glm does, and the program reads it row by row. */
void writeMaterial(material::slang::Uniforms& uniforms,
                   const Surface& surface) {
  if (!surface.recipe) return;
  for (const material::Field& field : surface.recipe->layout().fields) {
    const size_t bytes = field.floats * sizeof(float);
    if (field.offset + bytes > surface.upload.bytes.size()) continue;
    const auto* values = reinterpret_cast<const float*>(
        surface.upload.bytes.data() + field.offset);
    if (field.kind == material::ParameterType::Mat3) {
      float rows[9];
      for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) rows[r * 3 + c] = values[c * 3 + r];
      uniforms.set(field.name, rows, 9);
      continue;
    }
    uniforms.set(field.name, values, field.floats);
  }
}

/** The number the scaffold reads a facing by. */
float facingNumber(Facing facing) {
  switch (facing) {
    case Facing::None:
      return 0.0f;
    case Facing::Camera:
      return 1.0f;
    case Facing::CameraUpright:
      return 2.0f;
  }
  return 0.0f;
}

/** What a draw's instances need of the scaffold: whether each one turns
 *  to the camera and the camera's axes it turns onto, and the keys of the
 *  scalars each one carries. */
void writeInstancing(material::slang::Uniforms& uniforms, const glm::mat4& view,
                     Facing facing, const device::InstanceBuffers& instances) {
  uniforms.set("uView", view);
  uniforms.set("uFacing", facingNumber(facing), 0.0f, 0.0f, 0.0f);
  // The view is rigid, so its transpose carries the picture's right, up
  // and backward into the world, and its inverse's translation is the
  // eye.
  const glm::mat3 axes = glm::transpose(glm::mat3(view));
  const glm::vec3 eye = glm::vec3(glm::inverse(view)[3]);
  uniforms.set("uCameraRight", axes[0].x, axes[0].y, axes[0].z, 0.0f);
  uniforms.set("uCameraUp", axes[1].x, axes[1].y, axes[1].z, 0.0f);
  uniforms.set("uCameraBack", axes[2].x, axes[2].y, axes[2].z, 0.0f);
  uniforms.set("uCameraEye", eye.x, eye.y, eye.z, 1.0f);
  float keys[4];
  for (int k = 0; k < 4; ++k)
    keys[k] = material::instanceAttributeKey(instances.attributeNames[k]);
  uniforms.set("uInstanceAttributeKeys", keys, 4);
}

/** Everything a draw's scaffold needs: where the body stands, where the
 *  camera is, what is shining on it, and how the camera reads it. */
void writeScaffold(material::slang::Uniforms& uniforms,
                   const material::slang::Compiled& program, const Gpu& gpu,
                   const glm::mat4& viewProj, const glm::mat4& view,
                   const glm::mat4& model, glm::vec4 baseColor,
                   std::span<const PlacedLight* const> lights,
                   const Environment& sky, int levels, bool lit,
                   bool depthReadable) {
  uniforms.set("uViewProj", viewProj);
  uniforms.set("uModel", model);
  uniforms.set("uBaseColor", baseColor.r, baseColor.g, baseColor.b,
               baseColor.a);
  // The camera's reading, which an unlit body runs backward, and what a
  // body fading into the scene reads the scene's depth with.
  uniforms.set("uTone", gpu.exposure, curveNumber(gpu.toneCurve), 0.0f, 0.0f);
  uniforms.set("uInvProj", glm::inverse(viewProj * glm::inverse(view)));
  uniforms.set("uDepthFade",
               gpu.extent.width() ? 1.0f / (float)gpu.extent.width() : 0.0f,
               gpu.extent.height() ? 1.0f / (float)gpu.extent.height() : 0.0f,
               depthReadable ? 1.0f : 0.0f, 0.0f);
  if (!lit || !program.uniform("uLightCount")) return;

  const glm::mat4 modelView = view * model;
  uniforms.set("uModelView", modelView);
  uniforms.set("uNormalMatrix",
               glm::mat4(glm::inverseTranspose(glm::mat3(modelView))));
  const bool hasSky = levels > 0;
  uniforms.set("uEnvTint", sky.intensity, sky.intensity, sky.intensity,
               std::clamp(sky.crossfade, 0.0f, 1.0f));
  uniforms.set("uEnvDials", hasSky ? sky.diffuse : 0.0f,
               hasSky ? sky.specular : 0.0f, sky.roughnessBias,
               hasSky ? (float)levels : 0.0f);
  // The shading is written in view space and a panorama is of the world,
  // so a direction goes out through the view's inverse and then into the
  // panorama's own frame, which its turn put it in.
  uniforms.set("uEnvMatrix",
               glm::mat4(sky.orientation *
                         glm::transpose(glm::mat3(
                             glm::inverseTranspose(glm::mat3(view))))));
  // NO LIGHT IS SUPPLIED THAT THE SET DID NOT STATE: a set with no
  // ambient light and no environment has black shadows.
  const size_t count = std::min(lights.size(), kLights);
  for (size_t i = 0; i < count; ++i) writeLight(uniforms, i, *lights[i], view);
  uniforms.set("uLightCount", (float)count,
               (float)material::kEnvironmentBrdfSize, 0.0f, 0.0f);
}

/** HOW ONE BODY COMPOSITES: its blend, whether it writes depth, and
 *  whether it fades into the scene behind it. */
struct Blending {
  SkBlendMode blend = SkBlendMode::kSrcOver;
  bool depthWrite = true;
  bool fades = false;
};

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

/** WHAT A SLOT HOLDS when its texture does not say: a surface's normal,
 *  roughness, metalness, occlusion, opacity and clear-coat maps are data,
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
      slot == surface::kDistortionSlot;
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

/** What one body is drawn as: its mesh, the artefact number its buffers
 *  are held under, and the instances it is repeated at — null for one
 *  neutral instance — with whether each turns to the camera. */
struct Shape {
  uint64_t artefact = 0;
  const geometry::mesh::Mesh* mesh = nullptr;
  const geometry::mesh::Instances* instances = nullptr;
  Facing facing = Facing::None;
};

/** One body, drawn. @p map is the texture it is dressed with, or null;
 *  @p sceneDepth the copy of the opaque depth, or null; @p sceneColor the
 *  copy of the scene a body that sees through itself reads, or null. */
void drawBody(Gpu& gpu, const glm::mat4& viewProj, const glm::mat4& view,
              const Shape& shape, const glm::mat4& model, glm::vec4 baseColor,
              const material::Material* worn, const material::Texture* map,
              std::span<const PlacedLight* const> lights,
              const Environment& sky, bool lit, const Blending& blending,
              bool cull, dg::ITexture* sceneDepth,
              dg::ITexture* sceneColor = nullptr) {
  const MeshBuffers* buffers = gpu.meshes.upload(shape.artefact, *shape.mesh);
  if (!buffers) return;
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
  const Surface surface = surfaceOf(material, lit, gpu.seconds);
  if (!surface.program || surface.program->empty()) return;

  PipelineKey key{surface.program, blending.blend, true, blending.depthWrite,
                  false,           false,          cull};
  key.target = kRadianceFormat;
  key.instanced = true;
  const Pipeline* pipeline = gpu.pipelines.pipeline(key);
  if (!pipeline) return;

  // THE PANORAMA, uploaded once per map and kept. Its level count is
  // what says there is a sky at all, so it is asked for before the
  // uniforms are written.
  dg::ITexture* panorama = lit ? gpu.maps.environment(sky.map) : nullptr;
  dg::ITexture* panoramaNext = lit ? gpu.maps.environment(sky.next) : nullptr;
  dg::ITexture* lobe = lit ? gpu.maps.irradiance(sky.map) : nullptr;
  dg::ITexture* lobeNext = lit ? gpu.maps.irradiance(sky.next) : nullptr;
  if (!panoramaNext) panoramaNext = panorama;
  if (!lobeNext) lobeNext = lobe;
  const int levels = panorama ? (int)panorama->GetDesc().MipLevels : 0;

  material::slang::Uniforms uniforms(*surface.program);
  writeScaffold(uniforms, *surface.program, gpu, viewProj, view, model,
                baseColor, lights, sky, levels, lit, sceneDepth != nullptr);
  writeInstancing(uniforms, view, shape.instances ? shape.facing : Facing::None,
                  *instances);
  writeMaterial(uniforms, surface);
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
            *map, formFor(material::surface::kBaseColorSlot, *map));
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
    // a sky is a property of the set every body in it reads, so it is
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
    if (!material) continue;
    // …and every OTHER slot is the material's own. `material::surface::map`
    // answers null for a slot still holding the neutral dressing a
    // surface is built with, which is what keeps an undressed body
    // reading the one white texel rather than a map that says nothing.
    const material::Texture* own = material::surface::map(*material, slot);
    if (own) {
      if (own != map) textures[i] = gpu.maps.sample(*own, formFor(slot, *own));
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
  context->SetIndexBuffer(buffers->indices, 0,
                          dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  dg::DrawIndexedAttribs draw;
  draw.NumIndices = buffers->indexCount;
  draw.NumInstances = instances->count;
  draw.IndexType = dg::VT_UINT32;
  draw.Flags = dg::DRAW_FLAG_VERIFY_ALL;
  context->DrawIndexed(draw);
}

/** THE SKY, drawn over the whole target before every body in it. It is a
 *  pass and not a body because a body would need a mesh, a placement and
 *  a depth, and a sky has none of the three: it is what is there when
 *  nothing else is. */
void drawBackdrop(Gpu& gpu, const View& view, const glm::mat4& projection,
                  const glm::mat4& viewMatrix) {
  const Environment& sky = view.environment;
  if (!sky.valid() || sky.shown <= 0) return;
  const material::slang::Compiled& program = backdropProgram();
  if (program.empty()) return;
  dg::ITexture* panorama = gpu.maps.environment(sky.map);
  if (!panorama) return;
  dg::ITexture* panoramaNext = gpu.maps.environment(sky.next);
  if (!panoramaNext) panoramaNext = panorama;
  dg::ITexture* lobe = gpu.maps.irradiance(sky.map);
  dg::ITexture* lobeNext = gpu.maps.irradiance(sky.next);
  if (!lobeNext) lobeNext = lobe;

  // The sky is opaque and stands behind everything, so it writes no
  // depth and takes none: whatever is drawn after it covers it.
  PipelineKey key{&program, SkBlendMode::kSrc, false, false, true};
  key.target = kRadianceFormat;
  const Pipeline* pipeline = gpu.pipelines.pipeline(key);
  if (!pipeline) return;

  material::slang::Uniforms uniforms(program);
  uniforms.set("uEnvTint", sky.intensity, sky.intensity, sky.intensity,
               std::clamp(sky.crossfade, 0.0f, 1.0f));
  uniforms.set("uEnvDials", sky.diffuse, sky.specular, sky.roughnessBias,
               (float)panorama->GetDesc().MipLevels);
  uniforms.set("uTone", gpu.exposure, curveNumber(gpu.toneCurve), 0.0f, 0.0f);
  uniforms.set("uBackdrop", sky.shown, std::clamp(sky.blur, 0.0f, 1.0f), 0.0f,
               0.0f);
  // The ground sphere stands at the world origin, carried into the space
  // the ray is found in, where the eye is the origin — so the eye's own
  // position is never stated twice.
  const glm::vec3 centre = glm::vec3(viewMatrix * glm::vec4(0, 0, 0, 1.0f));
  uniforms.set("uGround", centre.x, centre.y, centre.z, sky.groundRadius);
  uniforms.set("uEnvMatrix",
               glm::mat4(sky.orientation *
                         glm::transpose(glm::mat3(
                             glm::inverseTranspose(glm::mat3(viewMatrix))))));
  uniforms.set("uInvProj", glm::inverse(projection));

  std::vector<dg::ITexture*> textures(program.textures.size(), nullptr);
  for (size_t i = 0; i < program.textures.size(); ++i) {
    const std::string& slot = program.textures[i];
    if (slot == kEnvironmentSlots[0])
      textures[i] = panorama;
    else if (slot == kEnvironmentSlots[1])
      textures[i] = panoramaNext;
    else if (slot == kEnvironmentSlots[2])
      textures[i] = lobe;
    else if (slot == kEnvironmentSlots[3])
      textures[i] = lobeNext;
  }

  dg::IDeviceContext* context = gpu.device->context();
  context->SetPipelineState(pipeline->state);
  device::bindDraw(gpu.shared, *pipeline, program, uniforms, textures,
                   SkFilterMode::kLinear, false, &isEnvironmentSlot);
  dg::DrawAttribs draw;
  draw.NumVertices = 3;
  draw.Flags = dg::DRAW_FLAG_VERIFY_ALL;
  context->Draw(draw);
}

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
  const material::Lobes carried = material::surface::lobes(*body.material);
  return carried.transmission || carried.distortion;
}

/** The phases a frame's bodies are drawn in, in order. */
enum class Phase : uint8_t { LaidOver, SeenThrough, Blended };

/** Copies @p from into @p into, both on the device. The caller binds its
 *  target again afterwards: a copy ends the drawing that was under way. */
void copyForReading(Gpu& gpu, dg::ITexture* from, dg::ITexture* into) {
  // The target is let go of first: a copy cannot read what is bound to be
  // drawn into.
  gpu.device->context()->SetRenderTargets(
      0, nullptr, nullptr, dg::RESOURCE_STATE_TRANSITION_MODE_NONE);
  dg::CopyTextureAttribs attribs;
  attribs.pSrcTexture = from;
  attribs.SrcTextureTransitionMode =
      dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  attribs.pDstTexture = into;
  attribs.DstTextureTransitionMode =
      dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  gpu.device->context()->CopyTexture(attribs);
}

/** The frame's bodies, or the ones a collection leaves: the ones laid
 *  over normally first, then the ones that see through themselves over
 *  them, then the blended ones, each phase back to front. A body that
 *  sees through itself reads the lit colour and the depth as they stood
 *  once the first phase was down, copied before the first of them is
 *  drawn; a blended body that fades into the scene reads that same depth,
 *  or copies it before the first of them when nothing copied it yet. */
void drawBodies(Gpu& gpu, const View& view, const glm::mat4& viewProj,
                const glm::mat4& viewMatrix, const Collection* only, bool lit,
                const glm::vec4* flat, dg::ITexture* target) {
  // THE LIGHTS EACH BODY IS REACHED BY: every light, unless its light
  // link leaves the body out. Asked per body, against the same member a
  // pass's collection reads, so a light link and a pass narrow on one
  // vocabulary.
  std::vector<const PlacedLight*> reaching;
  reaching.reserve(view.lights.size());
  dg::ITexture* sceneDepth = nullptr;
  // A FLAT colour replaces the body's own AND its material: coverage and
  // a variant re-draw are about where a body is, not what it is made of.
  // Whether the flat colour is then SHADED is the pass's call — coverage
  // is not a picture and is drawn unlit; a variant surface is a colour
  // laid over the bodies a collection names and stands under the pass's
  // lights like any other.
  dg::ITexture* sceneColor = nullptr;
  for (const Phase phase :
       {Phase::LaidOver, Phase::SeenThrough, Phase::Blended}) {
    for (const Draw& body : view.draws) {
      if (!body.mesh) continue;
      const Member member = memberOf(body);
      if (only && !only->contains(member)) continue;
      const glm::vec4 colour = flat ? *flat : body.baseColor;
      const Blending blending = blendingOf(body, colour, flat != nullptr);
      const Phase own = seesThrough(body, flat != nullptr) ? Phase::SeenThrough
                        : blended(body, blending, flat != nullptr)
                            ? Phase::Blended
                            : Phase::LaidOver;
      if (own != phase) continue;
      // THE SCENE BEHIND, copied once every body that does not see
      // through itself is laid over and before the first that does: the
      // lit colour with its blurrier levels derived from it, and the depth
      // beside it, which says where what was copied stands.
      if (phase == Phase::SeenThrough && !sceneColor) {
        if (dg::ITexture* copy = gpu.sceneColorForReading()) {
          copyForReading(gpu, target, copy);
          gpu.device->context()->GenerateMips(
              copy->GetDefaultView(dg::TEXTURE_VIEW_SHADER_RESOURCE));
          sceneColor = copy;
        }
        if (!sceneDepth && gpu.depth)
          if (dg::ITexture* depthCopy = gpu.depthForReading()) {
            copyForReading(gpu, gpu.depth, depthCopy);
            sceneDepth = depthCopy;
          }
        resumeTarget(gpu, target, true);
      }
      // THE OPAQUE DEPTH, copied once the last opaque body is down and
      // before the first body that fades into it, with the target bound
      // again afterwards: a copy ends the drawing that was under way.
      if (blending.fades && !sceneDepth && gpu.depth) {
        if (dg::ITexture* copy = gpu.depthForReading()) {
          copyForReading(gpu, gpu.depth, copy);
          resumeTarget(gpu, target, true);
          sceneDepth = copy;
        }
      }
      reaching.clear();
      for (const PlacedLight& light : view.lights)
        if (light.light.lightLink.everything() ||
            light.light.lightLink.contains(member))
          reaching.push_back(&light);
      // A BODY THAT IS ITS OWN LIGHT is drawn unlit whatever the pass
      // says, because that is what its surface is and not how this pass
      // reads it — a variant laid over it included.
      drawBody(
          gpu, viewProj, viewMatrix,
          Shape{body.geometry, body.mesh, body.instances, body.facing},
          body.world, colour, flat ? nullptr : body.material,
          flat ? nullptr : body.texture, reaching, view.environment,
          lit && body.lit, blending,
          /*cull=*/body.backface == material::Backface::Hidden,
          blending.fades || phase == Phase::SeenThrough ? sceneDepth : nullptr,
          phase == Phase::SeenThrough ? sceneColor : nullptr);
    }
  }
}

}  // namespace

void paintGeometry(Gpu& gpu, const PassWork& work, const View& view,
                   Targets& targets) {
  const Pass& pass = *work.pass;
  const std::span<const std::string> writes = pass.writes();
  if (writes.empty()) return;
  dg::ITexture* colour = gpu.target(writes.front());
  if (!colour) return;

  const glm::mat4 viewProj = clipProjection(view);
  const glm::mat4 viewMatrix = view.camera.view();

  const float clearNothing[4] = {0, 0, 0, 0};
  const bool asCoverage = work.realisation == Selection::Mask;
  if (asCoverage) {
    // Coverage is where the selection is, not what it looks like: flat
    // white, and nothing else reaches it.
    openTarget(gpu, colour, clearNothing, true);
    const glm::vec4 white{1, 1, 1, 1};
    drawBodies(gpu, view, viewProj, viewMatrix, &pass.collection(),
               /*lit=*/false, &white, colour);
    return;
  }

  // THE CLEAR IS AN AUTHORED COLOUR, so it is filled as the light the
  // final stage turns back into it, and the depth is cleared beside it.
  openTarget(gpu, colour, clearNothing, true);
  fillTarget(gpu, colour, pass.clear());
  resumeTarget(gpu, colour, true);
  // The PROJECTION alone is what turns a pixel into a ray, and it is the
  // clip transform with the view taken back out of it — the clip
  // transform is where the target's aspect and the depth convention
  // already are, so deriving it is what keeps the sky's rays and the
  // bodies' positions answering to one camera.
  drawBackdrop(gpu, view, viewProj * glm::inverse(viewMatrix), viewMatrix);
  const bool cull = work.realisation == Selection::Cull;
  drawBodies(gpu, view, viewProj, viewMatrix,
             cull ? &pass.collection() : nullptr, /*lit=*/true, nullptr,
             colour);

  // The stamps of every point set the pass reads, drawn as instanced
  // geometry like any instanced body. A compute pass writes points; this
  // is what makes them visible, and a pass with no stamp draws none of
  // them.
  if (!pass.stamp().positions.empty()) {
    // A pass's stamps belong to no node: they joined no collection and
    // stand under no key, so a light reaches them unless its link
    // includes only what they are not.
    std::vector<const PlacedLight*> everyLight;
    for (const PlacedLight& light : view.lights)
      if (light.light.lightLink.contains(Member{}))
        everyLight.push_back(&light);
    // A STAMPING IS AN ARTEFACT LIKE ANY OTHER: formed once per distinct
    // (cloud, stamp) in the store, and uploaded once under the number
    // that pair folds to. A set that has not moved between two frames is
    // therefore neither instanced again nor re-uploaded, however many
    // frames draw it.
    for (const std::string& name : pass.reads()) {
      // Read without inserting: a stamped pass reads image names too,
      // and the inserting overload would leave an empty cloud behind
      // for each of them that nothing ever erases.
      const geometry::mesh::Cloud* cloud = std::as_const(targets).points(name);
      if (!cloud || cloud->positions.empty()) continue;
      uint64_t key = 0;
      const geometry::mesh::Instances* stamped =
          targets.stamped(*cloud, pass.stamp(), &key);
      if (!stamped || stamped->stamp.indices.empty()) continue;
      // The top bit is the half of the numbering these take, so the
      // store's key is carried in the rest of it.
      const uint64_t artefact = kStampArtefact | (key >> 1u);
      drawBody(gpu, viewProj, viewMatrix,
               Shape{artefact, &stamped->stamp, stamped}, glm::mat4(1.0f),
               {0.9f, 0.9f, 0.95f, 1.0f}, nullptr, nullptr, everyLight,
               view.environment, /*lit=*/true, Blending{}, /*cull=*/true,
               nullptr);
    }
  }

  // The selection, drawn again in the surface the pass named — which is
  // what makes it visible in a pass that paints everything.
  if (work.realisation == Selection::Variant && pass.variant()) {
    const material::Field* field =
        pass.variant()->recipe().parameters().find("baseColor");
    const glm::vec4 variant = field && field->floats == 4
                                  ? pass.variant()->get<glm::vec4>("baseColor")
                                  : glm::vec4{1, 1, 1, 1};
    drawBodies(gpu, view, viewProj, viewMatrix, &pass.collection(),
               /*lit=*/true, &variant, colour);
  }

  // One coverage per masked pass behind this one, each holding the
  // selection that pass asked about.
  for (const Coverage& painted : work.coverageOut) {
    dg::ITexture* coverage = gpu.target(painted.name);
    if (!coverage) continue;
    openTarget(gpu, coverage, clearNothing, true);
    const glm::vec4 white{1, 1, 1, 1};
    drawBodies(gpu, view, viewProj, viewMatrix, &painted.of, /*lit=*/false,
               &white, coverage);
  }
}

}  // namespace sigil::world::diligent
