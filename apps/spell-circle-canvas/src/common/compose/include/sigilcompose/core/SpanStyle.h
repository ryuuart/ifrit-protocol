#pragma once

/** @file
 * @ingroup compose-core
 *
 * The style a text span lays over the range it finds: the font and the
 * ink, in a value with no selector and no node to land on.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/verbs/Font.h>

namespace sigil::compose {

/** THE STYLE A SPAN LAYS OVER ITS RANGE: the element's own font and ink
 *  verbs — `font`, its longhands and `ink` — written into a value that
 *  belongs to no element, so a range is restyled in the words the
 *  passage itself was styled in:
 *  `text.span(where, SpanStyle().fontWeight(700).ink(accent))`. What it
 *  leaves unsaid the range keeps. No box verb and no text property: a
 *  range has no box of its own.
 *  @trap A paint stated here is laid in the passage's own coordinates, as
 *  it is, unless its `ink` names a unit, when it restarts on each unit
 *  of the range: a ramp meant to span the whole leaf belongs on the
 *  leaf's own `ink`, which maps it onto the leaf's box. */
class SpanStyle : public detail::Declaring, public FontVerbs<SpanStyle> {
 public:
  SpanStyle() = default;  ///< States nothing.

 private:
  friend struct detail::NodeAccess;
};

}  // namespace sigil::compose
