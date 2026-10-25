#pragma once

/** @file
 * @ingroup weave-shaping
 *
 * THE NAMES A FACE GOES BY — its family's, and the font's own name for its
 * style — and whether two names are one: the questions the font context
 * asks of the faces it adopted and the catalogue asks of every face it
 * lists.
 *
 *     weave::fonts::familyName(*typeface);                  // "Georgia"
 *     weave::fonts::sameFamily("Avenir Next", "avenir next");  // true
 *     weave::fonts::styleName(*typeface);                   // "Book"
 *     weave::fonts::sameStyleName("Demi Bold", "demibold");    // true
 */

#include <string>
#include <string_view>

class SkFontStyle;
class SkTypeface;

namespace sigil::weave::fonts {

/** The family name @p typeface states, as its font manager reports it. */
[[nodiscard]] std::string familyName(const SkTypeface& typeface);

/** Whether @p left and @p right name one family: equal with ASCII case
 *  aside, as CSS compares family names. */
[[nodiscard]] bool sameFamily(std::string_view left, std::string_view right);

/** CSS's name for @p weight, rounded to the nearest hundred: "Thin",
 *  "Extra Light", "Light", "Normal", "Medium", "Semi Bold", "Bold",
 *  "Extra Bold", "Black". */
[[nodiscard]] std::string_view weightName(int weight);

/** The name a style goes by where its font names none: CSS's name for its
 *  weight, then "Italic" or "Oblique" where it leans — "Semi Bold
 *  Italic". */
[[nodiscard]] std::string composedStyleName(const SkFontStyle& style);

/** THE FONT'S OWN NAME FOR THE STYLE OF @p typeface, read from its naming
 *  table: the typographic subfamily where the font states one, else the
 *  subfamily — in English from the Unicode or Windows records, else the
 *  Macintosh record — and the composed name where the table names
 *  neither. */
[[nodiscard]] std::string styleName(const SkTypeface& typeface);

/** Whether @p left and @p right name one style, ASCII case and spaces
 *  aside: a font says "Demi Bold" where a writer types "DemiBold", and
 *  neither CSS nor the naming table holds a style name to one spelling. */
[[nodiscard]] bool sameStyleName(std::string_view left, std::string_view right);

}  // namespace sigil::weave::fonts
