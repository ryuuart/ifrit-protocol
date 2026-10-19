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
 * THE SHADOW STAGE COMES BEFORE ALL OF IT. In a pass under a light that
 * casts, every such light's map is drawn into one depth atlas before the
 * target is opened: each body that casts, seen from the light, writing
 * depth and nothing else. A pass under no light that casts draws no map,
 * binds no atlas and is the pass it would be without this stage.
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
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/trigonometric.hpp>
#include <limits>
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
    case LightKind::Rect:
      return 5.0f;
    case LightKind::Sphere:
      return 6.0f;
    case LightKind::Disk:
      return 7.0f;
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
  float rowColour[4] = {colour.r, colour.g, colour.b, cosOuter};
  float rowGround[4] = {ground.r, ground.g, ground.b, cosInner};
  // A LIGHT WITH A SIZE carries it where a cone or a ground would be: its
  // half width (or radius) beside the colour, and for a rect its width
  // axis — squared to the direction it travels — with its half height.
  if (light.kind == LightKind::Rect || light.kind == LightKind::Sphere ||
      light.kind == LightKind::Disk) {
    const glm::vec3 unitDirection = glm::length(direction) > 1e-5f
                                        ? glm::normalize(direction)
                                        : glm::vec3(0, -1, 0);
    glm::vec3 across = glm::mat3(view) * placed.across;
    across = across - unitDirection * glm::dot(across, unitDirection);
    if (glm::dot(across, across) < 1e-8f) {
      const glm::vec3 other = std::abs(unitDirection.x) < 0.9f
                                  ? glm::vec3(1, 0, 0)
                                  : glm::vec3(0, 0, 1);
      across = other - unitDirection * glm::dot(other, unitDirection);
    }
    across = glm::normalize(across);
    const bool rect = light.kind == LightKind::Rect;
    rowColour[3] = rect ? light.width * 0.5f : light.radius;
    rowGround[0] = across.x;
    rowGround[1] = across.y;
    rowGround[2] = across.z;
    rowGround[3] = rect ? light.height * 0.5f : 0.0f;
  }
  uniforms.setElement("uLightPosition", index, rowPosition, 4);
  uniforms.setElement("uLightDirection", index, rowDirection, 4);
  uniforms.setElement("uLightColor", index, rowColour, 4);
  uniforms.setElement("uLightGround", index, rowGround, 4);
}

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

/** What one body is drawn with: its program, the bytes its material
 *  resolved to, and whether the emitters reach it. */
