#pragma once

/** @file
 * @ingroup paragraph
 *
 * The GRANULARITY a passage is addressed by: `Unit`, one enumeration with
 * one spelling — `Unit::Word`.
 *
 * Every question something asks of finished text — which glyphs a
 * selection covers, what a stagger steps over, what an annotation stands
 * beside — is asked at one of these sizes. Five of them are sizes this
 * engine already segments at: the shaper's clusters, the Unicode leaf's
 * word and sentence breaks, the breaker's lines. The sixth is the extent
 * the caller named, which the engine segments at nothing and reads off
 * the question instead.
 */

#include <cstdint>

namespace sigil::weave {

/** The granularity a passage is addressed by.
 *
 *  `Cluster` is the one that keeps text correct: a base letter and its
 *  combining marks, or the several glyphs an emoji sequence shapes to,
 *  are ONE cluster and move together. `Glyph` is the raw shaping unit and
 *  will separate those marks from what they sit on.
 *
 *  `Selection` is the odd one: not a size the engine segments at, but the
 *  extent the CALLER named — one unit per stretch a selector addresses
 *  without interruption, however many clusters, words or lines that
 *  stretch turns out to be made of, and a second unit wherever the
 *  address stops and starts again. It is what a reading over a compound
 *  is placed from, because a compound is a selection and not a break
 *  opportunity: a breaker may open an opportunity inside one. Nothing
 *  numbers it before the selector is resolved, so it is answered where
 *  the selection is and not off the placement. */
enum class Unit : uint8_t { Glyph, Cluster, Word, Line, Sentence, Selection };

}  // namespace sigil::weave
