#pragma once

/** @file
 * @ingroup weave-shaping
 *
 * THE NAME A FACE'S FAMILY GOES BY, and whether two names are one family:
 * the two questions the font context asks of the faces it adopted and the
 * catalogue asks of every face it lists.
 *
 *     weave::fonts::familyName(*typeface);                  // "Georgia"
 *     weave::fonts::sameFamily("Avenir Next", "avenir next");  // true
 */

#include <string>
#include <string_view>

class SkTypeface;

namespace sigil::weave::fonts {

/** The family name @p typeface states, as its font manager reports it. */
[[nodiscard]] std::string familyName(const SkTypeface& typeface);

/** Whether @p left and @p right name one family: equal with ASCII case
 *  aside, as CSS compares family names. */
[[nodiscard]] bool sameFamily(std::string_view left, std::string_view right);

}  // namespace sigil::weave::fonts
