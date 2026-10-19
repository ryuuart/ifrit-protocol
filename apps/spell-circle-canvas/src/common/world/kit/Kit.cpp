/** @file
 * The presets: where three lights stand round a subject, the rail a
 * turntable rides, the set both of them make with a ground plane, the
 * two other rails — a loop that rises and falls, and a winding round a
 * shell — and an environment map baked from a gradient and soft boxes.
 */

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmotion/values/Time.h>
#include <sigilworld/kit/Kit.h>
#include <sigilworld/node/Light.h>
#include <sigilworld/node/Mesh.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::world::kit {

namespace {

constexpr float kDegrees = 3.14159265358979323846f / 180.0f;
constexpr float kTwoPi = 6.283185307179586f;

/** At least three stations: a closed loop needs three points to be one. */
int atLeastThree(int stations) { return stations < 3 ? 3 : stations; }

/** How far the key stands from the subject, in the rig's extents. */
constexpr float kKeyDistance = 2.2f;

/** The one colour this library states: a ground plane that was given no
 *  surface. Mid grey, so that what stands on it is what is read. */
constexpr glm::vec4 kNeutralGround{0.18f, 0.18f, 0.19f, 1.0f};

/** A station on a circle round @p at: @p bearing degrees about the up
 *  axis, @p elevation degrees above the plane, @p distance away. */
glm::vec3 station(glm::vec3 at, float distance, float bearing,
                  float elevation) {
  const float yaw = bearing * kDegrees;
  const float pitch = elevation * kDegrees;
  const float flat = distance * std::cos(pitch);
  return at + glm::vec3(flat * std::sin(yaw), distance * std::sin(pitch),
                        flat * std::cos(yaw));
}

/** One light of the rig: a point light standing at @p position, with no
 *  range, at the intensity that delivers @p strength at the subject. */
Light lamp(std::string_view key, glm::vec3 position,
           motion::Animatable<material::Color> color,
           motion::Animatable<float> strength, bool casts = false,
           LightShadow shadow = {}) {
  return pointLight({.color = std::move(color),
                     .intensity = intensityAt(std::move(strength),
                                              glm::length(position)),
                     .castShadow = casts,
                     .shadow = shadow})
      .key(key)
      .at(position);
}

}  // namespace

Group threePoint(const Rig& rig) {
  const glm::vec3 centre{0.0f, 0.0f, 0.0f};
  const float distance = kKeyDistance * rig.extent;
  std::vector<Node> lights{
      lamp("key", station(centre, distance, rig.bearing, rig.elevation),
           rig.color, rig.intensity, rig.keyCastsShadow, rig.keyShadow),
      // A quarter turn the other way and lower: the fill is what
      // keeps the side the key does not reach from being black.
      lamp("fill",
           station(centre, distance, rig.bearing + 90.0f, rig.elevation * 0.4f),
           rig.fillColor, rig.fillIntensity),
      // Opposite and higher: the back light is what separates the
      // subject from what stands behind it.
      lamp(
          "back",
          station(centre, distance, rig.bearing + 180.0f, rig.elevation * 1.6f),
          rig.backColor, rig.backIntensity),
  };
  if (rig.ambientIntensity > 0.0f)
    lights.push_back(ambientLight({.color = rig.ambientColor,
                                   .intensity = rig.ambientIntensity})
                         .key("ambient"));
  return group(lights).key("rig");
}

geometry::mesh::curve::Spline3 rail(const Turntable& table) {
  geometry::mesh::curve::Spline3 spline;
  const int stations = atLeastThree(table.stations);
  const float turn = kTwoPi / (float)stations;
  for (int i = 0; i < stations; ++i) {
    const float angle = (float)i * turn;
    spline.points.emplace_back(table.at.x + table.radius * std::cos(angle),
                               table.at.y + table.height,
                               table.at.z + table.radius * std::sin(angle));
  }
  spline.closed = true;
  return spline;
}

geometry::mesh::curve::Spline3 wave(const Wave& wave) {
  geometry::mesh::curve::Spline3 spline;
  const int knots = atLeastThree(wave.knots);
  for (int i = 0; i < knots; ++i) {
    const float angle = (float)i * kTwoPi / (float)knots;
    const bool outer = i % 2 == 0;
    const float radius = outer ? wave.radius : wave.inner;
    const float height = outer ? wave.high : wave.low;
    spline.points.emplace_back(wave.at.x + radius * std::cos(angle),
                               wave.at.y + height,
                               wave.at.z + radius * std::sin(angle));
  }
  spline.closed = true;
  return spline;
}

geometry::mesh::curve::Spline3 winding(const Winding& winding) {
  geometry::mesh::curve::Spline3 spline;
  const int knots = atLeastThree(winding.knots);
  for (int i = 0; i < knots; ++i) {
    const float t = (float)i / (float)knots;
    // Latitude swings between ±1 radian `wraps` times a lap while the
    // azimuth makes `turns` laps of its own; the two counts sharing no
    // factor is what keeps the wraps from landing on one another.
    const float latitude = std::sin(kTwoPi * winding.wraps * t);
    const float azimuth = -kTwoPi * winding.turns * t;
    spline.points.emplace_back(
        winding.at.x + winding.shell.x * std::cos(latitude) * std::cos(azimuth),
        winding.at.y + winding.shell.y * std::sin(latitude),
        winding.at.z +
            winding.shell.z * std::cos(latitude) * std::sin(azimuth));
  }
  spline.closed = true;
  return spline;
}

