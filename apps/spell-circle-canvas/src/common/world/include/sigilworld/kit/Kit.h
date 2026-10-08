#pragma once

/** @file
 * @ingroup world-kit
 * Presets: nodes composed out of the verbs a tree is already written in
 * — a three-point rig, a turntable, and a set with both over a ground
 * plane — the rails a body rides, an environment map made from a
 * gradient and soft boxes, and a light with a size made visible. NOTHING HERE
 * DECIDES A LOOK: each returns an ordinary value whose every part the caller
 * can read, replace or ignore, and the only constants in it are the geometry of
 * the arrangement plus one neutral grey for an unsurfaced ground.
 */

#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmotion/time/Duration.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilworld/frame/Environment.h>
#include <sigilworld/node/Camera.h>
#include <sigilworld/node/Group.h>
#include <sigilworld/node/Light.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <optional>
#include <vector>

/** STOCK ARRANGEMENTS OVER THE NODE KINDS: values that compose the same
 *  verbs an author would write, so a scene is described in one call
 *  instead of a dozen. Reach for it for a lighting rig or another
 *  well-worn arrangement. Nothing here decides anything the node kinds
 *  do not already say. */
namespace sigil::world::kit {

/** WHERE THREE LIGHTS STAND ROUND A SUBJECT, stated relative to the
 *  subject so one rig serves a thumbnail and a room: `extent` is how far
 *  across the subject is and every distance is a multiple of it, and
 *  `bearing` turns the whole rig about its up axis. The rig stands at
 *  the origin of the group it returns, so `.at()` on that group moves it
 *  with its subject. */
struct Rig {
  /** How far across the subject is. */
  float extent = 100.0f;
  /** The key's bearing round the subject and its height above it, in
   *  degrees. The fill stands a quarter turn the other way and lower;
   *  the back light stands opposite and higher. */
  float bearing = -35.0f;
  float elevation = 28.0f;
  /** THE KEY'S STRENGTH — what it delivers at the subject, which the
   *  rig turns into an intensity at the distance the key stands — and its
   *  colour. Both bindable, so a live value dims and tints the key
   *  without the rig being made again. A white matte surface square to it
   *  shows one over pi of the strength. */
  motion::Animatable<float> intensity{3.0f};
  motion::Animatable<material::Color> color = material::Color{1, 1, 1, 1};
  /** The fill's and the back light's own strengths and colours, delivered
   *  at the subject the same way. Neither follows the key: a key that
   *  dims leaves the shadow side and the rim as they were stated. */
  float fillIntensity = 1.0f;
  material::Color fillColor = material::Color{1, 1, 1, 1};
  float backIntensity = 1.6f;
  material::Color backColor = material::Color{1, 1, 1, 1};
  /** Past zero, an ambient light of this strength and colour joins the
   *  rig, keyed `ambient`: light arriving equally from everywhere, which
   *  is what keeps a shadow from being black in a scene with no
   *  environment. Zero adds none. */
  float ambientIntensity = 0.0f;
  material::Color ambientColor = material::Color{1, 1, 1, 1};
  /** WHETHER THE KEY CASTS SHADOWS, and how they are made. Off unless
   *  stated, so a rig written without shadows lights exactly as it did;
   *  the fill and the back light never cast. */
  bool keyCastsShadow = false;
  LightShadow keyShadow = {};
};

/** THE RIG, as one group with three keyed point lights — `key`, `fill`
 *  and `back` — each standing where the arrangement puts it, falling off
 *  with the square of the distance and stated by what it delivers at the
 *  subject, and the `ambient` light when the rig states one. Nothing else
 *  lights the shadows. */
Group threePoint(const Rig& rig = {});

/** A CAMERA CIRCLING A SUBJECT: how far out it stands, how high, and
 *  how long one turn takes. */
struct Turntable {
  glm::vec3 at{0.0f, 0.0f, 0.0f};
  float radius = 420.0f;
  float height = 170.0f;
  /** The time one full turn takes. Zero or less parks the camera at its
   *  first station. */
  motion::Duration period{8.0};
  /** The lens's vertical field of view, in degrees. */
  float fieldOfView = 42.0f;
  /** How many points the rail is drawn through; more is rounder. Fewer
   *  than three is three, a closed loop needing three points to be
   *  one. */
  int stations = 12;
};

/** THE RAIL a turntable rides: a closed loop of `stations` points round
 *  `at`, at the table's radius and height. */
geometry::mesh::curve::Spline3 rail(const Turntable& table);

/** A RAIL THAT RISES AND FALLS: `knots` stations round `at`, the even
 *  ones standing out at `radius` and `high` above the centre, the odd
 *  ones in at `inner` and `low` — so a body riding it, a comet
 *  scattered along it or a band swept over it reads as a curve in space
 *  rather than as a ring seen at an angle. The two radii are absolute
 *  and the two heights are signed, so both may stand above the centre
 *  or both below it. */
struct Wave {
  glm::vec3 at{0.0f, 0.0f, 0.0f};
  float radius = 210.0f;
  float inner = 130.0f;
  float high = 80.0f;
  float low = -60.0f;
  /** Stations round the loop. An even count alternates cleanly all the
   *  way round; an odd one puts two outer stations side by side at the
   *  seam. Fewer than three is three. */
  int knots = 6;
};

/** THE WAVE as a closed spline through its stations. */
geometry::mesh::curve::Spline3 wave(const Wave& wave = {});

/** A RAIL THAT WINDS A SHELL: a closed loop on the ellipsoid of
 *  half-extents `shell` round `at`, climbing and diving `wraps` times per
 *  lap while the plane it winds in turns `turns` times. Two counts with
 *  no common factor keep a later wrap from retracing an earlier one, so
 *  the loop crosses in front of and behind itself and a body riding it
 *  is seen from every side in one lap; the winding runs from +x toward
 *  −z. */
struct Winding {
  glm::vec3 at{0.0f, 0.0f, 0.0f};
  glm::vec3 shell{230.0f, 120.0f, 190.0f};
  float wraps = 3.0f;
  float turns = 2.0f;
  /** Stations round the loop. The curve is smooth between them, so the
   *  count decides how faithfully the winding is drawn rather than what
   *  it is; fewer than three is three. */
  int knots = 72;
};

/** THE WINDING as a closed spline through its stations. */
geometry::mesh::curve::Spline3 winding(const Winding& winding = {});

/** THE CAMERA at scene time @p now, as one camera keyed `camera` riding
 *  that rail and looking at the subject from wherever it has reached.
 *  The lens is written in the rail's own moving frame, which makes the
 *  aim independent of how far the camera has travelled. */
Camera turntable(const Turntable& table, motion::Duration now);

/** A LIT SET: a ground plane, a rig over it, a turntable round it, and
 *  whatever is being looked at. */
struct Set {
  Rig rig;
  Turntable table;
  /** How wide the ground plane is, in the rig's extents. Zero lays
   *  none. */
  float ground = 7.0f;
  /** How far below the subject the ground lies, in extents. */
  float drop = 0.6f;
  /** What the ground is made of. Left empty it is one neutral grey,
   *  which is the only colour this library states and is stated so that
   *  a set has a floor rather than because grey is the right answer. */
  std::optional<material::Material> surface;
};

/** The set at scene time @p now, with @p subject standing on it. The
 *  tree is `set` with the children `ground`, `rig`, `camera` and
 *  `subject`, in that order — a caller who wants one of them alone
 *  builds it alone. */
Group litSet(Node subject, const Set& set = {},
             motion::Duration now = motion::Duration{});

/** A LIGHT THAT IS SEEN. A light is not a body, so nothing shows where
 *  it stands; this returns a group holding @p light and, for a light with
 *  a size, an unlit mesh of its shape and size in its colour — a rect's
 *  panel, a disk's round panel, a sphere's ball — facing the way it
 *  shines. Place, turn, ride and aim the GROUP, not the light, so the two
 *  move as one: the way the light shines is the group's forward axis, so
 *  `lookAt(key)` on the group shines the light at that node and turns
 *  the panel to face it, wherever `at` or `along` puts the group. The mesh is
 * drawn as authored in the light's colour as it stands when this is called — no
 * light reaches it and no exposure moves it — casts and receives no shadow, and
 * is keyed `emitter`; any other kind of light comes back alone in its group. */
Group visibleLight(Light light);

/** ONE SOFT BOX in an environment map: an elliptical glow whose radiance
 *  falls off as a Gaussian, centred at `centre` in the map's `(u, v)` —
 *  u round the horizon from 0 to 1, wrapping, v from the zenith at 0 to
 *  the nadir at 1 — with `size` the Gaussian's spread along each. */
struct SoftBox {
  glm::vec2 centre{0.5f, 0.5f};
  glm::vec2 size{0.05f, 0.2f};
  glm::vec3 radiance{1.0f, 1.0f, 1.0f};
};

/** An environment map made of a vertical gradient and soft boxes. */
struct EnvironmentMapOptions {
  /** How many pixels round the panorama; the map is half as tall. */
  int width = 256;
  /** The radiance straight up and straight down. Between them the
   *  zenith's share at a height is `(1 - v)` raised to `exponent`. */
  glm::vec3 zenith{0.0f, 0.0f, 0.0f};
  glm::vec3 nadir{0.0f, 0.0f, 0.0f};
  float exponent = 1.0f;
  /** The soft boxes, added over the gradient. */
  std::vector<SoftBox> softBoxes;
};

/** AN ENVIRONMENT MAP from a gradient and soft boxes — a studio, a
 *  window-lit room, a sky — baked once. Empty options bake a black map;
 *  nothing here states a look. */
EnvironmentMap environmentMap(const EnvironmentMapOptions& options);

}  // namespace sigil::world::kit
