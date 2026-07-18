#pragma once

/** @file
 * Shared fixtures and helpers for the textflow_test suite. Every test TU
 * uses these so CoreText font-manager construction (which enumerates the
 * system font set) happens exactly once per process.
 */

#include <textflow/TextFlow.h>

#include <span>
#include <string>
#include <string_view>

namespace textflow::test {

/// The process-wide FontContext over the system (CoreText) font manager.
FontContext &sharedContext();

/// A default TextStyle at the given size.
TextStyle basicStyle(float fontSize = 16.0f);

/// A single-span paragraph over `utf8` at the given size.
Paragraph makeParagraph(std::u8string_view utf8, float fontSize = 16.0f);

/// Exact pen x where a run ends. Blob ink bounds are conservative
/// (font-bounds based), so line-edge checks use shaped advances instead.
/// Each run is one word *segment* (multi-segment words emit several runs,
/// each offset by its own advanceOffset), so use the segment's shaped
/// advance.
float runEnd(const Paragraph &paragraph, const PositionedRun &run);

/// True when every glyph in the paragraph resolved to a real glyph (no
/// .notdef): the script actually shaped with a covering font.
bool allGlyphsResolved(const Paragraph &paragraph);

/// Number of distinct cluster values in a shaped word.
size_t uniqueClusterCount(const ShapedWord &shapedWord);

/// Deterministic large test text: `wordCount` words drawn from `pool` by an
/// mt19937 seeded with `seed`, each followed by a single space.
std::u8string makePooledText(std::span<const char8_t *const> pool,
                             int wordCount, uint32_t seed);

} // namespace textflow::test
