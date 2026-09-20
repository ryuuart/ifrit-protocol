#pragma once

/** @file
 * @ingroup core-comparable
 *
 * Every header of the comparable leaf, for a consumer that wants the
 * whole of it.
 */

/** @defgroup core-comparable Comparable erasure
 *  What a value needs before anything can decide it did not change: type
 *  erasure that keeps its equality, so a set of operations can ride on a
 *  value and two holders can still ask whether they carry the same one;
 *  and the field pin, which fails the build when a struct grows a member
 *  a hand-written comparator does not mention. */

#include <sigilcore/comparable/Erased.h>
#include <sigilcore/comparable/Fields.h>
