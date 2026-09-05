#pragma once

/** @file
 * @ingroup paragraph
 *
 * The GRANULARITY a passage is addressed by: `Unit`, spelled `unit::Word`
 * where a call site reads better for it.
 *
 * Every question something asks of finished text — which glyphs a
 * selection covers, what a stagger steps over, what an annotation stands
 * beside — is asked at one of these five sizes, and they are the sizes
 * this engine already segments at: the shaper's clusters, the Unicode
 * leaf's word and sentence breaks, the breaker's lines. Nothing here
 * names a size the engine would have to invent.
 */

#include <cstdint>

namespace sigil::weave {

/** The granularity a passage is addressed by.
 *
 *  `Cluster` is the one that keeps text correct: a base letter and its
 *  combining marks, or the several glyphs an emoji sequence shapes to,
 *  are ONE cluster and move together. `Glyph` is the raw shaping unit and
 *  will separate those marks from what they sit on. */
enum class Unit : uint8_t { Glyph, Cluster, Word, Line, Sentence };

/** The granularities as constants, for the call sites that read better
 *  spelling one out: `unit::Word`. */
namespace unit {
inline constexpr Unit Glyph = Unit::Glyph;
inline constexpr Unit Cluster = Unit::Cluster;
inline constexpr Unit Word = Unit::Word;
inline constexpr Unit Line = Unit::Line;
inline constexpr Unit Sentence = Unit::Sentence;
}  // namespace unit

}  // namespace sigil::weave
