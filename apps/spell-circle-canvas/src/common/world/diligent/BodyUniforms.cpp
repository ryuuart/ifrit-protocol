/** @file
 * What a body's draw writes into its program's uniforms: every light as
 * the scaffold reads it, the shadow maps and which light reads which, the
 * camera its instances turn to, the material's resolved bytes, and the
 * placement, camera reading and sky the scaffold needs.
 */

#include <sigilmaterial/advanced/Terms.h>
#include <sigilmaterial/core/Instance.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Parameters.h>
#include <sigilworld/advanced/Light.h>

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/trigonometric.hpp>
#include <iterator>
#include <span>

#include "GeometryPass.h"

namespace sigil::world::diligent {

namespace {

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

}  // namespace

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

}  // namespace sigil::world::diligent
