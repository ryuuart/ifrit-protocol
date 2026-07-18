#pragma once

/** @file
 * Deterministic sample content for demos, stress tests, and benchmarks —
 * shared so every showcase target exercises the same corpus instead of
 * growing its own subtly different filler.
 */

#include <textflow/Paragraph.h>

#include <array>

namespace textflowkit {

/// The showcase palette shared by the gallery and demo targets: warm paper,
/// near-black ink, and two accent colors that read on it.
namespace palette {
inline constexpr SkColor kInk = 0xFF23252B;
inline constexpr SkColor kAccent = 0xFFC63D2F;
inline constexpr SkColor kBlue = 0xFF2B5AA7;
inline constexpr SkColor kShape = 0x33808A99;
inline constexpr SkColor kPaper = 0xFFFAF7F0;
} // namespace palette

/** Builds ~`wordCount` words of mixed Latin/CJK/Hangul filler in alternating
 *  color chunks (multiple spans, cross-script fallback, CJK line-break
 *  opportunities). Deterministic: the same arguments always produce the
 *  same paragraph, so timings and screenshots stay comparable. */
[[nodiscard]] textflow::Paragraph
mixedScriptFiller(int wordCount, float fontSize,
                  std::array<SkColor, 3> chunkColors = {
                      palette::kInk, palette::kBlue, palette::kAccent});

} // namespace textflowkit
