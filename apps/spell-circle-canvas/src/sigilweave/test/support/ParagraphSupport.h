#pragma once

/** @file
 * Support for weave_paragraph_test: the font context, paragraph helpers,
 * and a cluster count for shaped words.
 */

#include <boost/unordered/unordered_flat_set.hpp>
#include <cstddef>

#include "Fonts.h"
#include "Paragraphs.h"

namespace sigil::weave::test {

/// Number of distinct cluster values in a shaped word.
inline size_t uniqueClusterCount(const ShapedWord& shapedWord) {
  boost::unordered_flat_set<uint32_t> unique(shapedWord.clusters.begin(),
                                             shapedWord.clusters.end());
  return unique.size();
}

}  // namespace sigil::weave::test
