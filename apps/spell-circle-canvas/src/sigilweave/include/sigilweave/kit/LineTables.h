#pragma once

/** @file
 * @ingroup weave-kit
 *
 * WHAT MAY STAND AT A LINE'S EDGE, as DATA — the two tables the layout
 * asks for and holds no opinion about: which characters may not open or
 * close a line, and how far a character may stand OUTSIDE the measure.
 * Both are stock values, and a caller's own table is a peer of them.
 */

#include "sigilweave/paragraph/Hyphenation.h"

namespace sigil::weave::kit {

/** Prohibition sets — which characters may not stand at a line's edge. */
namespace kinsoku {

/** The Japanese set, DERIVED from the line-break class each character
 *  carries rather than typed out, and narrowed to the characters SET IN A
 *  FULL-WIDTH CELL, ASCII punctuation being left to the segmentation.
 *  @trap A TAILORING COMES FIRST: a locale naming its line-break rules
 *  already refuses most of these boundaries, so a table is what a HOUSE
 *  adds on top rather than the prohibition itself. */
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