struct Surface {
  const material::slang::Compiled* program = nullptr;
  material::Material::Resolved upload;
  const material::Recipe* recipe = nullptr;
  bool lit = true;
  /** Drawn as a stand-in while its own program compiles. */
  bool standIn = false;
};

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
                  bool shadow = false, bool standIns = false,
                  uint32_t lights = 0u) {
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

/** THE SHADOWS a lit draw reads: the maps the pass drew, carried from the
 *  space the shading is written in, and which of @p lights reads which.
 *  A body that receives none, or a pass that drew none, says so and is
 *  shaded exactly as it would be without them. */
/** HOW WIDE THE SOURCE OF @p light's SHADOW IS, as the map reads it: on
 *  an orthographic map the spread of a shadow's edge per unit between
 *  caster and receiver, on a perspective one the source's width in scene
 *  units. A light with a size of its own casts from it. */
float sourceWidth(const PlacedLight& placed, bool perspective) {
  const LightDescription& light = placed.light;
  float width = std::max(light.shadow.size, 0.0f);
  switch (light.kind) {
    case LightKind::Rect:
      width =
          std::sqrt(std::max(light.width, 0.0f) * std::max(light.height, 0.0f));
      break;
    case LightKind::Disk:
    case LightKind::Sphere:
      width = 2.0f * std::max(light.radius, 0.0f);
      break;
    default:
      break;
  }
  if (perspective || light.kind != LightKind::Directional) return width;
  return 2.0f * std::tan(glm::radians(std::min(width, 60.0f)) * 0.5f);
}

void writeShadows(material::slang::Uniforms& uniforms, const Gpu& gpu,
                  const glm::mat4& view,
                  std::span<const PlacedLight* const> lights, bool receives) {
  const bool reads =
      receives && !gpu.shadows.empty() && gpu.shadowAtlasSide > 0;
  const float side = (float)std::max(gpu.shadowAtlasSide, 1);
  uniforms.set("uShadowDials", 1.0f / side, reads ? 1.0f : 0.0f,
               (float)gpu.shadows.size(), 0.0f);
  if (!reads) return;
  // The shading's points stand in the camera's view space, so each map's
  // projection is taken from there.
  const glm::mat4 back = glm::inverse(view);
  for (size_t m = 0; m < gpu.shadows.size(); ++m) {
    const ShadowMap& map = gpu.shadows[m];
    const glm::mat4 carried = map.viewProjection * back;
    for (int r = 0; r < 4; ++r) {
      const float row[4] = {carried[0][r], carried[1][r], carried[2][r],
                            carried[3][r]};
      uniforms.setElement("uShadowMatrix", m * 4 + (size_t)r, row, 4);
    }
    const float tile[4] = {(float)map.x / side, (float)map.y / side,
                           (float)map.size / side, (float)map.size / side};
    uniforms.setElement("uShadowTile", m, tile, 4);
    const float depth[4] = {map.near, map.far,
                            map.cube          ? 2.0f
                            : map.perspective ? 1.0f
                                              : 0.0f,
                            map.texel};
    uniforms.setElement("uShadowDepth", m, depth, 4);
    const LightShadow& stated = map.light->light.shadow;
    const float bias[4] = {std::max(stated.bias, 0.0f),
                           std::max(stated.normalBias, 0.0f),
                           std::max(stated.radius, 0.0f), map.fade};
    uniforms.setElement("uShadowBias", m, bias, 4);
    const float ends[4] = {map.ends.x, map.ends.y, map.ends.z, map.ends.w};
    uniforms.setElement("uShadowCascade", m, ends, 4);
    const float source[4] = {sourceWidth(*map.light, map.perspective), 0.0f,
                             0.0f, 0.0f};
    uniforms.setElement("uShadowSource", m, source, 4);
  }
  float slots[16];
  std::fill(std::begin(slots), std::end(slots), -1.0f);
  // A point light's six faces stand side by side; the light reads the
  // first and the shading finds the face it needs from there.
  for (size_t i = 0; i < lights.size() && i < 16; ++i)
    for (size_t m = 0; m < gpu.shadows.size(); ++m)
      if (gpu.shadows[m].light == lights[i]) {
        slots[i] = (float)m;
        break;
      }
  for (size_t row = 0; row < 4; ++row)
    uniforms.setElement("uLightShadow", row, slots + row * 4, 4);
}

/** Everything a draw's scaffold needs: where the body stands, where the
 *  camera is, what is shining on it, and how the camera reads it. */
void writeScaffold(material::slang::Uniforms& uniforms,
                   const material::slang::Compiled& program, const Gpu& gpu,
                   const glm::mat4& viewProj, const glm::mat4& view,
                   const glm::mat4& model, glm::vec4 baseColor,
                   std::span<const PlacedLight* const> lights,
                   const Environment& sky, int levels, bool lit,
                   bool depthReadable, bool receives) {
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
  // NO LIGHT IS SUPPLIED THAT THE SCENE DID NOT STATE: a scene with no
  // ambient light and no environment has black shadows.
  const size_t count = std::min(lights.size(), kLights);
  for (size_t i = 0; i < count; ++i) writeLight(uniforms, i, *lights[i], view);
  uniforms.set("uLightCount", (float)count,
               (float)material::kEnvironmentBrdfSize, 0.0f, 0.0f);
  writeShadows(uniforms, gpu, view, lights.first(count), receives);
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

/** What one body is drawn as: its mesh, the artefact number its buffers
 *  are held under, and the instances it is repeated at — null for one
 *  neutral instance — with whether each turns to the camera. */
struct Shape {
  uint64_t artefact = 0;
  const geometry::mesh::Mesh* mesh = nullptr;
  const geometry::mesh::Instances* instances = nullptr;
  Facing facing = Facing::None;
  /** THE ONE MATERIAL SLOT drawn, of a mesh whose faces wear several: only
   *  the triangles its "Material" lane names with it. Below zero draws
   *  every triangle. */
  int slot = -1;
  /** The primitive lane whose colour multiplies each triangle, or empty:
   *  part of how the buffers are packed, so every draw of one body names
   *  the same one. */
  std::string_view primitiveColor;
};

/** A BODY DRAWN INTO A SHADOW MAP: the light's projection, and whether
 *  the body's surface decides where it covers. */
struct Caster {
  glm::mat4 viewProjection{1.0f};
  bool coverage = false;
  /** Drawn into the tint beside the map rather than into its depth: the
   *  body's colour, the share of a light it lets through, laid over what
   *  is there by multiplying, behind nothing the map holds. */
  bool tint = false;
};

/** WHAT OF A LIGHT A TRANSMISSIVE SURFACE LETS THROUGH, as its tint reads
 *  it: its colour in linear light, the share it transmits, how deep it is,
 *  and its volume's colour and distance. The tint multiplies the first two
 *  numbers by their maps, dims the colour by the volume across the depth,
 *  and lets all of the share it does not transmit through, which its depth
 *  in the map already answers for. A number the material does not state
 *  reads as a clear pane's. */
struct TintTerms {
  glm::vec3 colour{1.0f};
  float weight = 1.0f;
  float thickness = 0.0f;
  float distance = 0.0f;
  glm::vec3 attenuation{1.0f};
};

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

/** HOW A BODY'S PIPELINE DIFFERS from another built out of @p program:
 *  into the picture, by how it blends and whether it writes depth and
 *  drops its back faces; into a shadow map (@p caster), depth and no
 *  colour from both faces, or the tint laid over by multiplying. */
PipelineKey bodyKey(const material::slang::Compiled& program,
                    const Blending& blending, bool cull, const Caster* caster) {
  // A BODY WRITING DEPTH IS OPAQUE: what it covers it replaces, so a
  // body composited normally and writing depth is drawn with blending
  // off, which is also what lets the device skip the pixels a nearer
  // opaque body hides. Translucency is a body that writes no depth or
  // states a blend of its own.
  const SkBlendMode blend =
      blending.depthWrite && blending.blend == SkBlendMode::kSrcOver
          ? SkBlendMode::kSrc
          : blending.blend;
  PipelineKey key{&program, blend, true, blending.depthWrite, false, cull};
  key.target = kRadianceFormat;
  key.instanced = true;
  key.tangent = true;
  if (caster) {
    // Depth and no colour; both faces, so a card blocks the light from
    // behind as well as in front.
    key = PipelineKey{&program, SkBlendMode::kSrc, true, true, false, false};
    key.target = dg::TEX_FORMAT_UNKNOWN;
    key.instanced = true;
    key.tangent = true;
    if (caster->tint) {
      key.blend = SkBlendMode::kModulate;
      key.depthWrite = false;
      key.target = dg::TEX_FORMAT_RGBA8_UNORM;
    }
  }
  return key;
}

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
              bool cull, dg::ITexture* sceneDepth,
              dg::ITexture* sceneColor = nullptr,
              const Caster* caster = nullptr, bool receives = true) {
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
  dg::ITexture* panorama =
      prefilteredPanorama(gpu, gpu.maps.environment(sky.map));
  if (!panorama) return;
  dg::ITexture* panoramaNext =
      prefilteredPanorama(gpu, gpu.maps.environment(sky.next));
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
  const material::Lobes stated = statedLobes(*body.material);
  return stated.transmission || stated.distortion;
}

/** WHERE A BODY STANDS IN THE PICTURE: the rectangle its box covers, in
 *  the picture's own normalised coordinates, and how far in front of the
 *  eye its box reaches — nearest, middle and farthest. A box reaching
 *  behind the eye covers the whole picture. */
struct Region {
  glm::vec4 rect{-1.0f, -1.0f, 1.0f, 1.0f};
  float nearest = 0.0f;
  float middle = 0.0f;
  float farthest = 0.0f;
};
Region regionOf(const Draw& body, const glm::mat4& viewProj,
                const glm::mat4& view);

/** Whether @p behind, drawn earlier, stands behind @p front where they
 *  overlap in the picture: the middle of its box farther than the whole of
 *  the other's. Bodies standing at one depth — drops on one pane — are
 *  beside each other, not behind. */
bool standsBehind(const Region& behind, const Region& front) {
  const bool overlaps =
      behind.rect.x < front.rect.z && front.rect.x < behind.rect.z &&
      behind.rect.y < front.rect.w && front.rect.y < behind.rect.w;
  return overlaps && behind.middle > front.farthest;
}

/** HOW MANY OF THE SCENE COPY'S SMALLER LEVELS @p body READS, out of the
 *  copy's @p levels: none for a smooth surface, which reads the copy as it
 *  is, and up to the level its roughness picks — scattered by how strongly
 *  it bends — and the one past it a read between two levels takes. A
 *  roughness map multiplies the number, so the number bounds it; a body
 *  whose numbers this cannot read is taken to read every level. */
int levelsRead(const Draw& body, int levels) {
  const material::Material& worn = *body.material;
  if (!worn.hasProgram()) return levels - 1;
  const material::Schema& fields = worn.recipe().parameters();
  const material::Field* roughness = fields.find("roughness");
  const material::Field* ior = fields.find("ior");
  const material::Lobes carried = material::surface::lobes(worn);
  if (!carried.transmission) return carried.distortion ? 0 : levels - 1;
  if (!roughness || !ior || roughness->floats != 1 || ior->floats != 1)
    return levels - 1;
  const float rough = std::clamp(worn.get<float>("roughness"), 0.0f, 1.0f);
  const float bend =
      std::clamp(worn.get<float>("ior") * 2.0f - 2.0f, 0.0f, 1.0f);
  const float level = rough * bend * (float)(levels - 1);
  return std::min((int)std::ceil(level) + (level > 0.0f ? 1 : 0), levels - 1);
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
 *  drawn — and copied again before one that stands in front of another
 *  drawn since the last copy, so glass seen through glass shows both, as
 *  deep as the bodies are layered; a blended body that fades into the
 *  scene reads that same depth, or copies it before the first of them
 *  when nothing copied it yet. */
void drawBodies(Gpu& gpu, const View& view, const glm::mat4& viewProj,
                const glm::mat4& viewMatrix, const Collection* only, bool lit,
                const glm::vec4* flat, dg::ITexture* target,
                bool timed = false) {
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
  // The bodies that see through themselves drawn since the scene was last
  // copied, by where they stand in the picture.
  std::vector<Region> sinceCopied;
  // How many of the copy's smaller levels the frame's bodies read, found
  // at the first copy.
  std::optional<int> readLevels;
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
      // …AND AGAIN before one standing in front of a body that saw through
      // itself since, which the copy then holds as it was drawn.
      bool copying = false;
      bool drewSeen = false;
      if (phase == Phase::SeenThrough) {
        const Region here = regionOf(body, viewProj, viewMatrix);
        copying =
            !sceneColor || std::any_of(sinceCopied.begin(), sinceCopied.end(),
                                       [&](const Region& drawn) {
                                         return standsBehind(drawn, here);
                                       });
        drewSeen = !sinceCopied.empty();
        if (copying) sinceCopied.clear();
        sinceCopied.push_back(here);
      }
      if (copying) {
        // What the bodies seen through before this copy cost is theirs,
        // not the copy's.
        if (timed && drewSeen) gpu.mark("seen");
        if (dg::ITexture* copy = gpu.sceneColorForReading()) {
          copyForReading(gpu, target, copy);
          if (timed) gpu.mark("colour-copy");
          // ONLY THE LEVELS SOMETHING READS: the deepest any body that sees
          // through itself in this frame reaches, and none at all where
          // every one of them is smooth.
          if (!readLevels) {
            readLevels = 0;
            const int levels = (int)copy->GetDesc().MipLevels;
            for (const Draw& other : view.draws)
              if (other.mesh && (!only || only->contains(memberOf(other))) &&
                  seesThrough(other, flat != nullptr))
                *readLevels = std::max(*readLevels, levelsRead(other, levels));
          }
          if (*readLevels > 0)
            if (dg::ITextureView* chain =
                    gpu.sceneColorLevels(*readLevels + 1)) {
              gpu.device->context()->GenerateMips(chain);
              if (timed) gpu.mark("mips");
            }
          sceneColor = copy;
        }
        if (gpu.depth)
          if (dg::ITexture* depthCopy = gpu.depthForReading()) {
            copyForReading(gpu, gpu.depth, depthCopy);
            if (timed) gpu.mark("depth-copy");
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
          if (timed) gpu.mark("depth-copy");
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
      // A MESH WHOSE FACES WEAR SEVERAL SLOTS is drawn once a slot, each
      // draw the triangles its "Material" lane names with that slot,
      // wearing that slot's surface and map.
      const bool perSlot = !flat && body.slots.size() > 1 && !body.instances;
      const size_t draws = perSlot ? body.slots.size() : 1;
      for (size_t k = 0; k < draws; ++k) {
        const material::Material* worn = flat      ? nullptr
                                         : perSlot ? &body.slots[k]
                                                   : body.material;
        const material::Texture* map =
            flat      ? nullptr
            : perSlot ? material::surface::map(
                            body.slots[k], material::surface::kBaseColorSlot)
                      : body.texture;
        drawBody(gpu, viewProj, viewMatrix,
                 Shape{body.geometry, body.mesh, body.instances, body.facing,
                       perSlot ? (int)k : -1, body.primitiveColor},
                 body.world, colour, worn, map, reaching, view.environment,
                 lit && body.lit, blending,
                 /*cull=*/body.backface == material::Backface::Hidden,
                 blending.fades || phase == Phase::SeenThrough ? sceneDepth
                                                               : nullptr,
                 phase == Phase::SeenThrough ? sceneColor : nullptr, nullptr,
                 body.receiveShadow);
      }
    }
    if (timed)
      gpu.mark(phase == Phase::LaidOver      ? "laid"
               : phase == Phase::SeenThrough ? "seen"
                                             : "blended");
  }
}

// ---- the shadow stage -------------------------------------------------
//
// THE FITTING IS WRITTEN HERE. DiligentFX's ShadowMapManager fits cascades
// for one directional light into a texture array of its own; a frame here
// holds directional and spot lights side by side, each with its own map
// size, shadow link and bias, in one atlas the surface program reads with
// the rest of its slots — which that manager neither allocates nor fits.

/** An axis-aligned box in the world, empty until a point is added. */
struct Box {
  glm::vec3 low{std::numeric_limits<float>::max()};
  glm::vec3 high{std::numeric_limits<float>::lowest()};
  void add(const glm::vec3& p) {
    low = glm::min(low, p);
    high = glm::max(high, p);
  }
  void add(const Box& other) {
    if (other.empty()) return;
    add(other.low);
    add(other.high);
  }
  [[nodiscard]] bool empty() const { return low.x > high.x; }
  [[nodiscard]] glm::vec3 corner(int i) const {
    return {(i & 1) ? high.x : low.x, (i & 2) ? high.y : low.y,
            (i & 4) ? high.z : low.z};
  }
};

/** WHERE @p body STANDS in the world: its mesh's box, grown to reach every
 *  instance it is repeated at, carried through its placement. */
Box boxOf(const Draw& body) {
  Box out;
  if (!body.mesh || body.mesh->positions.empty()) return out;
  glm::vec3 low;
  glm::vec3 high;
  body.mesh->bounds(&low, &high);
  Box local;
  if (body.instances && body.instances->size()) {
    // Each copy may turn, so it is given the sphere its stamp could sweep.
    const float reach = std::max(glm::length(low), glm::length(high));
    const geometry::mesh::Instances& copies = *body.instances;
    for (size_t i = 0; i < copies.size(); ++i) {
      const glm::vec3 scale =
          i < copies.scales.size() ? glm::abs(copies.scales[i]) : glm::vec3(1);
      const float grown = reach * std::max({scale.x, scale.y, scale.z});
      local.add(copies.positions[i] - glm::vec3(grown));
      local.add(copies.positions[i] + glm::vec3(grown));
    }
  } else {
    local.add(low);
    local.add(high);
  }
  for (int i = 0; i < 8; ++i)
    out.add(glm::vec3(body.world * glm::vec4(local.corner(i), 1.0f)));
  return out;
}

Region regionOf(const Draw& body, const glm::mat4& viewProj,
                const glm::mat4& view) {
  Region out;
  const Box box = boxOf(body);
  if (box.low.x > box.high.x) return out;
  glm::vec2 low(std::numeric_limits<float>::max());
  glm::vec2 high(std::numeric_limits<float>::lowest());
  float nearest = std::numeric_limits<float>::max();
  float farthest = std::numeric_limits<float>::lowest();
  bool behindEye = false;
  for (int i = 0; i < 8; ++i) {
    const glm::vec4 corner(box.corner(i), 1.0f);
    const float depth = -(view * corner).z;
    nearest = std::min(nearest, depth);
    farthest = std::max(farthest, depth);
    const glm::vec4 clip = viewProj * corner;
    if (clip.w <= 1e-4f) {
      behindEye = true;
      continue;
    }
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    low = glm::min(low, ndc);
    high = glm::max(high, ndc);
  }
  if (!behindEye) out.rect = glm::vec4(low, high);
  out.nearest = nearest;
  out.farthest = farthest;
  out.middle = (nearest + farthest) * 0.5f;
  return out;
}

/** WHETHER @p body BLOCKS LIGHT: it says it casts, it is lit, and it is
 *  laid over writing depth. A body that adds, multiplies, screens, fades or
 *  writes no depth, one that sees through itself, and one that is its own
 *  light cast nothing. */
bool casts(const Draw& body) {
  if (!body.mesh || !body.castShadow || !body.lit) return false;
  if (seesThrough(body, false)) return false;
  const Blending blending = blendingOf(body, body.baseColor, false);
  if (blended(body, blending, false)) return false;
  return blending.depthWrite;
}

/** WHETHER @p body's SURFACE DECIDES WHERE IT COVERS: it is dressed with a
 *  map, whose alpha may cut it out, or with an opacity map. */
bool coverageOf(const Draw& body) {
  if (body.texture) return true;
  return body.material &&
         material::surface::map(*body.material,
                                material::surface::kOpacitySlot) != nullptr;
}

/** A view from @p eye toward @p ahead, with an up that is never parallel
 *  to it. */
glm::mat4 lightView(const glm::vec3& eye, const glm::vec3& ahead) {
  const glm::vec3 up =
      std::abs(ahead.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
  return glm::lookAtRH(eye, eye + ahead, up);
}

/** A projection's depth taken from minus one to one onto zero to one, the
 *  range the device stores, near at zero. */
glm::mat4 toDeviceDepth(const glm::mat4& projection) {
  glm::mat4 depth(1.0f);
  depth[2][2] = 0.5f;
  depth[3][2] = 0.5f;
  return depth * projection;
}

/** THE CORNERS OF WHAT THE CAMERA SEES between @p from and @p to in front
 *  of it, each measured along the way it looks; zero for either is the
 *  near or the far plane. */
std::array<glm::vec3, 8> cameraCorners(const View& view, float from, float to) {
  const glm::mat4 inverse = glm::inverse(clipProjection(view));
  std::array<glm::vec3, 8> out;
  const glm::vec3 eye = view.camera.eye;
  const glm::vec3 forward = glm::normalize(view.camera.target - eye);
  for (int i = 0; i < 4; ++i) {
    const glm::vec2 ndc{(i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f};
    const glm::vec4 nearPoint = inverse * glm::vec4(ndc, 0.0f, 1.0f);
    const glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0f, 1.0f);
    const glm::vec3 a = glm::vec3(nearPoint) / nearPoint.w;
    const glm::vec3 b = glm::vec3(farPoint) / farPoint.w;
    const float along = glm::dot(b - a, forward);
    const float start = glm::dot(a - eye, forward);
    // The point of this corner's ray at a distance in front of the eye.
    const auto at = [&](float distance, glm::vec3 otherwise) {
      if (distance <= 0.0f || along <= 0.0f) return otherwise;
      return a + (b - a) * std::clamp((distance - start) / along, 0.0f, 1.0f);
    };
    out[(size_t)i] = at(from, a);
    out[(size_t)i + 4] = at(to, b);
  }
  return out;
}

/** HOW FAR IN FRONT OF THE CAMERA a directional light's maps reach: the
 *  distance its shadow states, or the far side of the scene. */
float shadowReach(const PlacedLight& light, const View& view,
                  const Box& scene) {
  if (light.light.shadow.distance > 0.0f) return light.light.shadow.distance;
  const glm::vec3 eye = view.camera.eye;
  const glm::vec3 forward = glm::normalize(view.camera.target - eye);
  float reach = 0.0f;
  for (int i = 0; i < 8; ++i)
    reach = std::max(reach, glm::dot(scene.corner(i) - eye, forward));
  return std::min(reach, view.camera.farPlane);
}

/** A DIRECTIONAL LIGHT'S MAP: an orthographic view along the light, square
 *  across, covering what the camera sees of the scene between @p from and
 *  @p to in front of it — zero for either is the near or the far plane —
 *  and deep enough to hold every body between the light and them.
 *
 *  IT HOLDS STILL AS THE CAMERA TURNS. Across, it covers the sphere round
 *  that slice of the camera's view, whose size is the lens's and not the
 *  way it faces, or the scene's own box where that is the smaller; its
 *  corner is snapped to whole texels in the light's own frame. So the
 *  texels stand at the same places in the world from one frame to the
 *  next, and a still shadow's edge does not crawl while the camera moves. */
ShadowMap fitDirectional(const PlacedLight& light, const View& view,
                         const Box& scene, int size, float from, float to) {
  ShadowMap out;
  out.light = &light;
  out.size = size;
  const glm::vec3 ahead = glm::length(light.light.direction) > 1e-5f
                              ? glm::normalize(light.light.direction)
                              : glm::vec3(0, -1, 0);
  const glm::vec3 centre = (scene.low + scene.high) * 0.5f;
  const float radius =
      std::max(glm::length(scene.high - scene.low) * 0.5f, 1e-3f);
  const glm::mat4 looking = lightView(centre - ahead * (radius * 2.0f), ahead);
  Box sceneSeen;
  for (int i = 0; i < 8; ++i)
    sceneSeen.add(glm::vec3(looking * glm::vec4(scene.corner(i), 1.0f)));
  // THE SPHERE ROUND THE SLICE: its centre where the slice's corners
  // average, its radius the farthest of them from it — a length the lens
  // decides and the turn does not — rounded up to a step so it does not
  // flicker in its last digits.
  const std::array<glm::vec3, 8> corners = cameraCorners(view, from, to);
  glm::vec3 middleOfSlice(0.0f);
  for (const glm::vec3& corner : corners) middleOfSlice += corner / 8.0f;
  float reach = 0.0f;
  for (const glm::vec3& corner : corners)
    reach = std::max(reach, glm::length(corner - middleOfSlice));
  const float step = std::max(reach / 64.0f, 1e-3f);
  reach = std::ceil(reach / step) * step;
  const glm::vec2 sliceMiddle =
      glm::vec2(looking * glm::vec4(middleOfSlice, 1.0f));
  glm::vec2 low = sliceMiddle - glm::vec2(reach);
  glm::vec2 high = sliceMiddle + glm::vec2(reach);
  const glm::vec2 sceneLow = glm::vec2(sceneSeen.low);
  const glm::vec2 sceneHigh = glm::vec2(sceneSeen.high);
  // The scene's own box, where it is no wider than the sphere, holds still
  // too, and spends the map on less.
  bool clipped = true;
  if (std::max(sceneHigh.x - sceneLow.x, sceneHigh.y - sceneLow.y) <=
      2.0f * reach) {
    low = sceneLow;
    high = sceneHigh;
    clipped = false;
  }
  // A little past what it covers on every side, so a shadow at its edge is
  // never the last texel of the map.
  const float half =
      std::max(high.x - low.x, high.y - low.y) * 0.5f * 1.02f + 1e-3f;
  out.texel = 2.0f * half / (float)size;
  glm::vec2 middle = (low + high) * 0.5f;
  middle = glm::floor(middle / out.texel) * out.texel;
  out.near = std::max(-sceneSeen.high.z - radius * 0.01f, 0.0f);
  out.far = -sceneSeen.low.z + radius * 0.01f;
  out.viewProjection = toDeviceDepth(glm::orthoRH_NO(
                           middle.x - half, middle.x + half, middle.y - half,
                           middle.y + half, out.near, out.far)) *
                       looking;
  out.fade = clipped ? 0.05f : 0.0f;
  return out;
}

/** A DIRECTIONAL LIGHT'S CASCADES: its reach split along the camera's
 *  depth into as many slices as its shadow states, each fitted with its
 *  own map — the slices growing with the distance, three parts of the way
 *  from even to geometric, so the nearer maps cover less of the scene at the
 *  same size. One cascade is the single map for the whole reach. */
std::vector<ShadowMap> fitCascades(const PlacedLight& light, const View& view,
                                   const Box& scene, int size) {
  const int count = std::clamp(light.light.shadow.cascades, 1, 4);
  if (count == 1) {
    const float distance = std::max(light.light.shadow.distance, 0.0f);
    return {fitDirectional(light, view, scene, size, 0.0f, distance)};
  }
  const float near = std::max(view.camera.nearPlane, 1e-3f);
  const float far = std::max(shadowReach(light, view, scene), near * 2.0f);
  std::vector<ShadowMap> out;
  glm::vec4 ends{0.0f};
  float from = 0.0f;
  for (int k = 0; k < count; ++k) {
    const float share = (float)(k + 1) / (float)count;
    const float geometric = near * std::pow(far / near, share);
    const float even = near + (far - near) * share;
    const float to = k + 1 == count ? far : 0.75f * geometric + 0.25f * even;
    ShadowMap map = fitDirectional(light, view, scene, size, from, to);
    // Only the last fades at its edge: every other one hands over to the
    // next before it reaches it.
    if (k + 1 < count) map.fade = 0.0f;
    ends[k] = to;
    out.push_back(map);
    from = to;
  }
  out.front().ends = ends;
  return out;
}

/** A SPOT LIGHT'S MAP: a perspective view from where it stands along where
 *  it aims, opening a little wider than its cone, reaching its range or,
 *  with none, the far side of the scene. */
ShadowMap fitSpot(const PlacedLight& light, const Box& scene, int size) {
  ShadowMap out;
  out.light = &light;
  out.size = size;
  out.perspective = true;
  const glm::vec3 ahead = glm::length(light.light.direction) > 1e-5f
                              ? glm::normalize(light.light.direction)
                              : glm::vec3(0, -1, 0);
  float far = light.light.range;
  if (far <= 0.0f) {
    for (int i = 0; i < 8; ++i)
      far = std::max(far, glm::length(scene.corner(i) - light.position));
  }
  out.far = std::max(far, 1e-2f);
  out.near = std::max(out.far * 0.002f, 1e-3f);
  const float outer = light.light.outerAngle;
  const float opening = std::min(
      glm::radians(std::max(outer, 1.0f) * 2.0f + 2.0f), glm::radians(170.0f));
  out.texel = 2.0f * std::tan(opening * 0.5f) / (float)size;
  out.viewProjection =
      toDeviceDepth(glm::perspectiveRH_NO(opening, 1.0f, out.near, out.far)) *
      lightView(light.position, ahead);
  return out;
}

/** THE WIDTH ONE FACE OF A POINT LIGHT'S MAP OPENS beyond a quarter turn,
 *  as the tangent of its half angle: a little past one, so the filter's
 *  taps near a face's edge still read that face. */
constexpr float kFaceReach = 1.05f;

/** A POINT LIGHT'S MAP: six perspective views from where it stands, one
 *  along each axis each way, every one reaching its range or, with none,
 *  the far side of the scene. The shading reads the face the point stands
 *  most squarely in front of. */
std::array<ShadowMap, 6> fitPoint(const PlacedLight& light, const Box& scene,
                                  int size, const glm::mat3& axes) {
  float far = light.light.range;
  if (far <= 0.0f)
    for (int i = 0; i < 8; ++i)
      far = std::max(far, glm::length(scene.corner(i) - light.position));
  far = std::max(far, 1e-2f);
  const float near = std::max(far * 0.002f, 1e-3f);
  const glm::mat4 projection = toDeviceDepth(
      glm::perspectiveRH_NO(2.0f * std::atan(kFaceReach), 1.0f, near, far));
  const glm::vec3 kAxes[6] = {axes[0],  -axes[0], axes[1],
                              -axes[1], axes[2],  -axes[2]};
  std::array<ShadowMap, 6> out;
  for (size_t face = 0; face < 6; ++face) {
    ShadowMap& map = out[face];
    map.light = &light;
    map.size = size;
    map.perspective = true;
    map.cube = true;
    map.near = near;
    map.far = far;
    map.texel = 2.0f * kFaceReach / (float)size;
    map.viewProjection = projection * lightView(light.position, kAxes[face]);
  }
  return out;
}

/** A FOLD OF BYTES INTO A NUMBER, one value at a time. */
struct Fold {
  uint64_t value = 1469598103934665603ull;
  void bytes(const void* data, size_t size) {
    const auto* at = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) {
      value ^= at[i];
      value *= 1099511628211ull;
    }
  }
  template <class T>
  void add(const T& thing) {
    bytes(&thing, sizeof thing);
  }
  template <class T>
  void all(const std::vector<T>& things) {
    add(things.size());
    if (!things.empty()) bytes(things.data(), things.size() * sizeof(T));
  }
};

/** WHAT ONE MAP'S DEPTH IS A FUNCTION OF: where it stands in the atlas
 *  and how it projects, and every body its light's link keeps that casts
 *  — where it stands, which cooked triangles it is, every instance of
 *  it, the eye's turn where it turns to face the eye, and, where its
 *  surface cuts it out, the map and the moment that surface is read at.
 *  Two frames folding to the same number draw the same depth into the
 *  tile, so the second keeps the first's. */
uint64_t shadowFold(const ShadowMap& map, const View& view,
                    const glm::mat4& cameraView, double seconds) {
  Fold fold;
  fold.add(map.viewProjection);
  fold.add(map.x);
  fold.add(map.y);
  fold.add(map.size);
  const Collection& link = map.light->light.shadowLink;
  for (size_t i = 0; i < view.draws.size(); ++i) {
    const Draw& body = view.draws[i];
    if (!casts(body)) continue;
    if (!link.everything() && !link.contains(memberOf(body))) continue;
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
    if (coverageOf(body)) {
      // A caster drawn as a stand-in is drawn again once its program
      // arrives.
      if (view.standIns && body.material)
        fold.add(
            programReady(material::inlined(*body.material), kVariantShadow));
      fold.add(body.material);
      fold.add(body.texture);
      fold.add(body.baseColor);
      fold.add(seconds);
    }
  }
  return fold.value;
}

/** A TILE OF CLEARED DEPTH as large as @p side, cleared once when made:
 *  what one map's tile is reset from by a copy, since a clear reaches the
 *  whole atlas and a tile kept from the frame before must survive it. */
dg::ITexture* blankTile(Gpu& gpu, int side) {
  if (gpu.shadowBlank && gpu.shadowBlankSide >= side) return gpu.shadowBlank;
  gpu.shadowBlank.Release();
  gpu.shadowBlankSide = 0;
  dg::TextureDesc desc;
  desc.Name = "world shadow blank tile";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = desc.Height = (dg::Uint32)side;
  desc.Format = kDepthFormat;
  desc.BindFlags = dg::BIND_DEPTH_STENCIL;
  desc.Usage = dg::USAGE_DEFAULT;
  gpu.device->renderDevice()->CreateTexture(desc, nullptr, &gpu.shadowBlank);
  if (!gpu.shadowBlank) return nullptr;
  dg::IDeviceContext* context = gpu.device->context();
  dg::ITextureView* dsv =
      gpu.shadowBlank->GetDefaultView(dg::TEXTURE_VIEW_DEPTH_STENCIL);
  context->SetRenderTargets(0, nullptr, dsv,
                            dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->ClearDepthStencil(dsv, dg::CLEAR_DEPTH_FLAG, 1.0f, 0,
                             dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  gpu.shadowBlankSide = side;
  return gpu.shadowBlank;
}

/** THE SHADOW STAGE: every casting light's map drawn into the atlas, each
 *  in its own tile, from the bodies that cast and that its shadow link
 *  keeps. Draws nothing, and binds nothing afterwards, in a pass under no
 *  light that casts.
 *
 *  A MAP NOTHING MOVED IN IS KEPT. The atlas stands from frame to frame,
 *  and a tile is redrawn only when what its depth is a function of
 *  changed — its light, its fit, or a body that casts into it. Answers
 *  how many maps it drew. */
int64_t drawShadows(Gpu& gpu, const View& view, const glm::mat4& cameraView) {
  gpu.shadows.clear();
  const std::vector<size_t> chosen =
      shadowCasters(std::span<const PlacedLight>(view.lights));
  if (chosen.empty()) return 0;
  Box scene;
  std::vector<Box> boxes(view.draws.size());
  for (size_t i = 0; i < view.draws.size(); ++i) {
    boxes[i] = boxOf(view.draws[i]);
    scene.add(boxes[i]);
  }
  if (scene.empty()) return 0;

  // EVERY TILE THE SIZE OF THE LARGEST MAP ASKED FOR, in a square grid
  // holding one tile per map — six for a point light, one per cascade for
  // a directional light — and no more than
  // kAtlasSide texels a side: a frame holding more maps than that fits at
  // the size asked for draws every one of them smaller. A map asking for
  // less than its tile draws into the tile's corner.
  constexpr int kAtlasSide = 4096;
  int tile = 64;
  size_t count = 0;
  for (const size_t index : chosen) {
    const PlacedLight& light = view.lights[index];
    tile = std::max(tile, std::clamp(light.light.shadow.mapSize, 64, 2048));
    count += light.light.kind == LightKind::Point ||
                     light.light.kind == LightKind::Sphere ||
                     light.light.kind == LightKind::Rect ||
                     light.light.kind == LightKind::Disk
                 ? 6
             : light.light.kind == LightKind::Directional
                 ? (size_t)std::clamp(light.light.shadow.cascades, 1, 4)
                 : 1;
  }
  int across = 1;
  while ((size_t)(across * across) < count) ++across;
  tile = std::min(tile, kAtlasSide / across);
  dg::ITexture* atlas = gpu.shadowAtlasAt(tile * across);
  if (!atlas) return 0;
  const auto place = [&](ShadowMap map) {
    const int at = (int)gpu.shadows.size();
    map.x = (at % across) * tile;
    map.y = (at / across) * tile;
    gpu.shadows.push_back(map);
  };
  for (const size_t index : chosen) {
    const PlacedLight& light = view.lights[index];
    const int size = std::clamp(light.light.shadow.mapSize, 64, tile);
    // A light with a size casts from its centre through six maps, so a
    // panel's whole front half is covered: a sphere's along the world's
    // axes as a point light's, a rect's or a disk's turned to its face.
    if (light.light.kind == LightKind::Point ||
        light.light.kind == LightKind::Sphere) {
      for (const ShadowMap& face :
           fitPoint(light, scene, size, glm::mat3(1.0f)))
        place(face);
    } else if (light.light.kind == LightKind::Rect ||
               light.light.kind == LightKind::Disk) {
      const glm::vec3 ahead = glm::length(light.light.direction) > 1e-5f
                                  ? glm::normalize(light.light.direction)
                                  : glm::vec3(0, -1, 0);
      glm::vec3 across = light.across - ahead * glm::dot(light.across, ahead);
      if (glm::length(across) < 1e-4f)
        across =
            std::abs(ahead.x) < 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 0, 1);
      across = glm::normalize(across - ahead * glm::dot(across, ahead));
      const glm::mat3 axes(ahead, across, glm::cross(ahead, across));
      for (const ShadowMap& face : fitPoint(light, scene, size, axes))
        place(face);
    } else if (light.light.kind == LightKind::Spot) {
      place(fitSpot(light, scene, size));
    } else {
      for (const ShadowMap& slice : fitCascades(light, view, scene, size))
        place(slice);
    }
  }

  // WHICH TILES MUST BE DRAWN AGAIN. An atlas made anew, or laid out
  // anew, holds nothing worth keeping.
  const bool laidOut = atlas == gpu.shadowFoldsAtlas &&
                       gpu.shadowFolds.size() == gpu.shadows.size();
  std::vector<uint64_t> folds(gpu.shadows.size());
  std::vector<bool> redraw(gpu.shadows.size(), !laidOut);
  size_t redrawn = 0;
  for (size_t at = 0; at < gpu.shadows.size(); ++at) {
    folds[at] = shadowFold(gpu.shadows[at], view, cameraView, gpu.seconds);
    if (laidOut && folds[at] != gpu.shadowFolds[at]) redraw[at] = true;
    redrawn += redraw[at] ? 1 : 0;
  }
  gpu.shadowFolds = std::move(folds);
  gpu.shadowFoldsAtlas = atlas;
  if (redrawn == 0) return 0;

  dg::IDeviceContext* context = gpu.device->context();
  dg::ITextureView* dsv = atlas->GetDefaultView(dg::TEXTURE_VIEW_DEPTH_STENCIL);
  dg::ITexture* blank =
      redrawn == gpu.shadows.size() ? nullptr : blankTile(gpu, tile);
  if (blank) {
    // Each tile drawn again is reset from the blank one; every other tile
    // keeps the depth the frame before drew.
    for (size_t at = 0; at < gpu.shadows.size(); ++at) {
      if (!redraw[at]) continue;
      dg::Box whole;
      whole.MaxX = whole.MaxY = (dg::Uint32)tile;
      dg::CopyTextureAttribs copy(
          blank, dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, atlas,
          dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      copy.pSrcBox = &whole;
      copy.DstX = (dg::Uint32)gpu.shadows[at].x;
      copy.DstY = (dg::Uint32)gpu.shadows[at].y;
      context->CopyTexture(copy);
    }
    context->SetRenderTargets(0, nullptr, dsv,
                              dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  } else {
    std::fill(redraw.begin(), redraw.end(), true);
    redrawn = gpu.shadows.size();
    context->SetRenderTargets(0, nullptr, dsv,
                              dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->ClearDepthStencil(dsv, dg::CLEAR_DEPTH_FLAG, 1.0f, 0,
                               dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }
  const dg::Uint32 side = (dg::Uint32)gpu.shadowAtlasSide;
  for (size_t at = 0; at < gpu.shadows.size(); ++at) {
    if (!redraw[at]) continue;
    const ShadowMap& map = gpu.shadows[at];
    dg::Viewport viewport;
    viewport.TopLeftX = (float)map.x;
    viewport.TopLeftY = (float)map.y;
    viewport.Width = viewport.Height = (float)map.size;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->SetViewports(1, &viewport, side, side);
    const Collection& link = map.light->light.shadowLink;
    const int64_t pendingBefore = gpu.programsPending;
    for (size_t i = 0; i < view.draws.size(); ++i) {
      const Draw& body = view.draws[i];
      if (!casts(body)) continue;
      if (!link.everything() && !link.contains(memberOf(body))) continue;
      const Caster caster{map.viewProjection, coverageOf(body)};
      // A body turned to the camera keeps the turn the eye sees it at, so
      // its shadow is the shape it shows.
      drawBody(
          gpu, map.viewProjection, cameraView,
          Shape{body.geometry, body.mesh, body.instances, body.facing, -1,
                body.primitiveColor},
          body.world, body.baseColor, caster.coverage ? body.material : nullptr,
          caster.coverage ? body.texture : nullptr, {}, view.environment,
          /*lit=*/false, Blending{}, /*cull=*/false, nullptr, nullptr, &caster);
    }
    // A tile a body stood in for, or was left out of, is drawn again on
    // the next frame, whatever its depth is a function of.
    if (gpu.programsPending != pendingBefore)
      gpu.shadowFolds[at] = ~gpu.shadowFolds[at];
  }
  return (int64_t)redrawn;
}

/** THE PAGES THE FRAME'S BODIES WEAR, made ready before any target is
 *  opened: every map a body's material holds, at any depth of its tree,
 *  that names a page standing on the device is wrapped, waited for and
 *  has its levels filled here — so the draws inside a pass find them
 *  filled and none of them breaks its pass to fill one. A material two
 *  bodies share is walked once. */
void preparePages(Gpu& gpu, const View& view) {
  const std::chrono::duration<double> time(gpu.seconds);
  std::vector<const material::Material*> pending;
  std::vector<const material::Material*> walked;
  for (const Draw& body : view.draws) {
    if (body.texture) gpu.maps.prepare(*body.texture, time);
    if (body.material) pending.push_back(body.material);
    for (const material::Material& slot : body.slots) pending.push_back(&slot);
  }
  while (!pending.empty()) {
    const material::Material* worn = pending.back();
    pending.pop_back();
    if (std::find(walked.begin(), walked.end(), worn) != walked.end()) continue;
    walked.push_back(worn);
    for (const auto& [name, slot] : worn->slots()) {
      if (slot.material) {
        pending.push_back(slot.material.get());
      } else if (const auto* map =
                     dynamic_cast<const material::Texture*>(slot.leaf.get())) {
        gpu.maps.prepare(*map, time);
      }
    }
  }
}

}  // namespace

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

void prepareFixedPipelines(Gpu& gpu) {
  // THE BODIES' OWN: a stand-in and a body with no Slang body of its own,
  // opaque, with its back faces dropped and kept, and the two depth-only
  // programs a shadow map is drawn with.
  for (const bool cull : {true, false}) {
    gpu.pipelines.prepare(bodyKey(scaffold(false), Blending{}, cull, nullptr));
    gpu.pipelines.prepare(bodyKey(scaffold(true), Blending{}, cull, nullptr));
  }
  const Caster depthOnly{};
  gpu.pipelines.prepare(bodyKey(shadowScaffold(/*coverage=*/false), Blending{},
                                false, &depthOnly));
  gpu.pipelines.prepare(bodyKey(shadowScaffold(/*coverage=*/true), Blending{},
                                false, &depthOnly));
  // THE FRAME'S OWN: the sky, the clear, a copy and the final stage, each a
  // triangle over the whole target, the last into the display's format.
  const PostPrograms& programs = postPrograms();
  const auto stage = [&](const material::slang::Compiled& program,
                         dg::TEXTURE_FORMAT target) {
    PipelineKey key{&program, SkBlendMode::kSrc, false, false, true};
    key.target = target;
    gpu.pipelines.prepare(key);
  };
  stage(backdropProgram(), kRadianceFormat);
  stage(programs.fill, kRadianceFormat);
  stage(programs.copy, kRadianceFormat);
  stage(programs.final, kColorFormat);
}

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
  gpu.shadows.clear();
  gpu.standIns = view.standIns;
  gpu.programsPending = 0;
  // The programs a light kind entering started on an earlier pass are
  // waited for here, before anything is drawn, so the stand-ins it cost
  // stop at the pass that met it.
  for (const auto& [recipe, variant] : gpu.awaitedPrograms)
    (void)material::ProgramCache::shared().program(
        recipe, material::Target::Slang, material::Variant{variant});
  gpu.awaitedPrograms.clear();
  // Every body this pass drew as a stand-in is told to the scene, however
  // the pass returns.
  struct Report {
    Gpu& gpu;
    Targets& targets;
    ~Report() { targets.addProgramsPending(gpu.programsPending); }
  } report{gpu, targets};
  if (asCoverage) {
    // Coverage is where the selection is, not what it looks like: flat
    // white, and nothing else reaches it.
    openTarget(gpu, colour, clearNothing, true);
    const glm::vec4 white{1, 1, 1, 1};
    drawBodies(gpu, view, viewProj, viewMatrix, &pass.collection(),
               /*lit=*/false, &white, colour);
    gpu.mark("coverage");
    return;
  }

  // THE PAGES' CHAINS, before any target is opened: the shadow stage
  // below reads a page's opacity and transmission too.
  preparePages(gpu, view);
  // THE PANORAMA'S FILTERED CHAIN, before the target is opened: made on
  // the device the first time a sky is met, which draws into a target of
  // its own.
  if (view.environment.valid()) {
    (void)prefilteredPanorama(gpu, gpu.maps.environment(view.environment.map));
    (void)prefilteredPanorama(gpu, gpu.maps.environment(view.environment.next));
  }
  // THE SHADOW STAGE, before the target is opened: the maps every lit
  // body below reads.
  const int64_t mapsDrawn = drawShadows(gpu, view, viewMatrix);
  targets.addShadowMapsDrawn(mapsDrawn +
                             drawTints(gpu, view, viewMatrix, mapsDrawn));
  gpu.mark("shadow");
  // THE BACKDROPS' TEXTURES, before the target is opened, so a page met
  // here is wrapped and waited for outside the pass that lays it.
  std::vector<dg::ITexture*> backdrops;
  for (const material::Texture& picture : view.backdrops)
    if (dg::ITexture* texture = gpu.maps.sample(
            picture, {.mipmaps = false},
            std::chrono::duration<double>(gpu.seconds)))
      backdrops.push_back(texture);
  // THE CLEAR IS AN AUTHORED COLOUR, so it is filled as the light the
  // final stage turns back into it, and the depth is cleared beside it.
  openTarget(gpu, colour, clearNothing, true);
  if (!gpu.shadows.empty()) {
    // The stage drew into tiles of the atlas; the picture is drawn whole.
    dg::Viewport whole;
    whole.Width = (float)gpu.extent.width();
    whole.Height = (float)gpu.extent.height();
    gpu.device->context()->SetViewports(1, &whole,
                                        (dg::Uint32)gpu.extent.width(),
                                        (dg::Uint32)gpu.extent.height());
  }
  fillTarget(gpu, colour, pass.clear());
  resumeTarget(gpu, colour, true);
  // The PROJECTION alone is what turns a pixel into a ray, and it is the
  // clip transform with the view taken back out of it — the clip
  // transform is where the target's aspect and the depth convention
  // already are, so deriving it is what keeps the sky's rays and the
  // bodies' positions answering to one camera.
  drawBackdrop(gpu, view, viewProj * glm::inverse(viewMatrix), viewMatrix);
  // THE FRAME'S BACKDROPS, over the clear and the sky and under every
  // body: each the whole picture, of which a target that is a part of it
  // shows its own window.
  if (!backdrops.empty()) {
    glm::vec4 window{0.0f, 0.0f, 1.0f, 1.0f};
    if (isPart(view.offset, view.extent))
      window = {(float)view.offset.origin.x / (float)view.offset.whole.x,
                (float)view.offset.origin.y / (float)view.offset.whole.y,
                (float)view.extent.x / (float)view.offset.whole.x,
                (float)view.extent.y / (float)view.offset.whole.y};
    for (dg::ITexture* picture : backdrops)
      layAuthored(gpu, colour, picture, window);
    resumeTarget(gpu, colour, true);
  }
  const bool cull = work.realisation == Selection::Cull;
  drawBodies(gpu, view, viewProj, viewMatrix,
             cull ? &pass.collection() : nullptr, /*lit=*/true, nullptr, colour,
             /*timed=*/true);

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
    // that pair folds to. A scene that has not moved between two frames is
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
  gpu.mark("extras");
}

}  // namespace sigil::world::diligent
