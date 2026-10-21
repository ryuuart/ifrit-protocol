/** @file
 * The type catalogue: the font manager's families and the faces a context
 * adopted, read as named styles and axes.
 */

#include <include/core/SkData.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkString.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/ports/Catalogue.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <ranges>
#include <utility>

#include "sigilweave/advanced/Skia.h"

namespace sigil::weave::fonts {

namespace {

/** Whether @p left and @p right name one style, ASCII case and spaces
 *  aside: a font says "Demi Bold" where a writer types "DemiBold", and
 *  neither CSS nor the naming table holds a style name to one spelling. */
bool sameStyleName(std::string_view left, std::string_view right) {
  const auto letters = [](std::string_view name) {
    return name | std::views::filter([](char character) {
             return character != ' ';
           });
  };
  return std::ranges::equal(
      letters(left), letters(right),
      [](char leftCharacter, char rightCharacter) {
        return std::tolower(static_cast<unsigned char>(leftCharacter)) ==
               std::tolower(static_cast<unsigned char>(rightCharacter));
      });
}

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
std::string styleNameOf(const SkTypeface& typeface) {
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

/** The name a style goes by where its font names none: its weight's, and
 *  the slant when it leans. */
std::string composedStyleName(const Style& style) {
  std::string name(style.weightName);
  if (style.slant == FaceSlant::Italic) name += " Italic";
  if (style.slant == FaceSlant::Oblique) name += " Oblique";
  return name;
}

Style styleOf(const SkFontStyle& fontStyle, std::string name) {
  const FaceStyle faceStyle = fromSk(fontStyle);
  Style style{.weight = faceStyle.weight,
              .weightName = std::string(weightName(faceStyle.weight)),
              .width = faceStyle.width,
              .slant = faceStyle.slant,
              .name = std::move(name)};
  if (style.name.empty()) style.name = composedStyleName(style);
  return style;
}

/** Adds the axes @p face carries that @p family does not list yet. */
void gatherAxes(Family& family, const Face& face) {
  for (const VariationAxis& axis : face.variationAxes())
    if (std::ranges::none_of(family.axes, [&axis](const VariationAxis& held) {
          return std::memcmp(held.tag, axis.tag, 4) == 0;
        }))
      family.axes.push_back(axis);
}

std::optional<Family> managerFamily(SkFontMgr* manager, std::string_view name) {
  if (!manager || name.empty()) return std::nullopt;
  const std::string terminated(name);
  const sk_sp<SkFontStyleSet> set = manager->matchFamily(terminated.c_str());
  if (!set || set->count() == 0) return std::nullopt;
  Family family{.name = terminated};
  for (int index = 0; index < set->count(); ++index) {
    SkFontStyle fontStyle;
    SkString styleName;
    set->getStyle(index, &fontStyle, &styleName);
    family.styles.push_back(styleOf(fontStyle, styleName.c_str()));
    const sk_sp<SkTypeface> typeface = set->createTypeface(index);
    if (!typeface) continue;
    if (index == 0) family.name = familyName(*typeface);
    gatherAxes(family, fromSk(typeface));
  }
  return family;
}

/** The face of @p family whose style goes by @p styleName, each style
 *  named as `managerFamily` names it, so a name the family lists is a
 *  name this finds. */
Face managerFaceNamed(SkFontMgr* manager, std::string_view family,
                      std::string_view styleName) {
  if (!manager || family.empty()) return {};
  const std::string terminated(family);
  const sk_sp<SkFontStyleSet> set = manager->matchFamily(terminated.c_str());
  if (!set) return {};
  for (int index = 0; index < set->count(); ++index) {
    SkFontStyle fontStyle;
    SkString name;
    set->getStyle(index, &fontStyle, &name);
    if (sameStyleName(styleOf(fontStyle, name.c_str()).name, styleName))
      return fromSk(set->createTypeface(index));
  }
  return {};
}

/** An adopted face's style, named as its own naming table names it. */
Style adoptedStyle(const SkTypeface& typeface) {
  return styleOf(typeface.fontStyle(), styleNameOf(typeface));
}

std::optional<Family> adoptedFamily(const FontContext& context,
                                    std::string_view name) {
  if (name.empty()) return std::nullopt;
  std::optional<Family> family;
  for (const AdoptedFace& adopted : context.adoptedFaces()) {
    const sk_sp<SkTypeface> typeface = toSk(adopted.face);
    if (!typeface || !sameFamily(familyName(*typeface), name)) continue;
    if (!family)
      family = Family{.name = familyName(*typeface), .bundled = true};
    Style style = adoptedStyle(*typeface);
    style.uri = adopted.origin;
    family->styles.push_back(std::move(style));
    gatherAxes(*family, adopted.face);
  }
  return family;
}

void appendManagerFamilies(SkFontMgr* manager,
                           std::vector<std::string>& names) {
  if (!manager) return;
  const size_t adopted = names.size();
  for (int index = 0; index < manager->countFamilies(); ++index) {
    SkString name;
    manager->getFamilyName(index, &name);
    const std::string_view candidate(name.c_str(), name.size());
    if (candidate.empty()) continue;
    // An installed family an adopted one stands in front of is listed
    // once, as the adopted one.
    if (std::any_of(names.begin(), names.begin() + static_cast<long>(adopted),
                    [candidate](const std::string& held) {
                      return sameFamily(held, candidate);
                    }))
      continue;
    names.emplace_back(candidate);
  }
}

}  // namespace

std::string_view weightName(int weight) {
  static constexpr std::array<std::string_view, 9> kNames = {
      "Thin",      "Extra Light", "Light",      "Normal", "Medium",
      "Semi Bold", "Bold",        "Extra Bold", "Black"};
  const int hundred = std::clamp((weight + 50) / 100, 1, 9);
  return kNames[static_cast<size_t>(hundred - 1)];
}

std::vector<std::string> families() {
  std::vector<std::string> names;
  appendManagerFamilies(ports::systemFontManager().get(), names);
  return names;
}

std::vector<std::string> families(const FontContext& context) {
  std::vector<std::string> names;
  for (const AdoptedFace& adopted : context.adoptedFaces()) {
    const sk_sp<SkTypeface> typeface = toSk(adopted.face);
    if (!typeface) continue;
    std::string name = familyName(*typeface);
    if (std::ranges::none_of(names, [&name](const std::string& held) {
          return sameFamily(held, name);
        }))
      names.push_back(std::move(name));
  }
  appendManagerFamilies(context.fontManager(), names);
  return names;
}

std::optional<Family> family(std::string_view name) {
  return managerFamily(ports::systemFontManager().get(), name);
}

std::optional<Family> family(std::string_view name,
                             const FontContext& context) {
  if (std::optional<Family> adopted = adoptedFamily(context, name))
    return adopted;
  return managerFamily(context.fontManager(), name);
}

Face find(std::string_view family, FaceStyle style) {
  if (family.empty()) return {};
  const std::string terminated(family);
  return fromSk(ports::systemFontManager()->matchFamilyStyle(terminated.c_str(),
                                                             toSk(style)));
}

Face find(std::string_view family, FaceStyle style, FontContext& context) {
  return context.familyTypeface(family, style);
}

const Style* Family::style(std::string_view name) const {
  const auto found = std::ranges::find_if(styles, [name](const Style& held) {
    return sameStyleName(held.name, name);
  });
  return found == styles.end() ? nullptr : &*found;
}

Face find(std::string_view family, std::string_view styleName) {
  return managerFaceNamed(ports::systemFontManager().get(), family, styleName);
}

Face find(std::string_view family, std::string_view styleName,
          const FontContext& context) {
  bool adoptsFamily = false;
  for (const AdoptedFace& adopted : context.adoptedFaces()) {
    const sk_sp<SkTypeface> typeface = toSk(adopted.face);
    if (!typeface || !sameFamily(familyName(*typeface), family)) continue;
    adoptsFamily = true;
    if (sameStyleName(adoptedStyle(*typeface).name, styleName))
      return adopted.face;
  }
  if (adoptsFamily) return {};
  return managerFaceNamed(context.fontManager(), family, styleName);
}

}  // namespace sigil::weave::fonts
