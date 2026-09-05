/** @file
 * The spread's three whole-value operations: nesting a second cascade
 * inside every beat, the declare-time span, and structural equality.
 */

#include <sigilcore/comparable/Fields.h>
#include <sigilmotion/bind/BoundFloat.h>
#include <sigilmotion/schedule/Cascade.h>
#include <sigilmotion/schedule/Spread.h>

#include <utility>

namespace sigil::motion {

Spread& Spread::then(Spread nested) {
  inner = std::make_shared<const Spread>(std::move(nested));
  return *this;
}

float Spread::spanMs(uint32_t count, uint32_t innerCount) const {
  // The one arithmetic: the same resolved-cascade body a host runs per
  // frame, handed its counts directly instead of a laid-out run.
  Cascade cascade;
  cascade.build(*this, count, innerCount);
  return cascade.totalMs;
}

static_assert(core::kFieldCount<Spread> == 10,
              "Spread gained or lost a field — rule on it in "
              "Spread::operator== below, then bump this count. A field left "
              "out makes two different cascades compare equal, the node that "
              "holds one prunes, and it keeps beating to the old ladder "
              "forever.");
bool Spread::operator==(const Spread& other) const {
  if (eachMs != other.eachMs || amountMs != other.amountMs ||
      durationMs != other.durationMs || loopMs != other.loopMs ||
      from != other.from || seed != other.seed || cueMs != other.cueMs ||
      rankBy != other.rankBy)
    return false;
  if (!easeEqual(distribution, other.distribution)) return false;
  if (inner == other.inner) return true;  // both absent, or one shared value
  if (!inner || !other.inner) return false;
  return *inner == *other.inner;
}

}  // namespace sigil::motion
