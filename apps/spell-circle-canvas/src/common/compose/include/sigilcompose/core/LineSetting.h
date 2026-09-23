#pragma once

/** @file
 * @ingroup compose-core
 *
 * How the lines of a paragraph are broken and how a justified one is
 * fitted, in CSS's words: the keywords `textWrap` and `textJustify` take.
 */

#include <cstdint>

namespace sigil::compose {

/** HOW THE LINES BREAK — CSS `text-wrap`'s style. `Auto` and `Stable`
 *  fill each line as full as it goes before starting the next, so a line
 *  never moves because a later one changed; `Pretty` weighs the whole
 *  paragraph at once (Knuth-Plass); `Balance` weighs it and then sets it
 *  in the narrowest measure that keeps the same number of lines, which
 *  evens the rag. */
enum class TextWrap : uint8_t { Auto, Balance, Stable, Pretty };

/** WHERE A JUSTIFIED LINE SPENDS ITS SLACK — CSS `text-justify`. `Auto`
 *  opens the word gaps, the gaps between ideographs, and whatever letter
 *  and glyph passes the justification is tuned for; `InterWord` opens the
 *  word separators alone; `InterCharacter` opens every grapheme cluster
 *  and every word separator alike, each by at most half the size by
 *  default; `None` justifies nothing and sets a justified line at its
 *  start. */
enum class TextJustify : uint8_t { Auto, InterWord, InterCharacter, None };

}  // namespace sigil::compose
