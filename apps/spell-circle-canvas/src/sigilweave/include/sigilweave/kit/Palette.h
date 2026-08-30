#pragma once

/** @file
 * The showcase palette the gallery and demo targets share: warm paper,
 * near-black ink, and the accent colors that read on it.
 */

#include <include/core/SkColor.h>

namespace sigil::weave::kit {

/// The showcase palette shared by the gallery and demo targets: warm paper,
/// near-black ink, and two accent colors that read on it.
namespace palette {
inline constexpr SkColor kInk = 0xFF23252B;
inline constexpr SkColor kAccent = 0xFFC63D2F;
inline constexpr SkColor kBlue = 0xFF2B5AA7;
inline constexpr SkColor kShape = 0x33808A99;
inline constexpr SkColor kPaper = 0xFFFAF7F0;
}  // namespace palette

}  // namespace sigil::weave::kit
