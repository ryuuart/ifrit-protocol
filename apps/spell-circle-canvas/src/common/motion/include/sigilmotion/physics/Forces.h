#pragma once

/** @file
 * WHAT PUSHES ON A POINT SET: one force value with a kind and the props
 * each kind reads, and the stock values that name the ones a study
 * reaches for.
 *
 * One value rather than a type per force, for the same reason a noise
 * field is one value with props: a force chosen for a scene is carried
 * as data — compared, stored, handed across a describe — and a list of
 * them is a plain array with no indirection in the inner loop. A prop a
 * kind does not read is ignored, and saying so is cheaper than a family
 * of near-identical structs.
 */

#include <sigilcore/compute/Field.h>
#include <sigilmotion/physics/Points.h>

#include <cstdint>

namespace sigil::motion::physics {

/** WHAT A FORCE IS. */
enum class ForceKind : uint8_t {
  /** The same acceleration on everything, whatever it weighs — gravity,
   *  and any other steady pull. Stated as an acceleration rather than a
   *  push because that is what makes a heavy point and a light one fall
   *  together; `vector` is it. */
  Uniform,
  /** A push against the way a point is going, proportional to how fast
   *  it is going: `strength` per second of speed. What air, water and a
   *  settling cloud all are, and the reason a field of particles comes
   *  to rest instead of ringing. */
  Drag,
  /** A pull towards `point`, or a push away from it when `strength` is
   *  negative, falling off with distance and reaching `radius`. The
   *  attractor a cursor is, the repulsor an explosion is. */
  Attract,
  /** A push whose DIRECTION is read from a noise field at the point's
   *  own position: the flow a drifting field of particles follows. The
   *  field answers one number, which is read as an angle, so neighbouring
   *  points are pushed in neighbouring directions and the cloud
   *  streams. */
  Wind,
  /** THE THREE STEERINGS OF A FLOCK, over whichever points are within
   *  `radius`: away from the ones too close, along with the ones nearby,
   *  and towards where they are. One force rather than three, because
   *  the three share the neighbour search and running them apart would
   *  do it three times. */
  Flock,
  /** The caller's own push, as a captureless function over the props.
   *  The escape hatch, and it is a plain function pointer so a force
   *  carrying one still compares. */
  Body,
};

/** HOW HARD EACH STEERING OF A FLOCK PULLS. The three are weights on one
 *  another and not absolute numbers: doubling all three is a flock that
 *  turns twice as hard, and the shape of the flock is the ratios. */
struct Flocking {
  /** Away from the neighbours nearest it — the one that stops the flock
   *  collapsing to a point. Usually the strongest. */
  float separation = 1.5f;
  /** Towards the average heading of its neighbours. */
  float alignment = 1.0f;
  /** Towards where its neighbours are. */
  float cohesion = 1.0f;

  bool operator==(const Flocking&) const = default;
};

struct Force;

/** The caller's own force: a captureless function over the point set and
 *  the force value that carries its props. */
using ForceBody = void (*)(Points& points, float seconds, const Force& force);

/** ONE FORCE, as a value.
 *
 *  Every member is a number, a small enumeration, a noise field or a
 *  function pointer, so two forces compare exactly and a scene's force
 *  list is comparable — which is what lets a describe carry one and a
 *  patch prove nothing changed. */
struct Force {
  ForceKind kind = ForceKind::Uniform;
  /** `Uniform`: the acceleration, in units per second squared. */
  Vec2 vector{};
  /** `Attract`: what it pulls towards. */
  Vec2 point{};
  /** How hard, in the units its kind is stated in: `Drag`'s per-second
   *  fraction of speed, `Attract`'s pull at one unit away (negative
   *  pushes), `Wind`'s push, `Flock`'s overall weight. */
  float strength = 1.0f;
  /** How far it reaches: `Attract` falls to nothing at this distance and
   *  `Flock` looks no further than it for neighbours. Zero is unbounded
   *  for `Attract` and answers nothing for `Flock`, which has no
   *  neighbours without a reach. */
  float radius = 0.0f;
  /** `Flock`: the three weights. */
  Flocking flock{};
  /** `Wind`: the field read at each point's position, whose one number
   *  is taken as an angle over a whole turn. Its own `frequency` is how
   *  many lattice cells one unit of the caller's coordinates spans, so
   *  the scale of the flow is stated on the field and not here. */
  core::noise::Field field{};
  /** `Body`: the push itself. */
  ForceBody body = nullptr;

  bool operator==(const Force&) const = default;

  /** Add what this force is doing to the point set's force lane. Every
   *  force reads the lanes as they are and writes only `force`, so the
   *  order a list of them is applied in does not change the answer. */
  void apply(Points& points, float seconds) const;
};

/** THE SAME PULL ON EVERYTHING: an acceleration, so weight does not
 *  enter it. Down is whichever way the caller's coordinates go down. */
[[nodiscard]] inline Force gravity(Vec2 acceleration) {
  return {.kind = ForceKind::Uniform, .vector = acceleration};
}

/** A PUSH AGAINST THE MOTION: @p perSecond of the speed, per second. At
 *  1 a point loses about two thirds of its speed a second, at 0.1 it
 *  coasts. */
[[nodiscard]] inline Force drag(float perSecond) {
  return {.kind = ForceKind::Drag, .strength = perSecond};
}

/** A PULL TOWARDS @p centre, @p strength at one unit away, falling off
 *  with distance and reaching nothing past @p radius. Zero radius
 *  reaches everywhere. */
[[nodiscard]] inline Force attract(Vec2 centre, float strength,
                                   float radius = 0.0f) {
  return {.kind = ForceKind::Attract,
          .point = centre,
          .strength = strength,
          .radius = radius};
}

/** THE SAME THING PUSHING: an attraction with the sign turned over, so
 *  the two are one value and a scene can flip a cursor from pulling to
 *  pushing by negating a number rather than by swapping a type. */
[[nodiscard]] inline Force repel(Vec2 centre, float strength,
                                 float radius = 0.0f) {
  return attract(centre, -strength, radius);
}

/** A PUSH ALONG A FLOW: @p field read at each point's position and taken
 *  as an angle, at @p strength. */
[[nodiscard]] inline Force wind(const core::noise::Field& field,
                                float strength) {
  return {.kind = ForceKind::Wind, .strength = strength, .field = field};
}

/** THE FLOCK: the three steerings over every point within @p radius, at
 *  @p strength overall. */
[[nodiscard]] inline Force boids(Flocking weights, float radius,
                                 float strength = 1.0f) {
  return {.kind = ForceKind::Flock,
          .strength = strength,
          .radius = radius,
          .flock = weights};
}

}  // namespace sigil::motion::physics
