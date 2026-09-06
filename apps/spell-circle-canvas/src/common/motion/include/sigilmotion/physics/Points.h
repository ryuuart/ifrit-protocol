#pragma once

/** @file
 * THE POINT SET A SIMULATION IS: one value holding a lane per property,
 * and the two-number position those lanes are written in.
 *
 * Lanes rather than a vector of particles because everything that reads
 * a simulation reads one property of all of it — a stepper walks the
 * positions, a stamping leaf walks the positions and the colours, a
 * draw walks the velocities. A structure of arrays is the shape all of
 * those want, and it is the shape an instanced draw takes its lanes in.
 */

#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sigil::motion::physics {

/** A TWO-FLOAT POINT SOMEONE ELSE'S LIBRARY SPELLS: the members Skia
 *  names its point's. Matching it by shape rather than by name is what
 *  lets a position be built from a renderer's point without this library
 *  including a renderer's header — the same crossing the colour leaf
 *  makes for a four-float colour. */
template <class P>
concept TwoFloatPoint = requires(const P& point) {
  { point.fX } -> std::convertible_to<float>;
  { point.fY } -> std::convertible_to<float>;
};

/** A POSITION OR A DISPLACEMENT, in whatever units the caller is
 *  simulating in.
 *
 *  This library states no unit: a simulation is in pixels for a screen,
 *  in metres for a model of something, in whatever a sketch is drawing
 *  in. Every number below — a gravity, a stiffness, a radius — is in
 *  those units and the seconds the stepper is given, and nothing here
 *  converts between two of them. */
struct Vec2 {
  float x = 0.0f, y = 0.0f;

  constexpr Vec2() = default;
  constexpr Vec2(float first, float second) : x(first), y(second) {}
  /** A two-float point, field for field. Implicit because a conversion
   *  spelled by hand at each call site is a place where an axis order
   *  drifts silently; this is the one place the mapping is written. */
  template <TwoFloatPoint P>
  constexpr Vec2(const P& point)  // NOLINT: the crossing is the point
      : x((float)point.fX), y((float)point.fY) {}

  bool operator==(const Vec2&) const = default;

  constexpr Vec2 operator+(const Vec2& other) const {
    return {x + other.x, y + other.y};
  }
  constexpr Vec2 operator-(const Vec2& other) const {
    return {x - other.x, y - other.y};
  }
  constexpr Vec2 operator*(float scalar) const { return {x * scalar, y * scalar}; }
  constexpr Vec2 operator-() const { return {-x, -y}; }
  constexpr Vec2& operator+=(const Vec2& other) {
    x += other.x;
    y += other.y;
    return *this;
  }
  constexpr Vec2& operator-=(const Vec2& other) {
    x -= other.x;
    y -= other.y;
    return *this;
  }
  constexpr Vec2& operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    return *this;
  }

  /** How long it is, and its length SQUARED — the second because every
   *  comparison of two distances answers the same either way and the
   *  square root is the expensive half of a neighbour search. */
  [[nodiscard]] float lengthSquared() const { return x * x + y * y; }
  [[nodiscard]] float length() const { return std::sqrt(lengthSquared()); }
  /** The same direction at length one, or the zero vector when there is
   *  no direction to answer — which is what a pair of points sitting on
   *  each other hands every force below. */
  [[nodiscard]] Vec2 normalized() const {
    const float len = length();
    return len > 0.0f ? Vec2{x / len, y / len} : Vec2{};
  }
};

inline constexpr Vec2 operator*(float scalar, const Vec2& v) { return v * scalar; }

/** THE POINT SET: one lane per property, all the same length.
 *
 *  The lanes are public because a simulation is a value a caller reads
 *  and writes, not a machine that owns its state: a describe walks
 *  `position` to place its instances, an emitter writes `velocity` into
 *  the rows it just added, and a picker writes `pinned` where a cursor
 *  grabbed something. `add` and `remove` are here so the lanes cannot be
 *  left at different lengths, and nothing else about the set is hidden.
 *
 *  READING IT INTO AN INSTANCED DRAW is one loop: the stamping leaf's
 *  pool holds its own lanes of two-float positions, so a frame copies
 *  `position[i]` into the pool's position lane and whatever else it is
 *  drawing from — a rotation off the velocity's angle, a scale off the
 *  mass, an alpha off an age the caller keeps beside these lanes. This
 *  library holds no colour and no age: what a point IS on screen is the
 *  drawing's business, and a simulation that carried it would have to
 *  name a renderer. */
struct Points {
  /** Where each point is now. */
  std::vector<Vec2> position;
  /** Where it was when the current step began — the position the
   *  constraint passes moved it away from, which is what turns a
   *  position correction back into a velocity. */
  std::vector<Vec2> previous;
  /** How fast it is going, in units per second. It is the lane the
   *  stepper carries motion in and the one a drag, a wind and a flock
   *  read; the stepper rewrites it from the movement a step actually
   *  achieved, so a point stopped by a constraint loses the speed the
   *  constraint took. */
  std::vector<Vec2> velocity;
  /** What is pushing on it this step, in mass times units per second
   *  squared. Cleared at the start of every step and filled by the
   *  forces, so a caller may add its own push to this lane between two
   *  steps and it will be spent exactly once. */
  std::vector<Vec2> force;
  /** How much of it there is. Zero or less is IMMOVABLE — infinitely
   *  heavy — which is the same answer `pinned` gives and is reached by a
   *  different road: a mass of zero is a wall, a pin is a point held
   *  where something put it. */
  std::vector<float> mass;
  /** Held still: the stepper does not move it and the constraints do not
   *  push it. One byte rather than a bit, because these lanes are read
   *  in loops and a bit vector cannot hand out a reference. */
  std::vector<uint8_t> pinned;

  [[nodiscard]] size_t size() const { return position.size(); }
  [[nodiscard]] bool empty() const { return position.empty(); }

  /** A point at @p at, moving at @p velocity, and its index. */
  size_t add(Vec2 at, Vec2 startingVelocity = {}, float startingMass = 1.0f,
             bool held = false);

  /** Drop the point at @p index by moving the LAST one into its place —
   *  which is what a field of particles wants, since the order of a
   *  cloud means nothing and shuffling every lane down would cost the
   *  whole set per death.
   *
   *  It renumbers, so anything holding an index INTO the set — a
   *  constraint, a pin, a caller's own table — is holding the wrong
   *  point afterwards. A set with constraints over it is grown and
   *  cleared, not thinned. */
  void remove(size_t index);

  /** Every lane emptied. */
  void clear();

  /** Whether the stepper and the constraints may move this point. */
  [[nodiscard]] bool movable(size_t index) const {
    return !pinned[index] && mass[index] > 0.0f;
  }
  /** One over the mass, or zero for anything immovable — the weight a
   *  constraint shares its correction by, where an immovable point takes
   *  none of it and its partner takes all. */
  [[nodiscard]] float inverseMass(size_t index) const {
    return movable(index) ? 1.0f / mass[index] : 0.0f;
  }
};

}  // namespace sigil::motion::physics
