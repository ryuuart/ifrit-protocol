#pragma once

/** @file
 * THE EXECUTOR SEAM a chain is cooked over: what a runtime must be able
 * to do, the comparable value one is carried as, the built-in CPU
 * executor, the cook that dispatches to it, and the helpers every
 * executor shares — the attribute store, the generator that seeds it,
 * the reading that exports it, the parameter addressing a control
 * surface reaches through, the noise field an operator displaces by,
 * and the one table between a Cloud's lane names and this language's
 * attribute names.
 *
 * The shared ends are what make the middle comparable: every executor
 * seeds through one generator and exports through one reading, so two
 * runtimes can be held to bit identity rather than to a tolerance.
 */

#include <sigilcore/comparable/Erased.h>

#include <boost/container/map.hpp>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sigilgeometry/mesh/pop/Ops.h"

namespace sigil::geometry::mesh {
namespace pop {

/** WHAT A RUNTIME DOES: it cooks a chain into a Cloud.
 *
 *  One operation, because one is the whole of the seam. The
 *  mesh-forming sinks below stand on the cooked cloud and hand back a
 *  `Mesh` — CPU buffers either way — so a device-side former would
 *  have to read its own result back to answer them; the place a
 *  device replaces ring forming is `pop::sweep` over a rail, not
 *  here. Cooking the points is the step that is genuinely a thousand
 *  parallel lanes, and it is the step this seam moves.
 *
 *  An executor also declares, per operator, whether it can run it.
 *  `pop::cook` asks before it dispatches, so an operator a runtime
 *  lacks stops the cook with a message naming both rather than
 *  quietly leaving the chain short. */
class Executor {
 public:
  virtual ~Executor() = default;

  /** What this runtime is called, in the message an unsupported
   *  operator produces. */
  virtual std::string name() const = 0;

  /** Can this runtime run @p op? A runtime that runs everything
   *  answers true to every operator. */
  virtual bool supports(const Op& op) const = 0;

  /** Evaluate @p chain into a Cloud with the conventional lanes.
   *  Every executor is required to produce the same cloud from the
   *  same chain, bit for bit. */
  virtual Cloud cook(const Chain& chain) const = 0;
};

/** The executor a cook runs on, carried as a comparable value. Two
 *  runtimes are equal when they hold the same model with the same
 *  value, so a reconciler can ask whether a description's runtime
 *  changed. */
class Runtime : public core::Erased<Executor> {
 public:
  using core::Erased<Executor>::Erased;
  Runtime() = default;
  Runtime(core::Erased<Executor> erased)  // NOLINT: a Runtime IS its value
      : core::Erased<Executor>(std::move(erased)) {}

  /** The built-in executor: every operator evaluated on the CPU, and
   *  the definition every other executor reproduces. Every call
   *  returns the same value, so two default cooks compare equal. */
  static Runtime cpu();

