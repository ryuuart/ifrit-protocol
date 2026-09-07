#pragma once

/** @file
 * The showcase palette the gallery and the demo share: warm paper,
 * near-black ink, and the accent colours that read on it.
 *
 * A chosen set of colours, which is a picture rather than a piece of the
 * engine — so it stands with the examples that draw it.
 */

#include <include/core/SkColor.h>

namespace sigil::weave::examples {

namespace palette {
inline constexpr SkColor kInk = 0xFF23252B;
inline constexpr SkColor kAccent = 0xFFC63D2F;
inline constexpr SkColor kBlue = 0xFF2B5AA7;
inline constexpr SkColor kShape = 0x33808A99;
inline constexpr SkColor kPaper = 0xFFFAF7F0;
}  // namespace palette

}  // namespace sigil::weave::examples
