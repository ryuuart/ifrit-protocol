/** @file
 * dart_flight — a body riding a curve, aimed by the curve.
 *
 * One winding closed loop, swept into a rail; a chrome dart flying it;
 * and a line of gates standing on the same loop at fixed distances, so
 * the dart's travel can be read against something that does not move.
 *
 * EVERYTHING HERE IS `along()`. A node given a spline and a distance
 * stands at that distance and is TURNED ONTO THE CURVE'S OWN FRAME — the
 * tangent is its up axis, so a body modelled nose-up flies nose-first
 * without a matrix being written anywhere. The distance is a lane like
 * any other: the dart's is a function of the scene time, the gates' are
 * constants, and one verb serves both. The gates each add a turn of
 * their own INSIDE that frame, which is what the composition rule means
 * — `along()` replaces the translation and the axis turn, and the three
 * rotation lanes still apply after it.
 *
 * The rail is `curve::sweep` over the same spline the dart rides, so
 * there is exactly one curve in this file and both the picture and the
 * flight are read off it.
 */

#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/kit/Kit.h>

#include <cmath>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;

using namespace sigil::world;

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr float kTwoPi = 6.283185307179586f;
/** How many gates stand on the loop, and how fast the dart flies it —
 *  loops per second, so one number says both how long a lap takes and
 *  how far apart two frames are. */
constexpr int kGates = 9;
constexpr float kLapsPerSecond = 0.11f;

/** THE FLIGHT LOOP: a closed winding that leaves its own plane, so the
 *  rail crosses in front of and behind itself and a body riding it is
 *  seen from every side of the frame during one lap. The two rates are
 *  coprime, which is what keeps a later wrap from retracing an earlier
 *  one. */
Spline3 flight() {
  Spline3 loop;
  constexpr int kKnots = 72;
  constexpr float kWraps = 3.0f;    // rises and falls per lap
  constexpr float kPrecess = 2.0f;  // turns of the winding plane per lap
  constexpr glm::vec3 kShell{230.0f, 120.0f, 190.0f};
  for (int i = 0; i < kKnots; ++i) {
    const float t = (float)i / (float)kKnots;
    const float latitude = std::sin(kTwoPi * kWraps * t);
    const float azimuth = -kTwoPi * kPrecess * t;
    loop.points.emplace_back(kShell.x * std::cos(latitude) * std::cos(azimuth),
                             kShell.y * std::sin(latitude),
                             kShell.z * std::cos(latitude) * std::sin(azimuth));
  }
  loop.closed = true;
  return loop;
}

/** The dart: a profile revolved about its own up axis, nose at the top.
 *  `along()` puts the tangent where that axis is, so this is the whole
 *  of aiming it. */
gm::Mesh dart() {
  const std::vector<glm::vec2> profile = {
      {0, 52}, {12, 18}, {17, -9}, {9, -26}, {0, -30}};
  return gm::revolve(profile, {.segments = 20});
}

/** A gate: a hexagonal ring the rail passes through, its hole about the
 *  same axis the dart's nose points along — so the curve's own frame
 *  stands it ACROSS the flight and nothing here has to say so. Six sides
 *  rather than round, because a round ring's roll about the rail is a
 *  turn with nothing to see and a hexagon's is the point. */
gm::Mesh gate() { return gm::torus(26.0f, 3.0f, 6, 10); }

}  // namespace

namespace {

struct DartFlight final : sketch::Set {
  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(880, 600);
    ctx.background({0.028f, 0.032f, 0.046f, 1.0f});
    ctx.captureAt(1.5);
  }

  world::Frame describe(float seconds) override {
    const Spline3 loop = flight();
    const float lap = loop.length();

    const gm::Mesh rail =
        gm::curve::sweep(loop, gm::curve::profile::circle(12),
                         {.segments = 260,
                          .scale = 3.4f,
                          .normals = gm::curve::SweepOptions::Normals::Radial});

    const material::Material chrome = material::kit::surface(
        {.baseColor = {0.90f, 0.93f, 1.0f, 1.0f}, .roughness = 0.12f});
    const material::Material wire = material::kit::surface(
        {.baseColor = {0.38f, 0.44f, 0.60f, 1.0f}, .roughness = 0.5f});
    const material::Material brass = material::kit::surface(
        {.baseColor = {0.86f, 0.62f, 0.28f, 1.0f}, .roughness = 0.35f});

    Element subject = Element().key("flight");
    subject.child(Element().key("rail").mesh(rail).fill(wire).tag("rail"));

    // The gates: one node per station, each standing at a constant
    // distance along the loop and ROLLED about the rail inside the frame
    // the curve put it in — which is `along()` composing with the
    // rotation lanes rather than replacing them.
    for (int i = 0; i < kGates; ++i) {
      const float where = lap * ((float)i / (float)kGates);
      subject.child(Element()
                        .key("gate" + std::to_string(i))
                        .along(loop, where)
                        .rotateY((float)i * 11.0f)
                        .mesh(gate())
                        .fill(brass)
                        .tag("gate"));
    }

    // …and the dart, on the same curve, at a distance that is a function
    // of the moment and of nothing else.
    subject.child(
        Element()
            .key("dart")
            .along(loop, std::fmod(seconds * kLapsPerSecond, 1.0f) * lap)
            .mesh(dart())
            .fill(chrome)
            .tag("dart"));

    kit::Set set;
    set.rig.extent = 220.0f;
    set.rig.bearing = -30.0f;
    set.rig.elevation = 32.0f;
    set.rig.intensity = 1.1f;
    set.ground = 4.0f;
    set.drop = 0.85f;
    set.table.radius = 700.0f;
    set.table.height = 300.0f;
    set.table.period = 18.0f;
    set.table.fovYDeg = 42.0f;
    return Frame(kit::litSet(std::move(subject), set, seconds));
  }
};

}  // namespace

SIGIL_SKETCH(DartFlight, "Set",
             "A dart flying a closed loop \xe2\x80\x94 one curve, swept into "
             "a rail and ridden by everything on it, each node aimed by the "
             "curve's own frame")
