#pragma once

/** @file
 * @ingroup weave-document
 *
 * The GRANULARITY a passage is addressed by: `Unit`, one enumeration.
 * Every question something asks of finished text is asked at one of its
 * sizes — five the engine already segments at, and the extent the caller
 * named, which it segments at nothing and reads off the question.
 */

#include <cstdint>

namespace sigil::weave {

/** The granularity a passage is addressed by.
 *
 *  `Cluster` is the one that keeps text correct: a base letter and its
 *  combining marks, or the several glyphs an emoji sequence shapes to,
 *  are ONE cluster and move together. `Glyph` is the raw shaping unit and
 *  will separate those marks from what they sit on.
 *  @trap `Selection` is not a size the engine segments at but the extent
 *  the CALLER named, so nothing numbers it before the selector is
 *  resolved. */
enum class Unit : uint8_t { Glyph, Cluster, Word, Line, Sentence, Selection };

}  // namespace sigil::weave
