#pragma once

/** @file
 * Support for weave_shaping_test: the font context, paragraph helpers, the
 * layout call the typography tests check placement through, and a cluster
 * count for shaped words.
 */

#include <absl/container/flat_hash_set.h>

#include <cstddef>

#include "Fonts.h"
#include "Layouts.h"
#include "Paragraphs.h"

namespace sigil::weave::test {

/// Number of distinct cluster values in a shaped word.
inline size_t uniqueClusterCount(const ShapedWord& shapedWord) {
  absl::flat_hash_set<uint32_t> unique(shapedWord.clusters.begin(),
                                       shapedWord.clusters.end());
  return unique.size();
}

}  // namespace sigil::weave::test
