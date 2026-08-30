#pragma once

/** @file
 * Deterministic sample content for demos, stress tests, and benchmarks —
 * shared so every showcase target exercises the same corpus instead of
 * growing its own subtly different filler.
 */

#include <sigilweave/paragraph/Paragraph.h>

#include <array>

#include "sigilweave/kit/Palette.h"

namespace sigil::weave::kit {

/** Builds ~`wordCount` words of mixed Latin/CJK/Hangul filler in alternating
 *  color chunks (multiple spans, cross-script fallback, CJK line-break
 *  opportunities). Deterministic: the same arguments always produce the
 *  same paragraph, so timings and screenshots stay comparable. */
[[nodiscard]] sigil::weave::Paragraph mixedScriptFiller(
    int wordCount, float fontSize,
    std::array<SkColor, 3> chunkColors = {palette::kInk, palette::kBlue,
                                          palette::kAccent});

}  // namespace sigil::weave::kit
