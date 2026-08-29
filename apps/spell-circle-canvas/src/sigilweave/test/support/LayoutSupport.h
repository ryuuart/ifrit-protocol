#pragma once

/** @file
 * Support for weave_layout_test: the font context, paragraph helpers, the
 * layout API and run positions, and a deterministic large text.
 */

#include <cstdint>
#include <random>
#include <span>
#include <string>

#include "Fonts.h"
#include "Layouts.h"
#include "Paragraphs.h"

namespace sigil::weave::test {

/// Deterministic large test text: `wordCount` words drawn from `pool` by an
/// mt19937 seeded with `seed`, each followed by a single space.
inline std::u8string makePooledText(std::span<const char8_t* const> pool,
                                    int wordCount, uint32_t seed) {
  std::mt19937 randomEngine(seed);
  std::u8string text;
  for (int wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
    text += pool[randomEngine() % pool.size()];
    text += ' ';
  }
  return text;
}

}  // namespace sigil::weave::test
