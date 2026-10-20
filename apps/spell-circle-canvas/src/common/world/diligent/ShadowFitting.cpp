/** @file
 * Where each casting light's shadow map looks from: a directional light's
 * orthographic map, held still as the camera turns, and its cascades along
 * the camera's depth; a spot light's perspective map; a point light's six
 * faces. And where a body stands in the world, which every fit covers.
 */

#include <sigilworld/advanced/Light.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <vector>

#include "GeometryPass.h"

namespace sigil::world::diligent {

// THE FITTING IS WRITTEN HERE. DiligentFX's ShadowMapManager fits cascades
// for one directional light into a texture array of its own; a frame here
// holds directional and spot lights side by side, each with its own map
// size, shadow link and bias, in one atlas the surface program reads with
// the rest of its slots — which that manager neither allocates nor fits.

namespace {

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

/** THE WIDTH ONE FACE OF A POINT LIGHT'S MAP OPENS beyond a quarter turn,
 *  as the tangent of its half angle: a little past one, so the filter's
 *  taps near a face's edge still read that face. */
constexpr float kFaceReach = 1.05f;

}  // namespace

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

}  // namespace sigil::world::diligent
