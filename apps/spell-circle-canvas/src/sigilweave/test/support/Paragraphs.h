#pragma once

/** @file
 * Paragraph construction helpers shared by every test binary that builds a
 * Paragraph: a default style at a size, a single-span paragraph over it, a
 * deterministic text drawn from a word pool, a three-sentence two-span
 * fixture, an offset lookup into the text, and the two readings taken off
 * a shaped paragraph — how many glyphs it put down and whether they all
 * resolved.
 */

#include <Fonts.h>
#include <sigilweave/paragraph/Paragraph.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <string>
#include <string_view>

namespace sigil::weave::test {

/// Deterministic text: `wordCount` words drawn from `pool` by an mt19937
/// seeded with `seed`, each followed by `separator`. The pool is anything
/// indexable and sized whose elements append to a u8string — a run of
/// literals, or a vector of them.
template <typename Pool>
inline std::u8string makePooledText(const Pool& pool, int wordCount,
                                    uint32_t seed,
                                    std::u8string_view separator = u8" ") {
  std::mt19937 randomEngine(seed);
  std::u8string text;
  for (int wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
    text += pool[randomEngine() % std::size(pool)];
    text += separator;
  }
  return text;
}

/// A TextStyle at the given size in the instrument that puts every letter
/// on one known advance, so a width or a line count read off it is the
/// same on every machine. A case whose claim is the machine's own face
/// asks for `machineStyle` instead.
inline TextStyle basicStyle(float fontSize = 16.0f) {
  TextStyle style;
  style.shaping.typeface = sigil::test::instrument::sans();
  style.shaping.fontSize = fontSize;
  return style;
}

/// A TextStyle at the given size that names no face, so the font context
/// resolves the machine's default and falls back through the machine's
/// families: the style for a case whose claim IS that resolution, which
/// carries the `fonts` label because a runner without faces fails it.
inline TextStyle machineStyle(float fontSize = 16.0f) {
  TextStyle style;
  style.shaping.fontSize = fontSize;
  return style;
}

/// A single-span paragraph over `utf8` in the given style — what a case
/// naming its own face or its own features builds instead of `makeParagraph`.
inline Paragraph paragraphIn(std::u8string_view utf8, const TextStyle& style) {
  Paragraph paragraph;
  paragraph.appendText(utf8, style);
  return paragraph;
}

/// A single-span paragraph over `utf8` at the given size, in the instrument.
inline Paragraph makeParagraph(std::u8string_view utf8,
                               float fontSize = 16.0f) {
  return paragraphIn(utf8, basicStyle(fontSize));
}

/// A single-span paragraph over `utf8` at the given size in the machine's
/// default face, for a case about what the machine resolves.
inline Paragraph machineParagraph(std::u8string_view utf8,
                                  float fontSize = 16.0f) {
  return paragraphIn(utf8, machineStyle(fontSize));
}

/// Glyphs the paragraph shaped, over every word and every segment.
inline size_t shapedGlyphCount(const Paragraph& paragraph) {
  size_t count = 0;
  for (const Word& word : paragraph.words())
    for (const WordSegment& segment : word.segments())
      count += segment.shaped->glyphs.size();
  return count;
}

/// True when every glyph in the paragraph resolved to a real glyph (no
/// .notdef): the script actually shaped with a covering font.
inline bool allGlyphsResolved(const Paragraph& paragraph) {
  for (const Word& word : paragraph.words())
    for (const WordSegment& seg : word.segments())
      for (uint16_t glyph : seg.shaped->glyphs)
        if (glyph == 0) return false;
  return true;
}

/// Three sentences over two style spans, long enough to wrap: the fixture
/// every index assertion reads against.
inline Paragraph mixedStyleParagraph() {
  TextStyle base = basicStyle(18.0f);
  TextStyle accent = base;
  accent.paint.foreground.setColor(SK_ColorRED);
  ParagraphBuilder builder(base);
  builder.addText(u8"Letters leave their lines. ")
      .pushStyle(accent)
      .addText(u8"Some of them come back!")
      .popStyle()
      .addText(u8" The rest keep falling.");
  return builder.build();
}

/// UTF-16 offset of `needle` in the paragraph's text.
inline uint32_t offsetOf(const Paragraph& paragraph,
                         std::u16string_view needle) {
  const size_t position = paragraph.text().find(needle);
  return position == std::u16string::npos ? ~0u
                                          : static_cast<uint32_t>(position);
}

}  // namespace sigil::weave::test
