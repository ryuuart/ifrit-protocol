#pragma once

/** @file
 * WHAT IS BORN, AGES AND DIES: a point set with the two attributes that make
 * a point a particle, the attributes a consumer names for itself, and the
 * emitter that fills all of them from a seeded stream.
 *
 * The point set beneath is unchanged — the same positions, the same
 * forces, the same stepper — because a particle is not a different kind
 * of point. What it has that a point does not is a beginning and an end:
 * something put it there with a velocity drawn from a range, it carries
 * numbers of its own that drift while it lives, and it leaves the set
 * when its time is up. Those three are here, and nothing else is.
 */

#include <sigilcore/compute/Chance.h>
#include <sigilmotion/physics/Points.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::motion::physics {

/** A NUMBER DRAWN AROUND A MEAN: `(mean + Rand · variation) · scale`,
 *  where Rand is uniform on [-1, 1).
 *
 *  A birth attribute is stated as a middle and how far either side of it
 *  a draw may fall, rather than as a pair of bounds, because that is how
 *  a recipe is read and adjusted: the middle is the thing being made and
 *  the variation is how ragged the set of them is, and either can be
 *  changed without recomputing the other.
 *
 *  `scale` MULTIPLIES the whole draw and is not folded into the two
 *  numbers, because one recipe is worn at many sizes: a smaller instance
 *  of the same fire is the same middle and the same raggedness at a
 *  smaller scale, and reading a recipe back off a pre-multiplied pair is
 *  arithmetic nobody does by eye.
 *
 *  IT ALWAYS DRAWS. A variation of zero still spends a word, so a birth
 *  costs the same number of words whatever the numbers on it are and two
 *  runs of the same emitter over the same stream stay in step even when
 *  one of its attributes is a constant.
 *
 *  It is a `chance` shape — an `Answer` and a `draw` — so a stream
 *  samples it the way it samples the stock distributions. */
struct Roughly {
  /** The middle of the draw. */
  float mean = 0.0f;
  /** How far either side of the middle a draw may fall. */
  float variation = 0.0f;
  /** What the whole draw is multiplied by. */
  float scale = 1.0f;

  using Answer = float;
  [[nodiscard]] Answer draw(core::chance::Stream& stream) const {
    return (mean + stream.signedUnit() * variation) * scale;
  }

  bool operator==(const Roughly&) const = default;
};

/** AN ATTRIBUTE A CONSUMER NAMES FOR ITSELF: one float per particle,
 *  carried through birth and death with the point it belongs to.
 *
 *  The point set holds what a STEP needs — where a thing is, how fast,
 *  how heavy. A drawing needs more than that: a size, a colour, a spin,
 *  a number saying which of many emitters threw this one. Those cannot
 *  be a parallel array the consumer keeps, because a death moves the
 *  last particle into the hole it leaves and a parallel array would be
 *  renumbered without knowing it. So they are held here and moved with
 *  it.
 *
 *  An attribute DRIFTS. `rate` is the whole of the model an attribute changes
 *  under while a particle lives — a size that grows, a colour that
 *  cools, a spin that slows — applied by `Particles::live` and bounded
 *  by `least` and `most`, which is what stops a channel falling past
 *  zero into a number no renderer can read. */
struct Attribute {
  /** What the consumer calls it. */
  std::string name;
  /** How far the value moves per unit of time lived. Zero is an attribute
   *  nothing but the consumer writes. */
  float rate = 0.0f;
  /** How far the drift may take it, down and up. The default pair is no
   *  bound at all. */
  float least = -std::numeric_limits<float>::infinity();
  float most = std::numeric_limits<float>::infinity();
  /** One value per particle, in the set's own order. */
  std::vector<float> values;
};

/** THE TWO NAMES A BIRTH MAY FILL THAT ARE NOT ATTRIBUTES. A set holds
 *  its age and its lifetime in vectors of their own, because every
 *  particle system reads both and the death rule is written over them,
 *  so an emitter naming either fills THAT vector. Nothing else answers to
 *  them: `Particles::attribute` asked for one would add an attribute of
 *  that name which nothing reads. */
inline constexpr std::string_view kAge = "age";
inline constexpr std::string_view kLife = "life";

