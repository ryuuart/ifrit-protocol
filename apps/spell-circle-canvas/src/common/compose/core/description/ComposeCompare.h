#pragma once

/** @file
 * Description comparators and field counts that keep structural pruning
 * exhaustive when a properties block gains a field.
 */

#include <sigilcore/comparable/Fields.h>
#include <sigilmotion/advanced/Held.h>

namespace sigil::compose::detail {

struct ElementNode;

// A hand-written comparator must account for every field or pruning can
// replay stale content and skip transitions. Aggregate field-count assertions
// force that decision when a block changes; unlike sizeof, they detect fields
// added in padding. Nested aggregates count as one field. Defaulted equality
// is already exhaustive, and types with private state are not aggregates.
using ::sigil::core::kFieldCount;

/** Whether descriptions are provably identical for structural pruning. */
bool propertiesEqual(const ElementNode& a, const ElementNode& b);
/** The facts and operator list a layout pass reads. A changed value must
 *  wake the pass even when the node's Yoga styles stayed the same. */
bool operatorInputsEqual(const ElementNode& a, const ElementNode& b);
/** An Animatable compared where every other animated slot is:
 *  SigilMotion's form-by-form comparator. */
using ::sigil::motion::propertyEqual;

/** Constant, live, or described — one animatable flattened. */
using ::sigil::motion::ResolvedProperty;
using ::sigil::motion::resolveProperty;

/** Easing curves compare equal only as identical plain function pointers;
 *  lambda-valued curves remain conservatively unequal. */
using ::sigil::motion::easeEqual;
/** Same duration, same delay, same curve under easeEqual. */
using ::sigil::motion::tweenEqual;
/** Whether the declared transform lanes, including travel(), are equal. */
bool describedTransformEqual(const ElementNode& a, const ElementNode& b);

}  // namespace sigil::compose::detail
