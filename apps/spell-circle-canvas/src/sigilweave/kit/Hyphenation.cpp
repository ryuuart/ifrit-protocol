/** @file
 * Liang's pattern method over a loaded table. The one table this kit
 * carries is a page of data, so it sits beside this in EnglishPatterns.cpp.
 */

#include "sigilweave/kit/Hyphenation.h"

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <functional>

#include "sigilweave/unicode/Unicode.h"

namespace sigil::weave::kit {

namespace {

/** Hashes a pattern's letters through a view, so looking a substring of a
 *  word up in the table needs no string of its own. */
struct LettersHash {
  using is_transparent = void;
  size_t operator()(std::u32string_view letters) const noexcept {
    return std::hash<std::u32string_view>{}(letters);
  }
};

/** Lower-cases a word into `letters`, one entry per CODE POINT, recording
 *  in `offsets` the word offset each one came from, and reports whether
 *  the word is made of letters alone.
 *
 *  A word carrying a digit, an apostrophe or a hyphen of its own is left
 *  whole: a pattern table is built over letters and a match across
 *  anything else is a coincidence. The letters may be of any script — a
 *  table declares the language it answers for and it is the table, not
 *  this engine, that knows which alphabet it was written over.
 *
 *  The mapping is the simple per-code-point one, so `letters` and
 *  `offsets` stay the same length and a position in the matched form still
 *  names the character it came from; the answer must be offsets into the
 *  word as the caller spelled it. */
bool foldToLetters(std::u16string_view word, std::u32string& letters,
                   std::vector<uint32_t>& offsets) {
  letters.clear();
  offsets.clear();
  size_t cursor = 0;
  while (cursor < word.size()) {
    const size_t codePointStart = cursor;
    const char32_t codePoint = unicode::decodeAt(word, cursor);
    if (!unicode::isLetter(codePoint)) return false;
    letters.push_back(unicode::lowerCased(codePoint));
    offsets.push_back(static_cast<uint32_t>(codePointStart));
  }
  return !letters.empty();
}

}  // namespace

/** The table: patterns keyed by their letter run, each carrying the digits
 *  that sit between those letters, plus the exception spellings. Letters
 *  are code points, so one entry is one letter whatever its script and
 *  however many code units spell it. */
struct PatternHyphenator::Table {
  // letters → one value per position, [0, letters.size()]
  boost::unordered_flat_map<std::u32string, std::vector<uint8_t>, LettersHash,
                            std::equal_to<>>
      patterns;
  // whole lower-cased word → the letter positions it may break at
  boost::unordered_flat_map<std::u32string, std::vector<uint32_t>, LettersHash,
                            std::equal_to<>>
      exceptions;
  size_t longestPattern = 0;
};

PatternHyphenator::PatternHyphenator(std::string languagePrefix,
                                     std::string_view patternFile) {
  load(std::move(languagePrefix), patternFile);
}

PatternHyphenator::~PatternHyphenator() = default;

size_t PatternHyphenator::patternCount() const {
  return m_table ? m_table->patterns.size() : 0;
}

void PatternHyphenator::load(std::string languagePrefix,
                             std::string_view patternFile) {
  m_language = std::move(languagePrefix);
  m_table = std::make_unique<Table>();

  // Pattern files are UTF-8; that is the encoding every published table
  // outside English needs and the only one that can spell them. Text that
  // is not valid UTF-8 loads as nothing, which costs break points and
  // never correctness.
  const std::u16string text = unicode::toUtf16(
      std::u8string_view(reinterpret_cast<const char8_t*>(patternFile.data()),
                         patternFile.size()));

  bool inExceptions = false;
  bool inComment = false;
  std::u32string token;
  std::u32string letters;
  std::vector<uint8_t> values;

  // One whitespace-separated token, read as a pattern or as an exception
  // spelling depending on which half of the file it stands in.
  const auto settleToken = [&] {
    if (token.empty()) return;
    if (token == U"exceptions") {
      inExceptions = true;
      token.clear();
      return;
    }
    letters.clear();
    if (inExceptions) {
      std::vector<uint32_t> breaks;
      for (const char32_t codePoint : token) {
        if (codePoint == U'-') {
          breaks.push_back(static_cast<uint32_t>(letters.size()));
          continue;
        }
        letters.push_back(unicode::lowerCased(codePoint));
      }
      if (!letters.empty()) m_table->exceptions[letters] = std::move(breaks);
      token.clear();
      return;
    }
    // A pattern: letters with digits between them, and `.` standing for a
    // word boundary. The value list is one longer than the letters,
    // because a digit may stand at either end.
    values.assign(1, 0);
    bool valid = true;
    for (const char32_t codePoint : token) {
      if (codePoint >= U'0' && codePoint <= U'9') {
        values.back() = static_cast<uint8_t>(codePoint - U'0');
        continue;
      }
      if (codePoint != U'.' && !unicode::isLetter(codePoint)) {
        valid = false;
        break;
      }
      letters.push_back(unicode::lowerCased(codePoint));
      values.push_back(0);
    }
    if (valid && !letters.empty()) {
      m_table->longestPattern =
          std::max(m_table->longestPattern, letters.size());
      m_table->patterns[letters] = values;
    }
    token.clear();
  };

  size_t cursor = 0;
  while (cursor < text.size()) {
    const char32_t codePoint = unicode::decodeAt(text, cursor);
    if (codePoint == U'\n' || codePoint == U'\r') {
      inComment = false;
      settleToken();
      continue;
    }
    if (inComment) continue;
    if (codePoint == U'%') {
      // A published table carries its licence and its provenance as
      // comments running from a percent sign to the end of the line, and
      // the words in them are not patterns.
      inComment = true;
      settleToken();
      continue;
    }
    if (unicode::isWhitespace(codePoint)) {
      settleToken();
      continue;
    }
    token.push_back(codePoint);
  }
  settleToken();
}

void PatternHyphenator::breakPoints(std::u16string_view word,
                                    std::string_view languageTag,
                                    std::vector<uint32_t>& out) const {
  if (!m_table || m_language.empty()) return;
  // The tag must be this table's language. An empty tag is a text that
  // never said, and a table cannot answer for a language nobody named.
  if (languageTag.size() < m_language.size() ||
      languageTag.compare(0, m_language.size(), m_language) != 0)
    return;

  std::u32string letters;
  std::vector<uint32_t> offsets;
  if (!foldToLetters(word, letters, offsets)) return;

  const auto exception = m_table->exceptions.find(letters);
  if (exception != m_table->exceptions.end()) {
    for (const uint32_t letterIndex : exception->second)
      if (letterIndex < offsets.size()) out.push_back(offsets[letterIndex]);
    return;
  }

  // Liang's method: the word is padded with the boundary marker, every
  // substring is looked up, and each pattern's digits are taken at their
  // maximum against the positions they cover. An ODD value is a break.
  const std::u32string padded = U"." + letters + U".";
  const std::u32string_view scan(padded);
  std::vector<uint8_t> points(padded.size() + 1, 0);
  for (size_t start = 0; start < padded.size(); ++start) {
    const size_t longest =
        std::min(m_table->longestPattern, padded.size() - start);
    for (size_t length = 1; length <= longest; ++length) {
      const auto found = m_table->patterns.find(scan.substr(start, length));
      if (found == m_table->patterns.end()) continue;
      const std::vector<uint8_t>& pattern = found->second;
      for (size_t index = 0; index < pattern.size(); ++index)
        points[start + index] = std::max(points[start + index], pattern[index]);
    }
  }
  // points[i] sits BEFORE padded[i]; padded[0] is the boundary marker, so
  // a break before padded[i] falls before the word's letter i - 1, and
  // that letter's own offset is where it lands in the caller's word.
  for (size_t index = 2; index + 2 < padded.size(); ++index)
    if (points[index] & 1u) out.push_back(offsets[index - 1]);
}

}  // namespace sigil::weave::kit