/** A POINT SET THAT IS BORN, AGES AND DIES.
 *
 *  `points` is the simulation and is stepped by whatever steps a point
 *  set — the same forces, the same constraints, the same `Verlet`. What
 *  is added is the three things a particle has that a point does not:
 *  how long it has lived, how long it may live, and the attributes that say
 *  what it looks like while it does.
 *
 *  Everything is public, for the reason the point set's attributes are: a
 *  cloud is a value a consumer reads and writes, and the methods are
 *  here only so the attributes cannot be left at different lengths. A DEATH
 *  MOVES THE LAST PARTICLE INTO THE HOLE, in every attribute at once — the
 *  order of a cloud means nothing, and shuffling the whole set down
 *  would cost the whole set per death. Nothing may hold an index into a
 *  cloud across a `reap`. */
struct Particles {
  /** Where they are, how fast, how heavy — the set a stepper steps. */
  Points points;
  /** How long each has lived, in whatever unit the consumer lives them
   *  through. */
  std::vector<float> age;
  /** How long each may live. ZERO OR LESS NEVER EXPIRES, which is what
   *  a cloud nobody gave a lifetime to wants. */
  std::vector<float> life;
  /** The attributes the consumer named, in the order it first asked for
   *  them. */
  std::vector<Attribute> attributes;

  [[nodiscard]] size_t size() const { return points.size(); }
  [[nodiscard]] bool empty() const { return points.empty(); }

  /** A particle at @p at, moving at @p startingVelocity, allowed
   *  @p lifetime, and its index. Every attribute grows with it, at zero. */
  size_t add(Vec2 at, Vec2 startingVelocity = {}, float lifetime = 0.0f,
             float startingMass = 1.0f);

  /** Drop the particle at @p index by moving the LAST one into its
   *  place, in every attribute. It renumbers, exactly as the point set's own
   *  `remove` does. */
  void remove(size_t index);

  /** Every attribute emptied. The attributes themselves stay, with their names,
   *  their rates and their bounds, so a cloud that is cleared and
   *  regrown is the same cloud. */
  void clear();

  /** THE ATTRIBUTE NAMED @p name, ADDED at the set's length and filled
   *  with zeros if it is not there yet.
   *
   *  `kAge` and `kLife` are not attributes: they are `age` and `life`,
   *  which are members.
   *
   *  The reference is to the attribute's own storage and stays good while
   *  particles are born and die. ADDING ANOTHER ATTRIBUTE is what
   *  invalidates it, since they are held in a vector of their own, so a
   *  loop takes every attribute it reads before it emits into the set. */
  Attribute& attribute(std::string_view name);

  /** The attribute named @p name, or nothing. */
  [[nodiscard]] const Attribute* attribute(std::string_view name) const;

  /** Whether @p index has lived out its life. A lifetime of zero or less
   *  never expires. */
  [[nodiscard]] bool expired(size_t index) const {
    return life[index] > 0.0f && age[index] >= life[index];
  }

  /** EVERY PARTICLE @p elapsed OLDER, and every attribute moved by its own
   *  rate over that time and held inside its bounds. It removes nothing:
   *  what is dead afterwards is what `reap` answers, and a consumer with
   *  extinction rules of its own gets to write them between the two. */
  void live(float elapsed);

  /** THE EXPIRED ONES REMOVED, and how many went. */
  size_t reap();

  /** The ones @p dead answers for, removed, and how many went.
   *
   *  A cloud usually dies of more than age — of running out of light, of
   *  falling below the ground, of leaving the frame — and those rules
   *  are the consumer's, not this library's. @p dead is asked for one
   *  index at a time and may read any attribute; `expired` is the age rule
   *  written out, so a consumer's own rule is spelled beside it rather
   *  than instead of it. The particle a death moves into the hole is
   *  asked in its turn, so one pass buries everything. */
  template <class Dead>
  size_t reap(Dead&& dead) {
    size_t buried = 0;
    for (size_t index = 0; index < size();) {
      if (dead((size_t)index)) {
        remove(index);
        ++buried;
      } else {
        ++index;
      }
    }
    return buried;
  }
};

/** WHERE A BIRTH LANDS: the shape of the emitter's mouth, in its own
 *  frame — `Emitter::along` is that frame's first axis and the
 *  perpendicular to it is its second. */