Camera turntable(const Turntable& table, motion::Duration now) {
  const geometry::mesh::curve::Spline3 track = rail(table);
  geometry::mesh::camera::Camera lens;
  lens.eye = {0.0f, 0.0f, 0.0f};
  lens.target = {table.radius, -table.height, 0.0f};
  lens.fovYDegrees = table.fieldOfView;

  // One turn per period, as a wrapping phase along the rail. A
  // non-positive period is a still camera, which is what the phase
  // answers 0 for.
  const float travelled = motion::phase(now, table.period) * track.length();
  return camera(lens).key("camera").along(track, travelled);
}

Group litSet(Node subject, const Set& set, motion::Duration now) {
  std::vector<Node> children;
  if (set.ground > 0.0f) {
    const float side = set.ground * set.rig.extent;
    material::Material surface =
        set.surface ? *set.surface
                    : material::surface::program(
                          {.baseColor = {kNeutralGround.r, kNeutralGround.g,
                                         kNeutralGround.b, kNeutralGround.a}});
    children.push_back(mesh(geometry::mesh::quad(side, side))
                           .key("ground")
                           .at({0.0f, -set.drop * set.rig.extent, 0.0f})
                           // A quad stands up; a ground plane is one laid
                           // down.
                           .rotateX(-90.0f)
                           .fill(std::move(surface))
                           .collection("ground"));
  }
  children.push_back(threePoint(set.rig));
  children.push_back(turntable(set.table, now));
  children.push_back(group({std::move(subject)}).key("subject"));
  return group(children).key("set");
}

Group visibleLight(Light light) {
  const LightDescription& described = light.description();
  const float radius = described.radius;
  std::optional<geometry::mesh::Mesh> shape;
  switch (described.kind) {
    case LightKind::Rect:
      shape = geometry::mesh::quad(described.width, described.height);
      break;
    case LightKind::Disk:
      // A fan of rings in the xy plane, its face toward +z.
      shape = geometry::mesh::grid(
          48, 4, [radius](float u, float v) -> std::pair<glm::vec3, glm::vec3> {
            const float angle = -u * kTwoPi;
            return {{std::cos(angle) * radius * v, std::sin(angle) * radius * v,
                     0.0f},
                    {0.0f, 0.0f, 1.0f}};
          });
      break;
    case LightKind::Sphere:
      shape = geometry::mesh::superellipsoid({radius, radius, radius}, 2.0f, 48,
                                             32);
      break;
    default:
      break;
  }
  // The way the light shines is the group's forward axis, so an aim
  // turns the light and the panel together onto what it looks at.
  const glm::vec3 ahead = glm::length(described.direction) > 1e-5f
                              ? glm::normalize(described.direction)
                              : glm::vec3(0, -1, 0);
  if (!shape) {
    Group alone = group({std::move(light)});
    alone.edit().forward = ahead;
    return alone;
  }
  // The mesh's face, +z, turned onto the way the light shines, its x axis
  // onto the light's width axis: the node's x made square to the
  // direction, or its z for a light shining along x.
  glm::vec3 across = glm::vec3(1, 0, 0) - ahead * ahead.x;
  if (glm::dot(across, across) < 1e-6f)
    across = glm::vec3(0, 0, 1) - ahead * ahead.z;
  across = glm::normalize(across);
  const glm::vec3 up = glm::cross(ahead, across);
  const glm::mat4 facing(glm::vec4(across, 0), glm::vec4(up, 0),
                         glm::vec4(ahead, 0), glm::vec4(0, 0, 0, 1));
  const material::Color colour = described.color.value();
  Node emitter = mesh(std::move(*shape))
                     .key("emitter")
                     .transform(facing)
                     .castShadow(false)
                     .receiveShadow(false)
                     .fill(material::surface::unlit(
                         {.baseColor = {colour.r, colour.g, colour.b, 1}}));
  Group seen = group({std::move(light), std::move(emitter)});
  seen.edit().forward = ahead;
  return seen;
}

EnvironmentMap environmentMap(const EnvironmentMapOptions& options) {
  return EnvironmentMap::baked(
      options.width, [options](float u, float v) -> glm::vec3 {
        const float share = options.exponent == 1.0f
                                ? 1.0f - v
                                : std::pow(1.0f - v, options.exponent);
        glm::vec3 radiance =
            options.nadir + (options.zenith - options.nadir) * share;
        for (const SoftBox& box : options.softBoxes) {
          // Round the horizon the map wraps, so the nearer way round is
          // the distance a box falls off over.
          const float apart = std::abs(u - box.centre.x);
          const float across = std::min(apart, 1.0f - apart) / box.size.x;
          const float down = (v - box.centre.y) / box.size.y;
          radiance +=
              box.radiance * std::exp(-0.5f * (across * across + down * down));
        }
        return radiance;
      });
}

}  // namespace sigil::world::kit
