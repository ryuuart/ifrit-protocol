#pragma once

/** @file
 * Deterministic sample content for demos, stress tests, and benchmarks —
 * shared so every showcase target exercises the same corpus instead of
 * growing its own subtly different filler.
 */

#include <sigilweave/paragraph/Paragraph.h>

#include <array>

namespace sigil::weave::kit {

/** Builds ~`wordCount` words of mixed Latin/CJK/Hangul filler in alternating
 *  color chunks (multiple spans, cross-script fallback, CJK line-break
 *  opportunities). Deterministic: the same arguments always produce the
 *  same paragraph, so timings and screenshots stay comparable.
 *
 *  The default chunk colours are ink, blue and a warm accent — three that
 *  tell the spans apart on paper. A caller drawing into its own palette
 *  passes its own. */
[[nodiscard]] sigil::weave::Paragraph mixedScriptFiller(
    int wordCount, float fontSize,
    std::array<SkColor, 3> chunkColors = {0xFF23252B, 0xFF2B5AA7,
                                          0xFFC63D2F});

}  // namespace sigil::weave::kit
