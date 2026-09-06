#pragma once

/** @file
 * WHAT A POINT SET IS NOT ALLOWED TO DO: one constraint value, projected
 * onto the positions after they move, and the stock values that name the
 * ones a structure is built from.
 *
 * A constraint moves POSITIONS, not velocities. That is what makes a
 * chain of them solvable by going round the list a few times — each pass
 * fixes what it can and hands the rest to the next — and it is why the
 * stepper recovers the velocity from the movement a step actually
 * achieved: a point stopped by a stick has to lose the speed the stick
 * took, and no force said so.
 */

#include <sigilmotion/physics/Points.h>

#include <cstdint>

namespace sigil::motion::physics {

/** WHAT A CONSTRAINT SAYS. */
enum class ConstraintKind : uint8_t {
  /** How far apart two points are allowed to be: no closer than `rest`,
   *  no further than `rest + slack`. ONE value covers the stick, the
   *  spring and the inequality, because those are one band read three
   *  ways — a band of no width is a rigid length, a band pulled at a
   *  fraction of its error is a spring, and a wide band is a rope that
   *  does nothing until it is taut. */
  Distance,
  /** One point held at `at`, wherever the forces took it. The pin a
   *  cursor drags a cloth by, and the anchor a chain hangs from: it is a
   *  constraint rather than the `pinned` lane because the place is
   *  allowed to move between steps. */
  Pin,
};

/** ONE CONSTRAINT, as a value: the kind, the points it names and the
 *  numbers that kind reads. Comparable, and small enough that a list of
 *  them is walked without touching anything else. */
struct Constraint {
  ConstraintKind kind = ConstraintKind::Distance;
  /** `Distance`: the two points. `Pin`: `a` alone. */
  size_t a = 0, b = 0;
  /** `Distance`: the shortest the pair may be. */
  float rest = 0.0f;
  /** `Distance`: how much longer than `rest` is free. Zero is a rigid
   *  length; a width is a rope, a joint limit, a bead on a wire. */
  float slack = 0.0f;
  /** How much of the error one pass takes out, in [0, 1]. One is rigid —
   *  the constraint is satisfied by the end of the pass — and less is
   *  soft, which is a spring stated as how fast it converges rather than
   *  as a stiffness in force units that has to be retuned when the step
   *  changes. A number outside the range is HELD to it: taking more than
   *  the whole error would move the pair past the band and ring, and
   *  taking less than none would widen it. */
  float stiffness = 1.0f;
  /** `Pin`: where `a` is held. */
  Vec2 at{};

  bool operator==(const Constraint&) const = default;

  /** Move the points until this is satisfied, as far as `stiffness`
   *  allows. The correction is shared by inverse mass, so a light point
   *  tied to a heavy one does the moving and one tied to an immovable
   *  one does all of it. */
  void project(Points& points) const;
};

/** A RIGID LENGTH between two points. */
[[nodiscard]] inline Constraint distance(size_t a, size_t b, float length) {
  return {.kind = ConstraintKind::Distance, .a = a, .b = b, .rest = length};
}

/** A SOFT LENGTH: the same band, pulled at @p stiffness of its error per
 *  pass. The spring a cloth, a hair and a jelly are made of — and the
 *  reason it is not the velocity spring beside it is that this one lives
 *  inside the constraint solve, where a chain of them can be satisfied
 *  together. */
[[nodiscard]] inline Constraint spring(size_t a, size_t b, float rest,
                                       float stiffness) {
  return {.kind = ConstraintKind::Distance,
          .a = a,
          .b = b,
          .rest = rest,
          .stiffness = stiffness};
}

/** A BAND: no closer than @p minimum, no further than @p maximum, and
 *  free between. The rope, the joint limit, the pair that may not
 *  overlap. */
[[nodiscard]] inline Constraint range(size_t a, size_t b, float minimum,
                                      float maximum) {
  return {.kind = ConstraintKind::Distance,
          .a = a,
          .b = b,
          .rest = minimum,
          .slack = maximum > minimum ? maximum - minimum : 0.0f};
}

/** A POINT HELD AT @p at. */
[[nodiscard]] inline Constraint pin(size_t index, Vec2 at) {
  return {.kind = ConstraintKind::Pin, .a = index, .at = at};
}

}  // namespace sigil::motion::physics
