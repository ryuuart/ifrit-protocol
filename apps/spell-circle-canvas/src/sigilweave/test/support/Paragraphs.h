#pragma once

/** @file
 * Paragraph construction helpers shared by every test binary that builds a
 * Paragraph: a default style at a size, a single-span paragraph over it, a
 * deterministic text drawn from a word pool, a three-sentence two-span
 * fixture, and an offset lookup into the text.
 */

#include <sigilweave/paragraph/Paragraph.h>

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

/// A default TextStyle at the given size.
inline TextStyle basicStyle(float fontSize = 16.0f) {
  TextStyle style;
  style.shaping.fontSize = fontSize;
  return style;
}

/// A single-span paragraph over `utf8` at the given size.
inline Paragraph makeParagraph(std::u8string_view utf8,
                               float fontSize = 16.0f) {
  Paragraph paragraph;
  paragraph.appendText(utf8, basicStyle(fontSize));
  return paragraph;
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
