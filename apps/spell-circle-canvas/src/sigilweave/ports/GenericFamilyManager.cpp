/** @file
 * CSS's generic family names, as the families this platform draws them
 * in, and the font manager that answers them.
 */

#include "sigilweave/advanced/Skia.h"
#include "GenericFamilyManager.h"

#include <include/core/SkFontStyle.h>
#include <include/core/SkStream.h>
#include <include/core/SkString.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace sigil::weave::ports {

namespace {

/** One generic name and the installed families that stand for it, most
 *  wanted first. */
struct Generic {
  std::string_view name;
  std::span<const std::string_view> families;
};

// The families a browser on each platform draws the generic names in.
#if defined(__APPLE__)
constexpr std::string_view kSerif[] = {"Times", "Times New Roman"};
constexpr std::string_view kSansSerif[] = {"Helvetica", "Helvetica Neue"};
constexpr std::string_view kMonospace[] = {"Courier", "Courier New", "Menlo"};
constexpr std::string_view kSystemInterface[] = {".AppleSystemUIFont",
                                                 "Helvetica Neue"};
#elif defined(_WIN32)
constexpr std::string_view kSerif[] = {"Times New Roman"};
constexpr std::string_view kSansSerif[] = {"Arial"};
constexpr std::string_view kMonospace[] = {"Consolas", "Courier New"};
constexpr std::string_view kSystemInterface[] = {"Segoe UI"};
#else
// Fontconfig resolves the generic names itself, so each stands for its
// own name.
constexpr std::string_view kSerif[] = {"serif"};
constexpr std::string_view kSansSerif[] = {"sans-serif"};
constexpr std::string_view kMonospace[] = {"monospace"};
constexpr std::string_view kSystemInterface[] = {"system-ui", "sans-serif"};
#endif

constexpr std::array kGenerics = {
    Generic{"serif", kSerif},
    Generic{"sans-serif", kSansSerif},
    Generic{"monospace", kMonospace},
    Generic{"system-ui", kSystemInterface},
};

/** The platform manager with the generic names answered in front of it.
 *  Every other ask is the platform's own, passed through untouched: the
 *  families it lists, the faces it reads from files, the fallback it
 *  finds for a character. */
class GenericFamilyManager final : public SkFontMgr {
 public:
  explicit GenericFamilyManager(sk_sp<SkFontMgr> platform)
      : m_platform(std::move(platform)) {}

 protected:
  int onCountFamilies() const override { return m_platform->countFamilies(); }

  void onGetFamilyName(int index, SkString* familyName) const override {
    m_platform->getFamilyName(index, familyName);
  }

  sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override {
    return m_platform->createStyleSet(index);
  }

  sk_sp<SkFontStyleSet> onMatchFamily(const char familyName[]) const override {
    if (familyName != nullptr)
      if (const auto families = genericFamilies(familyName); !families.empty())
        return firstInstalled<sk_sp<SkFontStyleSet>>(
            families, [&](const char* family) {
              sk_sp<SkFontStyleSet> set = m_platform->matchFamily(family);
              return set && set->count() > 0 ? set : nullptr;
            });
    return m_platform->matchFamily(familyName);
  }

  sk_sp<SkTypeface> onMatchFamilyStyle(
      const char familyName[], const SkFontStyle& style) const override {
    if (familyName != nullptr)
      if (const auto families = genericFamilies(familyName); !families.empty())
        return firstInstalled<sk_sp<SkTypeface>>(
            families, [&](const char* family) {
              return m_platform->matchFamilyStyle(family, style);
            });
    return m_platform->matchFamilyStyle(familyName, style);
  }

  sk_sp<SkTypeface> onMatchFamilyStyleCharacter(
      const char familyName[], const SkFontStyle& style, const char* bcp47[],
      int bcp47Count, SkUnichar character) const override {
    return m_platform->matchFamilyStyleCharacter(familyName, style, bcp47,
                                                 bcp47Count, character);
  }

  sk_sp<SkTypeface> onMatch(const Request& request) const override {
    return m_platform->match(request);
  }

  sk_sp<SkTypeface> onFallback(const Request& request) const override {
    return m_platform->fallback(request);
  }

  sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData> data,
                                   int ttcIndex) const override {
    return m_platform->makeFromData(std::move(data), ttcIndex);
  }

  sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream,
                                          int ttcIndex) const override {
    return m_platform->makeFromStream(std::move(stream), ttcIndex);
  }

  sk_sp<SkTypeface> onMakeFromStreamArgs(
      std::unique_ptr<SkStreamAsset> stream,
      const SkFontArguments& arguments) const override {
    return m_platform->makeFromStream(std::move(stream), arguments);
  }

  sk_sp<SkTypeface> onMakeFromFile(const char path[],
                                   int ttcIndex) const override {
    return m_platform->makeFromFile(path, ttcIndex);
  }

  sk_sp<SkTypeface> onLegacyMakeTypeface(const char familyName[],
                                         SkFontStyle style) const override {
    if (familyName != nullptr && !genericFamilies(familyName).empty())
      if (sk_sp<SkTypeface> found = onMatchFamilyStyle(familyName, style))
        return found;
    return m_platform->legacyMakeTypeface(familyName, style);
  }

 private:
  /** The first answer @p ask gives over @p families, each name handed to
   *  the platform as the C string it reads. */
  template <class Answer, class Ask>
  static Answer firstInstalled(std::span<const std::string_view> families,
                               const Ask& ask) {
    std::string name;
    for (const std::string_view family : families) {
      name.assign(family);
      if (Answer found = ask(name.c_str())) return found;
    }
    return nullptr;
  }

  sk_sp<SkFontMgr> m_platform;
};

}  // namespace

std::span<const std::string_view> genericFamilies(std::string_view name) {
  // A generic name is a keyword, and CSS reads keywords without regard to
  // ASCII case.
  const auto same = [](std::string_view keyword, std::string_view name) {
    return std::equal(keyword.begin(), keyword.end(), name.begin(), name.end(),
                      [](char left, char right) {
                        const auto lower = [](char c) {
                          return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
                        };
                        return lower(left) == lower(right);
                      });
  };
  for (const Generic& generic : kGenerics)
    if (same(generic.name, name)) return generic.families;
  return {};
}

namespace detail {

sk_sp<SkFontMgr> answeringGenericFamilies(sk_sp<SkFontMgr> platform) {
  if (!platform) return platform;
  return sk_make_sp<GenericFamilyManager>(std::move(platform));
}

}  // namespace detail

}  // namespace sigil::weave::ports
