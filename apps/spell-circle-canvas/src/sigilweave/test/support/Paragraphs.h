#pragma once

/** @file
 * Paragraph construction helpers shared by every test binary that builds a
 * Paragraph: a default style at a size, and a single-span paragraph over it.
 */

#include <sigilweave/Paragraph.h>

#include <string_view>

namespace sigil::weave::test {

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
    for (const WordSegment& seg : word.segments)
      for (uint16_t glyph : seg.shaped->glyphs)
        if (glyph == 0) return false;
  return true;
}

}  // namespace sigil::weave::test