enum class EmitFrom : uint8_t {
  /** All of them at one place. */
  Point,
  /** Anywhere on a segment through `at`, `Emitter::size`'s first number
   *  from the middle to either end. The line of flame along a fissure,
   *  the edge a fountain runs off. */
  Segment,
  /** Anywhere in a rectangle about `at`, `Emitter::size`'s two numbers
   *  being its half extents. */
  Box,
  /** Anywhere in a disc of `Emitter::size`'s first number about `at`,
   *  evenly over its AREA rather than over its radius, so the middle is
   *  not crowded. */
  Disc,
  /** Anywhere on the circle of `Emitter::size`'s first number about
   *  `at` — the shell a burst goes out in. A disc and a ring do not read
   *  `Emitter::along`: they are the same shape whichever way they are
   *  turned. */
  Ring,
};

/** ONE ATTRIBUTE A BIRTH FILLS: which one, and what its first value is
 *  drawn from. `kLife` names the lifetime. */
struct BirthAttribute {
  std::string name;
  Roughly drawn;

  bool operator==(const BirthAttribute&) const = default;
};

/** WHAT PUTS PARTICLES INTO A CLOUD: a mouth, a direction, and the
 *  ranges every attribute of a birth is drawn from.
 *
 *  ONE VALUE WITH PROPS, like a force: an emitter chosen for a scene is
 *  carried as data, and a plume, a spark shower, a fountain and a wall
 *  of fire differ in their numbers rather than in their type. What it is
 *  NOT is a renderer or a schedule — it does not know what a particle
 *  looks like beyond the attributes it is told to fill, and it does not hold
 *  a clock.
 *
 *  THE DRAWS COME OFF THE STREAM IN ONE ORDER, and the order is part of
 *  what an emitter IS: the place, then the angle off the aim, then which
 *  side of the aim, then the speed, then `attributes` in the order they are
 *  written. A consumer that replays a seed gets the same cloud back only
 *  because that order is fixed, so an attribute is added at the END of the list
 *  when an existing cloud must not move. */
struct Emitter {
  /** The shape of the mouth. */
  EmitFrom from = EmitFrom::Point;
  /** Where the mouth is. */
  Vec2 at{};
  /** The mouth's own first axis, at length one. A segment lies along it
   *  and a box's first half extent measures along it. */
  Vec2 along{1.0f, 0.0f};
  /** How big the mouth is: along the first axis and across it, or a
   *  radius in the first number for the two round mouths. */
  Vec2 size{};
  /** The direction a birth is thrown in, at length one. */
  Vec2 aim{0.0f, -1.0f};
  /** How far off the aim a birth may be thrown, in radians — the half
   *  angle of the ejection cone. Zero throws everything straight along
   *  the aim. */
  float cone = 0.0f;
  /** How fast a birth is thrown. Negative speeds throw it backwards,
   *  which is a plume falling into its own mouth. */
  Roughly speed{};
  /** What a birth weighs. Every birth weighs the same, because a weight
   *  drawn per particle is an attribute like any other: write `points.mass`
   *  over the rows a birth answers. */
  float mass = 1.0f;
  /** The attributes a birth fills, drawn in this order. */
  std::vector<BirthAttribute> attributes;
  /** How many are born per unit of time. */
  float rate = 0.0f;
  /** THE FRACTION OF A PARTICLE a span of time left over, carried to the
   *  next span so that a rate finer than one birth per step still
   *  arrives at that rate rather than never. It is the emitter's only
   *  state, and it is public because an emitter is a value: assigning
   *  one rewinds it. */
  float carry = 0.0f;

  bool operator==(const Emitter&) const = default;

  /** EXACTLY @p count BIRTHS AT ONCE, whatever the rate says, and the
   *  count. The puff at the head of a plume, the shell of a firework,
   *  and the form a consumer whose own law says how many arrive this
   *  step reaches for.
   *
   *  They are the LAST @p count rows of @p particles, so an attribute no draw
   *  fills — a per-emitter constant, an index naming what threw them —
   *  is written over that tail. */
  size_t burst(Particles& particles, core::chance::Stream& stream,
               size_t count) const;

  /** THE BIRTHS @p elapsed OF TIME IS WORTH at `rate`, the fraction
   *  carried, and the count. */
  size_t emit(Particles& particles, core::chance::Stream& stream,
              float elapsed);
};

}  // namespace sigil::motion::physics
