#pragma once

/** @file
 * @ingroup compose-core
 *
 * A block of declarations with no selector and no node to land on: what
 * a text span states over the range it finds.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/verbs/Font.h>

namespace sigil::compose {

/** WHAT A SPAN STATES: the element's own font and ink verbs — `font`, its
 *  longhands and `ink` — written into a value that belongs to no element,
 *  so a range of a passage is restyled in the words the passage itself
 *  was styled in: `text.span(where,
 *  Declarations().fontWeight(700).ink(accent))`. What it leaves unsaid
 *  the range keeps.
 *  @trap A paint stated here is laid in the passage's own coordinates, as
 *  it is: a ramp meant to span the whole leaf belongs on the leaf's own
 *  `ink`, which maps it onto the leaf's box. */
class Declarations : public detail::Declaring, public FontVerbs<Declarations> {
 public:
  Declarations() = default;  ///< States nothing.

 private:
  friend struct detail::NodeAccess;
};

}  // namespace sigil::compose
