/** @file
 * A face's family name and the font's own name for its style, and the
 * comparisons that say two names are one.
 */

#include <include/core/SkData.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkString.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/fonts/FamilyName.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <ranges>

namespace sigil::weave::fonts {

std::string familyName(const SkTypeface& typeface) {
  SkString name;
  typeface.getFamilyName(&name);
  return {name.c_str(), name.size()};
}

bool sameFamily(std::string_view left, std::string_view right) {
  return std::ranges::equal(
      left, right, [](char leftCharacter, char rightCharacter) {
        return std::tolower(static_cast<unsigned char>(leftCharacter)) ==
               std::tolower(static_cast<unsigned char>(rightCharacter));
      });
}

namespace {

uint16_t bigEndian16(const uint8_t* at) {
  return static_cast<uint16_t>((at[0] << 8) | at[1]);
}

/** One code point as UTF-8, appended to @p text. */
void appendUtf8(std::string& text, uint32_t codePoint) {
  if (codePoint < 0x80) {
    text.push_back(static_cast<char>(codePoint));
  } else if (codePoint < 0x800) {
    text.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
    text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  } else if (codePoint < 0x10000) {
    text.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
    text.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  } else {
    text.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
    text.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
    text.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    text.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  }
}

/** THE FONT'S OWN NAME FOR ITS STYLE, read from its naming table: the
 *  typographic subfamily where the font states one, else the subfamily —
 *  in English from the Unicode or Windows records, else the Macintosh
 *  record. Empty where the table names neither. */
std::string tableStyleName(const SkTypeface& typeface) {
  constexpr SkFontTableTag kNameTable = SkSetFourByteTag('n', 'a', 'm', 'e');
  const sk_sp<SkData> table = typeface.copyTableData(kNameTable);
  if (!table || table->size() < 6) return {};
  const auto* bytes = static_cast<const uint8_t*>(table->data());
  const size_t size = table->size();
  const uint16_t count = bigEndian16(bytes + 2);
  const uint16_t storage = bigEndian16(bytes + 4);
  // The typographic subfamily (17) stands over the subfamily (2); within
  // one, a Unicode-encoded English record over a Macintosh one.
  for (const uint16_t wanted : {uint16_t{17}, uint16_t{2}}) {
    std::string macintosh;
    for (uint16_t index = 0; index < count; ++index) {
      const size_t record = 6 + size_t{index} * 12;
      if (record + 12 > size) break;
      const uint16_t platform = bigEndian16(bytes + record);
      const uint16_t encoding = bigEndian16(bytes + record + 2);
      const uint16_t language = bigEndian16(bytes + record + 4);
      const uint16_t nameId = bigEndian16(bytes + record + 6);
      const uint16_t length = bigEndian16(bytes + record + 8);
      const size_t offset = size_t{storage} + bigEndian16(bytes + record + 10);
      if (nameId != wanted || offset + length > size) continue;
      const uint8_t* text = bytes + offset;
      const bool unicode =
          platform == 0 ||
          (platform == 3 && (encoding == 1 || encoding == 10) &&
           (language & 0xFF) == 0x09);
      if (unicode) {
        std::string name;
        for (size_t at = 0; at + 1 < length; at += 2) {
          uint32_t unit = bigEndian16(text + at);
          if (unit >= 0xD800 && unit < 0xDC00 && at + 3 < length) {
            const uint32_t low = bigEndian16(text + at + 2);
            unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
            at += 2;
          }
          appendUtf8(name, unit);
        }
        if (!name.empty()) return name;
      } else if (platform == 1 && encoding == 0 && language == 0 &&
                 macintosh.empty()) {
        macintosh.assign(reinterpret_cast<const char*>(text), length);
      }
    }
    if (!macintosh.empty()) return macintosh;
  }
  return {};
}

}  // namespace

bool sameStyleName(std::string_view left, std::string_view right) {
  const auto letters = [](std::string_view name) {
    return name |
           std::views::filter([](char character) { return character != ' '; });
  };
  return std::ranges::equal(
      letters(left), letters(right),
      [](char leftCharacter, char rightCharacter) {
        return std::tolower(static_cast<unsigned char>(leftCharacter)) ==
               std::tolower(static_cast<unsigned char>(rightCharacter));
      });
}

std::string_view weightName(int weight) {
  static constexpr std::array<std::string_view, 9> kNames = {
      "Thin",      "Extra Light", "Light",      "Normal", "Medium",
      "Semi Bold", "Bold",        "Extra Bold", "Black"};
  const int hundred = std::clamp((weight + 50) / 100, 1, 9);
  return kNames[static_cast<size_t>(hundred - 1)];
}

std::string composedStyleName(const SkFontStyle& style) {
  const int weight = style.weight();
  std::string name(weightName(weight));
  if (style.slant() == SkFontStyle::kItalic_Slant) name += " Italic";
  if (style.slant() == SkFontStyle::kOblique_Slant) name += " Oblique";
  return name;
}

std::string styleName(const SkTypeface& typeface) {
  std::string named = tableStyleName(typeface);
  if (named.empty()) return composedStyleName(typeface.fontStyle());
  return named;
}

}  // namespace sigil::weave::fonts
