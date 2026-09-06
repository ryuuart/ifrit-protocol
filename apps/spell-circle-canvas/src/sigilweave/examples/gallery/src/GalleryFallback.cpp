/** @file
 * The gallery's own fallback resolver.
 */

#include "GalleryFallback.h"

#include <include/core/SkString.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/qt/SigilWeaveQt.h>

#include <QFont>
#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gallery {

sk_sp<SkTypeface> resolveGalleryTypeface(SkFontMgr* fontManager,
                                         const QString& family) {
  if (!fontManager || family.isEmpty()) return nullptr;
  return sigil::weave::qt::toSkTypeface(fontManager, QFont(family));
}

namespace {

constexpr std::string_view kNotoSerifPrefix = "Noto Serif";
constexpr std::string_view kNotoCuneiformFamily = "Noto Sans Cuneiform";
constexpr const char* kSystemNotoCuneiformPath =
    "/System/Library/Fonts/Supplemental/NotoSansCuneiform-Regular.ttf";

bool isCuneiform(int32_t codePoint) {
  return codePoint >= 0x12000 && codePoint <= 0x1254F;
}

bool startsWith(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         text.compare(0, prefix.size(), prefix) == 0;
}

std::string typefaceFamily(const SkTypeface& typeface) {
  SkString family;
  typeface.getFamilyName(&family);
  return {family.c_str(), family.size()};
}

std::string_view preferredCjkSerif(std::string_view languageTag) {
  if (startsWith(languageTag, "ja")) return "Noto Serif JP";
  if (startsWith(languageTag, "ko")) return "Noto Serif KR";
  if (startsWith(languageTag, "zh-Hant") || startsWith(languageTag, "zh-TW"))
    return "Noto Serif TC";
  if (startsWith(languageTag, "zh-HK")) return "Noto Serif HK";
  if (startsWith(languageTag, "zh")) return "Noto Serif SC";
  return {};
}

sk_sp<SkTypeface> resolvePlatformFallback(SkFontMgr& fontManager,
                                          const SkTypeface& primaryTypeface,
                                          int32_t codePoint,
                                          std::string_view languageTag) {
  const std::string language(languageTag);
  const char* languageTags[] = {language.c_str()};
  return fontManager.matchFamilyStyleCharacter(
      nullptr, primaryTypeface.fontStyle(),
      language.empty() ? nullptr : languageTags, language.empty() ? 0 : 1,
      codePoint);
}

}  // namespace

sigil::weave::FontContext::FallbackResolver makeGalleryFallbackResolver(
    SkFontMgr& fontManager) {
  std::vector<std::string> serifFamilies;
  sk_sp<SkTypeface> cuneiformTypeface =
      fontManager.makeFromFile(kSystemNotoCuneiformPath);
  const int familyCount = fontManager.countFamilies();
  serifFamilies.reserve(static_cast<size_t>(std::max(0, familyCount)));
  for (int familyIndex = 0; familyIndex < familyCount; ++familyIndex) {
    SkString family;
    fontManager.getFamilyName(familyIndex, &family);
    const std::string_view familyName(family.c_str(), family.size());
    if (startsWith(familyName, kNotoSerifPrefix))
      serifFamilies.emplace_back(familyName);
    else if (!cuneiformTypeface && familyName == kNotoCuneiformFamily)
      cuneiformTypeface = fontManager.matchFamilyStyle(
          std::string(kNotoCuneiformFamily).c_str(), SkFontStyle());
  }
  std::sort(serifFamilies.begin(), serifFamilies.end());
  serifFamilies.erase(std::unique(serifFamilies.begin(), serifFamilies.end()),
                      serifFamilies.end());

  return [serifFamilies = std::move(serifFamilies),
          cuneiformTypeface = std::move(cuneiformTypeface)](
             SkFontMgr& manager, const SkTypeface& primaryTypeface,
             int32_t codePoint,
             std::string_view languageTag) -> sk_sp<SkTypeface> {
    // macOS ships Noto Sans Cuneiform, but CoreText can resolve its character
    // map and rasterizer from different copies when a newer user-installed
    // font has the same PostScript name. Loading the system file directly
    // keeps HarfBuzz's glyph IDs and Skia's outlines from the same face.
    if (isCuneiform(codePoint) && cuneiformTypeface &&
        cuneiformTypeface->unicharToGlyph(codePoint) != 0)
      return cuneiformTypeface;

    const std::string primaryFamily = typefaceFamily(primaryTypeface);
    if (!startsWith(primaryFamily, kNotoSerifPrefix))
      return resolvePlatformFallback(manager, primaryTypeface, codePoint,
                                     languageTag);

    auto tryFamily = [&](std::string_view familyName) -> sk_sp<SkTypeface> {
      if (familyName.empty() || familyName == primaryFamily) return nullptr;
      const std::string terminatedFamilyName(familyName);
      sk_sp<SkTypeface> candidate = manager.matchFamilyStyle(
          terminatedFamilyName.c_str(), primaryTypeface.fontStyle());
      if (candidate && candidate->unicharToGlyph(codePoint) != 0)
        return candidate;
      return nullptr;
    };

    const std::string_view preferredFamily = preferredCjkSerif(languageTag);
    if (sk_sp<SkTypeface> preferred = tryFamily(preferredFamily))
      return preferred;
    for (const std::string& family : serifFamilies) {
      if (family != preferredFamily) {
        if (sk_sp<SkTypeface> candidate = tryFamily(family)) return candidate;
      }
    }
    return resolvePlatformFallback(manager, primaryTypeface, codePoint,
                                   languageTag);
  };
}

}  // namespace gallery
