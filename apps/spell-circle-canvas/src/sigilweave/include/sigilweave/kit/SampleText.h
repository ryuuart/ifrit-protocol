#pragma once

/** @file
 * @ingroup weave-kit
 *
 * Deterministic sample content for demos, stress tests, and benchmarks —
 * shared so every showcase target exercises the same corpus instead of
 * growing its own subtly different filler.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilweave/paragraph/Paragraph.h>

#include <array>

namespace sigil::weave::kit {

/** Builds about @p wordCount words of mixed Latin, CJK and Hangul filler
 *  in alternating colour chunks, exercising several spans, cross-script
 *  fallback and CJK break opportunities. Deterministic: the same
 *  arguments always produce the same paragraph. The default chunk colours
 *  are ink, blue and a warm accent. */
[[nodiscard]] sigil::weave::Paragraph mixedScriptFiller(
    int wordCount, float fontSize,
    std::array<material::Color, 3> chunkColors = {material::hexColor(0x23252B),
                                                  material::hexColor(0x2B5AA7),
                                                  material::hexColor(0xC63D2F)});

}  // namespace sigil::weave::kit
