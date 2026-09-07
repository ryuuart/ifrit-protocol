#pragma once

/** @file
 * WHAT MAY STAND AT A LINE'S EDGE, as DATA — the two tables the layout
 * asks for and holds no opinion about.
 *
 * `kinsoku::` says which characters may not open or close a line, and the
 * engine settles the prohibition during segmentation by never opening that
 * boundary. `hanging::` says how far a character may stand OUTSIDE the
 * measure, as a fraction of its own advance, so a line that begins on a
 * quote or ends in a comma squares optically rather than on its advances —
 * optical margin alignment down a page, burasagari down a column.
 *
 * Both are stock values and a caller's own table is a peer of them: which
 * marks a house hangs, and how far, is a decision, and decisions are the
 * caller's.
 */

#include "sigilweave/paragraph/Hyphenation.h"

namespace sigil::weave::kit {

/** Prohibition sets — which characters may not stand at a line's edge. */
namespace kinsoku {

/** The Japanese set, DERIVED from the line-break class each character
 *  carries rather than typed out: what may not OPEN a line is the closing
 *  punctuation and parentheses, the non-starters, the conditional Japanese
 *  starters, the exclamation and question marks and the infix numeric
 *  separators; what may not CLOSE one is the opening punctuation. The set
 *  is then narrowed to the characters SET IN A FULL-WIDTH CELL, which is
 *  the punctuation of the ideographic grid and exactly what the convention
 *  is about; ASCII punctuation is left to the segmentation.
 *
 *  A TAILORING COMES FIRST. Segmentation runs under a locale
 *  (`Paragraph::setLineBreakLocale`), and a locale that names its
 *  line-break rules — the strict Japanese ones a printed page is set under,
 *  the loose Chinese ones — already refuses most of these boundaries, so
 *  a script's own prohibitions are the segmentation's answer and not this
 *  table's. A table is what a HOUSE adds on top: one more character it
 *  forbids, a document that must not break before a numeral. This one is a
 *  peer of a caller's own rather than a rule the engine holds. */
[[nodiscard]] KinsokuTable japanese();

}  // namespace kinsoku

/** Hanging tables — how far a character may stand outside the measure. */
namespace hanging {

/** The Latin set: the marks whose ink sits high and thin, so a margin
 *  squared on their advances reads as ragged. The quotes and the hyphen
 *  hang most, the full stop and the comma least, and nothing with a stem
 *  hangs at all. */
[[nodiscard]] HangingTable latin();

/** The Japanese set (burasagari): the sentence marks alone, hanging at a
 *  line's end — which is the convention the name refers to, and is why
 *  nothing here hangs at the start. */
[[nodiscard]] HangingTable japanese();

}  // namespace hanging

}  // namespace sigil::weave::kit
