#pragma once

/** @file
 * The field pin: how many direct non-static data members a struct has,
 * read off the type, so a hand-written comparator over that struct fails
 * the build the moment the struct grows a field it does not mention.
 */

#include <boost/pfr/tuple_size.hpp>
#include <cstddef>

namespace sigil::core {

/** How many direct non-static data members @p T declares.
 *
 *  @p T must be an aggregate — a struct with public members, no bases and
 *  no user-declared constructors — which is what a comparable value type
 *  is anyway. */
template <class T>
inline constexpr std::size_t kFieldCount = boost::pfr::tuple_size_v<T>;

/** THE PIN. A comparator written by hand can leave a field out, and the
 *  failure is invisible by construction: two different values compare
 *  equal, the holder concludes nothing changed, and it keeps whatever the
 *  old value produced for as long as it lives. Nothing detects that from
 *  the outside, because the wrong answer is indistinguishable from a
 *  value that really did not change.
 *
 *  So each hand-written comparator sits beside
 *
 *      static_assert(kFieldCount<T> == N,
 *                    "T gained or lost a field — rule on it in "
 *                    "compare() below, then bump this count.");
 *
 *  and adding a field to T fails the build until someone rules on it in
 *  the comparator — participate, or a stated reason not to — and bumps
 *  the count. The message is what makes the failure actionable, so it
 *  names the comparator the reader has to go and fix. */

}  // namespace sigil::core
