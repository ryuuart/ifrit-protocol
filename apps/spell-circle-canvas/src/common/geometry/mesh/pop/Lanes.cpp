/** @file
 * THE TWO ENDS OF EVERY COOK: what an untouched lane holds, the table
 * between a Cloud's lane names and this language's attribute names, the
 * generator that seeds a chain's store, how a given cloud lays out as
 * attributes, and the reading that pours a store back into a Cloud.
 *
 * A generator is not a map over points and no kernel replaces it, so an
 * executor that performs the filters somewhere else still seeds and
 * exports through these: two ends written once are what make the middle
 * between them comparable.
 */

#include <sigilcore/compute/Noise.h>
#include <sigilcore/schedule/Parallel.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <string>

#include "CookInternal.h"
#include "sigilgeometry/mesh/pop/Pop.h"

namespace sigil::geometry::mesh {

// The tier's other features this file stands on, pulled in so the code
// below reads as one vocabulary.
using curve::Spline3;

namespace {

float wrap01(float t) { return t - std::floor(t); }

}  // namespace

glm::vec4 pop::laneFill(std::string_view name) {
  if (name == "Scale" || name == "Color") return {1, 1, 1, 1};
  if (name == "Tex") return {0, 0, 1, 1};
  if (name == "Dir") return {0, 0, 1, 0};
  return {0, 0, 0, 0};
}

std::string_view pop::attrFor(std::string_view lane) {
  if (lane == "t") return "T";
  if (lane == "size") return "Scale";
  if (lane == "dir" || lane == "normal") return "Dir";
  if (lane == "tint") return "Color";
  return lane;
}

std::string_view pop::cloudLaneFor(std::string_view attr) {
  if (attr == "T") return "t";
  if (attr == "Scale") return "size";
  if (attr == "Dir") return "dir";
  if (attr == "Color") return "tint";
  return attr;
}

void pop::seedAttrs(const Cloud& cloud, pop::Lanes& lanes) {
  const size_t n = cloud.size();
  const auto lane = [&](const std::string& name, glm::vec4 fill) -> auto& {
    auto [it, inserted] = lanes.try_emplace(name);
    if (inserted || it->second.size() != n) it->second.assign(n, fill);
    return it->second;
  };
  std::vector<glm::vec4>& P = lane("P", {0, 0, 0, 0});
  core::schedule::parallelFor(n, kLaneGrain, [&](size_t first, size_t last) {
    for (size_t i = first; i < last; ++i)
      P[i] = {cloud.positions[i].x, cloud.positions[i].y, cloud.positions[i].z,
              0};
  });
  lane("T", {0, 0, 0, 0});
  lane("Dir", {0, 0, 1, 0});
  lane("Scale", {1, 1, 1, 1});
  lane("Color", {1, 1, 1, 1});
  lane("Tex", {0, 0, 1, 1});
  for (const auto& [name, values] : cloud.scalars) {
    if (values.size() != n) continue;
    const std::string target(attrFor(name));
    std::vector<glm::vec4>& out = lane(target, {0, 0, 0, 0});
    core::schedule::parallelFor(n, kLaneGrain, [&](size_t first, size_t last) {
      for (size_t i = first; i < last; ++i)
        out[i] = target == "Scale"
                     ? glm::vec4{values[i], values[i], values[i], values[i]}
                     : glm::vec4{values[i], 0, 0, 0};
    });
  }
  for (const auto& [name, values] : cloud.vectors) {
    if (values.size() != n) continue;
    // "dir" is the cook's own export; "normal" is what generators and
    // importers write. The table maps either onto Dir, so "dir" has to
    // win where both exist.
    const bool skip = name == "normal" && cloud.vectorIf("dir");
    const std::string target(skip ? std::string_view(name) : attrFor(name));
    std::vector<glm::vec4>& out = lane(target, {0, 0, 1, 0});
    core::schedule::parallelFor(n, kLaneGrain, [&](size_t first, size_t last) {
      for (size_t i = first; i < last; ++i)
        out[i] = {values[i].x, values[i].y, values[i].z, 0};
    });
  }
  for (const auto& [name, values] : cloud.colors) {
    if (values.size() != n) continue;
    const std::string target(attrFor(name));
    lane(target, {1, 1, 1, 1}) = values;
  }
}

size_t pop::seedLanes(const pop::Chain& chain, pop::Lanes* lanes) {
  if (!lanes || chain.empty()) return 0;
  const auto* scatter = std::get_if<pop::SplineScatter>(&chain.front());
  const auto* surface = std::get_if<pop::MeshScatter>(&chain.front());
  const auto* given = std::get_if<pop::PointSet>(&chain.front());
  if (scatter && (scatter->loop.size() < 3 || scatter->count < 1)) return 0;
  if (surface && (surface->mesh.indices.empty() || surface->count < 1))
    return 0;
  if (given && given->cloud.positions.empty()) return 0;
  if (!scatter && !surface && !given) return 0;

  // THE SURFACE SCATTER IS RUN FIRST, because how many points it made
  // is how many this chain has. `points::onMesh` answers an empty cloud
  // for a mesh of no area, and returning the requested count over it
  // would cook a chain of points nothing ever placed — every one of them
  // at the origin.
  Cloud seeds;
  if (surface) {
    seeds = points::onMesh(surface->mesh, surface->count, surface->seed);
    if (seeds.positions.empty()) return 0;
  }

  const size_t count = scatter ? (size_t)scatter->count
                       : surface
                           ? std::min((size_t)surface->count, seeds.size())
                           : given->cloud.size();
  const auto lane = [&](const std::string& name) -> auto& {
    auto [it, inserted] = lanes->try_emplace(name);
    if (inserted) it->second.assign(count, pop::laneFill(name));
    return it->second;
  };
  std::vector<glm::vec4>& laneP = lane("P");
  std::vector<glm::vec4>& laneT = lane("T");
  std::vector<glm::vec4>& laneDir = lane("Dir");
  lane("Scale");
  lane("Color");

  if (given) pop::seedAttrs(given->cloud, *lanes);

  if (surface) {
    const std::vector<glm::vec3>* normals = seeds.vectorIf("normal");
    core::schedule::parallelFor(
        count, kLaneGrain, [&](size_t first, size_t last) {
          for (size_t i = first; i < last; ++i) {
            const glm::vec3& p = seeds.positions[i];
            laneP[i] = {p.x, p.y, p.z, 0};
            laneT[i] = {((float)i + 0.5f) / (float)count, 0, 0, 0};
            if (normals) {
              const glm::vec3& n = (*normals)[i];
              laneDir[i] = {n.x, n.y, n.z, 0};
            }
          }
        });
  }

  if (scatter) {
    Spline3 spline;
    spline.points = scatter->loop;
    spline.closed = true;
    core::schedule::parallelFor(
        count, kLaneGrain, [&](size_t first, size_t last) {
          for (size_t i = first; i < last; ++i) {
            const uint32_t seed = scatter->seed;
            const float u0 = ((float)i + 0.5f) / (float)count;
            const float t =
                scatter->head - scatter->span + scatter->span * u0 +
                (core::noise::pcgUnit((uint32_t)i * 3u + seed) - 0.5f) *
                    (scatter->span / (float)count) * 4.0f;
            const glm::vec3 p = spline.position(wrap01(t));
            glm::vec3 tangent = spline.position(wrap01(t + 0.002f)) -
                                spline.position(wrap01(t - 0.002f));
            const float len = glm::length(tangent);
            tangent = len > 1e-6f ? tangent * (1.0f / len) : glm::vec3{1, 0, 0};
            glm::vec3 n0 = std::abs(tangent.y) < 0.9f
                               ? glm::cross(tangent, {0, 1, 0})
                               : glm::cross(tangent, {1, 0, 0});
            n0 = n0 * (1.0f / glm::length(n0));
            const glm::vec3 b0 = glm::cross(tangent, n0);
            const float ang =
                core::noise::pcgUnit((uint32_t)i * 7u + seed + 2u) * 6.2831853f;
            const float rad =
                std::sqrt(core::noise::pcgUnit((uint32_t)i * 5u + seed + 3u)) *
                scatter->radius;
            const glm::vec3 placed =
                p + (n0 * std::cos(ang) + b0 * std::sin(ang)) * rad;
            laneP[i] = {placed.x, placed.y, placed.z, 0};
            laneT[i] = {u0, 0, 0, 0};
            laneDir[i] = {tangent.x, tangent.y, tangent.z, 0};
          }
        });
  }
  return count;
}

Cloud pop::exportLanes(const pop::Lanes& lanes, size_t count) {
  Cloud out;
  if (count == 0) return out;
  const auto find = [&](const char* name) -> const std::vector<glm::vec4>* {
    const auto it = lanes.find(std::string_view(name));
    return it == lanes.end() ? nullptr : &it->second;
  };
  const std::vector<glm::vec4>* P = find("P");
  const std::vector<glm::vec4>* T = find("T");
  const std::vector<glm::vec4>* Dir = find("Dir");
  const std::vector<glm::vec4>* Scale = find("Scale");
  const std::vector<glm::vec4>* Color = find("Color");
  if (!P || !T || !Dir || !Scale || !Color) return out;

  out.positions.resize(count);
  std::vector<float>& t = out.scalar(std::string(cloudLaneFor("T")));
  std::vector<glm::vec3>& dir = out.vector(std::string(cloudLaneFor("Dir")));
  std::vector<float>& size = out.scalar(std::string(cloudLaneFor("Scale")), 1);
  std::vector<glm::vec4>& tint = out.color(std::string(cloudLaneFor("Color")));
  core::schedule::parallelFor(count, kLaneGrain,
                              [&](size_t first, size_t last) {
                                for (size_t i = first; i < last; ++i) {
                                  const glm::vec4 p = (*P)[i];
                                  out.positions[i] = {p.x, p.y, p.z};
                                  t[i] = (*T)[i].x;
                                  const glm::vec4 d = (*Dir)[i];
                                  dir[i] = {d.x, d.y, d.z};
                                  size[i] = (*Scale)[i].x;
                                  tint[i] = (*Color)[i];
                                }
                              });
  for (const auto& [name, lane] : lanes) {
    if (name == "P" || name == "T" || name == "Dir" || name == "Scale" ||
        name == "Color")
      continue;
    std::vector<glm::vec4>& exported = out.color(name);
    for (size_t i = 0; i < count; ++i) exported[i] = lane[i];
  }
  return out;
}

}  // namespace sigil::geometry::mesh
