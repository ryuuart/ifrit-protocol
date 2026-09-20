#pragma once

/** @file
 * @ingroup weave-layout
 *
 * SETTING A RUN BESIDE ANOTHER'S EXTENT — the placement every reading
 * over or beside a base is made of: how much room it needs, where it
 * stands, and how it is split when its base breaks. Nothing here knows
 * what a ruby IS, or how big a reading should be relative to its base: a
 * reading's size is its own style's.
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
 *  `Before` is above a line and to the RIGHT of a column, `After` below a
 *  line and to the LEFT of one. The reading is centred on the base's own
 *  extent and stands `gap` px clear of its band across it.
 */
struct Beside {
  SkRect base = SkRect::MakeEmpty();  ///< the extent the base occupied
  WritingMode writingMode = WritingMode::kHorizontal;
  /** Which side of the base the reading stands on, named by the reading
   *  direction rather than by the screen. */
  enum class Side : uint8_t {
    Before,  ///< above a line, right of a column
    After    ///< below a line, left of a column
  };
  Side side = Side::Before;  ///< which side of the base to place on
  float gap = 0;             ///< px the reading stands clear of the base
};

/** The band a reading set in @p style needs beside a line, @p gap
 *  included — the number a block reserves before anything is broken. It
 *  is the reading's OWN strut, so it depends on nothing about the base,
 *  which is why a reservation costs no round of convergence.
 */
[[nodiscard]] float bandBeside(FontContext& fontContext, const TextStyle& style,
                               float gap);

/** Lays @p reading out beside the base's extent and returns where it
 *  landed: one line — or one column, in a vertical setting — at the
 *  reading's own natural width, centred on the base and standing clear of
 *  it. Its writing mode is set from @p beside before it is laid out.
 */
[[nodiscard]] ParagraphLayout layoutBeside(FontContext& fontContext,
                                           Paragraph& reading,
                                           const Beside& beside);

/** The part of @p reading that belongs to a piece of a base carrying
 *  @p here of the base's advance where the rest carries @p next: a base
 *  that breaks splits its reading in proportion to the advance either
 *  side, the reading's own characters not corresponding one for one. The
 *  cut lands on a UTF-16 boundary and never inside a surrogate pair.
 */
[[nodiscard]] std::u16string shareOfReading(std::u16string_view reading,
                                            float here, float next);

/** WHERE A NOTE SET IN TWO LINES INSIDE ONE LINE OF ITS BASE IS CUT, and
 *  what room it then needs — warichu, the aside a text sets small and
 *  doubled inside the line it interrupts. The cut is the break
 *  opportunity leaving the two lines CLOSEST IN ADVANCE, and the note's
 *  size is its own style's: nothing here halves anything.
 */
struct WarichuSplit {
  float advance = 0;     ///< the wider of the two lines
  float band = 0;        ///< the depth the two lines stack into
  uint32_t cutWord = 0;  ///< the note's first word on the second line
};

/** The cut, the advance and the band a two-line note needs. A note of one
 *  word is one line, and says so with `cutWord` past its last word. */
[[nodiscard]] WarichuSplit warichuSplit(FontContext& fontContext,
                                        Paragraph& note);

/** Lays @p note out as two lines inside @p slot — the inline box a
 *  placeholder reserved for it — and returns where it landed. The two
 *  lines stack across the box, in the note's own writing mode.
 *  @trap A slot narrower than the split's advance sets the note anyway:
 *  the note is the caller's to size.
 */
[[nodiscard]] ParagraphLayout layoutWarichu(FontContext& fontContext,
                                            Paragraph& note, const SkRect& slot,
                                            WritingMode writingMode);

}  // namespace sigil::weave