  /** The same executor dividing its passes at a chosen grain: how many
   *  points one worker takes at a time, with a range no larger than one
   *  grain staying on the calling thread. The stock grain suits a pass
   *  of a few operations per point, which is what the operators are; a
   *  caller whose chain is far heavier per point, or one that wants a
   *  small set of points divided at all, names its own. The cloud is the
   *  same either way, bit for bit — the grain decides only where the
   *  arithmetic runs — and two runtimes given different grains are
   *  different values. */
  static Runtime cpu(size_t itemGrain);
};

/** THE HELPERS EVERY EXECUTOR SHARES. A runtime that performs the
 *  filters somewhere else still seeds through the same generator and
 *  exports through the same reading, because the two ends of a cook
 *  are what make its middle comparable. */

/** THE ATTRIBUTE STORE a chain runs over: every attribute a named
 *  float4 lane, builtins ("P", "T", "Dir", "Scale", "Color", "Tex")
 *  and customs alike, each one value per point. */
using Lanes =
    boost::container::map<std::string, std::vector<glm::vec4>, std::less<>>;

/** What an untouched lane holds, by name: a scale and a colour start
 *  at one, a texture window at the whole image, a direction at +z, and
 *  everything else at zero — which is also what makes a mask lane
 *  nothing has written select nobody. */
glm::vec4 laneFill(std::string_view name);

/** THE CHAIN'S GENERATOR, run into @p lanes: the conventional lanes
 *  created, and whatever leads the chain — a loop scatter, a surface
 *  scatter, a given point set — written into them. Returns how many
 *  points there are, zero when nothing leads the chain or the leader
 *  has nothing to make.
 *
 *  A generator is not a map over points and no kernel replaces it, so
 *  every executor seeds from here: a seed that differed would make
 *  every comparison after it meaningless. */
size_t seedLanes(const Chain& chain, Lanes* lanes);

/** @p lanes poured back into a Cloud: the builtins under the
 *  conventional names — "t", "dir", "size", "tint" — and everything
 *  else, "Tex" included, as four-wide colour lanes under its own name,
 *  so nothing an operator wrote is unreachable downstream. */
Cloud exportLanes(const Lanes& lanes, size_t count);

/** How a PointSet's cloud lays out as attributes: the conventional
 *  lanes onto the builtins, everything else under its own name — one
 *  function, so the CPU cook and a device executor's initial upload
 *  agree lane for lane. @p lanes gains or overwrites the seeded names,
 *  every lane sized to the cloud. */
void seedAttrs(const Cloud& cloud, Lanes& lanes);

/** PARAMETER ADDRESSING: an operator's numeric fields by name, the way
 *  a control surface or an animation lane reaches into a chain without
 *  knowing the operator's type. Names are the struct's own field names,
 *  vector components dotted (`"center.x"`, `"add.w"`, `"from.g"`) —
 *  every float, int, bool and enum field an operator has, and nothing a
 *  string, a lane name, a mesh, a cloud or a matrix (those are
 *  descriptions, not dials). Ints truncate, bools read non-zero, enums
 *  take their integer value. `setField` returns false and writes
 *  nothing for a name the operator does not have; `getField` returns
 *  nullopt for it. */
bool setField(Op& op, std::string_view field, float value);
/** The read side of that addressing: the named field's value as a
 *  float, or nullopt when the operator has no such field. */
std::optional<float> getField(const Op& op, std::string_view field);

/** THE FIELD `Noise` DISPLACES BY, at @p p, in the units the operator
 *  carries: a sum of library sines, returned before the amplitude
 *  multiplies it. One function, so the operator and
 *  `points::displaceNoise` — which is the same verb reached for
 *  without a chain — answer with one field rather than two. */
glm::vec3 noiseField(glm::vec3 p, float frequency, float seed);

/** THE COOK: evaluates @p chain into a Cloud with the conventional
 *  lanes — "t" (scalar), "dir" (vector), "tint" (color), "size"
 *  (scalar).
 *
 *  The work runs on @p runtime, the built-in CPU executor by default,
 *  and every executor is required to agree with it bit for bit. An
 *  operator the runtime does not support stops the cook: a
 *  `std::runtime_error` is thrown whose message names the operator and
 *  the runtime — a chain that cannot run must say so, because a chain
 *  quietly missing an operator cooks a plausible cloud that is not the
 *  described one. */
Cloud cook(const Chain& chain, const Runtime& runtime = Runtime::cpu());

/** THE ONE TABLE between a Cloud's lane names and this language's
 *  attribute names, in each direction: `t`↔`T`, `size`↔`Scale`,
 *  `dir`↔`Dir`, `tint`↔`Color`, with `normal` also seeding `Dir`
 *  because that is what a generator or an importer writes. `P` has no
 *  lane of its own — it is the cloud's positions — and `Tex` and every
 *  custom name keep their own spelling on both sides.
 *
 *  Every path that crosses between the two spellings reads these, so a
 *  cloud seeded into a chain and exported back out again comes home to
 *  the lanes it left from. A name outside the table maps to itself. */
std::string_view attrFor(std::string_view lane);
std::string_view cloudLaneFor(std::string_view attr);

}  // namespace pop
}  // namespace sigil::geometry::mesh
