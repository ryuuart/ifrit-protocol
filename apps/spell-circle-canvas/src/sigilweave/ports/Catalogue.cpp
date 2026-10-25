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

Style styleOf(const SkFontStyle& fontStyle, std::string name) {
  const FaceStyle faceStyle = fromSk(fontStyle);
  Style style{.weight = faceStyle.weight,
              .weightName = std::string(weightName(faceStyle.weight)),
              .width = faceStyle.width,
              .slant = faceStyle.slant,
              .name = std::move(name)};
  if (style.name.empty()) style.name = composedStyleName(fontStyle);
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
  return styleOf(typeface.fontStyle(), styleName(typeface));
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
          FontContext& context) {
  return context.familyTypeface(family, styleName);
}

}  // namespace sigil::weave::fonts
