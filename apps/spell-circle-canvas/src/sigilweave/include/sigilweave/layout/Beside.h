#pragma once

/** @file
 * @ingroup layout
 *
 * SETTING A RUN BESIDE ANOTHER'S EXTENT — the placement every reading over
 * or beside a base is made of: furigana over a compound, emphasis marks
 * down a column, a gloss under a phrase.
 *
 * Three questions, and they are all the engine's rather than a caller's.
 * How much room does a reading of this type need beside a line — the
 * number that goes into a block's strut BEFORE the base is broken, which
 * is what makes a reservation a layout input rather than a cycle. Where
 * does the reading stand against the extent its base occupied. And when a
 * base breaks across a line or a column, which part of the reading goes
 * with which part of the base.
 *
 * Nothing here knows what a ruby IS, or which unit a caller annotated, or
 * how big a reading should be relative to its base — a reading's size is
 * its own style's, and there is no fraction of anything in this file.
 */

#include <include/core/SkRect.h>

#include <string>
#include <string_view>

#include "sigilweave/layout/ParagraphLayout.h"
#include "sigilweave/paragraph/Paragraph.h"
#include "sigilweave/style/Style.h"

namespace sigil::weave {

class FontContext;

/** WHERE A READING STANDS against the extent its base occupied.
 *
 *  `Before` is above a line and to the RIGHT of a column, `After` below a
 *  line and to the LEFT of one — the sides each writing mode reads its
 *  furniture on. The reading is centred on the base's own extent along the
 *  reading direction and stands `gap` clear of its band across it.
 */
struct Beside {
  SkRect base = SkRect::MakeEmpty();  ///< the extent the base occupied
  WritingMode writingMode = WritingMode::kHorizontal;
  enum class Side : uint8_t { Before, After };
  Side side = Side::Before;
  float gap = 0;
};

/** The band a reading set in `style` needs beside a line, `gap` included.
 *
 *  This is the number a block reserves (ParagraphLayoutOptions::reserved)
 *  before anything is broken: it is the reading's OWN strut, so it depends
 *  on the reading's type and on nothing about the base — which is the
 *  whole reason a reservation costs no round of convergence.
 */
[[nodiscard]] float bandBeside(FontContext& fontContext,
                               const TextStyle& style, float gap);

/** Lays `reading` out beside `beside.base` and returns where it landed.
 *
 *  One line — or one column, in a vertical setting — at the reading's own
 *  natural width, centred on the base and standing clear of it. The
 *  reading's writing mode is set from `beside` before it is laid out, so a
 *  column's reading runs down the column beside it.
 */
[[nodiscard]] ParagraphLayout layoutBeside(FontContext& fontContext,
                                           Paragraph& reading,
                                           const Beside& beside);

/** The part of `reading` that belongs to a piece of a base carrying
 *  `here` of the base's advance where the rest carries `next`.
 *
 *  A base that breaks across a line or a column splits its reading with
 *  it, in proportion to the advance either side — which is the only
 *  proportion a reading has to go by, since the reading's own characters
 *  need not correspond to the base's one for one. The cut lands on a
 *  UTF-16 boundary and never inside a surrogate pair.
 */
[[nodiscard]] std::u16string shareOfReading(std::u16string_view reading,
                                            float here, float next);

}  // namespace sigil::weave
