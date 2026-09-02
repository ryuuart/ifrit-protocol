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

/** The Japanese set, in the shape most houses use: the closing brackets,
 *  the sentence marks, the small kana, the sound marks and the prolonged
 *  sound mark may not OPEN a line; the opening brackets may not close
 *  one. Every entry is a prohibition somebody would notice the absence of
 *  in a printed page — a comma alone at the head of a column reads as a
 *  mistake rather than as a comma.
 *
 *  MUCH OF IT IS ALREADY TRUE. The segmentation is UAX #14's, which
 *  forbids most of these on its own, so this table changes little on a
 *  plain Japanese passage and is not where its value is. Its value is that
 *  a table is the seam: a house that forbids one more character, a script
 *  with its own prohibitions, a document that must not break before a
 *  numeral — each is a table of its own, and this one is a peer of them
 *  rather than a rule the engine holds. */
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
