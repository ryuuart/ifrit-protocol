/** @file
 * The presets: where three lights stand round a subject, the rail a
 * turntable rides, and the set both of them make with a ground plane.
 */

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmotion/values/Time.h>
#include <sigilworld/kit/Kit.h>

#include <cmath>
#include <string_view>
#include <utility>

namespace sigil::world::kit {

namespace {

constexpr float kDegrees = 3.14159265358979323846f / 180.0f;

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

/** One emitter of the rig: a lamp standing at @p position, reaching far
 *  enough past the subject that its falloff is not what the arrangement
 *  is about. */
Element lamp(std::string_view key, glm::vec3 position, glm::vec4 color,
             float intensity, float reach) {
  return Element().key(key).light(
      light::point(position, color, intensity, reach));
}

}  // namespace

Element threePoint(const Rig& rig) {
  const float distance = rig.distance * rig.extent;
  const float reach = distance * 4.0f;
  return Element()
      .key("rig")
      .child(lamp("key", station(rig.at, distance, rig.bearing, rig.elevation),
                  rig.color, rig.intensity, reach))
      // A quarter turn the other way and lower: the fill is what keeps
      // the side the key does not reach from being black.
      .child(lamp(
          "fill",
          station(rig.at, distance, rig.bearing + 90.0f, rig.elevation * 0.4f),
          rig.color, rig.intensity * rig.fill, reach))
      // Opposite and higher: the back light is what separates the
      // subject from what stands behind it.
      .child(lamp(
          "back",
          station(rig.at, distance, rig.bearing + 180.0f, rig.elevation * 1.6f),
          rig.color, rig.intensity * rig.back, reach));
}

Spline3 rail(const Turntable& table) {
  Spline3 spline;
  const int stations = table.stations < 3 ? 3 : table.stations;
  const float turn = 6.283185307179586f / (float)stations;
  for (int i = 0; i < stations; ++i) {
    const float angle = (float)i * turn;
    spline.points.emplace_back(table.at.x + table.radius * std::cos(angle),
                               table.at.y + table.height,
                               table.at.z + table.radius * std::sin(angle));
  }
  spline.closed = true;
  return spline;
}

Element turntable(const Turntable& table, float seconds) {
  const Spline3 track = rail(table);
  Camera lens;
  lens.eye = {0.0f, 0.0f, 0.0f};
  lens.target = {table.radius, -table.height, 0.0f};
  lens.fovYDeg = table.fovYDeg;

  // One turn per period, as a wrapping phase along the rail. A
  // non-positive period is a still camera, which is what the phase
  // answers 0 for.
  const float travelled = motion::phase(seconds, table.period) * track.length();
  return Element().key("camera").along(track, travelled).camera(lens);
}

Element litSet(Element subject, const Set& set, float seconds) {
  Element root;
  root.key("set");
  if (set.ground > 0.0f) {
    const float side = set.ground * set.rig.extent;
    material::Material surface =
        set.surface ? *set.surface
                    : material::kit::surface(
                          {.baseColor = {kNeutralGround.r, kNeutralGround.g,
                                         kNeutralGround.b, kNeutralGround.a}});
    root.child(Element()
                   .key("ground")
                   .at({set.rig.at.x, set.rig.at.y - set.drop * set.rig.extent,
                        set.rig.at.z})
                   // A quad stands up; a ground plane is one laid down.
                   .rotateX(-90.0f)
                   .mesh(geometry::mesh::quad(side, side))
                   .fill(std::move(surface))
                   .tag("ground"));
  }
  root.child(threePoint(set.rig));
  root.child(turntable(set.table, seconds));
  root.child(Element().key("subject").child(std::move(subject)));
  return root;
}

}  // namespace sigil::world::kit
